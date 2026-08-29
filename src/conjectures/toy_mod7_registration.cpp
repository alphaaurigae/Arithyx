#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "mathhunter/certificate/certificate.hpp"
#include "mathhunter/conjectures/toy_mod7.hpp"
#include "mathhunter/framework/backend_concepts.hpp"
#include "mathhunter/framework/cli_args.hpp"
#include "mathhunter/framework/module_registry.hpp"
#include "mathhunter/verifier/toy_mod7_verifier.hpp"
#include "mathhunter/verifier/verification_backend.hpp"

namespace mathhunter::conjectures::toy_mod7 {

namespace {

using namespace mathhunter;                // NOLINT
using framework::cli_args::parse_string_flag;  // NOLINT
using framework::cli_args::parse_u64_flag;      // NOLINT

static_assert(framework::RangeSearchDiscovery<decltype(&search_range), std::uint64_t, Witness>);
static_assert(verifier::CounterexampleVerifier<verifier::ToyMod7Verifier, Witness, Evidence>);
static_assert(verifier::BoundedResultVerifier<verifier::ToyMod7Verifier, std::uint64_t>);

int run_search(const std::vector<std::string>& args) {
    const auto begin = parse_u64_flag(args, "--begin");
    const auto end = parse_u64_flag(args, "--end");
    if (!begin || !end || *begin >= *end) {
        std::cerr << "search toy_mod7 requires --begin N --end N with begin < end\n";
        return 1;
    }

    std::cout << "Searching toy_mod7 in [" << *begin << ", " << *end << ")...\n";
    const auto outcome = search_range(*begin, *end);
    verifier::ToyMod7Verifier tv;

    if (std::holds_alternative<core::CandidateCounterexample<Witness>>(outcome)) {
        const auto candidate = std::get<core::CandidateCounterexample<Witness>>(outcome);
        std::cout << "CANDIDATE COUNTEREXAMPLE FOUND: n = " << candidate.witness.n << "\n";

        const auto out_path = parse_string_flag(args, "--out")
                                   .value_or("toy_mod7_counterexample_" +
                                             std::to_string(candidate.witness.n) + ".mhcert");
        auto payload = encode_counterexample_payload(candidate);
        auto write_status = certificate::write_certificate_file(
            out_path, core::ClaimId::ToyMod7, certificate::CertificateKind::ToyMod7Counterexample,
            payload);
        if (!write_status) {
            std::cerr << "Failed to write certificate: " << out_path << "\n";
            return 2;
        }
        std::cout << "Certificate written: " << out_path << "\n";

        auto verified = tv.verify_counterexample(candidate.witness);
        std::cout << (verified ? "VERIFIED COUNTEREXAMPLE\n"
                                : "VERIFICATION FAILED: " +
                                      std::string(verifier::to_string(verified.error())) + "\n");
        return 0;
    }

    const auto bounded = std::get<core::BoundedSearchResult<std::uint64_t>>(outcome);
    std::cout << "NO COUNTEREXAMPLE FOUND in [" << bounded.begin << ", " << bounded.end << ")\n";

    const auto out_path = parse_string_flag(args, "--out")
                               .value_or("toy_mod7_bounded_" + std::to_string(bounded.begin) +
                                         "_" + std::to_string(bounded.end) + ".mhcert");
    auto payload = encode_bounded_result_payload(bounded);
    auto write_status = certificate::write_certificate_file(
        out_path, core::ClaimId::ToyMod7, certificate::CertificateKind::ToyMod7BoundedResult,
        payload);
    if (!write_status) {
        std::cerr << "Failed to write certificate: " << out_path << "\n";
        return 2;
    }
    std::cout << "Certificate written: " << out_path << "\n";

    auto verified = tv.verify_bounded_result(bounded);
    std::cout << (verified ? "VERIFIED BOUNDED RESULT\n"
                            : "VERIFICATION FAILED: " +
                                  std::string(verifier::to_string(verified.error())) + "\n");
    return 0;
}

int run_benchmark() {
    using clock = std::chrono::steady_clock;
    const auto start = clock::now();
    const auto outcome = search_range(1, 10'000'000);
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count();
    (void)outcome;
    std::cout << "toy_mod7: searched [1, 10000000) in " << ms << " ms\n";
    return 0;
}

void inspect_counterexample(std::span<const std::uint8_t> payload) {
    auto decoded = decode_counterexample_payload(payload);
    if (decoded) {
        std::cout << "witness n:      " << decoded->witness.n << "\n";
    }
}

int verify_counterexample(std::span<const std::uint8_t> payload) {
    auto decoded = decode_counterexample_payload(payload);
    if (!decoded) {
        std::cout << "REJECTED: malformed payload\n";
        return 2;
    }
    verifier::ToyMod7Verifier tv;
    auto result = tv.verify_counterexample(decoded->witness);
    if (result) {
        std::cout << "VERIFIED COUNTEREXAMPLE: n = " << decoded->witness.n << "\n";
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
    }
}

int verify_bounded_result(std::span<const std::uint8_t> payload) {
    auto decoded = decode_bounded_result_payload(payload);
    if (!decoded) {
        std::cout << "REJECTED: malformed payload\n";
        return 2;
    }
    verifier::ToyMod7Verifier tv;
    auto result = tv.verify_bounded_result(*decoded);
    if (result) {
        std::cout << "VERIFIED BOUNDED RESULT\n";
        return 0;
    }
    std::cout << "REJECTED: " << verifier::to_string(result.error()) << "\n";
    return 1;
}

void register_module() {
    framework::ModuleDescriptor descriptor;
    descriptor.id = "toy_mod7";
    descriptor.claim = core::ClaimId::ToyMod7;
    descriptor.search_usage = "search toy_mod7 --begin N --end N [--out FILE]";
    descriptor.run_search = &run_search;
    descriptor.run_benchmark = &run_benchmark;
    descriptor.kinds.push_back({.kind = certificate::CertificateKind::ToyMod7Counterexample,
                                 .inspect = &inspect_counterexample,
                                 .verify = &verify_counterexample});
    descriptor.kinds.push_back({.kind = certificate::CertificateKind::ToyMod7BoundedResult,
                                 .inspect = &inspect_bounded_result,
                                 .verify = &verify_bounded_result});
    framework::registry().register_module(std::move(descriptor));
}

}  // namespace

[[gnu::constructor(1002)]] void mathhunter_toy_mod7_self_register() { register_module(); }

}  // namespace mathhunter::conjectures::toy_mod7
