1. Build OS: 
- ubuntu 26.04

2. Code tools:
- clangtidy

3. Libraries core stack:
- Poco (JSON is in active use as of Phase 3-5, src/cli/metadata_loader.*;
  other Poco components remain reserved for later phases, e.g. Poco::Data
  for Phase 4's tracker persistence once docs/GUIDANCE.md's Decision B is
  resolved)
- GMP
- MPFR (reserved; not yet used anywhere in the tree. Phase 3-5's work-unit
  cost estimation and range partitioning are exact-integer-only, so no
  floating-point precision policy is needed yet. If a future subsystem
  does need MPFR, follow the single-source-of-truth pattern: a named
  constant in one place, e.g. a `precision::BITSIZE`-style constant with
  its rationale documented at the definition, never a hardcoded bit-width
  scattered through call sites.)
