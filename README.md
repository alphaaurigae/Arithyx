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
`nlohmann-json3-dev`.

```sh
sudo apt-get install -y cmake libgmp-dev libssl-dev nlohmann-json3-dev
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
GUIDANCE.md):

```sh
sudo apt-get install -y clang-tidy libc++-dev libc++abi-dev
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
for f in src/*/*.cpp; do clang-tidy -p build "$f" --extra-arg=-stdlib=libc++; done
```

## Run

```sh
./build/bin/math-hunter list
./build/bin/math-hunter info kurepa
./build/bin/math-hunter search kurepa --begin 3 --end 100000
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

## Layout

```
include/mathhunter/   public headers, mirrors src/
src/core/              result-type model, hashing, binary I/O
src/arithmetic/        exact GMP-based modular arithmetic + fast native primality
src/certificate/       certificate file envelope (magic/version/payload/digest)
src/sat/               CNF representation + DPLL solver
src/framework/         module registry: the sole type-erasure boundary (ABI Phase 1)
src/conjectures/       search-side logic and self-registration per conjecture
                        (kurepa, toy_mod7, pigeonhole)
src/verifier/          independent verifier per conjecture (never trusts search code)
src/cli/               math-hunter and math-verifier entry points (registry-driven)
tests/                 unit, round-trip, and adversarial/corruption tests
conjectures/metadata/  versioned JSON metadata (never hardcoded in C++)
certificates/examples/ sample certificates, independently re-verifiable
```
