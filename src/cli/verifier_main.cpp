#include <iostream>
#include <string>
#include <vector>

#include "mathhunter/certificate/certificate.hpp"
#include "mathhunter/conjectures/kurepa.hpp"
#include "mathhunter/conjectures/pigeonhole.hpp"
#include "mathhunter/conjectures/toy_mod7.hpp"
#include "mathhunter/verifier/kurepa_verifier.hpp"
#include "mathhunter/verifier/pigeonhole_verifier.hpp"
#include "mathhunter/verifier/toy_mod7_verifier.hpp"

namespace {

using namespace mathhunter;

void print_usage() {
    std::cout << "math-verifier -- independent certificate verification\n\n"
                 "Usage:\n"
                 "  math-verifier verify <certificate-file>\n"
                 "  math-verifier inspect <certificate-file>\n\n"
                 "verify runs the independent verifier for the certificate's claim and kind.\n"
                 "inspect prints the parsed fields WITHOUT running any verification.\n";
}

int cmd_inspect(const std::string& path) {
    auto file = certificate::read_certificate_file(path);
    if (!file) {
        std::cerr << "MALFORMED CERTIFICATE: " << certificate::to_string(file.error()) << "\n";
        return 2;
    }
    std::cout << "-- certificate envelope (structurally valid, NOT verified) --\n";
    std::cout << "format_version: " << file->format_version << "\n";
    std::cout << "claim:          " << core::claim_id_name(file->claim) << "\n";
    std::cout << "kind:           " << certificate::to_string(file->kind) << "\n";
    std::cout << "payload_bytes:  " << file->payload.size() << "\n";
    std::cout << "digest:         " << core::to_hex(file->digest) << "\n";

    switch (file->kind) {
        case certificate::CertificateKind::KurepaCounterexample: {
            auto decoded = conjectures::kurepa::decode_counterexample_payload(file->payload);
            if (decoded) {
                std::cout << "witness p:      " << decoded->witness.p << "\n";
                std::cout << "discovered_by:  " << decoded->discovered_by << "\n";
            }
            break;
        }
        case certificate::CertificateKind::KurepaBoundedResult: {
            auto decoded = conjectures::kurepa::decode_bounded_result_payload(file->payload);
            if (decoded) {
                std::cout << "range:          [" << decoded->begin << ", " << decoded->end << ")\n";
                std::cout << "tested:         " << decoded->candidates_tested << "\n";
                std::cout << "algorithm:      " << decoded->algorithm_version << "\n";
            }
            break;
        }
        case certificate::CertificateKind::ToyMod7Counterexample: {
            auto decoded = conjectures::toy_mod7::decode_counterexample_payload(file->payload);
            if (decoded) {
                std::cout << "witness n:      " << decoded->witness.n << "\n";
            }
            break;
        }
        case certificate::CertificateKind::ToyMod7BoundedResult: {
            auto decoded = conjectures::toy_mod7::decode_bounded_result_payload(file->payload);
            if (decoded) {
                std::cout << "range:          [" << decoded->begin << ", " << decoded->end << ")\n";
                std::cout << "tested:         " << decoded->candidates_tested << "\n";
            }
            break;
        }
        case certificate::CertificateKind::PigeonholeUnsatResult: {
            auto decoded = conjectures::pigeonhole::decode_unsat_proof_payload(file->payload);
            if (decoded) {
                std::cout << "pigeons:        " << decoded->proof.pigeons << "\n";
                std::cout << "formula_hash:   " << core::to_hex(decoded->proof.formula_hash) << "\n";
            }
            break;
        }
    }

    std::cout << "\nThis is a structural inspection only. Run 'math-verifier verify' to "
                 "independently check the mathematical claim.\n";
    return 0;
}

int cmd_verify(const std::string& path) {
    auto file = certificate::read_certificate_file(path);
    if (!file) {
        std::cout << "REJECTED: malformed certificate (" << certificate::to_string(file.error())
                   << ")\n";
        return 2;
    }

    switch (file->kind) {
        case certificate::CertificateKind::KurepaCounterexample: {
            auto decoded = conjectures::kurepa::decode_counterexample_payload(file->payload);
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
        case certificate::CertificateKind::KurepaBoundedResult: {
            auto decoded = conjectures::kurepa::decode_bounded_result_payload(file->payload);
            if (!decoded) {
                std::cout << "REJECTED: malformed payload\n";
                return 2;
            }
            verifier::KurepaVerifier kv;
            auto result = kv.verify_bounded_result(*decoded);
            if (result) {
                std::cout << "VERIFIED BOUNDED RESULT: no counterexample in ["
                           << decoded->begin << ", " << decoded->end << ")\n";
                return 0;
            }
            std::cout << "REJECTED: " << verifier::to_string(result.error()) << "\n";
            return 1;
        }
        case certificate::CertificateKind::ToyMod7Counterexample: {
            auto decoded = conjectures::toy_mod7::decode_counterexample_payload(file->payload);
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
        case certificate::CertificateKind::ToyMod7BoundedResult: {
            auto decoded = conjectures::toy_mod7::decode_bounded_result_payload(file->payload);
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
        case certificate::CertificateKind::PigeonholeUnsatResult: {
            auto decoded = conjectures::pigeonhole::decode_unsat_proof_payload(file->payload);
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
    }

    std::cout << "REJECTED: unhandled certificate kind\n";
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.size() != 2) {
        print_usage();
        return 1;
    }
    if (args[0] == "verify") return cmd_verify(args[1]);
    if (args[0] == "inspect") return cmd_inspect(args[1]);
    print_usage();
    return 1;
}
