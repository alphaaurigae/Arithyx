#include <algorithm>
#include <chrono>
#include <cctype>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "mathhunter/certificate/certificate.hpp"
#include "mathhunter/conjectures/kurepa.hpp"
#include "mathhunter/conjectures/pigeonhole.hpp"
#include "mathhunter/conjectures/toy_mod7.hpp"
#include "mathhunter/verifier/kurepa_verifier.hpp"
#include "mathhunter/verifier/pigeonhole_verifier.hpp"
#include "mathhunter/verifier/toy_mod7_verifier.hpp"
#include "metadata_loader.hpp"

namespace {

using namespace mathhunter;

void print_usage() {
    std::cout <<
        "math-hunter -- computational conjecture search (discovery only; never trust "
        "this program's opinion of its own results -- use math-verifier)\n\n"
        "Usage:\n"
        "  math-hunter list\n"
        "  math-hunter info <conjecture>\n"
        "  math-hunter search kurepa --begin N --end N [--out FILE]\n"
        "  math-hunter search toy_mod7 --begin N --end N [--out FILE]\n"
        "  math-hunter search pigeonhole --pigeons N [--out FILE]\n"
        "  math-hunter benchmark <conjecture>\n";
}

std::optional<std::uint64_t> parse_u64_flag(const std::vector<std::string>& args,
                                              std::string_view flag) {
    for (std::size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == flag) {
            try {
                return std::stoull(args[i + 1]);
            } catch (...) {
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

std::optional<arithmetic::Integer> parse_integer_flag(const std::vector<std::string>& args,
                                                        std::string_view flag) {
    for (std::size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == flag) {
            const std::string& value = args[i + 1];
            if (value.empty() ||
                !std::ranges::all_of(value, [](unsigned char c) { return std::isdigit(c) != 0; })) {
                return std::nullopt;
            }
            try {
                return arithmetic::Integer(value);
            } catch (const std::invalid_argument&) {
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

std::optional<std::string> parse_string_flag(const std::vector<std::string>& args,
                                              std::string_view flag) {
    for (std::size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == flag) {
            return args[i + 1];
        }
    }
    return std::nullopt;
}

int cmd_list() {
    const auto ids = cli::list_conjecture_identifiers();
    if (ids.empty()) {
        std::cerr << "No conjecture metadata found. Set MATHHUNTER_METADATA_DIR or run from "
                     "the repository root.\n";
        return 1;
    }
    for (const auto& id : ids) {
        auto meta = cli::load_conjecture_metadata(id);
        if (!meta) continue;
        std::cout << id << "\t" << meta->value("name", "?") << "\t[" << meta->value("status", "?")
                   << "]\n";
    }
    return 0;
}

int cmd_info(const std::string& id) {
    auto meta = cli::load_conjecture_metadata(id);
    if (!meta) {
        std::cerr << "Unknown conjecture or metadata not found: " << id << "\n";
        return 1;
    }
    std::cout << (*meta).dump(2) << "\n";
    return 0;
}

int cmd_search_kurepa(const std::vector<std::string>& args) {
    const auto begin = parse_integer_flag(args, "--begin");
    const auto end = parse_integer_flag(args, "--end");
    if (!begin || !end || *begin >= *end) {
        std::cerr << "search kurepa requires --begin N --end N with begin < end\n";
        return 1;
    }

    std::cout << "Searching Kurepa's left factorial conjecture over odd primes in [" << *begin
               << ", " << *end << ")...\n";
    const auto outcome = conjectures::kurepa::search_range(*begin, *end);
    verifier::KurepaVerifier kv;

    if (std::holds_alternative<core::CandidateCounterexample<conjectures::kurepa::Witness>>(
            outcome)) {
        const auto candidate =
            std::get<core::CandidateCounterexample<conjectures::kurepa::Witness>>(outcome);
        std::cout << "CANDIDATE COUNTEREXAMPLE FOUND: p = " << candidate.witness.p << "\n";

        const auto out_path =
            parse_string_flag(args, "--out")
                .value_or("kurepa_counterexample_" + candidate.witness.p.get_str() + ".mhcert");
        auto payload = conjectures::kurepa::encode_counterexample_payload(candidate);
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
    auto payload = conjectures::kurepa::encode_bounded_result_payload(bounded);
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

int cmd_search_toy_mod7(const std::vector<std::string>& args) {
    const auto begin = parse_u64_flag(args, "--begin");
    const auto end = parse_u64_flag(args, "--end");
    if (!begin || !end || *begin >= *end) {
        std::cerr << "search toy_mod7 requires --begin N --end N with begin < end\n";
        return 1;
    }

    std::cout << "Searching toy_mod7 in [" << *begin << ", " << *end << ")...\n";
    const auto outcome = conjectures::toy_mod7::search_range(*begin, *end);
    verifier::ToyMod7Verifier tv;

    if (std::holds_alternative<core::CandidateCounterexample<conjectures::toy_mod7::Witness>>(
            outcome)) {
        const auto candidate =
            std::get<core::CandidateCounterexample<conjectures::toy_mod7::Witness>>(outcome);
        std::cout << "CANDIDATE COUNTEREXAMPLE FOUND: n = " << candidate.witness.n << "\n";

        const auto out_path = parse_string_flag(args, "--out")
                                   .value_or("toy_mod7_counterexample_" +
                                             std::to_string(candidate.witness.n) + ".mhcert");
        auto payload = conjectures::toy_mod7::encode_counterexample_payload(candidate);
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
    auto payload = conjectures::toy_mod7::encode_bounded_result_payload(bounded);
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

int cmd_search_pigeonhole(const std::vector<std::string>& args) {
    const auto pigeons = parse_u64_flag(args, "--pigeons");
    if (!pigeons || *pigeons == 0) {
        std::cerr << "search pigeonhole requires --pigeons N (N >= 1)\n";
        return 1;
    }

    std::cout << "Solving PHP(" << *pigeons << ") with the DPLL search backend...\n";
    const auto outcome =
        conjectures::pigeonhole::search_unsat_benchmark(static_cast<std::uint32_t>(*pigeons));

    if (std::holds_alternative<core::Observation<std::uint32_t>>(outcome)) {
        const auto obs = std::get<core::Observation<std::uint32_t>>(outcome);
        std::cout << "ANOMALY (not a mathematical result): " << obs.note << "\n";
        return 3;
    }

    const auto candidate =
        std::get<core::CandidateProof<conjectures::pigeonhole::UnsatProof>>(outcome);
    std::cout << "CANDIDATE UNSAT PROOF: PHP(" << candidate.proof.pigeons
               << ") is unsatisfiable (solver-reported; unverified)\n";

    const auto out_path = parse_string_flag(args, "--out").value_or(
        "pigeonhole_unsat_" + std::to_string(candidate.proof.pigeons) + ".mhcert");
    auto payload = conjectures::pigeonhole::encode_unsat_proof_payload(candidate);
    auto write_status = certificate::write_certificate_file(
        out_path, core::ClaimId::PigeonholeUnsat, certificate::CertificateKind::PigeonholeUnsatResult,
        payload);
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

int cmd_benchmark(const std::string& id) {
    using clock = std::chrono::steady_clock;
    if (id == "kurepa") {
        const auto start = clock::now();
        const auto outcome = conjectures::kurepa::search_range(3, 20000);
        const auto ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count();
        (void)outcome;
        std::cout << "kurepa: searched [3, 20000) in " << ms << " ms\n";
        return 0;
    }
    if (id == "toy_mod7") {
        const auto start = clock::now();
        const auto outcome = conjectures::toy_mod7::search_range(1, 10'000'000);
        const auto ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count();
        (void)outcome;
        std::cout << "toy_mod7: searched [1, 10000000) in " << ms << " ms\n";
        return 0;
    }
    if (id == "pigeonhole") {
        const auto start = clock::now();
        const auto outcome = conjectures::pigeonhole::search_unsat_benchmark(6);
        const auto ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count();
        (void)outcome;
        std::cout << "pigeonhole: solved PHP(6) in " << ms << " ms\n";
        return 0;
    }
    std::cerr << "No benchmark defined for: " << id << "\n";
    return 1;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) {
        print_usage();
        return 1;
    }

    const std::string& command = args[0];
    std::vector<std::string> rest(args.begin() + 1, args.end());

    if (command == "list") return cmd_list();
    if (command == "info" && !rest.empty()) return cmd_info(rest[0]);
    if (command == "benchmark" && !rest.empty()) return cmd_benchmark(rest[0]);
    if (command == "search" && !rest.empty()) {
        const std::string& conjecture = rest[0];
        std::vector<std::string> flags(rest.begin() + 1, rest.end());
        if (conjecture == "kurepa") return cmd_search_kurepa(flags);
        if (conjecture == "toy_mod7") return cmd_search_toy_mod7(flags);
        if (conjecture == "pigeonhole") return cmd_search_pigeonhole(flags);
        std::cerr << "Unknown conjecture: " << conjecture << "\n";
        return 1;
    }

    print_usage();
    return 1;
}
