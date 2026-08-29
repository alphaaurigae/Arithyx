#include <fstream>

#include "mathhunter/certificate/certificate.hpp"
#include "mathhunter/conjectures/toy_mod7.hpp"
#include "test_harness.hpp"

using namespace mathhunter;

namespace {

std::vector<std::uint8_t> read_raw(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void write_raw(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
}

std::filesystem::path make_valid_certificate() {
    const auto path = std::filesystem::temp_directory_path() / "mh_test_corrupt_base.bin";

    core::CandidateCounterexample<conjectures::toy_mod7::Witness> candidate;
    candidate.claim = core::ClaimId::ToyMod7;
    candidate.witness.n = 7;
    candidate.discovered_by = "toy-mod7-search-1";

    const auto payload = conjectures::toy_mod7::encode_counterexample_payload(candidate);
    auto status = certificate::write_certificate_file(
        path, core::ClaimId::ToyMod7, certificate::CertificateKind::ToyMod7Counterexample,
        payload);
    if (!status) {
        std::fprintf(stderr, "setup failed to write base certificate\n");
        std::exit(2);
    }
    return path;
}

}  // namespace

int main() {
    const auto base_path = make_valid_certificate();
    const auto base_bytes = read_raw(base_path);

    {
        auto good = certificate::read_certificate_file(base_path);
        MH_REQUIRE(good.has_value());
    }

    {
        const auto path = std::filesystem::temp_directory_path() / "mh_test_corrupt_1bit.bin";
        auto bytes = base_bytes;
        bytes[bytes.size() - 5] ^= 0x01;
        write_raw(path, bytes);
        auto result = certificate::read_certificate_file(path);
        MH_CHECK(!result.has_value());
        if (!result.has_value()) {
            MH_CHECK(result.error() == certificate::CertificateError::DigestMismatch);
        }
        std::filesystem::remove(path);
    }

    {
        const auto path = std::filesystem::temp_directory_path() / "mh_test_corrupt_magic.bin";
        auto bytes = base_bytes;
        bytes[0] ^= 0xFF;
        write_raw(path, bytes);
        auto result = certificate::read_certificate_file(path);
        MH_CHECK(!result.has_value());
        if (!result.has_value()) {
            MH_CHECK(result.error() == certificate::CertificateError::BadMagic);
        }
        std::filesystem::remove(path);
    }

    {
        const auto path = std::filesystem::temp_directory_path() / "mh_test_corrupt_trunc.bin";
        auto bytes = base_bytes;
        bytes.resize(bytes.size() - 10);
        write_raw(path, bytes);
        auto result = certificate::read_certificate_file(path);
        MH_CHECK(!result.has_value());
        if (!result.has_value()) {
            MH_CHECK(result.error() == certificate::CertificateError::Truncated ||
                     result.error() == certificate::CertificateError::DigestMismatch);
        }
        std::filesystem::remove(path);
    }

    {
        const auto path = std::filesystem::temp_directory_path() / "mh_test_corrupt_trailing.bin";
        auto bytes = base_bytes;
        bytes.push_back(0xAB);
        write_raw(path, bytes);
        auto result = certificate::read_certificate_file(path);
        MH_CHECK(!result.has_value());
        if (!result.has_value()) {
            MH_CHECK(result.error() == certificate::CertificateError::TrailingGarbage);
        }
        std::filesystem::remove(path);
    }

    {
        const auto path = std::filesystem::temp_directory_path() / "mh_test_corrupt_claim.bin";
        auto bytes = base_bytes;
        bytes[10] = 0xFF;
        write_raw(path, bytes);
        auto result = certificate::read_certificate_file(path);
        MH_CHECK(!result.has_value());
        if (!result.has_value()) {
            MH_CHECK(result.error() == certificate::CertificateError::UnknownClaim ||
                     result.error() == certificate::CertificateError::DigestMismatch);
        }
        std::filesystem::remove(path);
    }

    {
        const auto path = std::filesystem::temp_directory_path() / "mh_test_corrupt_empty.bin";
        write_raw(path, {});
        auto result = certificate::read_certificate_file(path);
        MH_CHECK(!result.has_value());
        if (!result.has_value()) {
            MH_CHECK(result.error() == certificate::CertificateError::Truncated);
        }
        std::filesystem::remove(path);
    }

    {
        const auto path = std::filesystem::temp_directory_path() / "mh_test_corrupt_huge_len.bin";
        auto bytes = base_bytes;
        bytes[11] = 0xFF;
        bytes[12] = 0xFF;
        bytes[13] = 0xFF;
        bytes[14] = 0x7F;
        write_raw(path, bytes);
        auto result = certificate::read_certificate_file(path);
        MH_CHECK(!result.has_value());
        std::filesystem::remove(path);
    }

    std::filesystem::remove(base_path);

    MH_TEST_MAIN_EPILOGUE();
}
