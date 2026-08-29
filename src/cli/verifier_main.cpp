#include <iostream>
#include <string>
#include <vector>

#include "mathhunter/certificate/certificate.hpp"
#include "mathhunter/framework/module_registry.hpp"

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

    if (const auto* ops = framework::registry().find_kind_ops(file->kind);
        ops != nullptr && ops->inspect) {
        ops->inspect(file->payload);
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

    const auto* ops = framework::registry().find_kind_ops(file->kind);
    if (ops == nullptr || !ops->verify) {
        std::cout << "REJECTED: unhandled certificate kind\n";
        return 2;
    }
    return ops->verify(file->payload);
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
