# Test Implementation Guidance

Standardized test output format and integration with `run_tests.pl`.

This document is the single source of truth for two decisions:

1. **Should this test be a C++ binary or a shell/perl script?** — §"Script vs C++ Decision Matrix"
2. **How should this test handle its database handle?** — §"Database handling in tests (T2)"

---

## Script vs C++ Decision Matrix

A test **must** be written in C++ if **any** of the following are true:

| Criterion | Reason |
|---|---|
| Uses `mpfr.h`, `gmp.h`, `mpfi.h`, or any wrapper (`MPFRGuard`, `MPFILocal`, `precision::real`) | No stable perl/bash binding to these libraries at the precision we require. |
| Exercises a C++ template, RAII wrapper, or namespace-scoped function (e.g. `rarity::engine::compute_rarity_report`, `sparkle::optics::*`) | The test is the contract for that symbol; must compile and link the same TU. |
| Parses JSON with `Poco::JSON` or round-trips a C++ `struct` | Catches Poco API drift. Moving to `jq` loses this. |
| Asserts IEEE-754 bitwise equality, `memcmp` of doubles, sanitizer-sensitive pointer arithmetic | Requires a C++ translation unit under the same build flags as production. |
| Depends on `test_harness::TestContext`-internal features beyond the three markers | Handled natively by the header — not worth re-implementing in script. |

A test **should** be migrated to a shell/perl script when **all** of the following are true:

| Criterion | Reason |
|---|---|
| Zero mpfr/gmp/mpfi use | Pure math precision is the only reason to keep C++. |
| Body is `SELECT COUNT(*)`, `PRAGMA`, or schema-shape queries against `diamonds.db` | `sqlite3` CLI covers this in ~10× less code. |
| Asserts exist/populated/monotonic/ordered properties over SQL result sets | Perl `sqlite3 -noheader` is sufficient; DBD::SQLite is not needed. |
| Test does not exercise a C++ symbol | There is no C++ contract being pinned. |
| Test cost < one full CMake rebuild cycle per change | Scripts iterate in seconds; C++ tests in minutes. |

A test **may** stay in C++ even when eligible for migration, if it is part of a
coherent suite where splitting it out would create a second DB-backed run.

### Script form requirements

Script-form tests **must** still emit the three standardized markers:

```
[TEST_SUITE_BEGIN] <suite_name>
[TEST_CASE] PASS <description>
[TEST_CASE] FAIL <description>
[TEST_SUITE_END] <passed> <failed> <elapsed>
```

Exit code: 0 on full pass, 1 on any failure.

Language preference (per house rules):

1. **perl 5** — preferred. Core modules only (`Time::HiRes`, `POSIX`, `File::Basename`); no CPAN deps. Use the `sqlite3` CLI via `open '-|', 'sqlite3', ...` rather than `DBD::SQLite` so the test has no runtime dependency beyond the base OS install.
2. **bash** — acceptable when the assertions are shell-native (file existence, process behaviour, grep on compiled binary output). **Do not use `set -euo pipefail`** — on Ubuntu 24.04 this has caused premature exits in this project.
3. Other languages only if clearly superior (profile & justify in a comment at the top of the script).

### Script files live under `script/test/`

```
script/test/test_<name>.pl
script/test/test_<name>.sh
```

and are registered in `run_tests.pl` with `type => 'script'` (see §"run_tests.pl integration").

---

## Database handling in tests (T2)

### Background

Tests open `diamonds.db` in WAL mode (inherited from `init_db_main`). Two classes of failure have been observed:

1. **`diamonds.db-wal` / `diamonds.db-shm` persist after `perl run_tests.pl` exits.** `db-initialize_and_import.sh` and `bin/update_db_main` close cleanly, but test binaries that call bare `sqlite3_close(db)` (not `sqlite3_close_v2`) leave the WAL sidecars on disk. If any statement is unfinalised or any iterator is mid-iteration when `sqlite3_close` runs, it returns `SQLITE_BUSY` and the handle is half-closed.
2. **No checkpoint before close.** Even on a clean `sqlite3_close`, the WAL sidecars are not truncated unless `PRAGMA wal_checkpoint(TRUNCATE)` has been issued by the last connection. This is standard SQLite semantics, not a bug — but it looks like a leak in CI.

### Required pattern — RAII `test_db::Guard`

Every test that opens `diamonds.db` must use `test_db_support.hpp`:

```cpp
#include "test_harness.hpp"
#include "test_db_support.hpp"

int main() {
  test_harness::TestContext ctx("test_my_feature");

  test_db::Guard db = test_db::open_ro();
  if (!db) {
    ctx.check(true, "DB-backed test skipped (no diamonds.db)");
    return ctx.finish();
  }

  run_assertions(db.get(), ctx);

  return ctx.finish();
  // db destructor here: wal_checkpoint(TRUNCATE) -> optimize -> close_v2
}
```

