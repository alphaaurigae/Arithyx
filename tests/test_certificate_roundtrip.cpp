#include "mathhunter/certificate/certificate.hpp"
#include "mathhunter/conjectures/kurepa.hpp"
#include "test_harness.hpp"

int main() {
    using namespace mathhunter;

    const auto path = std::filesystem::temp_directory_path() / "mh_test_roundtrip.bin";

    core::BoundedSearchResult<arithmetic::Integer> result;
    result.claim = core::ClaimId::KurepaLeftFactorial;
    result.begin = arithmetic::Integer(3);
    result.end = arithmetic::Integer(1000);
    result.candidates_tested = 167;
    result.algorithm_version = "kurepa-search-1";

    const auto payload = conjectures::kurepa::encode_bounded_result_payload(result);
    auto write_status = certificate::write_certificate_file(
        path, core::ClaimId::KurepaLeftFactorial, certificate::CertificateKind::KurepaBoundedResult,
        payload);
    MH_REQUIRE(write_status.has_value());

    auto read_back = certificate::read_certificate_file(path);
    MH_REQUIRE(read_back.has_value());
    MH_CHECK(read_back->claim == core::ClaimId::KurepaLeftFactorial);
    MH_CHECK(read_back->kind == certificate::CertificateKind::KurepaBoundedResult);

    auto decoded = conjectures::kurepa::decode_bounded_result_payload(read_back->payload);
    MH_REQUIRE(decoded.has_value());
    MH_CHECK(decoded->begin == result.begin);
    MH_CHECK(decoded->end == result.end);
    MH_CHECK(decoded->candidates_tested == result.candidates_tested);
    MH_CHECK(decoded->algorithm_version == result.algorithm_version);

    std::filesystem::remove(path);

    MH_TEST_MAIN_EPILOGUE();
}
