#include "mathhunter/conjectures/kurepa.hpp"
#include "mathhunter/conjectures/pigeonhole.hpp"
#include "mathhunter/conjectures/toy_mod7.hpp"
#include "mathhunter/framework/module_registry.hpp"
#include "mathhunter/framework/work_unit.hpp"
#include "test_harness.hpp"

using namespace mathhunter;

namespace {

// The "hostile fourth prototype" check from the original blueprint,
// implemented as raw synthetic WorkUnitKey shapes rather than four full
// conjecture modules: a range, a SAT-cube-like literal list, an opaque
// numeric id batch, and a graph-partition-like adjacency list. None of
// these correspond to a real search backend; the point is only to prove
// that WorkUnitKey encode/decode/identity never branch on what the bytes
// mean.
framework::WorkUnitKey make_synthetic(std::string_view module_id,
                                       const std::vector<std::uint8_t>& raw_bytes) {
    return framework::WorkUnitKey{.module_id = std::string(module_id),
                                   .partition = core::EncodedBlob{.schema_version = 1,
                                                                   .bytes = raw_bytes}};
}

}  // namespace

int main() {
    {
        const std::vector<framework::WorkUnitKey> shapes{
            make_synthetic("range_module", {0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 39, 15, 1, 0, 0, 0}),
            make_synthetic("sat_cube_module", {1, 0, 7, 1, 1, 0, 12, 0}),
            make_synthetic("opaque_id_batch_module", {18, 32, 0, 18, 32, 100}),
            make_synthetic("graph_partition_module", {0, 1, 2, 255, 4, 5, 254, 7}),
            make_synthetic("empty_partition_module", {}),
        };

        for (const auto& key : shapes) {
            const auto encoded = framework::encode_work_unit_key(key);
            auto decoded = framework::decode_work_unit_key(encoded);
            MH_REQUIRE(decoded.has_value());
            MH_CHECK(*decoded == key);
            MH_CHECK(decoded->identity_digest() == key.identity_digest());
        }

        for (std::size_t i = 0; i < shapes.size(); ++i) {
            for (std::size_t j = i + 1; j < shapes.size(); ++j) {
                MH_CHECK(shapes[i].identity_digest() != shapes[j].identity_digest());
            }
        }
    }

    {
        const auto key = make_synthetic("corruption_test", {10, 20, 30});
        auto encoded = framework::encode_work_unit_key(key);
        encoded[encoded.size() - 1] ^= 0xFF;
        auto decoded = framework::decode_work_unit_key(encoded);
        MH_CHECK(!decoded.has_value());
        if (!decoded.has_value()) {
            MH_CHECK(decoded.error() == framework::WorkUnitKeyError::DigestMismatch);
        }
    }

    {
        core::ByteWriter w;
        w.put_length_prefixed_string("");
        auto decoded = framework::decode_work_unit_key(w.bytes());
        MH_CHECK(!decoded.has_value());
        if (!decoded.has_value()) {
            MH_CHECK(decoded.error() == framework::WorkUnitKeyError::EmptyModuleId);
        }
    }

    for (const std::string_view module_id : {"kurepa", "toy_mod7", "pigeonhole"}) {
        const auto* module = framework::registry().find_by_id(module_id);
        MH_REQUIRE(module != nullptr);
        MH_REQUIRE(static_cast<bool>(module->work_unit.make_from_cli));
        MH_REQUIRE(static_cast<bool>(module->work_unit.estimate_cost));
        MH_REQUIRE(static_cast<bool>(module->work_unit.partition));
        MH_REQUIRE(static_cast<bool>(module->work_unit.run));
    }

    {
        const auto* kurepa = framework::registry().find_by_id("kurepa");
        MH_REQUIRE(kurepa != nullptr);
        auto key = kurepa->work_unit.make_from_cli({"--begin", "3", "--end", "5000"});
        MH_REQUIRE(key.has_value());

        const auto cost = kurepa->work_unit.estimate_cost(*key);
        MH_CHECK(cost.units == 4997);

        const auto chunks = kurepa->work_unit.partition(*key, 1000);
        MH_CHECK(chunks.size() == 5);
        std::uint64_t total = 0;
        for (const auto& chunk : chunks) {
            total += kurepa->work_unit.estimate_cost(chunk).units;
        }
        MH_CHECK(total == cost.units);

        const auto outcome = kurepa->work_unit.run(*key);
        MH_CHECK(outcome.kind == framework::WorkUnitOutcomeKind::BoundedCompletion);
        MH_REQUIRE(outcome.certificate_kind.has_value());
        MH_CHECK(*outcome.certificate_kind == certificate::CertificateKind::KurepaBoundedResult);

        const auto* ops = framework::registry().find_kind_ops(*outcome.certificate_kind);
        MH_REQUIRE(ops != nullptr);
        MH_REQUIRE(static_cast<bool>(ops->verify_quiet));
        MH_CHECK(ops->verify_quiet(outcome.payload) == framework::SilentVerifyOutcome::Verified);
    }

    {
        const auto* toy = framework::registry().find_by_id("toy_mod7");
        MH_REQUIRE(toy != nullptr);
        auto key = toy->work_unit.make_from_cli({"--begin", "1", "--end", "100"});
        MH_REQUIRE(key.has_value());

        const auto outcome = toy->work_unit.run(*key);
        MH_CHECK(outcome.kind == framework::WorkUnitOutcomeKind::Hit);
        MH_REQUIRE(outcome.certificate_kind.has_value());
        MH_CHECK(*outcome.certificate_kind == certificate::CertificateKind::ToyMod7Counterexample);

        const auto* ops = framework::registry().find_kind_ops(*outcome.certificate_kind);
        MH_REQUIRE(ops != nullptr);
        MH_CHECK(ops->verify_quiet(outcome.payload) == framework::SilentVerifyOutcome::Verified);
    }

    {
        const auto* pigeonhole = framework::registry().find_by_id("pigeonhole");
        MH_REQUIRE(pigeonhole != nullptr);
        auto key = pigeonhole->work_unit.make_from_cli({"--pigeons", "5"});
        MH_REQUIRE(key.has_value());

        const auto atomic = pigeonhole->work_unit.partition(*key, 1);
        MH_REQUIRE(atomic.size() == 1);
        MH_CHECK(atomic.front() == *key);

        const auto outcome = pigeonhole->work_unit.run(*key);
        MH_CHECK(outcome.kind == framework::WorkUnitOutcomeKind::BoundedCompletion);
        MH_REQUIRE(outcome.certificate_kind.has_value());
        MH_CHECK(*outcome.certificate_kind == certificate::CertificateKind::PigeonholeUnsatResult);

        const auto* ops = framework::registry().find_kind_ops(*outcome.certificate_kind);
        MH_REQUIRE(ops != nullptr);
        MH_CHECK(ops->verify_quiet(outcome.payload) == framework::SilentVerifyOutcome::Verified);
    }

    {
        const auto* kurepa = framework::registry().find_by_id("kurepa");
        MH_REQUIRE(kurepa != nullptr);
        auto malformed = make_synthetic("kurepa", {1, 2, 3});
        const auto outcome = kurepa->work_unit.run(malformed);
        MH_CHECK(outcome.kind == framework::WorkUnitOutcomeKind::Anomaly);
        MH_CHECK(!outcome.certificate_kind.has_value());
        MH_CHECK(!outcome.diagnostic.empty());
    }

    MH_TEST_MAIN_EPILOGUE();
}