Rules:

1. **Never call `sqlite3_close()` or `sqlite3_close_v2()` directly in a test.** The guard owns lifetime.
2. **Never hold a raw `sqlite3*` past the guard.** If the test helper needs the handle, pass `db.get()` by value or by `sqlite3*` parameter — not by reference, not by pointer-to-pointer.
3. **Setup helpers that open a private in-memory DB** (e.g. `setup_test_db()` variants that call `sqlite3_open(":memory:", ...)`) must return a `test_db::Guard` by value, not a raw `sqlite3*`.
4. For tests that currently use `setup_test_db()` returning a raw pointer: wrap the return site in `test_db::Guard{setup_test_db()};` at the call site. The signature can be changed later; the wrap is correct today.

### Failure-mode protection

`test_db::checkpoint_and_close()` is `noexcept` and guards against null. Destructors that run during stack unwinding from a `ctx.check()` failure still complete the checkpoint. There is no code path in a test body that can skip the checkpoint short of `std::abort()`.

### `run_tests.pl` integration

`run_tests.pl` is responsible for two additional steps:

1. **Pre-run cleanup.** Before any test executes, remove stale `diamonds.db-wal` / `diamonds.db-shm` / `diamonds.db-journal` from a previous crashed run. See `_t2_cleanup_sidecars` helper block (added by `script/debug/patch_run_tests_pl.sh`).
2. **Post-run leak check.** After each test binary exits, verify sidecars are absent. If present, report which test binary left them and force a `PRAGMA wal_checkpoint(TRUNCATE)` via the `sqlite3` CLI. See `_t2_check_leaks` helper.

Script-based tests are added to `@tests` with `type => 'script'`:

```perl
my @tests = (
  { name => "test_inclusion_incompatibility",
    bin  => "script/test/test_inclusion_incompatibility.pl",
    type => "script" },
  { name => "test_combined_scoring",
    bin  => "test_combined_scoring" },               # C++ default, type omitted
  # ...
);
```

The main loop branches on `$t->{type}`:

```perl
my @cmd;
if (($t->{type} // 'bin') eq 'script') {
  @cmd = ('perl', $t->{bin});
} else {
  @cmd = ("$BIN_DIR/$t->{bin}");
  next unless -x $cmd[0];
}
```

### Optional split runner — `run_tests_scripts.pl`

If isolation between C++ and script tests is desired (faster iteration on one layer without triggering the other), copy `run_tests.pl` to `run_tests_scripts.pl` and filter the loop:

```perl
next unless ($t->{type} // '') eq 'script';
```

Both runners share the same `@tests` list — duplication is avoided by `require`-ing a shared `tests_list.pl` if the project adopts the split. Not mandatory.

---

## Standardized Output Format

(unchanged — see prior sections. All tests, both C++ and script, emit the same three markers.)

### Required Markers

```
[TEST_SUITE_BEGIN] <suite_name>
... arbitrary diagnostic output (preserved as-is) ...
[TEST_CASE] PASS <description>
[TEST_CASE] FAIL <description>
... more diagnostic output ...
[TEST_SUITE_END] <passed_count> <failed_count> <elapsed>
```

Rules (same for script and C++):

1. All three marker types are mandatory.
2. Diagnostic output between markers is preserved verbatim.
3. `[TEST_CASE]` lines must start at column 0.
4. Exit code must be 0 on full pass, 1 on any failure.
5. Description strings < 120 chars, unique within a suite.

---

## Adding a New Test

### C++ test

1. Create `src/test/test_<name>.cpp` using `test_harness::TestContext` and (if DB-backed) `test_db::Guard`.
2. In `CMakeLists.txt` inside the `if(BUILD_TESTS)` block: `add_test_executable(test_<name>)`.
3. Register in `run_tests.pl` `@tests` array with **no** `type` field (defaults to `'bin'`).

### Script test

1. Create `script/test/test_<name>.pl` (preferred) or `script/test/test_<name>.sh`.
2. Emit the three standardized markers.
3. `chmod +x script/test/test_<name>.pl`.
4. Register in `run_tests.pl` `@tests` array with `type => 'script'` and `bin` set to the script path relative to repo root.

No CMake changes are required for script tests.

---

## Migrating an Existing C++ Test to Script Form

**Pre-check:** run the decision matrix above. If any "must be C++" criterion fires, stop — leave it in C++.

1. Write the script form under `script/test/`.
2. Run both old and new in parallel. Diff `[TEST_CASE]` lines. Fail the migration if counts or descriptions drift.
3. When green: delete the C++ source, remove its `add_test_executable(...)` line from `CMakeLists.txt`, and flip the `@tests` entry to `type => 'script'`.
4. Commit the deletion and the new script in the same commit so bisect stays clean.

---

(remainder of the document — rules, examples, design decisions — unchanged from previous version.)
