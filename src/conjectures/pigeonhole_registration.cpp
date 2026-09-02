#include <chrono>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "mathhunter/certificate/certificate.hpp"
#include "mathhunter/conjectures/pigeonhole.hpp"
#include "mathhunter/core/hash.hpp"
#include "mathhunter/framework/backend_concepts.hpp"
#include "mathhunter/framework/cli_args.hpp"
#include "mathhunter/framework/module_registry.hpp"
#include "mathhunter/verifier/pigeonhole_verifier.hpp"
#include "mathhunter/verifier/verification_backend.hpp"

namespace mathhunter::conjectures::pigeonhole {

namespace {

using namespace mathhunter;                    // NOLINT
using framework::cli_args::parse_string_flag;  // NOLINT
using framework::cli_args::parse_u64_flag;      // NOLINT

static_assert(framework::KeyedProofSearchDiscovery<decltype(&search_unsat_benchmark),
                                                     std::uint32_t, UnsatProof, std::uint32_t>);
static_assert(verifier::UnsatProofVerifier<verifier::PigeonholeVerifier, UnsatProof>);

constexpr std::string_view kModuleId = "pigeonhole";

core::EncodedBlob encode_pigeon_partition(std::uint32_t pigeons) {
    core::ByteWriter w;
    w.put_le<std::uint32_t>(pigeons);
    return core::EncodedBlob{.schema_version = 1, .bytes = std::move(w).take()};
}

std::expected<std::uint32_t, std::string> decode_pigeon_partition(const core::EncodedBlob& blob) {
    core::ByteReader r(blob.bytes);
    auto pigeons = r.get_le<std::uint32_t>();
    if (!pigeons) return std::unexpected("malformed pigeonhole work unit: bad pigeon count");
    if (auto e = r.expect_end(); !e) {
        return std::unexpected("malformed pigeonhole work unit: trailing bytes");
    }
    return *pigeons;
}

std::expected<framework::WorkUnitKey, std::string> pigeonhole_make_from_cli(
    const std::vector<std::string>& args) {
    const auto pigeons = parse_u64_flag(args, "--pigeons");
    if (!pigeons || *pigeons == 0 || *pigeons > std::numeric_limits<std::uint32_t>::max()) {
        return std::unexpected("work pigeonhole requires --pigeons N (N >= 1)");
    }
    return framework::WorkUnitKey{
        .module_id = std::string(kModuleId),
        .partition = encode_pigeon_partition(static_cast<std::uint32_t>(*pigeons))};
}

framework::WorkUnitCost pigeonhole_estimate_cost(const framework::WorkUnitKey& key) {
    auto pigeons = decode_pigeon_partition(key.partition);
    return framework::WorkUnitCost{.units = pigeons ? *pigeons : 0};
}

std::vector<framework::WorkUnitKey> pigeonhole_partition(const framework::WorkUnitKey& key,
                                                           std::uint64_t /*target_units*/) {
    return {key};
}

framework::WorkUnitOutcome pigeonhole_run(const framework::WorkUnitKey& key) {
    auto pigeons = decode_pigeon_partition(key.partition);
    if (!pigeons) {
        return framework::WorkUnitOutcome{.kind = framework::WorkUnitOutcomeKind::Anomaly,
                                           .certificate_kind = std::nullopt,
                                           .payload = {},
                                           .diagnostic = pigeons.error()};
    }
    const auto outcome = search_unsat_benchmark(*pigeons);
    if (std::holds_alternative<core::Observation<std::uint32_t>>(outcome)) {
        const auto obs = std::get<core::Observation<std::uint32_t>>(outcome);
        return framework::WorkUnitOutcome{.kind = framework::WorkUnitOutcomeKind::Anomaly,
                                           .certificate_kind = std::nullopt,
                                           .payload = {},
                                           .diagnostic = obs.note};
    }
    const auto candidate = std::get<core::CandidateProof<UnsatProof>>(outcome);
    return framework::WorkUnitOutcome{.kind = framework::WorkUnitOutcomeKind::BoundedCompletion,
                                       .certificate_kind =
                                           certificate::CertificateKind::PigeonholeUnsatResult,
                                       .payload = encode_unsat_proof_payload(candidate),
                                       .diagnostic = {}};
}

framework::SilentVerifyOutcome verify_unsat_result_quiet(std::span<const std::uint8_t> payload) {
    auto decoded = decode_unsat_proof_payload(payload);
    if (!decoded) return framework::SilentVerifyOutcome::Rejected;
    verifier::PigeonholeVerifier pv;
    auto result = pv.verify_unsat(decoded->proof);
    if (result) return framework::SilentVerifyOutcome::Verified;
    if (result.error() == verifier::VerificationFailure::DomainTooLargeForVerifier) {
        return framework::SilentVerifyOutcome::Unverifiable;
    }
    return framework::SilentVerifyOutcome::Rejected;
}

int run_search(const std::vector<std::string>& args) {
    const auto pigeons = parse_u64_flag(args, "--pigeons");
    if (!pigeons || *pigeons == 0) {
        std::cerr << "search pigeonhole requires --pigeons N (N >= 1)\n";
        return 1;
    }

    std::cout << "Solving PHP(" << *pigeons << ") with the DPLL search backend...\n";
    const auto outcome = search_unsat_benchmark(static_cast<std::uint32_t>(*pigeons));

    if (std::holds_alternative<core::Observation<std::uint32_t>>(outcome)) {
        const auto obs = std::get<core::Observation<std::uint32_t>>(outcome);
        std::cout << "ANOMALY (not a mathematical result): " << obs.note << "\n";
        return 3;
    }

    const auto candidate = std::get<core::CandidateProof<UnsatProof>>(outcome);
    std::cout << "CANDIDATE UNSAT PROOF: PHP(" << candidate.proof.pigeons
               << ") is unsatisfiable (solver-reported; unverified)\n";

    const auto out_path = parse_string_flag(args, "--out").value_or(
        "pigeonhole_unsat_" + std::to_string(candidate.proof.pigeons) + ".mhcert");
    auto payload = encode_unsat_proof_payload(candidate);
    auto write_status = certificate::write_certificate_file(
        out_path, core::ClaimId::PigeonholeUnsat,
        certificate::CertificateKind::PigeonholeUnsatResult, payload);
    if (!write_status) {
        std::cerr << "Failed to write certificate: " << out_path << "\n";
        return 2;
    }
    std::cout << "Certificate written: " << out_path << "\n";

    verifier::PigeonholeVerifier pv;
    auto verified = pv.verify_unsat(candidate.proof);
    if (verified) {
        std::cout << "VERIFIED UNSAT (independent brute-force enumeration confirmed no "
                     "satisfying assignment exists)\n";
    } else if (verified.error() == verifier::VerificationFailure::DomainTooLargeForVerifier) {
        std::cout << "VERIFICATION SKIPPED: " << verifier::to_string(verified.error()) << "\n";
    } else {
        std::cout << "VERIFICATION FAILED: " << verifier::to_string(verified.error()) << "\n";
    }
    return 0;
}

int run_benchmark() {
    using clock = std::chrono::steady_clock;
    const auto start = clock::now();
    const auto outcome = search_unsat_benchmark(6);
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count();
    (void)outcome;
    std::cout << "pigeonhole: solved PHP(6) in " << ms << " ms\n";
    return 0;
}

void inspect_unsat_result(std::span<const std::uint8_t> payload) {
    auto decoded = decode_unsat_proof_payload(payload);
    if (decoded) {
        std::cout << "pigeons:        " << decoded->proof.pigeons << "\n";
        std::cout << "formula_hash:   " << core::to_hex(decoded->proof.formula_hash) << "\n";
    }
}

int verify_unsat_result(std::span<const std::uint8_t> payload) {
    auto decoded = decode_unsat_proof_payload(payload);
    if (!decoded) {
        std::cout << "REJECTED: malformed payload\n";
        return 2;
    }
    verifier::PigeonholeVerifier pv;
    auto result = pv.verify_unsat(decoded->proof);
    if (result) {
        std::cout << "VERIFIED UNSAT: PHP(" << decoded->proof.pigeons << ")\n";
        return 0;
    }
    if (result.error() == verifier::VerificationFailure::DomainTooLargeForVerifier) {
        std::cout << "UNVERIFIABLE (not rejected): " << verifier::to_string(result.error())
                   << "\n";
        return 3;
    }
    std::cout << "REJECTED: " << verifier::to_string(result.error()) << "\n";
    return 1;
}

void register_module() {
    framework::ModuleDescriptor descriptor;
    descriptor.id = "pigeonhole";
    descriptor.claim = core::ClaimId::PigeonholeUnsat;
    descriptor.search_usage = "search pigeonhole --pigeons N [--out FILE]";
    descriptor.run_search = &run_search;
    descriptor.run_benchmark = &run_benchmark;
    descriptor.kinds.push_back({.kind = certificate::CertificateKind::PigeonholeUnsatResult,
                                 .inspect = &inspect_unsat_result,
                                 .verify = &verify_unsat_result,
                                 .verify_quiet = &verify_unsat_result_quiet});
    descriptor.work_unit = framework::WorkUnitOps{.make_from_cli = &pigeonhole_make_from_cli,
                                                    .estimate_cost = &pigeonhole_estimate_cost,
                                                    .partition = &pigeonhole_partition,
                                                    .run = &pigeonhole_run};
    framework::registry().register_module(std::move(descriptor));
}

}  // namespace

[[gnu::constructor(1003)]] void mathhunter_pigeonhole_self_register() { register_module(); }

}  // namespace mathhunter::conjectures::pigeonhole
