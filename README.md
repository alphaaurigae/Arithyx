# math-hunter

A prototype C++23 framework for computational mathematical conjecture hunting,
built around one non-negotiable rule: **discovery is not proof**. Every search
result is either an unverified `Candidate*`/`Observation`, or a `Verified*`
that can *only* be constructed by running an independent verifier.

See **[GUIDANCE.md](docs/GUIDANCE.md)** for the full design writeup: what was
implemented, why, what tradeoffs were made under the "smaller correct system"
mandate, and a concrete list of what to build next.

## Build

Dependencies (Ubuntu 24.04 LTS package names): `cmake`, a C++23 compiler
(GCC >= 12 tested with GCC 13.3), `libgmp-dev`, `libssl-dev`,
`libpoco-dev`.

```sh
sudo apt-get install -y cmake libgmp-dev libssl-dev libpoco-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

Sanitizer build (address+undefined+leak, matches the project's CI-grade
correctness bar):

```sh
cmake -S . -B build-san -DCMAKE_BUILD_TYPE=Debug -DMATHHUNTER_ENABLE_SANITIZERS=ON
cmake --build build-san -j"$(nproc)"
ctest --test-dir build-san --output-on-failure
```

Static analysis (clang-tidy; requires clang's own libc++ headers because the
installed clang's `__cpp_concepts` value is too old to unlock libstdc++'s
`<expected>` guard — this is a toolchain quirk, not a code issue; see
GUIDANCE.md). Since Phase 5's `LocalExecutor` uses `std::jthread`/
`std::stop_token`, libc++ additionally needs `-fexperimental-library` (those
are gated behind that flag until fully stabilized upstream):

```sh
sudo apt-get install -y clang-tidy libc++-dev libc++abi-dev
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
for f in src/*/*.cpp; do
    clang-tidy -p build "$f" --extra-arg=-stdlib=libc++ --extra-arg=-fexperimental-library
done
```

`.clang-tidy`'s `HeaderFilterRegex` expects the checkout directory itself to
be named `math-hunter` (matching the CMake project name); if your checkout
lives elsewhere, pass `--header-filter='.*/(include|src)/.*'` to see header
diagnostics too.

Layering regression guard (Phase 2, docs/GUIDANCE.md): confirms `sat::`
symbols stay confined to the SAT backend and the pigeonhole module. Runs
automatically as a `ctest` case (`check_layering`); can also be run directly:

```sh
tests/check_layering.sh
```

## Run

```sh
./build/bin/math-hunter list
./build/bin/math-hunter info kurepa
./build/bin/math-hunter search kurepa --begin 3 --end 10000
./build/bin/math-hunter search toy_mod7 --begin 1 --end 100
./build/bin/math-hunter search pigeonhole --pigeons 5

# math-verifier is a separate, standalone binary: it never calls into
# math-hunter and re-derives every mathematical fact from scratch.
./build/bin/math-verifier verify kurepa_bounded_3_100000.mhcert
./build/bin/math-verifier inspect kurepa_bounded_3_100000.mhcert
```

Example certificates produced by the commands above are checked into
`certificates/examples/` and can be re-verified at any time with
`math-verifier verify certificates/examples/<file>.mhcert`.

## Distributed work (Phase 3-5)

Beyond the one-shot CLI above, math-hunter has a generic, opaque work-unit
API (`include/mathhunter/framework/work_unit.hpp`) that every module already
implements (`ModuleDescriptor::work_unit`, wired in each module's
`*_registration.cpp`): `make_from_cli` builds a `WorkUnitKey` from flags,
`estimate_cost`/`partition` support splitting a job into smaller pieces, and
`run` is a pure function from `WorkUnitKey` to `WorkUnitOutcome` — no I/O, so
it is safe to hand to any executor. `WorkUnitTracker`
(`framework/tracker.hpp`) drives the PENDING → LEASED → RUNNING → RESULT →
VERIFIED/FLAGGED state machine with lease timeouts, retry limits, and a
configurable verification quorum (default 1, matching today's single-run
CLI behavior). `LocalExecutor` (`framework/executor.hpp`) is the practical,
single-workstation implementation of the `WorkUnitExecutor` concept: a
non-blocking thread pool, `submit()` returns a `std::future` immediately.
`tests/test_worker_pipeline.cpp` is the runnable reference for wiring these
three together end to end; `tests/test_work_unit.cpp` demonstrates that the
same API handles wildly different opaque work-unit shapes (a numeric range,
a synthetic SAT-cube-like blob, an opaque id batch, a graph-partition-like
blob) without the framework ever interpreting their contents. No cluster or
BOINC executor exists yet — see docs/GUIDANCE.md for what that would take
and why it is deliberately not built speculatively.

## Layout

```
include/mathhunter/   public headers, mirrors src/
src/core/              result-type model, hashing, binary I/O, EncodedBlob
                        (shared "versioned bytes + digest" primitive)
src/arithmetic/        exact GMP-based modular arithmetic + fast native primality
src/certificate/       certificate file envelope (magic/version/payload/digest)
src/sat/               CNF representation + DPLL solver
src/framework/         module registry (ABI Phase 1); WorkUnitKey/WorkUnitOps
                        (Phase 3); WorkUnitTracker (Phase 4); LocalExecutor +
                        the WorkUnitExecutor concept (Phase 5)
src/conjectures/       search-side logic and self-registration per conjecture
                        (kurepa, toy_mod7, pigeonhole), incl. each module's
                        WorkUnitOps (partition/cost/run) wiring
src/verifier/          independent verifier per conjecture (never trusts search code)
src/cli/               math-hunter and math-verifier entry points (registry-driven)
tests/                 unit, round-trip, and adversarial/corruption tests, plus
                        check_layering.sh (Phase 2 sat:: containment guard) and
                        the Phase 3-5 work-unit/tracker/executor test suite
conjectures/metadata/  versioned JSON metadata (never hardcoded in C++)
certificates/examples/ sample certificates, independently re-verifiable
```
