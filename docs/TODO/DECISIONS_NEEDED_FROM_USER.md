QA - answered.
# Decisions needed before resuming Kurepa large-range work

Companion to `NOTES_TO_CLAUDE.md`. These are things I need from you (or that
you need to decide) when we pick this back up with full repo access.

## 1. Target magnitude — this is the big one

`left_factorial_mod` is `O(n)` modular multiplications per candidate prime
`n`. Rough feasibility, single core:

| n magnitude | approx wall time per candidate |
|---|---|
| 10^12 | ~17 minutes |
| 10^14 | ~1.2 days |
| 10^15 | ~12 days |
| 10^18 | ~32 years |
| 10^20 | ~3170 years |

Your current runner targets `10^20.121...`. At that magnitude, no amount of
type-width fixing (uint64_t → GMP) makes this finish. Please decide one of:

- **(A) Scope the target down** to something in the 10^12–10^14 range (or
  tell me your actual CPU/core budget and time budget, and I'll compute a
  realistic ceiling for you), and I do the type-width fix (GMP bounds
  throughout the pipeline) so you can reach that ceiling correctly.
> cpu is amd threadripper 1920 as samnple, time budget per task is up to 24 hours - this should be the mean target for job max time here. 
> However, i want small single job tasks for the perl script. in the end the program should be scalable for boinc setup so that we can use multiple machines to scan a range coordinated -  thus its very helpful now to plan to be able to scan small ranges or even single numbers with large numbers.

- **(B) Fund the harder project**: research and implement a sub-linear
  (`~O(√n·polylog n)` class) algorithm for `n! mod p`, which is what's
  actually required to reach `10^18–10^20+`. This is weeks of work, not a
  patch, and needs correctness validation against the current O(n)
  reference before it can be trusted for anything certificate-bearing.
- **(C) Both, sequenced**: do the type-width fix now for correctness/
  future-proofing at reachable magnitudes, and separately scope (B) as a
  later project once you've decided it's worth pursuing.
- **(D) Abandon the 10^20 target** and treat the current Kurepa checker as
  useful only up to whatever magnitude is reachable in reasonable time on
  your hardware.

I need your answer here before I write any more code — building the GMP
plumbing without a computable target isn't useful.

## 2. Hardware / time budget

If you pick (A), tell me:
- Core count / whether you'll parallelize the search (the current DPLL/
  Kurepa search code is single-threaded as written).
> cpu is amd threadripper 1920 as samnple, time budget per task is up to 24 hours - this should be the mean target for job max time here. 
> code can and should be multithreaded where appropriate to improve performance. for single core sequential task that cant be multi threaded we can still make sure to have all other work like loggin with other worker threads to not occupy the task worker while not blocking.

- How long you're willing to let a search run per checkpoint (minutes?
  hours? left running overnight/over days?).
> as outlined ... goal is as short as possible single tasks while keeping track later of work ranges or numbers. take practical boinc behavious for prime search as example. primegrid for example searches for very large prime numbers.
 thus the work is organized in tasks per number or range - this is the prime example literally and practically and goal in the end - meanwhile plan practically with this isn mind.

This determines the real ceiling, not just the raw mulmod-rate table above.

## 3. obsolete
## 4. Verifier hardening (found along the way, separate from the above)

`KurepaVerifier::verify_bounded_result` currently has no upper bound on how
large a range a *certificate* can claim before the verifier brute-force
iterates it — `PigeonholeVerifier` already has this kind of guard
(`kMaxBruteForcePigeons` → `DomainTooLargeForVerifier`), `KurepaVerifier`
does not. Worth adding regardless of the magnitude decision above, so a
malformed/malicious certificate can't hang the verifier. Confirm you want
this included when I do the pass.

> operational security is important, graceful shutdowns / restarts should be aimed for in case of "emergencies"

