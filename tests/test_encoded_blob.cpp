#include "mathhunter/core/encoded_blob.hpp"
#include "test_harness.hpp"

using namespace mathhunter;

int main() {
    {
        core::EncodedBlob blob{.schema_version = 3, .bytes = {1, 2, 3, 4, 5}};
        const std::vector<std::uint8_t> header{0xAB, 0xCD};
        const auto framed = core::frame_encoded_blob(header, blob);
        auto unframed = core::unframe_encoded_blob(header, framed);
        MH_REQUIRE(unframed.has_value());
        MH_CHECK(unframed->blob == blob);
    }

    {
        core::EncodedBlob blob{.schema_version = 1, .bytes = {}};
        const auto framed = core::frame_encoded_blob({}, blob);
        auto unframed = core::unframe_encoded_blob({}, framed);
        MH_REQUIRE(unframed.has_value());
        MH_CHECK(unframed->blob.bytes.empty());
    }

    {
        core::EncodedBlob blob{.schema_version = 1, .bytes = {9, 9, 9}};
        auto framed = core::frame_encoded_blob({}, blob);
        framed[framed.size() - 1] ^= 0x01;
        auto result = core::unframe_encoded_blob({}, framed);
        MH_CHECK(!result.has_value());
        if (!result.has_value()) {
            MH_CHECK(result.error() == core::EncodedBlobError::DigestMismatch);
        }
    }

    {
        core::EncodedBlob blob{.schema_version = 1, .bytes = {9, 9, 9}};
        auto framed = core::frame_encoded_blob({}, blob);
        framed.resize(framed.size() - 5);
        auto result = core::unframe_encoded_blob({}, framed);
        MH_CHECK(!result.has_value());
    }

    {
        core::EncodedBlob blob{.schema_version = 1, .bytes = {9, 9, 9}};
        auto framed = core::frame_encoded_blob({}, blob);
        framed.push_back(0x42);
        auto result = core::unframe_encoded_blob({}, framed);
        MH_CHECK(!result.has_value());
        if (!result.has_value()) {
            MH_CHECK(result.error() == core::EncodedBlobError::TrailingGarbage);
        }
    }

    {
        core::EncodedBlob blob{.schema_version = 1, .bytes = std::vector<std::uint8_t>(100, 7)};
        const auto framed = core::frame_encoded_blob({}, blob);
        auto result = core::unframe_encoded_blob({}, framed, 10);
        MH_CHECK(!result.has_value());
        if (!result.has_value()) {
            MH_CHECK(result.error() == core::EncodedBlobError::PayloadTooLarge);
        }
    }

    {
        core::EncodedBlob blob{.schema_version = 1, .bytes = {1}};
        const std::vector<std::uint8_t> header{0x01, 0x02};
        const auto framed = core::frame_encoded_blob(header, blob);
        const std::vector<std::uint8_t> wrong_header{0x03, 0x04};
        auto result = core::unframe_encoded_blob(wrong_header, framed);
        MH_CHECK(!result.has_value());
    }

    MH_TEST_MAIN_EPILOGUE();
}
