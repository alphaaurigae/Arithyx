#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "mathhunter/certificate/bigint_codec.hpp"
#include "mathhunter/certificate/certificate.hpp"
#include "mathhunter/conjectures/kurepa.hpp"
#include "mathhunter/framework/backend_concepts.hpp"
#include "mathhunter/framework/cli_args.hpp"
#include "mathhunter/framework/module_registry.hpp"
#include "mathhunter/framework/range_partition.hpp"
#include "mathhunter/verifier/kurepa_verifier.hpp"
#include "mathhunter/verifier/verification_backend.hpp"

namespace mathhunter::conjectures::kurepa {

namespace {

using namespace mathhunter;                    // NOLINT
using framework::cli_args::parse_integer_flag;  // NOLINT
using framework::cli_args::parse_string_flag;   // NOLINT

static_assert(framework::RangeSearchDiscovery<decltype(&search_range), arithmetic::Integer, Witness>);
static_assert(verifier::CounterexampleVerifier<verifier::KurepaVerifier, Witness, Evidence>);
static_assert(verifier::BoundedResultVerifier<verifier::KurepaVerifier, arithmetic::Integer>);

constexpr std::string_view kModuleId = "kurepa";

core::EncodedBlob encode_range_partition(const arithmetic::Integer& begin,
                                          const arithmetic::Integer& end) {
    core::ByteWriter w;
    certificate::put_nonneg_integer(w, begin);
    certificate::put_nonneg_integer(w, end);
    return core::EncodedBlob{.schema_version = 1, .bytes = std::move(w).take()};
}

std::expected<std::pair<arithmetic::Integer, arithmetic::Integer>, std::string>
decode_range_partition(const core::EncodedBlob& blob) {
    core::ByteReader r(blob.bytes);
    auto begin = certificate::get_nonneg_integer(r);
    if (!begin) return std::unexpected("malformed kurepa work unit: bad begin");
    auto end = certificate::get_nonneg_integer(r);
    if (!end) return std::unexpected("malformed kurepa work unit: bad end");
    if (auto e = r.expect_end(); !e) return std::unexpected("malformed kurepa work unit: trailing bytes");
    return std::make_pair(*begin, *end);
}

std::expected<framework::WorkUnitKey, std::string> kurepa_make_from_cli(
    const std::vector<std::string>& args) {
    const auto begin = parse_integer_flag(args, "--begin");
    const auto end = parse_integer_flag(args, "--end");
    if (!begin || !end || *begin >= *end) {
        return std::unexpected("work kurepa requires --begin N --end N with begin < end");
    }
    return framework::WorkUnitKey{.module_id = std::string(kModuleId),
                                   .partition = encode_range_partition(*begin, *end)};
}

framework::WorkUnitCost kurepa_estimate_cost(const framework::WorkUnitKey& key) {
    auto range = decode_range_partition(key.partition);
    if (!range) return framework::WorkUnitCost{.units = 0};
    return framework::WorkUnitCost{
        .units = framework::saturating_range_size_u64(range->first, range->second)};
}

std::vector<framework::WorkUnitKey> kurepa_partition(const framework::WorkUnitKey& key,
                                                       std::uint64_t target_units) {
    auto range = decode_range_partition(key.partition);
    if (!range) return {key};
    const auto chunks = framework::partition_integer_range(range->first, range->second, target_units);
    std::vector<framework::WorkUnitKey> out;
    out.reserve(chunks.size());
    for (const auto& [chunk_begin, chunk_end] : chunks) {
        out.push_back(framework::WorkUnitKey{.module_id = std::string(kModuleId),
                                               .partition = encode_range_partition(chunk_begin, chunk_end)});
    }
    return out;
}

framework::WorkUnitOutcome kurepa_run(const framework::WorkUnitKey& key) {
    auto range = decode_range_partition(key.partition);
    if (!range) {
        return framework::WorkUnitOutcome{.kind = framework::WorkUnitOutcomeKind::Anomaly,
                                           .certificate_kind = std::nullopt,
                                           .payload = {},
                                           .diagnostic = range.error()};
    }
    const auto outcome = search_range(range->first, range->second);
    if (std::holds_alternative<core::CandidateCounterexample<Witness>>(outcome)) {
        const auto candidate = std::get<core::CandidateCounterexample<Witness>>(outcome);
        return framework::WorkUnitOutcome{.kind = framework::WorkUnitOutcomeKind::Hit,
                                           .certificate_kind =
                                               certificate::CertificateKind::KurepaCounterexample,
                                           .payload = encode_counterexample_payload(candidate),
                                           .diagnostic = {}};
    }
    const auto bounded = std::get<core::BoundedSearchResult<arithmetic::Integer>>(outcome);
    return framework::WorkUnitOutcome{.kind = framework::WorkUnitOutcomeKind::BoundedCompletion,
                                       .certificate_kind =
                                           certificate::CertificateKind::KurepaBoundedResult,
                                       .payload = encode_bounded_result_payload(bounded),
                                       .diagnostic = {}};
}

framework::SilentVerifyOutcome verify_counterexample_quiet(std::span<const std::uint8_t> payload) {
    auto decoded = decode_counterexample_payload(payload);
    if (!decoded) return framework::SilentVerifyOutcome::Rejected;
    verifier::KurepaVerifier kv;
    auto result = kv.verify_counterexample(decoded->witness);
    if (result) return framework::SilentVerifyOutcome::Verified;
    if (result.error() == verifier::VerificationFailure::DomainTooLargeForVerifier) {
        return framework::SilentVerifyOutcome::Unverifiable;
    }
    return framework::SilentVerifyOutcome::Rejected;
}

framework::SilentVerifyOutcome verify_bounded_result_quiet(std::span<const std::uint8_t> payload) {
    auto decoded = decode_bounded_result_payload(payload);
    if (!decoded) return framework::SilentVerifyOutcome::Rejected;
    verifier::KurepaVerifier kv;
    auto result = kv.verify_bounded_result(*decoded);
    if (result) return framework::SilentVerifyOutcome::Verified;
    if (result.error() == verifier::VerificationFailure::DomainTooLargeForVerifier) {
        return framework::SilentVerifyOutcome::Unverifiable;
    }
    return framework::SilentVerifyOutcome::Rejected;
}

int run_search(const std::vector<std::string>& args) {
    const auto begin = parse_integer_flag(args, "--begin");
    const auto end = parse_integer_flag(args, "--end");
    if (!begin || !end || *begin >= *end) {
        std::cerr << "search kurepa requires --begin N --end N with begin < end\n";
        return 1;
    }

    std::cout << "Searching Kurepa's left factorial conjecture over odd primes in [" << *begin
               << ", " << *end << ")...\n";
    const auto outcome = search_range(*begin, *end);
    verifier::KurepaVerifier kv;

    if (std::holds_alternative<core::CandidateCounterexample<Witness>>(outcome)) {
        const auto candidate = std::get<core::CandidateCounterexample<Witness>>(outcome);
        std::cout << "CANDIDATE COUNTEREXAMPLE FOUND: p = " << candidate.witness.p << "\n";

        const auto out_path =
            parse_string_flag(args, "--out")
                .value_or("kurepa_counterexample_" + candidate.witness.p.get_str() + ".mhcert");
        auto payload = encode_counterexample_payload(candidate);
        auto write_status = certificate::write_certificate_file(
            out_path, core::ClaimId::KurepaLeftFactorial,
            certificate::CertificateKind::KurepaCounterexample, payload);
        if (!write_status) {
            std::cerr << "Failed to write certificate: " << out_path << "\n";
            return 2;
        }
        std::cout << "Certificate written: " << out_path << "\n";

        auto verified = kv.verify_counterexample(candidate.witness);
        if (verified) {
            std::cout << "VERIFIED COUNTEREXAMPLE (independently re-derived; residue = "
                       << verified->get().evidence.residue << ")\n";
            std::cout << "*** This would refute Kurepa's conjecture. Given how well-tested this "
                         "conjecture is, treat this as an extraordinary claim: re-run with an "
                         "independently authored verifier before announcing anything. ***\n";
        } else {
            std::cout << "VERIFICATION FAILED: " << verifier::to_string(verified.error()) << "\n";
            std::cout << "This candidate is NOT a verified counterexample. Likely an internal "
                         "inconsistency between search and verification logic; please file a "
                         "bug rather than trust the search output.\n";
        }
        return 0;
    }

    const auto bounded = std::get<core::BoundedSearchResult<arithmetic::Integer>>(outcome);
    std::cout << "NO COUNTEREXAMPLE FOUND in [" << bounded.begin << ", " << bounded.end
               << ") (odd primes tested: " << bounded.candidates_tested << ")\n";
    std::cout << "This is a BOUNDED result. It says nothing about primes outside this interval.\n";

    const auto out_path = parse_string_flag(args, "--out").value_or(
        "kurepa_bounded_" + bounded.begin.get_str() + "_" + bounded.end.get_str() + ".mhcert");
    auto payload = encode_bounded_result_payload(bounded);
    auto write_status = certificate::write_certificate_file(
        out_path, core::ClaimId::KurepaLeftFactorial,
        certificate::CertificateKind::KurepaBoundedResult, payload);
    if (!write_status) {
        std::cerr << "Failed to write certificate: " << out_path << "\n";
        return 2;
    }
    std::cout << "Certificate written: " << out_path << "\n";

    auto verified = kv.verify_bounded_result(bounded);
    if (verified) {
        std::cout << "VERIFIED BOUNDED RESULT (independently re-derived from scratch)\n";
    } else {
        std::cout << "VERIFICATION FAILED: " << verifier::to_string(verified.error()) << "\n";
    }
    return 0;
}

int run_benchmark() {
    using clock = std::chrono::steady_clock;
    const auto start = clock::now();
    const auto outcome = search_range(3, 20000);
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count();
    (void)outcome;
    std::cout << "kurepa: searched [3, 20000) in " << ms << " ms\n";
    return 0;
}

void inspect_counterexample(std::span<const std::uint8_t> payload) {
    auto decoded = decode_counterexample_payload(payload);
    if (decoded) {
        std::cout << "witness p:      " << decoded->witness.p << "\n";
        std::cout << "discovered_by:  " << decoded->discovered_by << "\n";
    }
}

int verify_counterexample(std::span<const std::uint8_t> payload) {
    auto decoded = decode_counterexample_payload(payload);
    if (!decoded) {
        std::cout << "REJECTED: malformed payload\n";
        return 2;
    }
    verifier::KurepaVerifier kv;
    auto result = kv.verify_counterexample(decoded->witness);
    if (result) {
        std::cout << "VERIFIED COUNTEREXAMPLE: p = " << decoded->witness.p
                   << " (residue = " << result->get().evidence.residue << ")\n";
        return 0;
    }
    std::cout << "REJECTED: " << verifier::to_string(result.error()) << "\n";
    return 1;
}

void inspect_bounded_result(std::span<const std::uint8_t> payload) {
    auto decoded = decode_bounded_result_payload(payload);
    if (decoded) {
        std::cout << "range:          [" << decoded->begin << ", " << decoded->end << ")\n";
        std::cout << "tested:         " << decoded->candidates_tested << "\n";
        std::cout << "algorithm:      " << decoded->algorithm_version << "\n";
    }
}

int verify_bounded_result(std::span<const std::uint8_t> payload) {
    auto decoded = decode_bounded_result_payload(payload);
    if (!decoded) {
        std::cout << "REJECTED: malformed payload\n";
        return 2;
    }
    verifier::KurepaVerifier kv;
    auto result = kv.verify_bounded_result(*decoded);
    if (result) {
        std::cout << "VERIFIED BOUNDED RESULT: no counterexample in [" << decoded->begin << ", "
                   << decoded->end << ")\n";
        return 0;
    }
    std::cout << "REJECTED: " << verifier::to_string(result.error()) << "\n";
    return 1;
}

void register_module() {
    framework::ModuleDescriptor descriptor;
    descriptor.id = "kurepa";
    descriptor.claim = core::ClaimId::KurepaLeftFactorial;
    descriptor.search_usage = "search kurepa --begin N --end N [--out FILE]";
    descriptor.run_search = &run_search;
    descriptor.run_benchmark = &run_benchmark;
    descriptor.kinds.push_back({.kind = certificate::CertificateKind::KurepaCounterexample,
                                 .inspect = &inspect_counterexample,
                                 .verify = &verify_counterexample,
                                 .verify_quiet = &verify_counterexample_quiet});
    descriptor.kinds.push_back({.kind = certificate::CertificateKind::KurepaBoundedResult,
                                 .inspect = &inspect_bounded_result,
                                 .verify = &verify_bounded_result,
                                 .verify_quiet = &verify_bounded_result_quiet});
    descriptor.work_unit = framework::WorkUnitOps{.make_from_cli = &kurepa_make_from_cli,
                                                    .estimate_cost = &kurepa_estimate_cost,
                                                    .partition = &kurepa_partition,
                                                    .run = &kurepa_run};
    framework::registry().register_module(std::move(descriptor));
}

}  // namespace

// True self-registration (docs/GUIDANCE.md: "module registers itself via
// static init"). Nothing else in the program calls anything in this
// translation unit, so it only ends up in the final binary because every
// executable that links mathhunter_core does so with --whole-archive (see
// top-level CMakeLists.txt); this constructor is what actually runs.
[[gnu::constructor(1001)]] void mathhunter_kurepa_self_register() { register_module(); }

}  // namespace mathhunter::conjectures::kurepa
