<!--
 * Diamond Rarity & Sparkle Calculation Engine
 * Copyright (c) 2026 Marian Brockerhoff. All Rights Reserved.
 *
 * Licensed under PolyForm Noncommercial 1.0.0
 * See LICENSE file for terms.
-->

# C++23 Code Correctness Guidelines

## Reference:
### Compiler settings 
verysciencemuchbling/CMakeLists.txt
### Clangtidy configuration
- code must pass the clangtidy configuration for verysciencemuchbling/CMakeLists.txt see:
verysciencemuchbling/clangtidy.sh

**Scope**: All `.cpp` / `.hpp` sources under `src/`.
**Enforcement**: `script/clangtidy.sh` — clang-tidy + clang-format on every pre-commit run.
**Standard**: C++23 (`--extra-arg=-std=c++23`), Clang ≥ 17 / GCC ≥ 13.

---

## 1. Build & Toolchain Reference

### 1.1 `CMakeLists.txt` — Compiler Flags

| Flag group | Flags | Effect |
|---|---|---|
| Base (all builds) | `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wzero-as-null-pointer-constant -Wold-style-cast -Wnon-virtual-dtor -Woverloaded-virtual -Wnull-dereference` | Enables broad diagnostics |
| GCC-only | `-Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast` | Extra GCC idiom checks |
| Debug | `-O0 -g3 -ggdb` | Full debug info, no optimisation |
| Release | `-O3 -DNDEBUG -D_FORTIFY_SOURCE=3` | LTO enabled via `CMAKE_INTERPROCEDURAL_OPTIMIZATION` |
| Sanitizers (`ENABLE_SANITIZERS=ON`, default) | `-fsanitize=undefined,shift,shift-exponent,shift-base,integer-divide-by-zero,vla-bound,null,return,signed-integer-overflow,bounds,alignment,object-size,float-divide-by-zero,float-cast-overflow,nonnull-attribute,returns-nonnull-attribute,bool,enum,vptr,pointer-overflow,builtin,address,leak` | Full UB+ASan+LSan suite |
| Clang-only sanitizer extras | `-fno-sanitize-address-globals-dead-stripping -fsanitize-address-use-after-return=always -fsanitize-address-outline-instrumentation -fstack-protector-strong -fstack-clash-protection` | Hardened stack + ASan instrumentation |
| x86-64 | `-fcf-protection=full` | Control-flow integrity |
| AArch64 | `-fsanitize=shadow-call-stack` | Shadow call stack |

> **Rule**: Code must compile clean at `-O0` (Debug+Sanitizers) AND `-O3` (Release). A warning at any level is a defect.

### 1.2 `script/clangtidy.sh` — Active Check Groups
```
clang-analyzer-*
bugprone-*
performance-*
portability-*
modernize-*
cppcoreguidelines-*
readability-*
cert-*
misc-*
```

**Explicitly suppressed** (not required):

| Suppressed check | Reason |
|---|---|
| `hicpp-*` | Redundant with cppcoreguidelines-* |
| `llvmlibc-*` | LLVM libc specific, not applicable |
| `altera-*` | FPGA specific |
| `google-*` | Google style conflicts with project style |
| `modernize-use-trailing-return-type` | Codebase style: leading return type |
| `cppcoreguidelines-avoid-magic-numbers` / `readability-magic-numbers` | Suppressed project-wide |
| `readability-identifier-length` | Suppressed project-wide |
| `readability-braces-around-statements` | Suppressed project-wide |
| `misc-include-cleaner` | Suppressed project-wide |
| `misc-use-anonymous-namespace` | Suppressed project-wide |
| `llvm-header-guard` | Suppressed project-wide |

> **Rule**: Every non-suppressed check that fires is a defect to fix before merge. Suppressed checks require an explicit `// NOLINT(check-name) — reason` comment if a violation is intentional.

---

## 2. Const Correctness (`misc-const-correctness`)

**Triggered by**: `-Wconversion` + `misc-const-correctness`.

Every variable that is never re-assigned after initialisation **must** be declared `const`.
```cpp
// WRONG
int flags = SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX;
bool available = cv_.wait_for(...);
int fd = queue_.front();
std::lock_guard lk(mtx_);

// CORRECT
const int flags = SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX;
const bool available = cv_.wait_for(...);
const int fd = queue_.front();
const std::lock_guard lk(mtx_);
```

`std::lock_guard`, `std::unique_lock`, loop iterators, return-value captures — all `const` unless the variable is explicitly re-assigned.

---

## 3. Implicit Bool Conversion (`readability-implicit-bool-conversion`)

**Triggered by**: `-Wzero-as-null-pointer-constant` + `readability-implicit-bool-conversion`.

Pointer-to-bool and C API handle-to-bool conversions must be explicit.
```cpp
// WRONG
if (db)  { ... }
if (!db) { ... }
if (pw)  { ... }
if (allow && ...) { ... }
if (o)   { ... }

// CORRECT
if (db != nullptr)  { ... }
if (db == nullptr)  { ... }
if (pw != nullptr)  { ... }
if ((allow != nullptr) && ...) { ... }
if (o != nullptr)   { ... }
```

This applies to: `sqlite3*`, `struct passwd*`, `const char*` env results, `Poco::JSON::Object*`, any C API opaque handle, and any raw pointer.

---

## 4. Rule of Five (`cppcoreguidelines-special-member-functions`)

**Triggered by**: `cppcoreguidelines-special-member-functions`.

If **any** of: destructor, copy constructor, copy assignment, move constructor, move assignment is user-defined or deleted, **all five** must be explicitly declared.
```cpp
// WRONG — defines dtor + copy-delete but omits move
class Foo {
  ~Foo();
  Foo(const Foo &) = delete;
  Foo &operator=(const Foo &) = delete;
  // move ctor and move assignment missing → tidy fires
};

// CORRECT — non-moveable resource owner
class Foo {
  ~Foo();
  Foo(const Foo &) = delete;
  Foo &operator=(const Foo &) = delete;
  Foo(Foo &&) = delete;
  Foo &operator=(Foo &&) = delete;
};

// CORRECT — moveable RAII wrapper
class Bar {
  ~Bar();
  Bar(const Bar &) = delete;
  Bar &operator=(const Bar &) = delete;
  Bar(Bar &&o) noexcept;
  Bar &operator=(Bar &&o) noexcept;
};
```

Default rule: if the class owns an exclusive resource (fd, db handle, mutex-protected state), delete all four non-destructor specials unless a move is explicitly safe and tested.

---

## 5. Multiplication Widening (`bugprone-implicit-widening-of-multiplication-result`)

**Triggered by**: `bugprone-implicit-widening-of-multiplication-result`.

When the result of a multiplication is assigned to a wider type, the multiplication must be performed in the wider type.
```cpp
// WRONG — 1*1024*1024 computed as int (32-bit), then widened to size_t
static constexpr size_t MAX_REQUEST_BYTES = 1 * 1024 * 1024;

// CORRECT — at least the leftmost operand carries the target type
static constexpr size_t MAX_REQUEST_BYTES = 1UL * 1024UL * 1024UL;

// ALSO CORRECT
static constexpr size_t MAX_REQUEST_BYTES = size_t{1} * 1024 * 1024;
```

This applies to any constant expression or runtime multiplication where the operands are narrower than the assignment target.

---

## 6. Sign Conversion (`clang-diagnostic-sign-conversion`)

**Triggered by**: `-Wsign-conversion`.

Passing a signed integer (`int`) to a function expecting `size_t` (or vice versa) is a sign conversion warning.
```cpp
// WRONG
int count = 4;
pool_.reserve(count);           // int → size_t implicit
workers.reserve(cfg.worker_count);  // int → size_t implicit

// CORRECT
pool_.reserve(static_cast<size_t>(count));
workers.reserve(static_cast<size_t>(cfg.worker_count));
```

Container `.reserve()`, `.resize()`, `.assign()`, range-based indexing, and any STL function taking `size_type` must receive an explicit `static_cast<size_t>` when the source is signed.

---

## 7. Uninitialised Structs (`cppcoreguidelines-pro-type-member-init`)

**Triggered by**: `cppcoreguidelines-pro-type-member-init` + ASan/UBSan at runtime.

Plain C structs used as stack variables must be value-initialised.
```cpp
// WRONG — indeterminate padding bytes, UBSan may fire
struct timeval tv;
struct sockaddr_un addr;
struct ucred cred;

// CORRECT — zero-initialise with braced init
struct timeval tv{};
struct sockaddr_un addr{};
struct ucred cred{};
```

Every POD/C struct declared on the stack must use `{}` initialisation. No exceptions.

---

## 8. `[[nodiscard]]` (`modernize-use-nodiscard`)

**Triggered by**: `modernize-use-nodiscard`.

Any `const` member function that returns a value and whose return value must not be silently discarded must be marked `[[nodiscard]]`.
```cpp
// WRONG
int get() const { return fd_; }
int available();
size_t size();

// CORRECT
[[nodiscard]] int get() const { return fd_; }
[[nodiscard]] int available();
[[nodiscard]] size_t size();
```

Apply `[[nodiscard]]` to: getters, factory functions, error-code returns, acquire/pop methods on resource pools and queues, and any function where ignoring the return value is a programming error.

---

## 9. `auto` for Template-Cast Initialisers (`modernize-use-auto`)

**Triggered by**: `modernize-use-auto`.

When a variable is initialised via a template member function that explicitly names the return type, use `auto` to avoid repeating the type.
```cpp
// WRONG — type repeated on both sides
std::string dt = f->getValue<std::string>("diamond_type");
std::string sb = o->getValue<std::string>("sort_by");

// CORRECT
const auto dt = f->getValue<std::string>("diamond_type");
const auto sb = o->getValue<std::string>("sort_by");
```

Combine with `const` per §2. The rule applies whenever the RHS fully determines the type through an explicit template argument.

---

## 10. Pointer Arithmetic (`cppcoreguidelines-pro-bounds-pointer-arithmetic`)

**Triggered by**: `cppcoreguidelines-pro-bounds-pointer-arithmetic`.

Raw pointer arithmetic (`p + n`, `argv[i]`) is flagged. The correct approach depends on context.

### 10.1 POSIX I/O helpers — required `std::span` migration

`send`/`recv` byte-loop helpers **must** use `std::span<std::byte>` parameters. This concentrates
the single unavoidable `pointer-arithmetic` suppression inside the helper and eliminates it from
every call site.

```cpp
// WRONG — void* + size_t signature, pointer arithmetic scattered at every call site
static bool send_all(int fd, const void *buf, size_t len) {
  const auto *p = static_cast<const uint8_t *>(buf);
  while (sent < len) {
    ::send(fd, p + sent, ...); // suppression needed here AND at every caller
  }
}

// CORRECT — span signature; single NOLINT isolated inside helper, zero at call sites
static bool send_all(int fd, std::span<const std::byte> data) noexcept {
  std::size_t sent = 0;
  while (sent < data.size()) {
    const ssize_t n =
        ::send(fd, data.data() + sent, data.size() - sent, // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic) — span interior POSIX boundary
               MSG_NOSIGNAL);
    if (n <= 0) return false;
    sent += static_cast<std::size_t>(n);
  }
  return true;
}

static bool recv_all(int fd, std::span<std::byte> buf) noexcept {
  std::size_t got = 0;
  while (got < buf.size()) {
    const ssize_t n =
        ::recv(fd, buf.data() + got, buf.size() - got, 0); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic) — span interior POSIX boundary
    if (n <= 0) return false;
    got += static_cast<std::size_t>(n);
  }
  return true;
}

// Call sites — no arithmetic, no NOLINT needed:
const uint32_t len_net = htonl(static_cast<uint32_t>(json.size()));
send_all(fd, std::as_bytes(std::span{&len_net, 1}));
send_all(fd, std::as_bytes(std::span{json.data(), json.size()}));

std::string body(req_len, '\0');
recv_all(client_fd, std::as_writable_bytes(std::span{body.data(), body.size()}));
```

### 10.2 `argv` pointer arithmetic

See §21 for the full `argv` + `std::string_view` + `std::from_chars` pattern.

### 10.3 General rule

+> Suppress `cppcoreguidelines-pro-bounds-pointer-arithmetic` **only** at C/POSIX API boundaries
> where `std::span` cannot be used. Never in business logic. New code must not introduce
> raw pointer arithmetic outside of IO helpers or explicit C API adapter functions.

---

## 11. Ignored Return Values — C Signal API (`cert-err33-c`)

**Triggered by**: `cert-err33-c`.

`signal()` returns the previous signal handler or `SIG_ERR`. `sigaction()` returns 0 or -1.
Both return values are meaningful; ignoring them silently suppresses failure detection.

```cpp
// WRONG
signal(SIGPIPE, SIG_IGN);
sigaction(SIGTERM, &sa, nullptr);
sigaction(SIGINT, &sa, nullptr);

// CORRECT — explicitly discard when not needed
(void)signal(SIGPIPE, SIG_IGN);
// CORRECT — sigaction: also requires (void) cast when return is not checked
(void)sigaction(SIGTERM, &sa, nullptr);
(void)sigaction(SIGINT, &sa, nullptr);

// PREFERRED — use sigaction, which has no ignored-return issue and is async-signal-safe
struct sigaction sa{};
sa.sa_handler = SIG_IGN;
sigemptyset(&sa.sa_mask);
(void)sigaction(SIGPIPE, &sa, nullptr);
```

For all C functions listed in CERT rule ERR33-C — including `signal`, `sigaction`, `remove`,
`rename`, `fclose`, `setvbuf` — either check the return value or cast to `(void)` at the call site.
`sigaction` is in this set. Every call site must either branch on the return or cast to `(void)`.

---

## 12. `reinterpret_cast` (`cppcoreguidelines-pro-type-reinterpret-cast`)

**Triggered by**: `cppcoreguidelines-pro-type-reinterpret-cast`.

       reinterpret_cast<struct sockaddr *>( // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
           &addr),
        sizeof(addr));
```
 
**NOLINT placement**: The `// NOLINT(check-name)` comment must appear on the **same source line**
as the token that triggers the diagnostic. For `reinterpret_cast`, the diagnostic fires at the
`reinterpret_cast` keyword — not at the closing `)` or the argument. If line-breaking splits the
cast across lines, place the NOLINT on the line containing `reinterpret_cast<...>(`.

```cpp
// WRONG — NOLINT on argument line, diagnostic fires on reinterpret_cast line
reinterpret_cast<struct sockaddr *>(
    &addr), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)  ← INEFFECTIVE

// CORRECT — NOLINT on same line as reinterpret_cast keyword
reinterpret_cast<struct sockaddr *>( // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    &addr),
```


Never use `reinterpret_cast` in business logic, type-punning, or for type erasure. Use `std::bit_cast` (C++20) or `std::memcpy` for safe type-punning.

---

## 13. Cognitive Complexity (`readability-function-cognitive-complexity`)

**Triggered by**: `readability-function-cognitive-complexity` (threshold: 25).

Functions exceeding complexity 25 must be refactored. The clang-tidy check fires before the function grows further and becomes untestable.
```
parse_request()  → complexity 57  (VIOLATION)
main()           → complexity 67  (VIOLATION)
```

**Refactoring strategies**:

- Extract early-return guards into named predicate functions.
- Extract each argument-parsing branch into `parse_arg_*()` functions called from a dispatch table or `std::array<std::pair<std::string_view, Handler>>`.
- Extract JSON-section parsing (`filters`, `criteria`, `options`) into separate `parse_filters()`, `parse_criteria()`, `parse_options()` free functions.
- `main()`: move all argument parsing before socket setup into `parse_args(int argc, char** argv, ServerConfig&) -> bool`.

Target: every function ≤ 25 cognitive complexity. Aim for ≤ 15 in new code.

---

## 14. Easily Swappable Parameters (`bugprone-easily-swappable-parameters`)

**Triggered by**: `bugprone-easily-swappable-parameters`.

Adjacent function parameters of types that are **implicitly convertible** to each other
(e.g. `int` ↔ `size_t`, `int` ↔ `int`, `size_t` ↔ `unsigned long`) create silent
transposition bugs. clang-tidy fires when two or more adjacent parameters share a
convertible underlying type.

### 14.1 Named Aggregate Struct (group of related values)

Bundle logically related adjacent parameters into a single named struct:
```cpp
// WRONG — size_t position, int active, int total: all mutually convertible
bool send_status_frame(int fd, const char *state, size_t position, int active, int total);

// CORRECT
struct StatusFrame {
  size_t position;
  int    active;
  int    total;
};
static bool send_status_frame(int fd, std::string_view state, StatusFrame sf);

// Call site — aggregate init makes every field explicit:
send_status_frame(client_fd, "computing", StatusFrame{0, active, cfg.worker_count});
```

### 14.2 Tagged-Value-Struct (single value with a distinct type)

When the parameters are **not** logically grouped but are individually typed primitives
that happen to be convertible, wrap each in a minimal tagged struct with a single `.v`
member. The outer struct types are structurally distinct and not implicitly convertible,
so clang-tidy sees no swappable pair.

```cpp
// WRONG — int fd and size_t max_depth: int converts to size_t
bool push(int fd, size_t max_depth = 0);

// CORRECT — QueueDepth wraps size_t; no implicit int↔QueueDepth conversion exists
struct QueueDepth { size_t v; };
bool push(int fd, QueueDepth max_depth = {0});

// Inside the function: access underlying value via .v
bool push(int fd, QueueDepth max_depth = {0}) {
  if (max_depth.v > 0 && queue_.size() >= max_depth.v) { ... }
}

// Call site: named construction
work_queue.push(client_fd, QueueDepth{MAX_QUEUE_DEPTH});
```

**Member-initializer-list extraction** — the pattern that silences the check in
constructor parameter lists (derived from a multi-user chat application rekey path
where `SessionId`, `UserName`, `Frame`, `ParsedMsg` all wrapped primitives):

```cpp
struct SessionId  { int v; };
struct UserName   { std::string v; };

// Adjacent (SessionId, UserName): underlying int/string — not convertible to each other,
// but SessionId ↔ SessionId is fine. The tagged struct breaks any cross-type conversion.
explicit Session(SessionId s, UserName u)
    : sid_(s.v), uname_(std::move(u.v)) {}

// Composite case — wrapper carries a pointer, init list dereferences it:
struct Frame      { const FrameData *v; };
struct ParsedMsg  { const Msg *v; };

explicit Connection(UserName u, Frame f, ParsedMsg p_)
    : uname_(std::move(u.v)), frame_(*f.v), parsed_(*p_.v) {}

// Span case — std::span is itself a non-convertible named type, so it already
// satisfies the check when paired with structurally unrelated parameters:
explicit DataView(std::span<const unsigned char> d) : data_(d) {}
```

> **Naming convention**: the single wrapped member is always named `v`. This makes the
> pattern machine-recognizable across the codebase and avoids naming debates. Every
> extraction in a member-init list uses `.v` (or `*param.v` if the wrapped value is a
> non-null pointer that must be dereferenced).

### 14.3 `enum class` Strong Typedef (integer identity type)

Use when the integer value has **identity semantics** (e.g. a file descriptor that must
never be accidentally passed as a count or offset):

```cpp
enum class SocketFd : int {};

// Construction requires explicit cast — prevents int bleeding across boundaries:
const auto fd = SocketFd{::accept(listen_fd, ...)};

// Extraction for POSIX syscalls:
::send(static_cast<int>(fd), ...);
```

> `enum class` prevents arithmetic on the value; use tagged-value-struct (`struct T { int v; }`)
> when arithmetic or default-construction is needed. Use `enum class` when the integer is an
> opaque handle.

### 14.4 Decision table

| Situation | Fix |
|---|---|
| ≥2 adjacent params logically form a record | named aggregate struct (§14.1) |
| Single param of primitive type, not a handle | tagged-value-struct `struct T { X v; }` (§14.2) |
| Integer handle (fd, id) that must never do arithmetic | `enum class T : int {}` (§14.3) |
| Constructor params trigger the check | member-init-list `.v` extraction pattern (§14.2) |

> **Rule**: never suppress `bugprone-easily-swappable-parameters` with `// NOLINT`. The check
> is always fixable with one of the patterns above. `// NOLINT` on this check is rejected in
> code review without exception.
---

## 15. Non-Const Global Variables (`cppcoreguidelines-avoid-non-const-global-variables`)

**Triggered by**: `cppcoreguidelines-avoid-non-const-global-variables`.

`static std::atomic<bool> g_running`, `static std::atomic<int> g_listen_fd`, and `static std::atomic<int> g_active_workers` are flagged. These are signal-handler–accessible globals and cannot be trivially moved into a struct without careful analysis.

**Accepted suppression pattern** for signal-handler globals only:
```cpp
// NOLINT(cppcoreguidelines-avoid-non-const-global-variables) — signal handler boundary
static std::atomic<bool> g_running{true};
```

For all other non-const globals: refactor into a `struct ServerState` passed by reference. New code must not introduce non-const non-atomic globals without this suppression comment and a justification.

---

## 16. Array-to-Pointer Decay (`cppcoreguidelines-pro-bounds-array-to-pointer-decay`)

**Triggered by**: `cppcoreguidelines-pro-bounds-array-to-pointer-decay`.

Passing C array members (e.g. `addr.sun_path`) to functions expecting `char*` / `void*` decays the bounds information.

### 16.1 Preferred: explicit address-of element zero

Taking `&arr[0]` is explicit pointer formation, not array-to-pointer decay.
This avoids the diagnostic without requiring a `// NOLINT` suppression:
```cpp
// WRONG — implicit array-to-pointer decay
std::memcpy(addr.sun_path, src, len);

// CORRECT — explicit address-of avoids decay entirely, no NOLINT needed
std::memcpy(&addr.sun_path[0], src, len);
```

### 16.2 Fallback: NOLINT suppression at C API boundary

```cpp
// When &arr[0] is not applicable (e.g. API requires array reference):
some_c_api(
    addr.sun_path,  // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay) — C API boundary
    sizeof(addr.sun_path));
```

In new code, prefer `std::span<char>` or `std::array<char, N>` where the API allows it. At hard POSIX boundaries (`sockaddr_un::sun_path`, `ifr_name`), suppress with `// NOLINT` and a comment.

---

## 17. Constant Array Index (`cppcoreguidelines-pro-bounds-constant-array-index`)

**Triggered by**: `cppcoreguidelines-pro-bounds-constant-array-index`.

Array subscript with a non-constant index requires explicit bounds checking or use of `.at()`.
```cpp
// WRONG
addr.sun_path[std::min(cfg.socket_path.size(), sizeof(addr.sun_path) - 1)] = '\0';

// CORRECT — suppress only because std::min already guarantees the bound
// NOLINT(cppcoreguidelines-pro-bounds-constant-array-index) — index is min-bounded above
addr.sun_path[std::min(cfg.socket_path.size(), sizeof(addr.sun_path) - 1)] = '\0';
```

For business-logic arrays use `.at()` or `std::span` with explicit bounds checks. Suppress only at C struct boundaries where `.at()` is unavailable.

---



## 19. `NOLINT` Policy

`// NOLINT(check-name)` is permitted **only** at C/POSIX API boundaries where the violation is structurally unavoidable. Every suppression must include a brief reason on the same line:
```cpp
::bind(listen_fd,
       reinterpret_cast<struct sockaddr *>(&addr),  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast) — POSIX sockaddr boundary
       sizeof(addr));
```

`NOLINT` without a reason comment will be rejected in code review.

---

## 20. CMakeLists — Sanitizer Guard for `object-size` at `-O0`

**Triggered by**: `clang-diagnostic-invalid-command-line-argument`.

`-fsanitize=object-size` has no effect at `-O0` and emits a warning under Clang when
the Debug build type is active. The sanitizer list in `CMakeLists.txt` must exclude
`object-size` in Debug builds.

```cmake
# WRONG — object-size included unconditionally, fires warning at -O0
set(COMMON_SANITIZERS
    "undefined"
    ...
    "object-size"
    ...
)

# CORRECT — guard object-size behind non-Debug builds
set(COMMON_SANITIZERS
    "undefined"
    "shift"
    "shift-exponent"
    "shift-base"
    "integer-divide-by-zero"
    "vla-bound"
    "null"
    "return"
    "signed-integer-overflow"
    "bounds"
    "alignment"
    "float-divide-by-zero"
    "float-cast-overflow"
    "nonnull-attribute"
    "returns-nonnull-attribute"
    "bool"
    "enum"
    "vptr"
    "pointer-overflow"
    "builtin"
    "address"
    "leak"
)

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND COMMON_SANITIZERS "object-size")
endif()
```

> **Rule**: `object-size` must only be added to the sanitizer list when
> `CMAKE_BUILD_TYPE` is not `Debug`. This eliminates the
> `clang-diagnostic-invalid-command-line-argument` warning from every Debug build
> without removing the check from Release/RelWithDebInfo builds.

---

## 21. `argv` Handling and `std::from_chars` at C API Boundaries

**Triggered by**: `cppcoreguidelines-pro-bounds-pointer-arithmetic`.

`argv` is a C API boundary. All accesses via `argv[i]`, `argv[++i]`, and pointer
arithmetic within `argv` entries (e.g. `argv[i] + std::strlen(argv[i])`) fire the
`pro-bounds-pointer-arithmetic` check.

**Pattern A — argv iteration, first `const std::string` capture is already suppressed:**
```cpp
// CORRECT — suppress only at the argv boundary, once per iteration
const std::string arg(
    argv[i]); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic) — argv C API boundary

// WRONG — separate unguarded argv accesses in the same loop body
cfg.socket_path = argv[++i];

// CORRECT — suppress each argv access at the C boundary
cfg.socket_path =
    argv[++i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic) — argv C API boundary
```

**Pattern B — `std::from_chars` with argv pointer arithmetic:**
```cpp
// WRONG — raw argv pointer arithmetic as from_chars range
std::from_chars(argv[i], argv[i] + std::strlen(argv[i]), val);

// CORRECT — use std::string_view to wrap the argv entry; no pointer arithmetic at call site
const std::string_view sv(
    argv[i]); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic) — argv C API boundary
std::from_chars(sv.data(), sv.data() + sv.size(), val);
```

Using `std::string_view` around each `argv[i]` entry (suppressed once) eliminates all
downstream pointer arithmetic at the `from_chars` call site without additional suppressions.
Apply this pattern to every `--uid`, `--gid`, `--workers` (and equivalent) argument parsers.

> **Rule**: Wrap each `argv[i]` access in a `const std::string_view sv(argv[i])` with a
> single `NOLINT` at the `argv[i]` site. All subsequent use of `sv.data()` / `sv.size()`
> is clean. Never scatter `argv[i] + strlen(argv[i])` arithmetic across multiple call sites.


---

## 22. Range-Based For Loop Copy (`performance-for-range-copy`)

**Triggered by**: `performance-for-range-copy`.

Range-based for without a reference qualifier copies each element on every iteration.
```cpp
// WRONG — copies element, fires performance-for-range-copy
for (auto w : workers) { w.join(); }
for (auto elem : large_vector) { process(elem); }
for (auto kv : map) { use(kv.first, kv.second); }

// CORRECT — const ref when element is only read
for (const auto& elem : large_vector) { process(elem); }
for (const auto& [key, val] : map) { use(key, val); }

// CORRECT — mutable ref when element is modified in place
for (auto& w : workers) {
  if (w.joinable()) w.join();
}

// CORRECT — move from range when container is consumed and elements are move-only
for (auto&& elem : move_only_container) { sink(std::move(elem)); }
```

**Decision table**:

| Intent | Declaration |
|---|---|
| Read only | `const auto&` |
| Modify in place | `auto&` |
| Consume (move-only or last use) | `auto&&` + `std::move(elem)` |
| Copy intentionally (rare) | `auto` + `// NOLINT(performance-for-range-copy) — copy intentional` |

> **Rule**: every range-based for loop element must be `const auto&`, `auto&`, or `auto&&`.
> A bare `auto` element requires an explicit NOLINT with documented reason.

---

## 23. Unnecessary Value Parameters (`performance-unnecessary-value-param`)

**Triggered by**: `performance-unnecessary-value-param`.

A function parameter passed by value that is never moved-from, never stored by move,
and never passed to a consuming function, should be `const T&` to avoid copying.
```cpp
// WRONG — std::string copied on every call; parameter only read
void log_message(std::string msg) { std::cerr << msg << "\n"; }
void process(std::string path) { std::filesystem::exists(path); }

// CORRECT — const ref: no copy
void log_message(const std::string& msg) { std::cerr << msg << "\n"; }
void process(const std::string& path) { std::filesystem::exists(path); }

// CORRECT — by value AND move when parameter is stored/consumed
explicit Foo(std::string name) : name_(std::move(name)) {}
void push(std::string item) { queue_.push_back(std::move(item)); }

// CORRECT — std::string_view for read-only string parameters (preferred over const std::string&)
void log_message(std::string_view msg) { std::cerr << msg << "\n"; }
```

**Decision table for string/container parameters**:

| Parameter use | Correct declaration |
|---|---|
| Read-only, arbitrary string source | `std::string_view` |
| Read-only, always `std::string` owner | `const std::string&` |
| Stored by move | `std::string` (value) + `std::move` in body |
| Output parameter | `std::string&` |

> **Rule**: `std::string_view` is the default for read-only string parameters in new code.
> Value parameters of non-trivial type require documentation if not moved-from.

---

## 24. Qualified `auto` for Pointer Types (`readability-qualified-auto`)

**Triggered by**: `readability-qualified-auto`.

When `auto` is deduced as a pointer or const-pointer type, the pointer qualifier must be
written explicitly. A bare `auto` hides the indirection level.
```cpp
// WRONG — pointer nature and constness hidden behind auto
auto p = static_cast<uint8_t *>(buf);
auto obj = root->getObject("filters");
auto it = map.find(key);

// CORRECT — pointer qualifier explicit
auto* p = static_cast<uint8_t *>(buf);
const auto* obj = root->getObject("filters");  // read-only handle
auto it = map.find(key);                        // iterator: auto is correct, not a raw ptr
```

**Rules**:
- `auto*` when the deduced type is a mutable raw pointer.
- `const auto*` when the deduced type is a pointer-to-const or a read-only C-handle.
- `auto` is correct for iterators, smart pointers (`std::unique_ptr`, `Poco::JSON::Object::Ptr`),
 and non-pointer types; do not add `*` to these.
- When in doubt: write the explicit type. `auto` is not a substitute for type clarity at
  pointer boundaries.

> **Rule**: never write bare `auto` for a raw pointer initialisation. `auto*` or `const auto*`
> are required. A `static_cast` that produces a pointer type is always a pointer initialisation.

---

## 25. Dead Stores (`clang-analyzer-deadcode.DeadStores`)

**Triggered by**: `clang-analyzer-deadcode.DeadStores`.

A value is computed, assigned to a variable, and the variable is never subsequently read
before being overwritten or going out of scope. The analyzer considers this a logic error
(the computation was either wrong or the use was forgotten).
```cpp
// WRONG — first assignment to result is never read before overwrite
int result = 0;
result = compute();   // dead store: initial 0 never read

// WRONG — ptr set and immediately abandoned
auto* ptr = acquire();
ptr = nullptr;        // dead store: the acquired pointer is leaked and never freed

// CORRECT — initialise directly to final value
const int result = compute();

// CORRECT — explicit discard via (void) when a side-effect return is intentional
[[maybe_unused]] const auto old = std::exchange(flag_, true);
```

**Common sources**:
- Loop variables initialised to a sentinel that is never read before the first assignment.
- Error-code variables set to `0` then unconditionally overwritten.
- Structured binding members (`auto [ptr, ec] = ...`) where `ptr` is never used after
  `from_chars` — suppress with `(void)ptr` or `[[maybe_unused]]`.

```cpp
// from_chars — ptr member is unused; suppress cleanly
const std::string_view sv(argv[i]); // NOLINT(pro-bounds-pointer-arithmetic) — argv boundary
unsigned long val = 0;
const auto [end_ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
(void)end_ptr;   // end_ptr intentionally unused; ec is checked below
if (ec != std::errc{}) { ... }
```

> **Rule**: no variable may be assigned a value that is never read. Initialise at point of
> first meaningful value. Use `(void)` or `[[maybe_unused]]` only when discarding a
> side-effect result is intentional and documented.

---

## 26. Additional `bugprone-*` Checks

### 26.1 Integer Division Truncation (`bugprone-integer-division`)

**Triggered by**: `bugprone-integer-division`.

Integer division of two `int`/`size_t` operands assigned to `float`/`double` silently
truncates the fractional part before widening.
```cpp
// WRONG — integer division computed first, result is 0 for processed < total
double pct = processed * 100 / total;
obj->set("pct", total > 0 ? static_cast<int>((processed * 100) / total) : 0);

// CORRECT — cast before division; numerator carries floating type
const double pct =
    (total > 0) ? (static_cast<double>(processed) * 100.0) / static_cast<double>(total)
                : 0.0;

// CORRECT — integer percentage (truncating) made explicit
const int pct_int =
    (total > 0) ? static_cast<int>((processed * 100ULL) / total) : 0;
```

> **Rule**: when a division result is assigned to `float`/`double`, at least the numerator
> must be cast to `double` before the division operator. Never rely on implicit widening
> after the division.

### 26.2 Signed Char as Array Index (`bugprone-signed-char-misuse`)

**Triggered by**: `bugprone-signed-char-misuse`.

`char` is implementation-defined as signed or unsigned. Using a `char` directly as an
array index or passing it to `<cctype>` functions (`std::isalpha`, `std::tolower`) is
undefined for values > 127.
```cpp
// WRONG — char may be negative; lookup_table[negative] is UB
const char c = input[i];
return lookup_table[c];

// WRONG — std::tolower with signed char is UB per C standard
const char lower = static_cast<char>(std::tolower(c));

// CORRECT — cast to unsigned char before use as index or cctype argument
const auto c = static_cast<unsigned char>(input[i]);
return lookup_table[c];
const auto lower = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
```

> **Rule**: `char` must be cast to `unsigned char` before any use as an array index or
> argument to `<cctype>` functions. Prefer `uint8_t` for byte buffers where char semantics
> are not required.

### 26.3 Narrowing Conversions (`bugprone-narrowing-conversions`)

**Triggered by**: `bugprone-narrowing-conversions` / `-Wconversion`.

Implicit narrowing from wider to narrower integer type (e.g. `size_t` → `int`,
`ptrdiff_t` → `int`) must be explicit.
```cpp
// WRONG — silent truncation if vec.size() > INT_MAX
int count = vec.size();
int idx = static_cast<int>(some_size_t);

// CORRECT — use the appropriate signed/unsigned width
const auto count = static_cast<std::ptrdiff_t>(vec.size());
const auto idx   = static_cast<std::ptrdiff_t>(some_size_t);

// CORRECT — when int range is known and verified
assert(vec.size() <= static_cast<size_t>(std::numeric_limits<int>::max()));
const int count = static_cast<int>(vec.size());
```

> **Rule**: every narrowing conversion must be a named `static_cast`. Implicit narrowing
> at any arithmetic expression or function argument is a defect.

---

## 27. Dispatch Table for Argument Parsing

**Triggered by**: `readability-function-cognitive-complexity` on argument-parsing functions.

An `if/else if` chain over N argument names with `&&` guards and nested validation
produces O(N) cognitive complexity growth. Replace with a dispatch table: a `constexpr`
array of `{name, handler}` pairs searched via `std::ranges::find` with a projection.

```cpp
struct ValueArgEntry {
  std::string_view name;
  int (*handler)(std::string_view value, ServerConfig &cfg);
};

struct FlagArgEntry {
  std::string_view name;
  int (*handler)(ServerConfig &cfg);
};

static constexpr std::array<ValueArgEntry, N> value_arg_table{{
    {"--socket", handle_arg_socket},
    {"--db", handle_arg_db},
    // ...
}};

static constexpr std::array<FlagArgEntry, M> flag_arg_table{{
    {"--no-peer-check", handle_flag_no_peer_check},
    // ...
}};
```

Lookup in the parsing loop:
```cpp
const auto va = std::ranges::find(value_arg_table, arg, &ValueArgEntry::name);
if (va != value_arg_table.end()) {
  if (i + 1 >= argv.size()) { /* missing value error */ return 1; }
  ++i;
  const int rc = va->handler(std::string_view{argv[i]}, cfg);
  if (rc != 0) return rc;
  continue;
}
```

Each handler is a single-responsibility free function with complexity ≤ 5.
The dispatch loop itself stays under complexity 20 regardless of argument count.
Adding a new argument = one handler function + one table entry — no parse_args modification.

---

## 28. `main()` Decomposition — Socket Setup and Accept Loop

**Triggered by**: `readability-function-cognitive-complexity` on `main()`.

A monolithic `main()` that creates a socket, binds, listens, installs signal handlers,
spawns workers, and runs an accept loop trivially exceeds complexity 25. Extract into
named helpers that each own a single phase:

| Helper | Responsibility | Returns |
|---|---|---|
| `setup_listen_socket(cfg)` | `socket()` → `bind()` → `listen()` | fd on success, -1 on failure |
| `install_signal_handlers()` | `sigaction` + `signal(SIGPIPE)` | void |
| `run_accept_loop(fd, queue)` | `accept()` loop with error/shutdown checks | void (returns on shutdown) |

`main()` becomes a sequential pipeline: parse → validate → setup → signal → log → workers → loop → teardown.
Target: `main()` ≤ 15 cognitive complexity.

`setup_listen_socket` owns `::close(fd)` on all error paths before returning -1.
`main()` owns `::close(listen_fd)` only on the success/shutdown path.

**`&addr.sun_path[0]` in `setup_listen_socket`**: Use explicit address-of (§16.1) instead
of bare `addr.sun_path` to avoid `pro-bounds-array-to-pointer-decay`.

**`reinterpret_cast` NOLINT in `setup_listen_socket` / `run_accept_loop`**: Place the NOLINT
on the line containing the `reinterpret_cast` keyword (§12).



## 29. Enum Base Type Sizing (`performance-enum-size`)

**Triggered by**: `performance-enum-size`.

When an `enum class` declares a base type wider than necessary for its value set,
clang-tidy flags the wasted storage.  Using `std::size_t` (8 bytes on x86-64) for
an enum with ≤ 255 values wastes 7 bytes per instance and per array element.

```cpp
// WRONG — 13 values stored in 8 bytes
enum class BaseId : std::size_t {
  Red = 0, /* ... */ Count = 13
};

// CORRECT — 13 values fit in 1 byte
enum class BaseId : std::uint8_t {
  Red = 0, /* ... */ Count = 13
};
```

**Downstream impact**: `static_cast<std::size_t>(id)` for array indexing continues to
work (implicit widening from `uint8_t` → `size_t`).  `static_cast<BaseId>(unsigned_val)`
also works (explicit narrowing cast).  No call-site changes required.

> **Rule**: enum class base type must be the smallest unsigned integer type that covers
> the value range.  Use `std::uint8_t` for ≤ 255 values, `std::uint16_t` for ≤ 65535.

---

## 30. Redundant Inline on Constexpr Functions (`readability-redundant-inline-specifier`)

**Triggered by**: `readability-redundant-inline-specifier`.

`constexpr` on a function definition implies `inline` (C++17 §10.1.5/1).
Writing `inline constexpr` is redundant and clutters the signature.

```cpp
// WRONG — inline is redundant on constexpr function
inline constexpr std::string_view base_display(BaseId id) noexcept { ... }

// CORRECT
constexpr std::string_view base_display(BaseId id) noexcept { ... }
```

**Exception**: `inline constexpr` on a *variable* is not redundant in all standards;
`constexpr` implies `inline` for variables only since C++17 for static data members
and since C++17 for namespace-scope variables.  The check fires only on functions.

> **Rule**: never write `inline` before `constexpr` on a function definition.
> `constexpr` implies `inline`.

---

## 31. Uppercase Integer Literal Suffixes (`readability-uppercase-literal-suffix`)

**Triggered by**: `readability-uppercase-literal-suffix`.

Lowercase suffixes (`1u`, `0xFFu`, `3.14f`) are visually ambiguous (`1l` vs `11`).
All integer and floating-point literal suffixes must be uppercase.

```cpp
// WRONG
return 1u << n;
unsigned mask = 0xFFFFFFFFu;
constexpr unsigned max_overlays = 3u;

// CORRECT
return 1U << n;
unsigned mask = 0xFFFFFFFFU;
constexpr unsigned max_overlays = 3U;
```

> **Rule**: all literal suffixes must be uppercase: `U`, `L`, `UL`, `ULL`, `LL`, `F`.

---

## 32. Identical Branch Bodies (`bugprone-branch-clone`)

**Triggered by**: `bugprone-branch-clone`.

Fires when two or more consecutive branches (if/else if, switch cases) have identical
bodies.  This indicates either dead logic or an opportunity to merge.

### 32.1 Switch Fall-Through (identical return)

```cpp
// WRONG — 5 consecutive cases with same body
switch (id) {
case BaseId::Red:   return ThermalClass::Warm;
case BaseId::Orange: return ThermalClass::Warm;
case BaseId::Yellow: return ThermalClass::Warm;
case BaseId::Pink:   return ThermalClass::Warm;
case BaseId::Brown:  return ThermalClass::Warm;

// CORRECT — fall-through merges identical branches
case BaseId::Red:
case BaseId::Orange:
case BaseId::Yellow:
case BaseId::Pink:
case BaseId::Brown:
  return ThermalClass::Warm;
```

### 32.2 Conditional Chain Collapse

```cpp
// WRONG — both branches set at_boundary = false
if (is_ish_suffix) {
  at_boundary = false;
} else if (next != ' ') {
  at_boundary = false;
}

// CORRECT — merge into single condition (if the outer guard guarantees
// isalpha(next), both conditions always fire; simplify)
if (std::isalpha(static_cast<unsigned char>(next)) != 0) {
  at_boundary = false;
}
```

> **Rule**: identical consecutive branch bodies must be merged.  If the branches
> differ only in dead conditions, simplify.

---

## 33. Struct Member Default Initialization (`cppcoreguidelines-pro-type-member-init`)

**Triggered by**: `cppcoreguidelines-pro-type-member-init`.

Non-class-type members of a struct/class that has no user-defined constructor must be
default-initialized to avoid reading uninitialized memory.

```cpp
// WRONG — base, wheel_position, normalized_hue, is_warm uninitialized
struct WheelDecomposition {
  BaseId base;
  std::optional<OverlayId> modifier;
  std::size_t wheel_position;
  double normalized_hue;
  bool is_warm;
};

// CORRECT — default member initializers
struct WheelDecomposition {
  BaseId base{};
  std::optional<OverlayId> modifier;
  std::size_t wheel_position{};
  double normalized_hue{};
  bool is_warm{};
};
```

`std::optional` is self-initializing (defaults to `std::nullopt`) and does not need
explicit `{}`.  All scalar types (`int`, `bool`, `double`, `size_t`, enum classes)
require `{}` or `= value`.

> **Rule**: every non-class-type member of an aggregate struct must have a default
> member initializer.  Use `{}` for zero-initialization.

---

## 34. C-Style Arrays → `std::array` (`modernize-avoid-c-arrays`)

**Triggered by**: `modernize-avoid-c-arrays`, `cppcoreguidelines-avoid-c-arrays`.

C-style arrays (`T arr[]`, `T arr[N]`) decay to pointers and lose size information.

```cpp
// WRONG
static constexpr std::string_view intensities[] = {"fancy vivid", ...};

// CORRECT
static constexpr std::array<std::string_view, N> intensities = {"fancy vivid", ...};
```

> **Rule**: use `std::array<T, N>` for all fixed-size arrays.  C-style arrays are
> permitted only at C API boundaries (e.g., `argv[]`).

---

## 35. `emplace_back` vs `push_back` (`modernize-use-emplace`)

**Triggered by**: `modernize-use-emplace`.

When constructing elements in-place inside a container, use `emplace_back` to avoid
creating a temporary object that is immediately moved.

```cpp
// WRONG — constructs temporary pair, then moves into vector
found_modifiers.push_back({pos, output_form});

// CORRECT — constructs pair in-place
found_modifiers.emplace_back(pos, output_form);
```

For named structs where aggregate initialization is ambiguous, prefer explicit
construction:

```cpp
// CORRECT — named struct with emplace
m.emplace_back(OvEntry{light, light});
```

> **Rule**: prefer `emplace_back` over `push_back` when the argument list matches
> a constructor or aggregate initializer of the element type.

---

## 36. `<cctype>` Return Values and Implicit Bool Conversion

**Triggered by**: `readability-implicit-bool-conversion` (extends §3 for non-pointer cases).

C library `<cctype>` functions (`std::isalpha`, `std::isspace`, `std::isdigit`, etc.)
return `int`, not `bool`.  Using the return value directly in a boolean context is an
implicit `int` → `bool` conversion.

```cpp
// WRONG — implicit int→bool
if (std::isalpha(static_cast<unsigned char>(c))) { ... }
if (!std::isalpha(static_cast<unsigned char>(c))) { ... }

// CORRECT — explicit comparison
if (std::isalpha(static_cast<unsigned char>(c)) != 0) { ... }
if (std::isalpha(static_cast<unsigned char>(c)) == 0) { ... }
```

Also applies to `std::toupper` / `std::tolower` return values when assigned to `char`:
```cpp
// WRONG — narrowing int→char
const char c = std::toupper(static_cast<unsigned char>(x));

// CORRECT — explicit static_cast
const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(x)));
```

> **Rule**: `<cctype>` function returns must be compared with `!= 0` / `== 0` in boolean
> contexts.  Assignments to `char` from `std::toupper`/`std::tolower` require explicit
> `static_cast<char>(...)`.

---

## 37. Array Subscript with Runtime Index (`cppcoreguidelines-pro-bounds-constant-array-index`)

**Triggered by**: `cppcoreguidelines-pro-bounds-constant-array-index`.

Extends §17 (C struct boundaries) to general `std::array` usage.  Array subscript `[]`
with a non-constant-expression index is flagged because it performs no bounds checking.

```cpp
// WRONG — runtime index without bounds check
auto display = base_labels[bi].display;
names[i] = hue_wheel[i].name;

// CORRECT — .at() performs bounds-checked access
auto display = base_labels.at(bi).display;
names.at(i) = hue_wheel.at(i).name;
```

In `constexpr` contexts, `.at()` violations become compile-time errors (out-of-bounds
`at()` is not a constant expression).  This makes `.at()` strictly preferable to `[]`
in constexpr lambdas and constexpr functions.

> **Rule**: use `.at()` for all `std::array` subscripts with runtime indices.
> Reserve `[]` only for cases where the index is a compile-time constant expression
> or where profiling proves `.at()` is a hot-path bottleneck.

---
## 38. Non-Private Members in Structs with Methods (`misc-non-private-member-variables-in-classes`)

**Triggered by**: `misc-non-private-member-variables-in-classes`.

clang-tidy treats any `struct`/`class` with member functions as a "class" and flags
public data members. For aggregate data-transfer structs that have utility methods,
this produces false positives.

### 38.1 Preferred Fix: Extract Methods to Free Functions

If the struct is fundamentally a data container (DTO, record, aggregate) and the
methods are convenience utilities (serialization, formatting), extract them as free
functions in the same namespace. This makes the struct a pure aggregate and
eliminates all `misc-non-private-member-variables-in-classes` warnings on its members.

```cpp
// WRONG — struct has method → all 38 public members flagged
struct Certificate {
  std::string certificate_id;
  std::string lab;
  // ... many data fields ...
  std::vector<std::string> merchants_holding;

  std::string get_merchants_csv() const { ... }
  void set_merchants_from_csv(const std::string &csv) { ... }
};

// CORRECT — pure aggregate + free functions
struct Certificate {
  std::string certificate_id;
  std::string lab;
  // ... many data fields ...
  std::vector<std::string> merchants_holding;
};

[[nodiscard]] inline std::string get_merchants_csv(const Certificate &cert) { ... }
inline void set_merchants_from_csv(Certificate &cert, const std::string &csv) { ... }
```

**Call-site migration**: `cert.get_merchants_csv()` → `get_merchants_csv(cert)`.

### 38.2 Alternative: NOLINT at Struct Level

When the struct genuinely needs methods (e.g., static constexpr members used as
a backwards-compatibility wrapper with no data), suppress at the struct declaration:

```cpp
struct ColorRarityRules { // NOLINT(misc-non-private-member-variables-in-classes) — static method wrapper, no data members
  static constexpr bool is_warm_hue(std::string_view hue) { ... }
};
```

For pure data structs with methods that cannot be extracted (e.g., the method mutates
private invariants), the NOLINT must include a reason explaining why the public
data is intentional.

> **Rule**: aggregate data structs must not have member functions. Extract utility
> methods as free functions in the same namespace. Use NOLINT only for
> backwards-compatibility wrappers or when extraction is structurally impossible.

---

## 39. `[[nodiscard]]` on Value-Returning Functions (`modernize-use-nodiscard`)

**Triggered by**: `modernize-use-nodiscard`.

Functions that return a value and have no side effects should be marked `[[nodiscard]]`
to prevent silent discard of important return values.

```cpp
// WRONG — caller can silently ignore result
std::string get_merchants_csv(const Certificate &cert) { ... }
std::string to_full_name(const ColorDescription &desc) { ... }

// CORRECT
[[nodiscard]] std::string get_merchants_csv(const Certificate &cert) { ... }
[[nodiscard]] std::string to_full_name(const ColorDescription &desc) { ... }
```

**When to apply**:
- Pure functions (no side effects, return value is the entire point)
- Const member functions that compute and return a value
- Factory functions, accessors, conversion functions
- Any function where discarding the return value is almost certainly a bug

**When NOT to apply**:
- Functions with primary side effects where the return value is optional status
  (e.g., `bool try_send(...)` where the caller may legitimately ignore failure)
- `void`-returning functions (obviously)
- Operators where `[[nodiscard]]` on every invocation would be noisy

> **Rule**: all pure, side-effect-free value-returning functions must be marked
> `[[nodiscard]]`.

---

## 3.1 Implicit Bool Conversion for Bitmask Operations

**Triggered by**: `readability-implicit-bool-conversion` (extends §3).

§3 covers pointer-to-bool conversions. The same check also fires on integer
bitmask expressions used in boolean context. Bitwise AND results are `unsigned int`,
not `bool`.

```cpp
// WRONG — unsigned int → bool implicit conversion
if (overlayMask & (1U << oi)) { ... }
if (strong_mask & (1U << oi)) { ... }

// CORRECT — explicit comparison
if ((overlayMask & (1U << oi)) != 0U) { ... }
if ((strong_mask & (1U << oi)) != 0U) { ... }
```

This pattern appears frequently in bitmask-based overlay/modifier systems.
The outer parentheses around the bitwise AND expression are required for
operator precedence (`!=` binds tighter than `&`).

> **Rule**: bitmask expressions used in boolean context must compare explicitly
> with `!= 0U` (or `== 0U` for negation). Never rely on implicit integer-to-bool.

---


## 40. Cognitive Complexity — Filter Chain Extraction

**Triggered by**: `readability-function-cognitive-complexity` on filter/matching functions.

A function that tests N filter categories with nested `if (!empty) { for (...) { if (...) } }`
loops grows at O(N²) cognitive complexity due to nesting penalties. Each category adds
~10 points. At 15+ categories, the function exceeds any reasonable threshold.

### 40.1 Extract Per-Category Filter Helpers

Each filter category becomes a standalone `[[nodiscard]]` free function that returns
`true` (pass) or `false` (reject):

```cpp
[[nodiscard]] inline bool check_clarity(
    const db::MerchantLot &lot,
    const QueryFilters &filters) {
  if (filters.clarity_grades.empty())
    return true;
  return std::ranges::any_of(filters.clarity_grades,
                             [&lot](const auto &g) { return lot.clarity == g; });
}
```

### 40.2 Use `std::ranges::any_of` / `std::ranges::all_of`

The inner `for` + `bool match` + `break` pattern is replaced by `std::ranges::any_of`:

```cpp
// WRONG — manual iteration with bool flag
bool match = false;
for (const auto &grade : filters.clarity_grades) {
  if (lot.clarity == grade) { match = true; break; }
}
if (!match) return false;

// CORRECT
if (!std::ranges::any_of(filters.clarity_grades,
                         [&lot](const auto &g) { return lot.clarity == g; }))
  return false;
```

This simultaneously fixes `readability-use-anyofallof`.

### 40.3 Caller Becomes Linear Chain

The parent function calls each helper in sequence. Each `if (!check_X()) return false`
adds exactly +1 complexity (no nesting penalty), keeping the total at O(N):

```cpp
inline bool lot_matches_filters(const db::MerchantLot &lot,
                                const QueryFilters &filters) {
  const bool is_white = is_white_diamond_color(lot.color);
  if (filters.diamond_type == DiamondType::White && !is_white) return false;
  if (filters.diamond_type == DiamondType::Colored && is_white) return false;
  if (!filter::check_white_color(lot, is_white, filters)) return false;
  if (!filter::check_clarity(lot, filters)) return false;
  // ... one line per category
  return true;
}
```

> **Rule**: filter chains with >3 categories must be decomposed into per-category
> helpers. Each helper is `[[nodiscard]]`, returns `bool`, and uses
> `std::ranges::any_of` / `std::ranges::all_of` instead of manual loops.

---

## 41. Post-Calculation Score Filter Decomposition

**Triggered by**: `readability-function-cognitive-complexity` on `apply_post_calculation_filters`.

When a function contains 30+ sequential `if (!check(...)) return false` calls grouped
by logical domain (score filters, rarity component filters, sparkle component filters,
required-fields filters), extract each group into a named helper:

```cpp
[[nodiscard]] inline bool check_score_filters(
    const DiamondResult &result, const QueryFilters &filters);
[[nodiscard]] inline bool check_rarity_component_filters(
    const DiamondResult &result, const QueryFilters &filters);
[[nodiscard]] inline bool check_sparkle_component_filters(
    const DiamondResult &result, const QueryFilters &filters);
[[nodiscard]] inline bool check_required_fields(
    const DiamondResult &result, const QueryFilters &filters);
```

This splits a complexity-73 function into four complexity-~18 functions.

> **Rule**: when a function consists of groups of checks on logically distinct
> domains, split each group into a named helper even if the individual checks
> are trivial. The function name documents the domain.

---

## 42. `ProgressCallback` by `const` Reference (`performance-unnecessary-value-param`)

**Triggered by**: `performance-unnecessary-value-param`.

`std::function<>` has a non-trivial copy constructor (may allocate). When a
function only invokes the callback without storing it, accept by `const &`:

```cpp
// WRONG — copies std::function on every call
inline void calculate_rarity_scores(..., ProgressCallback progress_cb = nullptr);

// CORRECT
inline void calculate_rarity_scores(..., const ProgressCallback &progress_cb = nullptr);
```

> **Rule**: accept `std::function` parameters by `const &` when the function does
> not store the callback beyond its own lifetime. Use value semantics only when
> the callback is moved into a member or container for later invocation.

---

## 43. `display_progress_bar` — Swappable Parameter Mitigation

**Triggered by**: `bugprone-easily-swappable-parameters` on adjacent `size_t` + `int`.

Bundle `current`, `total`, and `bar_width` into a named aggregate:

```cpp
struct ProgressBarParams {
  size_t current;
  size_t total;
  int bar_width;
};

inline void display_progress_bar(const std::string &label,
                                 ProgressBarParams p);
```

Call sites use aggregate init: `display_progress_bar("label:", {i, n, 50})`.

Additionally, use `double` instead of `float` for the progress computation
to avoid `bugprone-narrowing-conversions` (`int * float` loses precision):

```cpp
const double progress =
    static_cast<double>(p.current) / static_cast<double>(p.total);
const int pos = static_cast<int>(static_cast<double>(p.bar_width) * progress);
```

> **Rule**: see §14 for general swappable-parameter patterns. For progress
> bars specifically, bundle metrics into a struct and use `double` arithmetic.

---

## 44. `bugprone-branch-clone` — Merge Identical Branches

**Triggered by**: `bugprone-branch-clone`.

Adjacent `if`/`else if` branches with identical bodies should be merged with `||`:

```cpp
// WRONG — two branches with identical body
if (i < pos)
  std::cout << ">";
else if (i == pos)
  std::cout << ">";

// CORRECT — single condition
if (i <= pos)
  std::cout << ">";
```

> **Rule**: merge branches that produce identical code. If the conditions
> are mutually exclusive with the same action, combine with `||` or `<=`/`>=`.

---

## 45. Poco JSON Owning Memory (`cppcoreguidelines-owning-memory`)

**Triggered by**: `cppcoreguidelines-owning-memory`.

`Poco::JSON::Object::Ptr` and `Poco::JSON::Array::Ptr` are `SharedPtr`
wrappers. Passing `new Poco::JSON::Array` to `root->set(...)` creates a
raw `new` whose ownership transfer is implicit.

### 45.1 Suppress at API Boundary

Poco's JSON API requires `new`-allocated objects. Use NOLINT:

```cpp
root->set("rarity",
    Poco::JSON::Array::Ptr(new Poco::JSON::Array)); // NOLINT(cppcoreguidelines-owning-memory) — Poco JSON API
```

### 45.2 Named Smart Pointer (Cleaner)

```cpp
Poco::JSON::Array::Ptr arr(new Poco::JSON::Array); // NOLINT(cppcoreguidelines-owning-memory)
root->set("rarity", arr);
```

> **Rule**: NOLINT with `— Poco JSON API` reason. Prefer wrapping in a named
> `Ptr` variable over inline `new` in function arguments.

---

## 46. Database Helper Extraction for Cognitive Complexity

**Triggered by**: `readability-function-cognitive-complexity` on `query_by_certificate_id` (79),
`query_lots_from_db` (53).

Both functions repeat the pattern: load merchant tables from registry → iterate tables →
prepare statement → read rows → filter → merge merchants. Extract shared sub-patterns:

```cpp
[[nodiscard]] inline std::vector<std::string>
load_merchant_tables(sqlite3 *db);

inline void merge_merchants_into_name(
    db::MerchantLot &lot,
    std::vector<std::string> &merchants);
```

`load_merchant_tables` encapsulates the `SELECT ... FROM merchant_registry` +
`reinterpret_cast` + null-check + `emplace_back` pattern. Both `query_by_certificate_id`
and `query_lots_from_db` call it instead of duplicating the code.

`merge_merchants_into_name` encapsulates sort → unique → erase → ostringstream join.

> **Rule**: when two or more functions share a non-trivial code pattern (>5 lines),
> extract the pattern into a named helper. This reduces both cognitive complexity
> and maintenance risk from divergent copies.

---

## 47. `std::to_array` for Constexpr Arrays

When converting C-style arrays to `std::array` (§34) and the element type is uniform
(e.g., `std::string_view`), `std::to_array` provides the cleanest conversion:

```cpp
// WRONG — C-style array
constexpr std::string_view columns[] = {"id", "name", "value"};

// CORRECT — std::to_array deduces size
constexpr auto columns = std::to_array<std::string_view>({"id", "name", "value"});
```

`std::to_array` is C++20 (`<array>` header). It deduces the array size from the
initializer list, avoiding manual `N` specification and `static_assert` drift.

Use `.at()` (§37) for runtime-indexed access to the resulting `std::array`.

> **Rule**: prefer `std::to_array<T>({...})` over manual `std::array<T, N>` when
> the element type is known and size is determined by the initializer.

---

## 48. Struct Method Extraction — Call-Site Migration Checklist

When extracting methods from structs to free functions (§38), all call sites must
be updated. The pattern is mechanical:

| Before | After |
|---|---|
| `criteria.to_exclusions()` | `to_exclusions(criteria)` |
| `criteria.to_field_names()` | `to_field_names(criteria)` |
| `result.get_group_key()` | `get_group_key(result)` |
| `group.sort_by_rarity()` | `sort_by_rarity(group)` |
| `group.display_summary(os, mode)` | `display_summary(group, os, mode)` |

Use `grep -rn 'object\.method_name(' src/` to find all call sites before applying.
Missing a call site produces a compile error (method no longer exists), so the
compiler enforces completeness.

> **Rule**: after extracting struct methods, run a full build to verify all call
> sites are updated. The compiler is the authoritative checker.

## 49. Duplicate Branch Bodies (`bugprone-branch-clone`)

**Triggered by**: `bugprone-branch-clone`.

When adjacent `if`/`else if` branches have identical bodies, clang-tidy flags them.
This is a common indicator that the branches should be combined with `||` or that
one branch has a copy-paste bug.

```cpp
// WRONG — green and blue have identical bodies
if (base_lower == "green") {
  pure_hue_factor = 2.0;
  pure_hue_uncertainty = 0.30;
} else if (base_lower == "blue") {
  pure_hue_factor = 2.0;
  pure_hue_uncertainty = 0.30;
}

// CORRECT — combined with ||
if (base_lower == "green" || base_lower == "blue") {
  pure_hue_factor = 2.0;
  pure_hue_uncertainty = 0.30;
}
```

If the identical bodies are intentional (e.g., future divergence planned), suppress
with NOLINT and a comment explaining the intent.

> **Rule**: adjacent branches with identical bodies must be combined with `||` unless
> documented as intentionally separate for future divergence.

---

## 50. Variable Shadowing in `if`/`else if` Chains (`-Wshadow`)

**Triggered by**: `-Wshadow`.

When structured bindings or `auto` variables in `if` initializers share the same name
across `else if` branches, the later declaration shadows the earlier one. While each
is in its own scope, `-Wshadow` flags this as confusing.

```cpp
// WRONG — r shadows previous r
if (auto r = try_parse_csv_filter(arg, i, argc, argv, filters); r.has_value()) {
  if (!*r) return false;
} else if (auto r = try_parse_double_filter(arg, i, argc, argv, filters); r.has_value()) {
  if (!*r) return false;
}

// CORRECT — unique names per branch
if (auto r_csv = try_parse_csv_filter(arg, i, argc, argv, filters); r_csv.has_value()) {
  if (!*r_csv) return false;
} else if (auto r_dbl = try_parse_double_filter(arg, i, argc, argv, filters); r_dbl.has_value()) {
  if (!*r_dbl) return false;
}
```

Alternative: use `r2`, `r3` suffixes when the variable purpose is identical across
branches (dispatch chain).

> **Rule**: every `if`-initializer variable in an `if`/`else if` chain must have
> a unique name. Use descriptive suffixes or numbered suffixes.

---

## 51. By-Value Parameters — Move Semantics (`performance-unnecessary-value-param`)

**Triggered by**: `performance-unnecessary-value-param`.

Extends §42 (which covers `std::function`) to general by-value parameters.

When a function takes a parameter by value and uses it only as an rvalue (passes it
to another function, returns it, or moves from it), the parameter should either:
(a) be taken by `const &` if only read, or
(b) remain by value but `std::move()` it at the single use site.

```cpp
// WRONG — copies s, then copies again into normalize_color_string
static inline std::string normalize_for_match(std::string s) {
  return color::normalize_color_string(s);
}

// CORRECT — move the owned copy
static inline std::string normalize_for_match(std::string s) {
  return color::normalize_color_string(std::move(s));
}
```

If the function reads the parameter (no mutation, no forwarding), prefer `const &`:
```cpp
// CORRECT — read-only access
static inline bool is_valid_name(const std::string &s) {
  return !s.empty() && s.size() < 128;
}
```

> **Rule**: by-value parameters that are only used once must be `std::move`'d at
> that use site. By-value parameters that are only read should be `const &` instead.

---

## 52. Unnecessary Copy Initialization (`performance-unnecessary-copy-initialization`)

**Triggered by**: `performance-unnecessary-copy-initialization`.

When a local variable is copy-initialized from another variable and never modified,
use `const auto &` instead of copying.

```cpp
// WRONG — copies norm into fluor, but fluor is never modified
std::string norm = normalize_fluorescence(lot.Fluoresence_Intensity);
std::string fluor = norm;

// CORRECT — reference to existing variable
const std::string norm = normalize_fluorescence(lot.Fluoresence_Intensity);
const auto &fluor = norm;
```

This also applies to copies from function return values when the result is only read:
```cpp
// WRONG
auto root = parsed.extract<Poco::JSON::Object::Ptr>();

// CORRECT
const auto &root = parsed.extract<Poco::JSON::Object::Ptr>();
```

> **Rule**: local variables initialized from another variable and never modified
> must use `const auto &`. Only copy when the variable will be mutated or outlives
> the source.

---

## 53. Template Score Application Pattern — Cognitive Complexity Reduction

**Triggered by**: `readability-function-cognitive-complexity` on scoring functions.

When a function contains N identical `if (criteria && has_data) { score; if (score > 0) code += X; }`
blocks, each block adds ~6 cognitive complexity due to nesting. Extract the pattern
into a template helper:

```cpp
template <typename F>
static inline void apply_score_if(
    bool criteria_flag,
    bool has_data,
    std::optional<double> &target,
    std::string &profile_codes,
    std::string_view code,
    F &&scorer) {
  if (criteria_flag && has_data) {
    target = std::forward<F>(scorer)();
    if (target && *target > 0.0)
      profile_codes += code;
  }
}
```

Each scoring block then becomes a single call:

```cpp
apply_score_if(criteria.calc_color, has_color, result.color_score, profile_codes, "C",
    [&]() -> std::optional<double> { return rarity::read::query_color_rarity(db, lot); });
```

9 blocks × 6 complexity = 54 points reduced to 9 × 1 = 9 points (each call adds +1
for the nesting level of the enclosing loop only, no internal branching penalty).

> **Rule**: when 3+ identical structural patterns appear in a function, extract
> the pattern into a template helper parameterized by a callable. This is preferred
> over macro extraction.

---

## 54. Prefix Matching with `std::ranges::any_of`

**Triggered by**: `readability-function-cognitive-complexity` on functions with nested
`for` + `if` + `bool` flag + `break` patterns for prefix matching.

```cpp
// WRONG — manual loop with bool flag (adds +4 cognitive complexity per use)
bool matches = false;
for (const auto &prefix : prefixes) {
  if (starts_with_case_insensitive(value, prefix)) {
    matches = true;
    break;
  }
}
if (!matches) continue;

// CORRECT — extracted helper
[[nodiscard]] static inline bool
match_any_prefix(const std::string &value,
                 const std::vector<std::string> &prefixes) {
  return std::ranges::any_of(prefixes, [&value](const auto &prefix) {
    return starts_with_case_insensitive(value, prefix);
  });
}

// Call site — zero nesting penalty
if (!match_any_prefix(cert_id, cert_id_prefixes))
  continue;
```

This simultaneously satisfies `readability-use-anyofallof` (§40.2) and reduces
cognitive complexity by eliminating the `bool` flag + `break` + negated-check pattern.

> **Rule**: all "match any element" patterns must use `std::ranges::any_of` or
> an extracted helper function. Manual `for` + `bool` + `break` is prohibited.

---

## 55. Progress Callback Extraction

**Triggered by**: `readability-function-cognitive-complexity` on functions with
duplicated progress-reporting blocks.

When multiple functions (e.g., `calculate_rarity_scores`, `calculate_sparkle_scores`)
contain identical progress-checking code with nested `if (progress_cb)` + elapsed
time check + early return, extract into a shared helper:

```cpp
static inline bool
handle_progress(const ProgressCallback &progress_cb,
                std::chrono::steady_clock::time_point &last_progress,
                std::string_view phase,
                std::size_t current,
                std::size_t total,
                std::vector<DiamondResult> &results) {
  const auto now = std::chrono::steady_clock::now();
  const auto elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - last_progress)
          .count();
  if (elapsed_ms >= static_cast<long long>(PROGRESS_TIME_INTERVAL_MS) ||
      current == 0) {
    if (!progress_cb(std::string(phase), current, total)) {
      results.clear();
      return false;
    }
    last_progress = now;
  }
  return true;
}
```

Each call site reduces from ~12 lines / +8 complexity to 2 lines / +2 complexity:
```cpp
if (progress_cb) {
  if (!handle_progress(progress_cb, last_progress, "rarity", i, lots.size(), results))
    return;
}
```

> **Rule**: progress-reporting blocks that appear in 2+ functions must be extracted
> into a shared helper.

---

## 56. Sign Conversion in Index Variables (`clang-diagnostic-sign-conversion`)

**Triggered by**: `-Wsign-conversion`.

Extends §6 to cover loop index variables used as container subscripts.

When an `int` or `ptrdiff_t` variable is used to index a container expecting `size_type`
(unsigned), the implicit sign conversion is flagged. Use `std::size_t` for indices
that are always non-negative, or `static_cast<std::size_t>()` when converting from
a signed computation.

```cpp
// WRONG — int idx used as vector subscript
int idx = find_position(canonical, value);
result.push_back(canonical[idx]);       // int → size_t implicit

// CORRECT — explicit cast at subscript
const std::ptrdiff_t idx = find_position(canonical, value);
result.push_back(canonical[static_cast<std::size_t>(idx)]);

// Also correct — use size_t directly when the value is guaranteed non-negative
const std::size_t idx = static_cast<std::size_t>(find_position(canonical, value));
result.push_back(canonical[idx]);
```

When the index can be negative (sentinel value -1), use `std::ptrdiff_t` and guard
the conversion:
```cpp
std::ptrdiff_t idx = -1;
// ... set idx ...
if (idx >= 0) {
  result.push_back(canonical[static_cast<std::size_t>(idx)]);
}
```

> **Rule**: container subscript indices must be `std::size_t` or explicitly cast
> via `static_cast<std::size_t>()`. Use `std::ptrdiff_t` for indices that may be
> negative, with explicit cast at the use site after a non-negative guard.


---

## 57. Tagged-Value-Struct for MPFR Precision Parameters (`bugprone-easily-swappable-parameters`)

**Triggered by**: `bugprone-easily-swappable-parameters` on adjacent `double` and `mpfr_prec_t` parameters.

`mpfr_prec_t` is `long` on most platforms, which is implicitly convertible to/from `double`.
When a function takes both types as adjacent parameters, clang-tidy flags the transposition risk.

```cpp
// WRONG — double and mpfr_prec_t are implicitly convertible
template <typename ReportT, typename IntervalT>
inline std::string
populate_probability_and_intervals(ReportT &report,
                                   const IntervalT &combined_interval,
                                   double combined_rarity_score,
                                   mpfr_prec_t prec);

// CORRECT — wrap mpfr_prec_t in tagged-value-struct (§14.2)
struct PrecisionTag {
  mpfr_prec_t v;
};

template <typename ReportT, typename IntervalT>
inline std::string
populate_probability_and_intervals(ReportT &report,
                                   const IntervalT &combined_interval,
                                   double combined_rarity_score,
                                   PrecisionTag prec_tag);

// Call site — explicit construction prevents silent swap
populate_probability_and_intervals(report, interval, score, PrecisionTag{prec});
```

Inside the function body, access the value via `prec_tag.v`.

> **Rule**: when `mpfr_prec_t` or other `long`-typed parameters appear adjacent to
> `double` parameters, wrap the non-double parameter in a tagged-value-struct per §14.2.

---

## 58. `detail::` Namespace for Cognitive Complexity Decomposition

**Triggered by**: `readability-function-cognitive-complexity` (extends §13).

When decomposing a high-complexity function into smaller helpers, place the helpers
in a nested `detail::` namespace to signal they are implementation-internal and not
part of the public API.

```cpp
namespace rarity {

namespace detail {

inline void collect_appearance_occurrences(
    std::vector<InclusionOccurrence> &occurrences,
    const std::vector<InclusionAppearance> &appearances,
    const std::vector<InclusionMineral> &minerals) {
  // ... extracted logic, each helper ≤ 25 complexity ...
}

inline void collect_orphan_mineral_occurrences(/*...*/) { /*...*/ }
inline void apply_overpressure_corrections(/*...*/) { /*...*/ }

} // namespace detail

inline std::vector<InclusionOccurrence> get_seed_inclusion_occurrences() {
  // ... setup ...
  detail::collect_appearance_occurrences(occurrences, appearances, minerals);
  detail::collect_orphan_mineral_occurrences(occurrences, appearances, minerals);
  detail::apply_overpressure_corrections(occurrences, minerals);
  return occurrences;
}

} // namespace rarity
```

**Advantages over other decomposition strategies**:
- Helpers are invisible to downstream `#include` consumers (they won't tab-complete `detail::`)
- Clear signal that these functions exist only to reduce complexity of the parent function
- No risk of accidental external usage creating coupling

> **Rule**: internal helpers extracted for complexity reduction must live in a nested
> `detail::` namespace. Public API surface remains unchanged.

---

## 59. Defaulted Comparison Operators Require Member Form

**Triggered by**: `misc-non-private-member-variables-in-classes` on structs with
defaulted `operator<=>` or `operator==`.

Per C++20 §11.10.1, defaulted comparison operators (`= default`) must be member functions.
They cannot be extracted to free functions per §38. When a struct has both public data
members and defaulted comparisons, the NOLINT must explain why extraction is impossible.

```cpp
// CORRECT — NOLINT with reason; comparisons cannot be extracted
struct CalculationProfile { // NOLINT(misc-non-private-member-variables-in-classes)
                            // — aggregate data struct; defaulted comparison
                            // operators require member form
  bool has_color = false;
  bool has_clarity = false;
  // ...

  auto operator<=>(const CalculationProfile &) const = default;
  bool operator==(const CalculationProfile &) const = default;
};

// Utility methods CAN and MUST be extracted per §38
[[nodiscard]] inline std::string to_string(const CalculationProfile &p) { ... }
```

**Decision table** for structs with both data and methods:

| Method type | Can extract to free function? | Action |
|---|---|---|
| Utility (to_string, serialize, format) | Yes | Extract per §38 |
| Defaulted `operator<=>` / `operator==` | No (C++20 requirement) | Keep as member, NOLINT on struct |
| Custom comparison logic | Yes (but consider if defaulted suffices) | Extract if not defaulted |
| Static constexpr members | Keep as member | NOLINT per §38.2 |

> **Rule**: defaulted comparison operators are the sole permitted exception to §38.
> The struct-level NOLINT must reference this section (§59) and explain the constraint.

---

## 60. Struct Scalar Member Default Initializers — `double` in Aggregates

**Triggered by**: `cppcoreguidelines-pro-type-member-init` (extends §33).

§33 covers general aggregate struct initialization. This section documents the
specific pattern for `double` and `bool` members in structs that cross translation
unit boundaries (e.g., passed to/from sqlite3 callbacks, serialized to JSON):

```cpp
// WRONG — uninitialized double and bool
struct ParsedInclusion {
  std::string gia_term;
  std::string mineral_name;
  double joint_probability_pct;  // indeterminate
  bool has_mineral;              // indeterminate
};

// CORRECT — zero-initialized with {}
struct ParsedInclusion {
  std::string gia_term;
  std::string mineral_name;
  double joint_probability_pct{};
  bool has_mineral{};
};
```

**Why `{}` not `= 0.0` / `= false`**: `{}` is idiomatic C++11+ value-initialization.
It works uniformly for all scalar types without requiring type-specific literals.
It also prevents narrowing conversions (e.g., `int x = 3.14;` compiles, `int x{3.14};`
does not).

> **Rule**: all scalar members (`double`, `bool`, `int`, `size_t`, enum classes) in
> aggregate structs must use `{}` default initialization. `= value` form is acceptable
> only when the default is non-zero.

---

## 61. sqlite3 `reinterpret_cast` with Null Check Pattern

**Triggered by**: `cppcoreguidelines-pro-type-reinterpret-cast` + `readability-implicit-bool-conversion`.

`sqlite3_column_text()` returns `const unsigned char *` which must be cast to
`const char *` for std::string construction. The combined pattern must handle:
1. The `reinterpret_cast` (unavoidable at sqlite3 boundary)
2. The null check (column may be SQL NULL)

```cpp
// WRONG — reinterpret_cast without NOLINT, implicit bool conversion
const char *gia_term =
    reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
occ.gia_term = gia_term ? gia_term : "";

// CORRECT — NOLINT with reason, explicit null check
const auto *gia_term = // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                       // — sqlite3 API boundary
    reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
occ.gia_term = (gia_term != nullptr) ? gia_term : "";
```

The `const auto *` declaration (§24 `readability-qualified-auto`) combined with the
NOLINT placement on the line containing the cast keyword (§19) is the canonical form.

> **Rule**: sqlite3 column text extraction must use the `const auto *` + NOLINT +
> explicit `!= nullptr` check pattern shown above.

---

---

## 62. MPFI/MPFR C Array Types — `cppcoreguidelines-pro-bounds-array-to-pointer-decay`

**Triggered by**: `cppcoreguidelines-pro-bounds-array-to-pointer-decay`.

`mpfi_t` and `mpfr_t` are C typedefs for fixed-size arrays (`__mpfi_struct[1]` /
`__mpfr_struct[1]`). Every use that passes them to a C API function triggers
array-to-pointer decay. This is inherent to the MPFR/MPFI C API design and
cannot be eliminated without wrapping the entire API.

```cpp
// mpfi_t value is __mpfi_struct[1] — every C API call decays it
mpfi_init2(value, prec);   // decay: value → &value[0]
mpfi_swap(value, other);   // decay: both operands
mpfi_clear(value);         // decay
return value;              // decay in get() accessor
```

**Pattern**: NOLINT on every line where the member array is passed to a C function:

```cpp
mpfi_init2(value, prec); // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay) — MPFI C API boundary
```

This also applies to `mpfr_t` members in `MPFRGuard`, `MPFRLocal`, `precision_config`,
and `sparkle::optics::real`.

> **Rule**: all `mpfi_t` / `mpfr_t` member array accesses passed to C API functions
> must have `// NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay) — MPFI C API boundary`
> (or `MPFR C API boundary`). No structural workaround exists for C array typedefs.

---

## 63. MPFI/MPFR Guard Classes — `cppcoreguidelines-pro-type-member-init`

**Triggered by**: `cppcoreguidelines-pro-type-member-init`.

Guard/RAII classes for `mpfi_t` / `mpfr_t` cannot initialize their array member
in the member-initializer list because C array types do not support copy/move
initialization. The initialization is performed by `mpfi_init2()` / `mpfr_init2()`
in the constructor body.

```cpp
class MPFIGuard {
  mpfi_t value; // NOLINT(cppcoreguidelines-pro-type-member-init)
                // — mpfi_t is a C array type; initialization is performed
                //   by mpfi_init2() in the constructor body.

public:
  explicit MPFIGuard(mpfr_prec_t prec) noexcept { // NOLINT(cppcoreguidelines-pro-type-member-init)
    mpfi_init2(value, prec); // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay) — MPFI C API boundary
  }
};
```

The NOLINT must appear on both:
1. The member declaration (for the "field not initialized" diagnostic on the class)
2. Each constructor declaration (for the "constructor does not initialize" diagnostic)

> **Rule**: MPFI/MPFR guard class constructors and their array member declarations
> must carry `// NOLINT(cppcoreguidelines-pro-type-member-init)` with the C array
> type explanation.

---

## 64. Aggregate Structs — `misc-non-private-member-variables-in-classes`

**Triggered by**: `misc-non-private-member-variables-in-classes`.

This check fires when a type has **both** public data members **and** member
functions (including static factories, non-static methods, and member operators
other than friend-declared ones).

**Fix hierarchy** (in order of preference):

1. **Remove all member functions** — extract static factories and instance
   methods to namespace-scope free functions. The struct becomes a pure
   aggregate and the check does not fire.

2. **Convert member `operator<=>` / `operator==` to friend declarations** —
   `friend constexpr auto operator<=>(const T&, const T&) = default;`
   Friend declarations are not member functions; the struct remains a pure
   aggregate.

3. **Make data private with accessors** — if the struct has genuine invariants
   maintained by its member functions, it is a class and should be treated
   as one.

4. **NOLINT** — only for MPFR/MPFI workspace structs where the C array
   member prevents alternatives (see §62, §63, §68).

**Decision table**:

| Struct type | Has member functions? | Fix |
|---|---|---|
| Pure data aggregate | No | No action needed |
| Aggregate with static factory | Yes (static) | Extract factory to free function |
| Aggregate with `operator<=>` | Yes (member) | Convert to `friend` declaration |
| Aggregate with instance methods | Yes | Extract methods to free functions |
| MPFR/MPFI workspace | Yes (constructor) | NOLINT (§62/§63) |
| Class with invariants | Yes | Private data + accessors |


## 65. MPFI Parameter Type Selection — `bugprone-easily-swappable-parameters` at C API Boundary

**Triggered by**: `bugprone-easily-swappable-parameters` on adjacent `mpfi_t` parameters.

When a function takes multiple `mpfi_t` parameters, all decay to `__mpfi_struct*`
and are flagged as swappable. The C typedef cannot be wrapped in tagged types.

**Mitigation hierarchy**:

1. **Use `mpfi_srcptr` for read-only inputs**: `mpfi_srcptr` is `const __mpfi_struct*`,
   structurally distinct from `mpfi_t` → `__mpfi_struct*`. This separates output
   parameters from input parameters.

2. **NOLINT for remaining same-type adjacencies**: When multiple inputs are all
   `mpfi_srcptr` (or all `mpfi_t`), NOLINT with reason.

```cpp
// WRONG — all mpfi_t, all swappable
void compute_occurrence_interval(mpfi_t result, mpfi_t base_occ,
                                 mpfi_t intensity, mpfi_t chroma,
                                 mpfi_t overlay);

// CORRECT — output is mpfi_t, inputs are mpfi_srcptr
void compute_occurrence_interval(
    mpfi_t result,
    mpfi_srcptr base_occ, // NOLINT(bugprone-easily-swappable-parameters) — MPFI C API boundary
    mpfi_srcptr intensity,
    mpfi_srcptr chroma,
    mpfi_srcptr overlay);
```

> **Rule**: always use `mpfi_srcptr` / `mpfr_srcptr` for function parameters that
> are not modified. NOLINT remaining same-type adjacencies with "MPFI C API boundary".

---

## 66. Swappable `double` Parameter Groups — `IntervalWeights` Pattern

**Triggered by**: `bugprone-easily-swappable-parameters` on adjacent `double` parameters.

Functions accepting many `double` weights (e.g., per-attribute scoring weights +
normalization factor) should bundle them into a named aggregate struct per §14.1:

```cpp
struct IntervalWeights {
  double color{};
  double clarity{};
  double weight{};
  double shape{};
  double fluorescence{};
  double inclusions{};
  double cut{};
  double polish{};
  double symmetry{};
  double norm_factor{};
};

void combine_weighted_intervals(mpfi_t result,
                                mpfi_srcptr color_score,
                                /* ... 8 more mpfi_srcptr ... */
                                const IntervalWeights &iw) noexcept;

// Call site — aggregate init
combine_weighted_intervals(result, c.get(), /* ... */, s.get(),
                           IntervalWeights{cw, clw, ww, sw, fw, iw, ctw, pw, smw, nf});
```

> **Rule**: when a function accepts ≥ 3 adjacent `double` parameters that represent
> logically grouped configuration (weights, thresholds, scaling factors), bundle
> into a named struct. Use `{}` default initialization for all members.

---

## 67. NOLINT Placement — Must Be on Diagnostic Source Line

**Triggered by**: any check where the NOLINT comment is placed on a continuation line
rather than the line where the diagnostic keyword appears.

clang-tidy attaches diagnostics to the source line containing the triggering token
(e.g., the `reinterpret_cast` keyword). A NOLINT comment on a subsequent continuation
line is ignored.

```cpp
// WRONG — NOLINT on closing paren line, diagnostic fires on reinterpret_cast line
return (c != nullptr)
           ? reinterpret_cast<
                 const char
                     *>( // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                 sqlite3_column_text(stmt, idx))
           : std::string{};

// CORRECT — NOLINT on same line as reinterpret_cast keyword
return (c != nullptr)
           ? reinterpret_cast<const char *>( // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast) — sqlite3 API boundary
                 sqlite3_column_text(stmt, idx))
           : std::string{};
```

**Implication**: when a multi-line expression triggers a diagnostic, either:
1. Collapse the expression to a single line with the NOLINT, or
2. Place the NOLINT on the specific line containing the triggering keyword/operator

> **Rule**: NOLINT comments must appear on the exact line number reported by
> clang-tidy. Use `--use-color` output to identify the correct line. Multi-line
> expressions may need to be reformatted to place the NOLINT correctly.

---


## 68. NOLINT Reservation Policy — C API Boundary Only

**Scope**: All non-MPFR/MPFI code.

NOLINT annotations are reserved **exclusively** for C API boundary interactions
where no C++ alternative exists. The qualifying C APIs in this project are:

- **MPFR** (`mpfr_t`, `mpfr_srcptr`, `mpfr_ptr`) — C array typedef for `__mpfr_struct[1]`
- **MPFI** (`mpfi_t`, `mpfi_srcptr`, `mpfi_ptr`) — C array typedef for `__mpfi_struct[1]`
- **SQLite3** (`reinterpret_cast` for `sqlite3_column_text`) — §20
- **Poco JSON** (`cppcoreguidelines-owning-memory` for `new Poco::JSON::Object`) — Poco API requires raw `new`

### Permitted NOLINT checks at C API boundaries

only apply nolint with // clang-format off` for the respective code, otherwise clang format will format the code and nolint has no effect.

| Check | When permitted |
|---|---|
| `cppcoreguidelines-pro-bounds-array-to-pointer-decay` | mpfr_t / mpfi_t passed to C functions |
| `cppcoreguidelines-pro-type-member-init` | mpfr_t / mpfi_t member in RAII class (init by C function) |
| `cppcoreguidelines-pro-type-reinterpret-cast` | sqlite3_column_text return |
| `cppcoreguidelines-owning-memory` | Poco JSON API (`new Poco::JSON::Object/Array`) |

### NOT permitted — use structural fixes instead

| Check | Wrong approach | Correct fix |
|---|---|---|
| `misc-non-private-member-variables-in-classes` on aggregate structs | NOLINT | Remove all member functions; extract statics to free functions, extract instance methods to free functions taking const ref. Pure aggregates do not trigger the check. |
| `misc-non-private-member-variables-in-classes` on structs with `operator<=>` | NOLINT | Use `friend constexpr auto operator<=>(const T&, const T&) = default;` — friend declarations are not member functions. |
| `bugprone-easily-swappable-parameters` on adjacent C++ types (`std::string &`, `const real &`, `double`) | NOLINT | Tagged-value-struct wrapping (§14.2). Exception: functions where parameters are physically distinct quantities with clear names and adding tagged wrappers would reduce readability (document rationale in NOLINT comment). |
| `misc-const-correctness` | NOLINT | Add `const` |
| `readability-make-member-function-const` | NOLINT | Add `const` to method |
| `clang-diagnostic-shadow` | NOLINT | Remove shadowing declaration |
| `performance-inefficient-vector-operation` | NOLINT | Add `.reserve()` before loop |
| `performance-unnecessary-value-param` | NOLINT | Take by `const &` or move |

### Mixed cases — mpfr_t in struct with other fields

When a struct wraps mpfr_t/mpfi_t AND has other non-C fields that trigger
`misc-non-private-member-variables-in-classes`:

1. The mpfr_t/mpfi_t member itself: NOLINT permitted (`cppcoreguidelines-pro-type-member-init`)
2. Other members of the same struct: either make private (if struct has behavior)
   or ensure struct is a pure aggregate (no member functions).
3. Do NOT use blanket NOLINT on the struct declaration or on non-C-type members.

Example — `MPFRGuard` correctly uses NOLINT only on the mpfr_t member and
constructor declarations, with the member renamed to `m_value` (private).

### MPFI workspace structs

Structs whose sole purpose is grouping pre-allocated `MPFIGuard` members
for reuse across interval computations (e.g., `IntervalWorkspace`) may use
`NOLINT(misc-non-private-member-variables-in-classes)` on the struct
declaration only — because the constructor is required to forward the
precision parameter to each `MPFIGuard` member, and making the members
private adds no invariant protection.

Field-level NOLINT on individual `MPFIGuard` members is NOT needed — the
struct-level annotation covers the entire type.

### `misc-no-recursion` — bounded recursion exception

When recursion is statically bounded to depth 1 (e.g., fallback lookup that
calls itself with a known base case), `NOLINTNEXTLINE(misc-no-recursion)` is
permitted with a comment explaining the bound:

```cpp
// NOLINTNEXTLINE(misc-no-recursion) — bounded single-depth fallback to round_brilliant
return for_shape(diamond_shape::round_brilliant, cfg);
```

This is the only non-C-API NOLINT that is permitted.

### Tagged-value structs (§14.2) and `misc-non-private-member-variables-in-classes`

Tagged-value structs (e.g., `Capacity`, `RefillRate`) have a single public
data member and an explicit constructor to prevent implicit conversions.
The explicit constructor triggers `misc-non-private-member-variables-in-classes`.
NOLINT is permitted on the data member with `— tagged-value-struct §14` reason,
because making `v` private would defeat the purpose of the lightweight wrapper.

## 69. `constexpr` Implies `inline` — Redundant Inline Specifier

**Triggered by**: `readability-redundant-inline-specifier`.

In C++17 and later, `constexpr` on a function implicitly makes it `inline`.
Adding `inline` explicitly is redundant and triggers the diagnostic.

```cpp
// WRONG — inline is redundant on constexpr function
[[nodiscard]] inline constexpr std::string_view cut_grade_label(CutGrade g) { ... }
inline constexpr mpfr_prec_t get_precision() noexcept { return BITSIZE; }
inline constexpr std::size_t get_parametric_color_count() { return colorCount; }

// CORRECT — constexpr implies inline
[[nodiscard]] constexpr std::string_view cut_grade_label(CutGrade g) { ... }
constexpr mpfr_prec_t get_precision() noexcept { return BITSIZE; }
constexpr std::size_t get_parametric_color_count() { return colorCount; }
```

Note: `inline` on non-constexpr functions in headers is still required and correct.
This rule applies only when `constexpr` is already present.

Also applies to `consteval` and `constinit` where relevant.

> **Rule**: never combine `inline` with `constexpr` on function declarations.
> `constexpr` already provides the `inline` linkage semantics.

---

## 70. `std::numbers` Constants — Prefer Standard Library Over Literals

**Triggered by**: `modernize-use-std-numbers`.

C++20 introduced `<numbers>` with mathematical constants. When a literal matches
a standard constant within machine epsilon, use the standard version.

```cpp
// WRONG — magic literal that matches std::numbers::log10e
constexpr double inv_ln10 = 0.43429448190325176;

// CORRECT — use standard library constant
#include <numbers>
constexpr double inv_ln10 = std::numbers::log10e;
```

Common constants and their `std::numbers` equivalents:

| Literal | Standard | Header |
|---|---|---|
| `3.14159265358979...` | `std::numbers::pi` | `<numbers>` |
| `2.71828182845904...` | `std::numbers::e` | `<numbers>` |
| `0.69314718055994...` | `std::numbers::ln2` | `<numbers>` |
| `0.43429448190325...` | `std::numbers::log10e` | `<numbers>` |
| `1.41421356237309...` | `std::numbers::sqrt2` | `<numbers>` |
| `0.31830988618379...` | `std::numbers::inv_pi` | `<numbers>` |

> **Rule**: use `std::numbers::*` constants instead of hand-typed literals.
> This eliminates transcription errors and makes the intent explicit.

---

## 71. `std::endl` vs `'\n'` — Avoid Unnecessary Flush

**Triggered by**: `performance-avoid-endl`.

`std::endl` writes a newline AND flushes the stream buffer. In most cases,
only the newline is needed. The flush is expensive and rarely necessary.

```cpp
// WRONG — flushes stream buffer unnecessarily
os << result << std::endl;

// CORRECT — newline without flush
os << result << '\n';
```

Use `std::endl` only when an explicit flush is semantically required (e.g.,
before a crash-prone operation where buffered output would be lost).

> **Rule**: use `'\n'` instead of `std::endl` unless an explicit flush is required.

---

## 72. Container Emptiness Check — `.empty()` vs `.size() > 0`

**Triggered by**: `readability-container-size-empty`.

Use `.empty()` to check whether a container has elements, not `.size() > 0`
or `.size() != 0`. The `.empty()` method is semantically clearer and may be
more efficient for some container types (e.g., `std::list`).

```cpp
// WRONG
if (groupsArray->size() > 0) { ... }
if (vec.size() != 0) { ... }

// CORRECT
if (!groupsArray->empty()) { ... }
if (!vec.empty()) { ... }
```

> **Rule**: use `.empty()` for emptiness checks. Never compare `.size()` to zero.

---

## 73. `modernize-use-auto` for Cast Expressions

**Triggered by**: `modernize-use-auto`.

When a variable is initialized with an explicit cast, `auto` avoids repeating
the type name. The cast already makes the type visible.

```cpp
// WRONG — type name repeated
const double nd = static_cast<double>(n);

// CORRECT — auto with cast makes type visible
const auto nd = static_cast<double>(n);
```

This applies to `static_cast`, `dynamic_cast`, `const_cast`, and
`reinterpret_cast` (where NOLINT-permitted). It does NOT apply to
`std::bit_cast` where the template argument may differ from the source type.

> **Rule**: when initializing with an explicit cast, prefer `auto` to avoid
> type repetition. The cast expression documents the type.

---

## 74. Consecutive Identical Switch Branches — Fall-Through Merge

**Triggered by**: `bugprone-branch-clone`.

When two or more consecutive `case` labels execute identical code, merge them
using fall-through:

```cpp
// WRONG — two identical branches
case ClarityGrade::si1:
  return position_class::table_center;
case ClarityGrade::si2:
  return position_class::table_center;

// CORRECT — fall-through
case ClarityGrade::si1:
case ClarityGrade::si2:
  return position_class::table_center;
```

If the identical branches are separated by other cases, extract the common
code into a helper function and call it from each case.

> **Rule**: consecutive switch cases with identical bodies must be merged
> via fall-through. Non-adjacent identical branches must call a shared helper.

---

## 75. Bounded Recursion — `misc-no-recursion` Suppression

**Triggered by**: `misc-no-recursion`.

Some functions use bounded single-depth recursion as a fallback pattern
(e.g., unknown shape → round brilliant defaults). When recursion depth
is provably bounded to 1, suppress with NOLINT and document the bound:

```cpp
// CORRECT — bounded fallback, NOLINT with proof
return for_shape(diamond_shape::round_brilliant, cfg);
  // NOLINT(misc-no-recursion) — bounded single-depth fallback to round_brilliant for unknown shapes
```

**Never suppress** `misc-no-recursion` for:
- Unbounded recursive algorithms (use iteration instead)
- Recursion depth > 1 without a formal termination proof
- Functions where the recursion target could itself recurse further

> **Rule**: suppress `misc-no-recursion` only when recursion depth is provably
> bounded to 1. Document the bound in the NOLINT reason.

---

## 76. MPFR/MPFI NOLINT Placement After `clang-format`

**Extends §62, §63, §67**.

After `clang-format` reformats multi-line expressions, NOLINT comments may
end up on continuation lines instead of the line containing the triggering
token (`m_value`, `value`, etc.). This makes the NOLINT ineffective.

**Verification procedure after clang-format**:
1. Run `clang-tidy` and check which array-decay warnings remain
2. For each remaining warning, verify the NOLINT is on the exact line
   number reported by clang-tidy
3. If clang-format moved the NOLINT to a continuation line, either:
   a. Collapse the expression to a single line, or
   b. Place the NOLINT on a separate comment line immediately before the call

```cpp
// Pattern (a) — single line
mpfr_init2(m_value, m_config.bits); // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay) — MPFR C API boundary

// Pattern (b) — preceding comment when clang-format splits the call
// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay) — MPFR C API boundary
mpfr_init2(
    m_value,
    m_config.bits);
```

`NOLINTNEXTLINE` is the preferred form when clang-format insists on splitting
the call across lines, because it attaches to the next source line regardless
of reformatting.

> **Rule**: after clang-format, re-run clang-tidy to verify all NOLINT
> comments are still effective. Use `NOLINTNEXTLINE` for split expressions.

---

## 77. `cppcoreguidelines-use-default-member-init` on C Array Types

**Triggered by**: `cppcoreguidelines-use-default-member-init`, `modernize-use-default-member-init`.

C array types (`mpfr_t`, `mpfi_t`, C-style `T[N]`) cannot use default member
initializers because array types do not support `= {}` initialization in
member declarations. The diagnostic is a false positive for these types.

```cpp
// WRONG — clang-tidy suggests `mpfr_t value_{}` but mpfr_t is __mpfr_struct[1]
// which cannot be default-initialized via `= {}`
mpfr_t value_;

// CORRECT — NOLINT with reason
mpfr_t value_; // NOLINT(cppcoreguidelines-use-default-member-init,modernize-use-default-member-init)
               // — mpfr_t is a C array type; initialized by mpfr_init2() in the constructor body
```

> **Rule**: suppress `use-default-member-init` on `mpfr_t` / `mpfi_t` / C array
> type members with NOLINT. These are initialized by C API functions in the
> constructor body.


## 79. Redundant `static` in Anonymous Namespace (`readability-static-definition-in-anonymous-namespace`)

**Triggered by**: `readability-static-definition-in-anonymous-namespace`.

Functions and variables defined inside an anonymous namespace already have internal
linkage. Adding `static` is redundant and clutters the signature.

```cpp
// WRONG — static is redundant inside anonymous namespace
namespace {
[[nodiscard]] static double photopic_centroid(...) { ... }
[[nodiscard]] static shape_params_t round_brilliant(...) { ... }
}

// CORRECT — anonymous namespace provides internal linkage
namespace {
[[nodiscard]] double photopic_centroid(...) { ... }
[[nodiscard]] shape_params_t round_brilliant(...) { ... }
}
```

> **Rule**: never use `static` on functions or variables inside an anonymous
> namespace. The namespace already provides internal linkage.

---

## 80. Container Membership with `.contains()` (`readability-container-contains`)

**Triggered by**: `readability-container-contains`.

C++20 added `.contains()` to associative containers (`std::set`, `std::map`,
`std::unordered_set`, `std::unordered_map`). Using `.count(key) > 0` is
the pre-C++20 idiom and should be replaced.

```cpp
// WRONG — pre-C++20 idiom
bool has_yellow = incompatible_colors.count("Yellow") > 0;
bool fl_ok = clarities.count("FL") > 0;

// CORRECT — C++20 .contains()
const bool has_yellow = incompatible_colors.contains("Yellow");
const bool fl_ok = clarities.contains("FL");
```

> **Rule**: use `.contains()` instead of `.count() > 0` for membership tests
> on all associative containers.

---

## 81. Empty String Comparison (`readability-container-size-empty`)

**Triggered by**: `readability-container-size-empty`.

Comparing a string to an empty literal `""` allocates a temporary string for
the comparison. Use `.empty()` instead.

```cpp
// WRONG — allocates temporary
ctx.check(strutil::to_upper("") == "", "to_upper empty string");

// CORRECT — no allocation
ctx.check(strutil::to_upper("").empty(), "to_upper empty string");
```

> **Rule**: use `.empty()` instead of comparing to `""` or `std::string()`.

---

## 82. Dead Store Elimination (`clang-analyzer-deadcode.DeadStores`)

**Triggered by**: `clang-analyzer-deadcode.DeadStores`.

When a variable is initialized with a value that is unconditionally overwritten
before any read, the initializer is a dead store. This often occurs when a
variable is initialized at declaration but then reassigned in every branch of
a following conditional.

```cpp
// WRONG — initial value is never read
double score_base = rarity::pareto::compute_pareto_weight_rarity(10.0);
double score_hi = rarity::pareto::compute_pareto_weight_rarity(10.0);
// ... loop that unconditionally overwrites score_base and score_hi ...

// CORRECT — defer initialization or use zero
double score_base = 0.0;
double score_hi = 0.0;
// ... loop that assigns real values ...
```

If the variable is always written before read, prefer deferring initialization to
the assignment point or use a trivial default (`0.0`, `0`, `{}`).

> **Rule**: do not call non-trivial functions in initializers when the result is
> unconditionally overwritten before first read.

---

## 83. NOLINTBEGIN/NOLINTEND for Test Utility Structs

**Triggered by**: `misc-non-private-member-variables-in-classes` on test harness structs.

Test utility structs (e.g., `TestContext`) that have member functions but serve as
aggregates should use `NOLINTBEGIN`/`NOLINTEND` blocks rather than per-member NOLINT
annotations, since the rationale applies uniformly to all members.

```cpp
// CORRECT — single block annotation for test aggregate
// NOLINTBEGIN(misc-non-private-member-variables-in-classes) — test utility aggregate
struct TestContext {
  std::string suite_name;
  int passed = 0;
  int failed = 0;
  // ...
};
// NOLINTEND(misc-non-private-member-variables-in-classes)
```

> **Rule**: use NOLINTBEGIN/END blocks (not per-line) when the suppression reason
> applies uniformly to an entire struct. Include a comment explaining why.

---

## 84. `performance-inefficient-vector-operation` — Reserve Before Loop

**Triggered by**: `performance-inefficient-vector-operation`.

When `push_back` or `emplace_back` is called inside a loop with a known iteration
count, call `.reserve()` before the loop to avoid repeated reallocations.

```cpp
// WRONG — may reallocate up to log2(20) times
for (int i = 0; i < 20; ++i) {
    many.push_back({...});
}

// CORRECT — single allocation
many.reserve(20);
for (int i = 0; i < 20; ++i) {
    many.push_back({...});
}
```

> **Rule**: call `.reserve(N)` before any loop that pushes a known or bounded
> number of elements.

---

## 85. `readability-isolate-declaration` + `cppcoreguidelines-init-variables`

**Triggered by**: `readability-isolate-declaration`, `cppcoreguidelines-init-variables`.

Multiple variable declarations on a single line reduce readability and can mask
uninitialized variables. Split and initialize each variable independently.

```cpp
// WRONG — two declarations, both uninitialized
uint64_t ua, ub;

// WRONG — two declarations, single line
double prev_p = -1.0, prev_s = -1.0;

// CORRECT — isolated and initialized
uint64_t ua = 0;
uint64_t ub = 0;

double prev_p = -1.0;
double prev_s = -1.0;
```

> **Rule**: one declaration per statement. Every non-class-type variable must be
> initialized at declaration.

---

## 86. `performance-unnecessary-copy-initialization`

**Triggered by**: `performance-unnecessary-copy-initialization`.

When a `const` variable is copy-initialized from a `const` reference or return
value, and the variable is only read (never modified), use a `const` reference
instead to avoid the copy.

```cpp
// WRONG — copies the extracted pointer wrapper
const auto arr = result.extract<Poco::JSON::Array::Ptr>();

// CORRECT — binds reference, no copy
const auto &arr = result.extract<Poco::JSON::Array::Ptr>();
```

> **Rule**: use `const auto &` when binding to a return value that is only read.
> Use `const auto` (value) only when a copy is intentionally needed.

## Followup — Remaining Open Problems

1. **`parse_request()` cognitive complexity 57** and **`main()` cognitive complexity 67**: **PARTIALLY RESOLVED**.
   - `parse_args()` (from `main()`): **RESOLVED** — dispatch table pattern (§27) reduces complexity from 33 → 18.
   - `main()`: **RESOLVED** — decomposed into `setup_listen_socket`, `install_signal_handlers`, `run_accept_loop` (§28). Complexity reduced from 28 → 13.
   - `parse_request()`: **RESOLVED** in prior pass — decomposed into `parse_filters()` / `parse_criteria()` / `parse_options()`.

2. **`bugprone-easily-swappable-parameters` on `push(int fd, size_t max_depth)` and `send_status_frame(...)`**: **RESOLVED** — `QueueDepth{size_t v}` tagged-value-struct introduced for `push`; `StatusFrame{size_t position, int active, int total}` aggregate struct introduced for `send_status_frame`. Pattern documented in §14. Remaining project-wide sweep: search all function signatures in `src/` for adjacent `int`/`size_t`/`unsigned` parameters and apply §14 decision table.

3. **`send_all` / `recv_all` pointer arithmetic**: **RESOLVED** — required `std::span<std::byte>`
   migration pattern documented in §10.1. Existing `void*`+`size_t` helpers in `src/` must be
   migrated; call sites using `json.data()`/`json.size()` convert via `std::as_bytes(std::span{...})`.

4. **All source files in `src/`**: the structural patterns identified here (missing `const`, missing Rule of 5 move ops, implicit pointer-to-bool, sign conversion on `.reserve()`) appear project-wide per the clang-tidy log preamble ("155453 warnings generated"). A mechanical pass with `clang-tidy --fix` on the `misc-const-correctness` and `readability-implicit-bool-conversion` checks across all files is the fastest path to baseline compliance.

5. **`argv` + `from_chars` pointer arithmetic in `main()`**: The `--uid`, `--gid`, `--workers`
   argument parsers should be migrated to the `std::string_view sv(argv[i])` + NOLINT
   pattern defined in §21. This eliminates 12+ `pro-bounds-pointer-arithmetic` suppressions
   currently needed per `from_chars` call site.

6. **`performance-for-range-copy`, `performance-unnecessary-value-param`, `readability-qualified-auto`,
   `clang-analyzer-deadcode.DeadStores`, `bugprone-integer-division`, `bugprone-signed-char-misuse`,
   `bugprone-narrowing-conversions`**: patterns now documented in §22–§26. Project-wide sweep
   required: run `clang-tidy --fix` with these specific checks isolated to auto-fix safe categories
   (`performance-for-range-copy`, `readability-qualified-auto`) before manual review of §23/§25/§26
   changes.


## Remaining Open Items

### R1. `compute_occurrence_interval` — Aliasing Concern

The original code used `mpfi_div(result, base_occ, result)` where `result` was both an input operand (product of intensity×chroma×overlay) and the output. The fix introduces a `divisor` temporary. **Verify** that MPFI handles the previous aliased form correctly — if it does, the temporary is a safety improvement but the original was not a bug. If MPFI does NOT handle self-aliasing in `mpfi_div`, the original code had latent UB.

### R2. `calculate_sparkle_scores` Full Decomposition

The catch-block extraction reduces complexity from 32 to approximately 22. If the threshold is 25, this passes. However, the try-block itself still contains progress-checking conditionals that contribute ~10 complexity. If a future pass adds more logic to the try block, it will exceed the threshold again. Consider extracting the full per-lot sparkle computation into `detail::compute_single_lot_sparkle()`.

### R3. `collect_merchant_certs` NOLINT Positioning

The NOLINT for `bugprone-easily-swappable-parameters` is placed on the function declaration line. clang-tidy may fire on the parameter declaration lines instead. After applying the diff, verify the NOLINT suppresses the warning. If not, move the NOLINT to the specific parameter line:
```cpp
    const std::vector<std::string> &merchant_tables, // NOLINT(bugprone-easily-swappable-parameters)
    const std::vector<std::string> &cert_id_prefixes,
```

### R4. `detail::` Namespace Scope Verification

The diffs introduce `detail::` namespaces in three files. Ensure each is opened within the correct enclosing namespace:
- `rarity_inclusion_data.hpp`: `namespace rarity { namespace detail { ... } }`
- `rarity_inclusion_ops.hpp`: `namespace rarity::write { namespace detail { ... } }`
- `query_v2_core.hpp`: `namespace query { namespace detail { ... } }` — note that `query_v2_core.hpp` may have multiple namespace scopes (e.g., `query::detail` for scoring helpers vs. a file-static scope for DB helpers). Place `detail::estimate_sparkle_from_fallback` in `namespace query::detail` since it's called from `calculate_sparkle_scores`. Place `detail::process_cert_row` near `collect_cert_table_entries` — since `collect_cert_table_entries` is `static inline`, `process_cert_row` should also be `static inline` or in an anonymous namespace rather than `detail::`.

### R5. Test Compilation Verification

Files requiring recompilation after these changes:
- `test_interval_bounds.cpp` — `combine_weighted_intervals` signature change
- `test_inclusion_refactor.cpp` — calls `get_seed_inclusion_occurrences()` (signature unchanged but internal restructuring may surface latent issues)
- `test_inclusion_incompatibility.cpp` — same
- `test_integration.cpp` — same
- `rarity_engine_calc.hpp` — `combine_weighted_intervals` call site
- `query_v2_main.cpp` — `calculate_sparkle_scores` (signature unchanged but `detail::` must be visible)

### R6. Project-Wide `misc-const-correctness` Sweep

The C1 log shows this warning is pervasive across the codebase. After applying these targeted fixes, run:
```bash
clang-tidy --checks=misc-const-correctness --fix -p build src/**/*.cpp
```
to auto-fix the safe cases project-wide.

### R7. `mpfr_precision_config.hpp` — Same Array-Decay Suppression Needed

`MPFRLocal` and `MPFILocal` in `mpfr_precision_config.hpp` have the same `mpfr_t`/`mpfi_t` member array-to-pointer-decay issue as `MPFIGuard`. These are not in the C1 warning log (likely not included transitively in the scanned `.cpp` files), but will fire when those headers are included from other translation units. Apply the same NOLINT pattern from §62/§63 preventively.

## Remaining Items After This Diff

### R1. Call-site migration required

The following files must be updated to match the API changes before compilation:

| Change | Affected files | Migration |
|--------|---------------|-----------|
| `resolve_girdle_factor` now takes `girdle_strings` | Any external caller beyond `from_db_lot` | Wrap 3 args in `{.thin=, .thick=, .from_to=}` |
| `sparkle_components` methods → free functions | `query_v2_types.hpp`, `output_json_query_v2.hpp`, all test files calling `.scaled_to_1000()` / `.total_score()` / `.get_fields_used_profile()` | `obj.method()` → `method(obj)` |
| `kPhotopicV[i]` → `.at(i)` | `sparkle_fire_spectral.hpp` | Single-line change (see CALLSITE_DIFFS.md §1) |

**Recommended order**: Apply `sparkle_fire_spectral.hpp` first (zero-risk), then the header replacements, then migrate callsites. Run `clangtidy.sh` after each step.

### R2. NOLINT audit pass

The replacement files introduce NOLINT suppressions at MPFR C API boundaries. These are structurally unavoidable (§16.2) but should be audited:

- `sparkle_optics_core.hpp`: ~25 NOLINT lines on MPFR calls. All follow the `// NOLINT(check-name) — reason` pattern per §19.
- Verify no NOLINT was added without a reason comment.

### R3. `herzberger_coefficients` struct NOLINT

The struct has a static factory method `diamond_type_iia()` which makes it technically a "class" to clang-tidy. The NOLINT is correct per §38 (pure aggregate with factory). If the factory is moved to a free function in a future pass, the NOLINT can be removed.

### R4. `wavelength` struct NOLINT

The `wavelength` struct has `to_meters()`, `to_micrometers()`, and static factory methods (`blue()`, `green()`, `red()`, `sodium_d_line()`). Extracting these to free functions would require touching every callsite across the sparkle module (~50+ locations). The NOLINT is the pragmatic choice until a larger refactor is planned. The NOLINT comment documents this rationale.

### R5. `color_transmission` and `n3_center_model` struct NLOINTs

Both structs have static factory methods and accessor methods operating on their public data. Per §38 the preferred fix is extraction, but these factories return the struct by value and are deeply integrated. The NOLINT with documented rationale is appropriate.

---

## Additional Issues Identified During Solution

### A1. `compute_physics_sparkle` cognitive complexity

After extracting `detail::apply_grade_factors`, the function's cognitive complexity drops from 46 to approximately 22-24 (under threshold 25). If future additions push it back over, further decomposition targets:
- Color transmission + usage tracking → `detail::resolve_color_transmission`
- Inclusion loss + usage tracking → `detail::resolve_inclusion_loss`
- Fancy color fire adjustment → `detail::apply_fancy_color_fire`

### A2. `from_db_lot` cognitive complexity

After extracting `detail::resolve_fancy_color_name`, `detail::resolve_diameter`, `detail::resolve_depth_percent`, and `detail::resolve_pavilion_depth_pct`, the function drops from 46 to approximately 18-20 (under threshold 25). The angle fallback logic (crown/pavilion > 50° checks) remains inline as it's shallow.

### A3. `sparkle_components` default initialization

The replacement adds `usage{}` zero-initialization on the `usage_flags` member. The `real` members (`brilliance`, `fire`, etc.) are default-constructed by `real`'s explicit default constructor (sets to zero). This matches existing behavior but is now explicit.

### A4. `diamond_specification` default member initializers

Added `{}` default initializers for `shape`, `color`, `cut`, `polish`, `symmetry`, `fluorescence`. All enum classes default to value `0` which maps to the first enumerator. Verify this is semantically correct for each:
- `shape{}` → first shape enum value (round_brilliant in most shape enums)
- `color{}` → `color_grade::D` — this is acceptable as a default; callers always set it explicitly
- `cut{}` → depends on `CutGrade` ordering; if `ags_ideal` is 0, that's the default
- Same pattern for polish, symmetry, fluorescence

All existing code sets these fields explicitly before use (in `from_db_lot` and test code), so the default values are never actually relied upon. The initializers exist solely to satisfy `cppcoreguidelines-pro-type-member-init`.

### A5. `sparkle_fire_spectral.hpp` `illum_spd[i]`

The `illum_spd` return type is `std::span<const double, kSpectralSamples>`. `std::span::operator[]` does not perform bounds checking. The `.at()` call is not available on `std::span` (it was added in C++26). Options:
- Leave as `illum_spd[i]` — the loop bound `i < cie::kSpectralSamples` guarantees in-bounds, and `std::span` with static extent provides compile-time size safety.
- Add a bounds-check wrapper if the clang-tidy check fires on `std::span` subscript (unlikely — the check targets `std::array` and C arrays, not `std::span`).

Current recommendation: Apply `.at(i)` only to `kPhotopicV` (which is `std::array`). Leave `illum_spd[i]` as-is since it's `std::span` with static extent and the index is loop-bounded.

## Remaining After These Corrections

### R8. `clang-format` Re-run Risk

After applying the `rarity_engine_calc.hpp` CalculationProfile diff, the members
will be on single lines with trailing NOLINT comments. If `clang-format` is run
with a column limit that causes these lines to wrap, the NOLINT will be displaced
again (§78). Verify after `clang-format` that the lines remain intact.

If the project `.clang-format` column limit is ≤100, the longest line:
```
  bool has_fluorescence = false; // NOLINT(misc-non-private-member-variables-in-classes)
```
is 85 characters — safe.

For `sparkle_optics_core.hpp`, the longest single-line MPFR call:
```
    mpfr_add(m_value, m_value, rhs.m_value, static_cast<mpfr_rnd_t>(m_config.rnd)); // NOLINT(cppcoreguidelines-pro-bounds-array-to-pointer-decay) — MPFR C API boundary
```
is ~160 characters. This **will** be split by `clang-format` if column limit < 160.
After reformatting, each `m_value` argument on its own line will re-trigger the warning.

**Mitigation options**:
1. Set `ColumnLimit: 0` in `.clang-format` for these files (not recommended globally)
2. Use `// clang-format off` / `// clang-format on` around the operator definitions
3. Use per-argument `NOLINT` if clang-format must split the line

Recommended: option 2 around the operator blocks only. This is the least invasive.

### R9. Test Compilation Verification

After applying all corrections, verify:
- `test_interval_bounds.cpp` — uses `MPFIGuard`, `combine_weighted_intervals`
- `test_fancy_color_rarity.cpp` — uses `sparkle_optics_core.hpp` types
- `test_color_normalization.cpp` — uses `MPFRGuard`
- `test_lot_coupling_and_string_utils.cpp` — uses raw `mpfr_t` directly (unaffected)
- All `query_v2_*.cpp` files — uses `col_text`, `collect_merchant_certs`

### R10. Project-Wide `misc-non-private-member-variables-in-classes` Audit

The following structs in the clang-tidy log also have public member warnings
that may need NOLINT on individual member lines (not just struct-level):

- `sparkle_color_absorption.hpp` — `n3_center_model` members
- `sparkle_optics_core.hpp` — `herzberger_coefficients`, `wavelength` (already have NOLINT)
- `rarity_interval_math.hpp` — `IntervalWorkspace` members (already have NOLINT)

Run `grep -rn 'misc-non-private-member-variables-in-classes' <clang-tidy-output>`
after applying all fixes to identify any remaining instances.
---

## Files Delivered

| File | Purpose |
|------|---------|
| `sparkle_optics_core.hpp` | Drop-in replacement |
| `sparkle_color_absorption.hpp` | Drop-in replacement |
| `sparkle_integration.hpp` | Drop-in replacement |
| `CALLSITE_DIFFS.md` | Diffs for other affected files |
| `code_correctness_guidelines_additions.md` | New §40-§44 for the guidelines |
| `FOLLOWUP.md` | This file |



update proposed for this file, pending:
"purpose: enforce correctness, safety, and concurrency integrity in systems code

rules:

  globals:
    forbid:
      - non_const_global_variables
    allow_if:
      - required_for_signal_boundary
    required_pattern:
      - use_singleton_with_function_local_static
      - enforce_private_construction
      - expose_state_via_reference_only
    validation:
      - zero_remaining_global_mutable_symbols

  concurrency:
    atomics:
      require_when:
        - variable_accessed_from_multiple_threads
      forbid:
        - plain_integral_shared_state
      memory_order:
        default: seq_cst
        allow:
          - acquire_release_if_proven
    data_race:
      must_prevent:
        - unsynchronized_read_write
        - unsynchronized_write_write
      enforce:
        - std::atomic
        - std::mutex
        - lock_free_structures_only_if_proven_correct
    thread_loops:
      forbid_pattern:
        - loop_condition_on_shutdown_flag_if_queue_exists
      required_pattern:
        - infinite_loop_with_explicit_termination_token
    counters:
      require:
        - atomic_or_mutex_protection

  signal_safety:
    forbid:
      - non_async_signal_safe_functions
    allowed_syscalls:
      - close
      - write
      - _exit
    forbid_examples:
      - shutdown
      - malloc
      - printf
    state_access:
      require:
        - atomic_only
    design:
      - signal_handler_must_be_minimal
      - defer_work_to_main_thread

  syscalls:
    require:
      - return_value_checked
    forbid:
      - ignoring_errno
    required_pattern:
      - explicit_error_branch
    coverage:
      - socket
      - accept
      - setsockopt
      - read
      - write
      - close

  memory:
    initialization:
      require:
        - zero_or_defined_initial_state
    clearing:
      forbid:
        - non_portable_functions
      require:
        - std_algorithms
    ownership:
      require:
        - RAII
        - smart_pointers
    forbid:
      - raw_new_delete

  api_design:
    functions:
      require:
        - single_responsibility
      complexity_limit:
        cognitive: 25
    arguments:
      require:
        - validated_input
    return_values:
      forbid:
        - ignored_critical_results

  error_handling:
    require:
      - deterministic_failure_paths
    forbid:
      - silent_failure
    pattern:
      - check_return_then_handle
    external_libs:
      require:
        - check_all_status_codes

  portability:
    forbid:
      - platform_specific_calls_without_guard
    require:
      - standard_library_preference
    examples:
      replace:
        explicit_bzero: std::fill

  build_configuration:
    optimization:
      forbid:
        - sanitizers_with_O0_when_ineffective
    required:
      - O2_or_higher_for_runtime_checks
    sanitizers:
      recommended:
        - address
        - undefined
        - thread

  testing:
    require:
      - full_pass_before_commit
      - no_regression
    insufficient:
      - tests_do_not_prove_absence_of_data_races

  shutdown_semantics:
    require:
      - no_task_loss
      - explicit_termination_signal
    forbid:
      - loop_exit_based_only_on_flag

  static_analysis:
    require:
      - clang_tidy_clean_or_justified
    forbid:
      - blanket_suppression
    enforcement:
      - fix_root_cause_not_warning

  code_review_triggers:
    must_review_if:
      - concurrency_modified
      - signal_handler_present
      - global_state_present
      - syscalls_added
      - shared_state_added

  verification_commands:
    required:
      - grep_no_globals: "grep -n 'g_'"
      - run_tests: "./test_binary"
      - tsan: "TSAN_OPTIONS=halt_on_error=1 ./binary"
      - clang_tidy: "clang-tidy -checks='-* ,cppcoreguidelines-*'"

  anti_patterns:
    forbid:
      - hidden_shared_state
      - unchecked_syscalls
      - signal_unsafe_operations
      - non_atomic_cross_thread_state
      - partial_refactors

  acceptance_criteria:
    must_hold:
      - no_data_races_detected
      - no_async_signal_violations
      - no_unchecked_syscalls
      - no_mutable_globals
      - deterministic_shutdown
      - all_tests_pass"

  concurrency:
    - id: atomic_shared_state_access
      rule: "All shared mutable state must be accessed through a single well-defined owner or accessor."
      enforce:
        - "No duplicated global/static state aliases"
        - "Singleton access must be consistent across all call sites"
    - id: signal_handler_safety
      rule: "Signal handlers must only call async-signal-safe functions."
      forbid:
        - "malloc/new"
        - "std::string usage"
        - "non-trivial function calls"
      allow:
        - "close"
        - "_exit"
        - "atomic store/load (lock-free only)"
    - id: atomic_memory_order
      rule: "Explicit memory ordering must be consistent across all accesses to the same atomic."
      enforce:
        - "store uses release => load must use acquire or stronger"
        - "no mixed relaxed/acquire on same semantic path unless proven safe"

  networking:
    - id: syscall_error_handling
      rule: "All syscalls must have return value checked and handled."
      enforce:
        - "setsockopt must check != 0"
        - "send/recv must handle partial transfers"
        - "retry logic required for EINTR where applicable"
    - id: fd_lifecycle
      rule: "File descriptors must have single ownership and deterministic close."
      enforce:
        - "RAII wrapper required"
        - "no double close"
        - "no use-after-close"

  memory:
    - id: buffer_handling
      rule: "Raw buffers must be wrapped in std::span or equivalent view."
      enforce:
        - "no pointer arithmetic on raw buffers"
        - "bounds derived from container size"
    - id: zeroing_sensitive_data
      rule: "Sensitive buffers must be cleared using guaranteed non-optimized method."
      allow:
        - "std::fill with volatile guarantee or explicit_bzero equivalent"
      forbid:
        - "memset without optimization barrier"

  api_design:
    - id: singleton_pattern
      rule: "Singleton must be thread-safe and minimal."
      enforce:
        - "function-local static initialization only"
        - "no external global mirrors"
    - id: function_exit_consistency
      rule: "All early returns must preserve invariants."
      enforce:
        - "resource guards must already be constructed"
        - "no partially initialized state leaks"

  control_flow:
    - id: loop_termination
      rule: "Loop termination conditions must be explicit and externally controllable."
      enforce:
        - "worker loops must have shutdown signal OR sentinel value"
        - "no infinite loops without break condition"
    - id: indentation_consistency
      rule: "Scope and indentation must match logical blocks."
      enforce:
        - "no misleading indentation"
        - "single-line conditionals must still be braced"

  type_safety:
    - id: signed_unsigned
      rule: "Avoid implicit conversions between signed and unsigned."
      enforce:
        - "explicit cast required"
    - id: syscall_types
      rule: "Use correct POSIX types for syscalls."
      enforce:
        - "ssize_t for recv/send"
        - "size_t for buffer sizes"

  error_propagation:
    - id: fail_fast
      rule: "On critical failure, return immediately and avoid undefined continuation."
      enforce:
        - "no silent fallback on system call failure"
    - id: logging_boundary
      rule: "Errors must be logged at boundary layers only."
      forbid:
        - "deep internal logging in hot paths"

  refactoring:
    - id: mechanical_changes
      rule: "Refactors must preserve semantics exactly."
      enforce:
        - "no behavioral drift"
        - "no removal of safety checks"
    - id: formatting_only_changes
      rule: "Formatting changes must not alter control flow."
      enforce:
        - "no accidental scope changes"
        - "no misplaced returns"

  validation:
    - id: diff_review
      rule: "Every diff must be validated for semantic equivalence."
      checklist:
        - "signal handling unchanged behavior"
        - "loop termination unchanged"
        - "error paths preserved"
        - "resource cleanup preserved"
    - id: compile_time_checks
      rule: "Enable strict compilation flags."
      flags:
        - "-Wall"
        - "-Wextra"
        - "-Werror"
        - "-Wconversion"
        - "-Wshadow"

  forbidden_patterns:
    - "global mutable state without encapsulation"
    - "unchecked syscall return values"
    - "mixed atomic memory orders without justification"
    - "implicit buffer bounds assumptions"
    - "control flow dependent on formatting"

id: clang-tidy-performance-unnecessary-copy
description: "Avoid unnecessary copy-initialization when extracting or returning large objects from functions if the object is only read."
problem:
  type: performance
  clang-tidy-warning: "performance-unnecessary-copy-initialization"
  example-before: "auto root = parsed.extract<Poco::JSON::Object::Ptr>();"
  example-after: "const auto &root = parsed.extract<Poco::JSON::Object::Ptr>();"
recommendation:
  - Use `const auto &` or `const T &` when binding to function return values that will not be modified.
  - Only copy if the object needs to be modified or stored beyond the lifetime of the original.
  - Apply this rule consistently to `std::vector`, `std::string`, smart pointers, and other large or reference-counted objects.

  "issue": "Argument parsing truncation and incomplete flag handling",
  "cause": "Truncated parse_args() and missing checks for help/unknown arguments",
  "solution": [
    "Ensure all argument branches are complete and include default/unknown case",
    "Validate numeric inputs using std::from_chars and check for errors",
    "Clamp numeric ranges where appropriate (e.g., worker count)",
    "Add automated tests for all flags and edge-case argument values",
    "Include -h/--help handling early to avoid unintentional exit code errors"

{
  "objective": "Eliminate redundant boolean return patterns while avoiding new clang-tidy warnings and preserving correctness, performance, and readability.",
  "detection_rules": [
    {
      "pattern": "if (expr) return true; return false;",
      "action": "flag"
    },
    {
      "pattern": "if (expr) return false; return true;",
      "action": "flag"
    }
  ],
  "transformation_rules": [
    {
      "name": "direct_boolean_return",
      "from": [
        "if (expr) return true; return false;",
        "if (expr) return false; return true;"
      ],
      "to": [
        "return expr;",
        "return !expr;"
      ]
    },
    {
      "name": "nested_scoped_conditions",
      "rule": "Only simplify the final decision branch; preserve surrounding control flow."
    },
    {
      "name": "preserve_short_circuit_semantics",
      "rule": "Do not restructure expressions in a way that introduces additional evaluations or removes guards."
    },
    {
      "name": "avoid_readability_regressions",
      "rule": "Do not simplify if it creates overly dense expressions, multiple negations, or mixed logical operators without parentheses."
    },
    {
      "name": "prefer_algorithmic_consistency",
      "rule": "For STL algorithms, prefer direct boolean comparisons such as std::find(begin, end, value) != end."
    },
    {
      "name": "avoid_double_negation",
      "rule": "Do not use negated equality forms when a direct inequality form exists."
    },
    {
      "name": "maintain_const_correctness_and_value_lifetime",
      "rule": "Do not introduce unnecessary temporaries or recomputation of expensive expressions."
    }
  ],
  "clang_tidy_guardrails": [
    "readability-simplify-boolean-expr",
    "readability-implicit-bool-conversion",
    "performance-unnecessary-copy-initialization",
    "bugprone-branch-clone"
  ],
  "safe_pattern_template": {
    "code": [
      "if (!guard.empty()) {",
      "  return predicate(...);",
      "}",
      "return fallback;"
    ],
    "constraints": [
      "predicate(...) must already be a boolean expression",
      "no redundant literals",
      "no additional evaluation",
      "no loss of short-circuit guards"
    ]
  },
  "anti_patterns": [
    "return condition ? true : false;",
    "return !!condition;",
    "return !(condition == true);",
    "return (condition == false) ? false : true;",
    "return !(std::find(...) == end);"
  ],
  "application_checklist": [
    "expression_is_already_bool",
    "no_additional_evaluation_introduced",
    "no_loss_of_short_circuit_guards",
    "no_readability_degradation",
    "no_double_negation",
    "no_branching_logic_collapse_beyond_final_return",
    "stl_usage_remains_idiomatic"
  ],
  "final_rule": "Only simplify when the resulting expression is strictly equivalent, simpler, and does not introduce new warnings or reduce clarity."
}

{
  "objective": "Eliminate redundant type duplication in cast-initialized variables while preserving type correctness and avoiding new clang-tidy warnings.",
  "detection_rules": [
    {
      "pattern": "T var = static_cast<T>(expr);",
      "action": "flag_if_T_repeated"
    }
  ],
  "transformation_rules": [
    {
      "name": "use_auto_with_cast",
      "from": "T var = static_cast<T>(expr);",
      "to": "auto var = static_cast<T>(expr);"
    }
  ],
  "constraints": [
    "The cast target type must remain explicit inside static_cast",
    "auto must not change the deduced type semantics",
    "Do not remove the cast when it is required for correctness (e.g. signed/unsigned safety)",
    "Do not apply when implicit narrowing or widening would occur without the cast"
  ],
  "do_not_apply": [
    {
      "case": "initializer_list_or_braced_init",
      "example": "auto x{static_cast<T>(expr)}"
    },
    {
      "case": "loss_of_type_clarity",
      "example": "auto used where exact type is semantically important for readability or API"
    },
    {
      "case": "reference_or_pointer_semantics_change",
      "example": "auto vs auto& vs auto* ambiguity"
    }
  ],
  "clang_tidy_guardrails": [
    "modernize-use-auto",
    "readability-implicit-bool-conversion",
    "cppcoreguidelines-narrowing-conversions",
    "hicpp-signed-bitwise"
  ],
  "application_checklist": [
    "type_is_duplicated_in_static_cast_and_variable",
    "auto_deduction_matches_expected_type",
    "explicit_cast_is_required_and_preserved",
    "no_change_in_signedness_behavior",
    "no_reference_or_pointer_decay",
    "no_loss_of_readability_in_context"
  ],
  "anti_patterns": [
    "unsigned char x = static_cast<unsigned char>(expr);",
    "const T var = static_cast<T>(expr);"
  ],
  "final_rule": "Use auto only when it removes redundant type repetition without altering type semantics, signedness behavior, or clarity."
}

## Summary Checklist (apply to every new function / class)

| # | Check | Requirement |
|---|---|---|
| 1 | Every non-reassigned local variable | `const` |
| 2 | Every pointer / C-handle used as bool | explicit `!= nullptr` or `== nullptr` |
| 3 | Any class defining destructor or copy ops | declare all 5 specials explicitly |
| 4 | Multiplication assigned to wider type | left operand carries target type literal suffix or `static_cast` |
| 5 | Any `int` passed to `size_t` parameter | `static_cast<size_t>(...)` |
| 6 | Every C struct declared on stack | `{}` initialise |
| 7 | Getter / factory / acquire returning value | `[[nodiscard]]` |
| 8 | `getValue<T>(...)` initialiser | `const auto` |
| 9 | Functions > 25 cognitive complexity | refactor before merge |
| 10 | ≥ 2 adjacent convertible-type parameters | named struct or strong typedef |
| 11 | `signal()` return value | `(void)signal(...)` or check |
| 12 | `sigaction()` return value | `(void)sigaction(...)` or check |
| 12 | `reinterpret_cast` | POSIX sockaddr boundary only + `// NOLINT` |
| 13 | Non-const globals | signal-handler atomics only + `// NOLINT` + justification |
| 14 | `argv[i]` / `p + n` arithmetic | `// NOLINT` at C boundary or wrap in `std::span` helper |
| 15 | C array member passed to function | `// NOLINT` at POSIX boundary only |
| 16 | `object-size` sanitizer in Debug builds | guard behind `if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")` in CMakeLists |
| 17 | `argv[i]` passed to `from_chars` | wrap in `const std::string_view sv(argv[i])` with single NOLINT; use `sv.data()/sv.size()` |
| 18 | Range-based for loop element type | `const auto&` unless mutation or move is required |
| 19 | Function parameter used read-only | `const T&` not `T`; apply §22 / §23 |
| 20 | `auto` initialised from pointer expression | qualify as `auto*` or `const auto*` (§24) |
| 21 | Computed value assigned then never read | eliminate dead store; use `[[maybe_unused]]` only if intentional (§25) |
| 22 | Integer division result assigned to float/double | cast numerator before division (§26) |
| 23 | `char` used as array index | cast to `unsigned char` before indexing (§26) |
| 24 | send/recv helpers | `std::span<std::byte>` parameter; arithmetic suppressed once inside helper (§10.1) |
| 25 | `enum class` base type | smallest unsigned type that covers value range (§29) |
| 26 | `inline constexpr` on function | remove `inline`; `constexpr` implies it (§30) |
| 27 | Integer/float literal suffixes | uppercase: `U`, `L`, `ULL`, `F` (§31) |
| 28 | Consecutive identical branches | merge via fall-through or condition simplification (§32) |
| 29 | Aggregate struct scalar members | default member initializer `{}` (§33) |
| 30 | Fixed-size C-style arrays | `std::array<T, N>` (§34) |
| 31 | Container `.push_back({...})` | prefer `.emplace_back(...)` (§35) |
| 32 | `<cctype>` return used as bool | explicit `!= 0` / `== 0` comparison (§36) |
| 33 | `std::array` subscript with runtime index | `.at()` not `[]` (§37) |
| 34 | Aggregate struct with utility methods + public data | extract methods to free functions (§38) |
| 35 | Value-returning pure function without `[[nodiscard]]` | add `[[nodiscard]]` (§39) |
| 36 | Bitmask expression used as bool | explicit `!= 0U` / `== 0U` comparison (§3.1) |
| `rarity_engine_calc.hpp` | `misc-non-private-member-variables-in-classes` ×18 | Extracted `has_any()`, `to_string()` to free functions; NOLINT on struct with `<=>` | §38, §59 |
| `rarity_engine_calc.hpp` | `modernize-use-nodiscard` ×2 | Added `[[nodiscard]]` to extracted free functions | §39 |
| `rarity_engine_calc.hpp` | `modernize-use-emplace` ×18 | `push_back("...")` → `emplace_back("...")` | §35 |
| `rarity_engine_calc.hpp` | `misc-const-correctness` ×8 | Added `const` to all non-reassigned variables | §2 |
| `rarity_engine_calc.hpp` | `bugprone-easily-swappable-parameters` ×1 | `PrecisionTag` wrapper for `mpfr_prec_t` | §14.2, §57 |
| `rarity_engine_calc.hpp` | `readability-implicit-bool-conversion` ×1 | `lot_ptr` → `(lot_ptr != nullptr)` | §3 |
| `rarity_inclusion_data.hpp` | `cppcoreguidelines-pro-type-member-init` ×1 | `double joint_probability_pct{}` | §33, §60 |
| `rarity_inclusion_data.hpp` | `readability-function-cognitive-complexity` ×1 | Decomposed into 3 `detail::` helpers | §13, §58 |
| `rarity_inclusion_data.hpp` | `modernize-use-emplace` ×1 | `push_back("FL")` → `emplace_back("FL")` | §35 |
| `rarity_inclusion_data.hpp` | `misc-const-correctness` ×1 | `const double adjustment` | §2 |
| `rarity_inclusion_ops.hpp` | `misc-const-correctness` ×14 | All `sql`/`sql2`/`sql3`/`rc`/`normalized`/`pos`/`sep`/`end`/`token` → `const` | §2 |
| `rarity_inclusion_ops.hpp` | `readability-implicit-bool-conversion` ×6 | All `err`/`gia_term`/`mineral_name` → `!= nullptr` | §3, §61 |
| `rarity_inclusion_ops.hpp` | `cppcoreguidelines-pro-type-member-init` ×1 | `ParsedInclusion` fields `{}` initialized | §33, §60 |
| `rarity_inclusion_ops.hpp` | `readability-function-cognitive-complexity` ×1 | Decomposed `seed_inclusion_occurrence_data` into 3 `detail::` helpers | §13, §58 |
| `rarity_inclusion_ops.hpp` | `cppcoreguidelines-pro-type-reinterpret-cast` ×2 | NOLINT with reason at sqlite3 boundary | §12, §61 |
---

---

## 78. Eliminating `misc-non-private-member-variables-in-classes` via Method Extraction (§64 Applied)

**Triggered by**: `misc-non-private-member-variables-in-classes` on structs with
member functions (including constructors beyond implicit defaults).

### 78.1 Constructor Elimination for Simple Aggregates

When a struct's only non-defaulted constructor forwards arguments to member
constructors, it can be eliminated in favor of aggregate initialization:

```cpp
// WRONG — constructor makes struct non-aggregate, triggers warning on nm
struct wavelength {
  real nm;
  explicit wavelength(double nm_val, const precision_config &cfg)
      : nm{nm_val, cfg} {}
};

// CORRECT — pure aggregate; callers use aggregate init
struct wavelength {
  real nm;
};

// Call sites:
wavelength w{real{450.0, cfg}};    // explicit config
wavelength w2{450.0};               // implicit via real(double) non-explicit ctor

// If 2-arg convenience is needed, provide a free factory:
inline wavelength make_wavelength(double nm_val, const precision_config &cfg) {
  return wavelength{real{nm_val, cfg}};
}
```

### 78.2 Member Method Extraction to Free Function

When a struct has a single method, extract it as a free function taking
`const StructType &` (or non-const `&` for mutators):

```cpp
// WRONG — compute_light_loss makes inclusion_in_diamond non-aggregate
struct inclusion_in_diamond {
  inclusion_properties properties;
  position_class position;
  [[nodiscard]] real compute_light_loss(const wavelength &, const real &) const;
};

// CORRECT — pure aggregate + free function
struct inclusion_in_diamond {
  inclusion_properties properties;
  position_class position;
};

[[nodiscard]] inline real compute_inclusion_light_loss(
    const inclusion_in_diamond &inc,
    const wavelength &lambda,
    const real &n_diamond) {
  // ... body using inc.properties, inc.position ...
}
```

Call-site migration: `inc.compute_light_loss(a, b)` → `compute_inclusion_light_loss(inc, a, b)`.

> **Rule**: when a struct has ≤ 2 methods and no invariants to protect, extract
> methods to free functions. Use `grep -rn '.method_name(' src/` to find all
> call sites. The compiler enforces completeness (member no longer exists).

---

## 79. Bounded Recursion Elimination (`misc-no-recursion`)

**Triggered by**: `misc-no-recursion`.

When a function contains a bounded-depth-1 recursive fallback (e.g., unknown
shape → round_brilliant), eliminate the recursion by extracting the base case
to a separate function:

```cpp
// WRONG — for_shape calls itself for unknown shapes
static params for_shape(shape s, const config &cfg) {
  switch (s) {
  case shape::round_brilliant: return {...};
  case shape::unknown:
    // NOLINTNEXTLINE(misc-no-recursion) — bounded
    return for_shape(shape::round_brilliant, cfg);
  }
  return for_shape(shape::round_brilliant, cfg);
}

// CORRECT — extract base case, eliminate recursion entirely
static params round_brilliant_params(const config &cfg) {
  return {...};  // exact same return value as case round_brilliant
}

static params for_shape(shape s, const config &cfg) {
  switch (s) {
  case shape::round_brilliant: return round_brilliant_params(cfg);
  case shape::unknown: return round_brilliant_params(cfg);
  }
  return round_brilliant_params(cfg);
}
```

> **Rule**: never use `NOLINTNEXTLINE(misc-no-recursion)` when the recursion
> can be trivially eliminated by function extraction. The helper function must
> return exactly the same values as the original base case to preserve behavior.

---

## 80. Dead Parameter Removal (`bugprone-easily-swappable-parameters` + `[[maybe_unused]]`)

When `bugprone-easily-swappable-parameters` fires on a parameter marked
`[[maybe_unused]]`, the parameter is both a transposition risk AND unused.
Remove it entirely rather than suppressing with NOLINT:

```cpp
// WRONG — hundred is unused and creates swappable pair with min_pct
static double interpolate_weight_rarity(
    sqlite3 *db, double weight,
    [[maybe_unused]] MPFRGuard &hundred,  // swappable with min_pct
    MPFRGuard &min_pct, MPFRGuard &max_pct,
    mpfr_prec_t precision);

// CORRECT — dead parameter removed
static double interpolate_weight_rarity(
    sqlite3 *db, double weight,
    MPFRGuard &min_pct, MPFRGuard &max_pct,
    mpfr_prec_t precision);
```

> **Rule**: `[[maybe_unused]]` on a function parameter is a code smell.
> If the parameter is truly unused, remove it from the signature and all
> call sites. If it will be used in future, add the implementation now
> or document the planned usage with a TODO.

---

## 81. Parameter Bundling for Related String Parameters (§14.1 Applied)

When multiple functions in a module share the same group of adjacent string
parameters (e.g., normalized/intensity/base_color in color parsing), create
a single aggregate struct that bundles them:

```cpp
struct ColorParseInput {
  std::string_view normalized;
  std::string_view intensity;
  std::string_view base_color;
};

// All four color parse functions use the same aggregate:
std::vector<...> find_overtone_modifiers(const ColorParseInput &input);
std::optional<std::string> extract_overtone(const ColorParseInput &input);
std::string strip_known_spans(const ColorParseInput &input, ...);
std::vector<ModifierToken> extract_modifier_tokens(const ColorParseInput &input);
```

Construction at the top-level entry point:

```cpp
const ColorParseInput parse_input{normalized, result.intensity, result.base_color};
auto overtone = extract_overtone(parse_input);
auto tokens = extract_modifier_tokens(parse_input);
```

> **Rule**: when ≥ 3 functions share ≥ 2 adjacent same-type parameters, bundle
> into a named aggregate. Use `std::string_view` members when the aggregate
> does not own the strings (lifetime managed by caller).


---

## 79. Poco JSON `getObject()` — Integer Precision Truncation

**Triggered by**: `clang-diagnostic-shorten-64-to-32`.

Poco's `JSON::Array::getObject()` takes `unsigned int`, but range iteration over
`arr->size()` yields `std::size_t` (64-bit on LP64). Passing `size_t` directly
truncates silently.

```cpp
// WRONG — implicit 64→32 truncation
for (size_t i = 0; i < arr->size(); ++i) {
  auto obj = arr->getObject(i);  // size_t → unsigned int

// CORRECT — explicit static_cast at Poco API boundary
for (std::size_t i = 0; i < arr->size(); ++i) {
  const auto obj = arr->getObject(static_cast<unsigned int>(i));
```

> **Rule**: all Poco JSON index arguments must use `static_cast<unsigned int>()`
> when the source index is `size_t`. This is safe when array sizes fit in 32 bits
> (JSON arrays exceeding 4B elements are not expected in this domain).

---

## 80. Empty Catch Blocks (`bugprone-empty-catch`)

**Triggered by**: `bugprone-empty-catch`.

An empty `catch (...)` block silently swallows all exceptions including logic errors
and resource exhaustion. When the catch-and-return-default pattern is intentional
(e.g., JSON type conversion fallback), annotate with NOLINT and a reason:

```cpp
// WRONG — hides all errors including OOM
} catch (...) {
}

// CORRECT — documented intentional fallback with NOLINT
} catch (...) { // NOLINT(bugprone-empty-catch) — Poco JSON may throw on type mismatch; fallback to default
}
```

If the catch block can log or handle the error, prefer that over NOLINT.

> **Rule**: empty catch blocks require either (a) a log/handle statement, or
> (b) `// NOLINT(bugprone-empty-catch) — <reason>`. Never leave a catch block
> both empty and unannotated.

---

## 81. `pipe2()` / C API with `std::array::data()`

**Triggered by**: `cppcoreguidelines-pro-bounds-array-to-pointer-decay`.

C APIs like `pipe2()` take raw pointers. When the underlying storage is a
`std::array`, use `.data()` instead of allowing implicit array-to-pointer decay:

```cpp
// WRONG — C-style array decays to pointer
static int g_sig_pipe[2] = {-1, -1};
pipe2(g_sig_pipe, O_NONBLOCK | O_CLOEXEC);

// CORRECT — std::array with .data()
static std::array<int, 2> g_sig_pipe = {-1, -1};
pipe2(g_sig_pipe.data(), O_NONBLOCK | O_CLOEXEC);
```

> **Rule**: replace C-style arrays used at C API boundaries with `std::array`
> and pass `.data()` to the C function. This preserves size information and
> avoids the array-to-pointer-decay diagnostic.

---

## 82. `strdup` / `free` at C Callback Boundaries

**Triggered by**: `cppcoreguidelines-no-malloc`, `cppcoreguidelines-owning-memory`.

Some C APIs (e.g., sqlite3 callbacks) require C-style allocation because the
callback signature is fixed by the library. When `strdup()` is required inside
a C callback and `free()` outside it, annotate both with NOLINT:

```cpp
// sqlite3_exec callback — signature mandated by C API
const auto cb = [](void *out, int, char **vals, char **) -> int {
  if (vals[0] != nullptr) // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic) — sqlite3 callback
    *static_cast<char **>(out) = strdup(vals[0]); // NOLINT(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
  return 0;
};
// ...
free(result); // NOLINT(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory) — strdup allocation
```

> **Rule**: `strdup`/`free` pairs at C callback boundaries require matching NOLINT
> annotations. Prefer C++ alternatives (std::string, std::unique_ptr) when the
> callback signature permits them.

---

## 83. Cognitive Complexity — Return-Struct for Multi-Value Extraction

**Triggered by**: `readability-function-cognitive-complexity` on functions that
compute multiple related values consumed by a subsequent function.

When decomposing a high-complexity function, intermediate computations that produce
multiple related values should return a named struct rather than modifying output
parameters or using std::tuple:

```cpp
// WRONG — output parameters increase caller complexity
inline void compute_scaling(const auto &colors, mpfr_prec_t prec,
                            MPFRGuard &fancy_out, MPFRGuard &nonfancy_out);

// CORRECT — named return struct
struct ScalingFactors {
  MPFRGuard fancy_scale;
  MPFRGuard nonfancy_scale;
  explicit ScalingFactors(mpfr_prec_t prec)
      : fancy_scale(prec), nonfancy_scale(prec) {}
};
inline ScalingFactors compute_color_scaling(const auto &colors, mpfr_prec_t prec);
```

> **Rule**: extracted helper functions that produce 2+ related outputs should
> return a named struct. This makes the caller's data flow explicit and avoids
> output-parameter cognitive overhead.


## 82. `std::span<char*>` over `argv` — Eliminating Pointer Arithmetic

**Triggered by**: `cppcoreguidelines-pro-bounds-pointer-arithmetic` on `argv[i]`,
`argv[++i]`, and similar indexed access to a raw `char **argv`.

Raw `argv` indexing decays to pointer arithmetic under the check. Wrap once at
the top of `main()`/`parse_arguments()` and work through `std::span`:

```cpp
inline bool parse_arguments(int argc, char **argv, ...) {
  const std::span<char *> args{argv, static_cast<std::size_t>(argc)};
  for (std::size_t i = 1; i < args.size(); ++i) {
    const std::string_view arg{args[i]};
    // ...
  }
}
```

Helper for string_view extraction (keeps call sites noise-free):

```cpp
[[nodiscard]] inline std::string_view
arg_at(std::span<char *> args, std::size_t idx) noexcept {
  return std::string_view{args[idx]};
}
```

> **Rule**: never index `argv` directly. Always wrap in `std::span<char *>` at
> the entry point and propagate the span to helpers. Helpers that advance the
> cursor take `std::span<char *> args, std::size_t &i` as adjacent parameters
> of distinct types — not swappable.

---

## 83. `std::string_view` + `std::from_chars` — Localized NOLINT Helper

**Triggered by**: `cppcoreguidelines-pro-bounds-pointer-arithmetic` on
`s.data() + s.size()` when feeding `std::from_chars`.

`std::from_chars` requires raw `const char *` bounds; computing the end pointer
from a `string_view` trips the pointer-arithmetic check. Isolate the single
required NOLINT in one helper:

```cpp
namespace parse_detail {
[[nodiscard]] inline const char *sv_end(std::string_view s) noexcept {
  return s.data() + s.size(); // std::from_chars C API boundary
}
}
```

All numeric parsing helpers then become clean:

```cpp
[[nodiscard]] inline bool to_int(std::string_view s, int &out) noexcept {
  const auto *const first = s.data();
  const auto *const last = parse_detail::sv_end(s);
  auto [ptr, ec] = std::from_chars(first, last, out);
  return ec == std::errc{} && ptr == last;
}
```

> **Rule**: when a C-style iterator pair API requires `ptr + size`, isolate the
> arithmetic in one named helper. All numeric-parse helpers funnel through it.
> This keeps the NOLINT surface minimal and auditable.

---

## 84. Dispatch Tables for `parse_arguments` (Extends §27)

**Triggered by**: `readability-function-cognitive-complexity` on
`query::parse_arguments` (complexity 85).

Extends §27 (server-side dispatch table) to the client-side query tool.
Structure: three table types searched in order of specificity.

1. CSV filter table (existing pattern — `kCsvFilterOptions`)
2. Double filter table (existing pattern — `kDoubleFilterOptions`)
3. Bool criteria table (existing pattern — `kBoolCriteriaOptions`)
4. **New: Value-arg table** (`--db`, `--sort-by`, `--format`, `--limit_output`,
   `--rarity-weight`, `--illuminant`)
5. **New: Flag-arg table** (`--white-only`, `--compact`, `--debug`, …)

Each dispatch stage returns a tri-state `DispatchResult { Handled, NotFound, Error }`.
The main loop fall-through on `NotFound` into the next stage, short-circuits on
`Error`. Complexity of the main loop reduces to ~8.

```cpp
enum class DispatchResult : std::uint8_t { Handled, NotFound, Error };

[[nodiscard]] inline DispatchResult
dispatch_one_arg(std::string_view arg,
                 std::span<char *> args,
                 std::size_t &i,
                 ParseCtx &ctx);
```

Handlers are free functions with `noexcept` specifiers and complexity ≤ 5.

> **Rule**: `parse_arguments` must not grow past complexity 20. New flags get
> a handler + table entry — never a new `else if` branch.

---

## 85. Validation Helpers — Split by Concern

**Triggered by**: cognitive complexity from post-parse validation blocks.

The trailing validation in `parse_arguments` (checking filter-type combinations,
fancy-color exclusions, lot/cert exclusivity) adds ~10 complexity points. Split
each concern into its own `[[nodiscard]]` predicate:

```cpp
[[nodiscard]] bool validate_white_exclusions(const QueryFilters &);
[[nodiscard]] bool validate_colored_exclusions(const QueryFilters &);
[[nodiscard]] bool validate_fancy_full_name_exclusions(const QueryFilters &);
[[nodiscard]] bool validate_lot_cert_exclusive(const QueryFilters &);
void              warn_lot_id_overrides(const QueryFilters &);

[[nodiscard]] bool validate_filter_combinations(const QueryFilters &filters) {
  return validate_white_exclusions(filters)
      && validate_colored_exclusions(filters)
      && validate_fancy_full_name_exclusions(filters)
      && validate_lot_cert_exclusive(filters)
      && (warn_lot_id_overrides(filters), true);
}
```

> **Rule**: each validation concern is one predicate. The combinator uses
> short-circuit `&&` for a complexity of ~5 regardless of concern count.

---

## 86. `std::forward` in Forwarding-Reference Templates

**Triggered by**: `cppcoreguidelines-missing-std-forward`.

A function template taking `T &&param` declares a forwarding reference. If the
parameter is invoked (or passed on), it **must** be forwarded with
`std::forward<T>(param)`. Not forwarding silently converts the rvalue case to
lvalue semantics.

```cpp
// WRONG
template <typename SortFn>
void render(SortFn &&sorter) {
  sorter(group); // lvalue call — UB risk if SortFn has rvalue-only op()
}

// CORRECT
template <typename SortFn>
void render(SortFn &&sorter) {
  std::forward<SortFn>(sorter)(group);
}
```

> **Rule**: every `T &&` template parameter that is used inside the function
> must be wrapped in `std::forward<T>(...)` at every use site — even for
> single-use callables.

---

## 87. Private Members for RAII Wrapper Classes

**Triggered by**: `misc-non-private-member-variables-in-classes`.

RAII wrapper **classes** (as opposed to pure aggregates) with behavior in
constructors/destructors must have private data members. The check does not
fire on pure aggregates, but does fire on classes with constructors that mutate
state.

```cpp
// WRONG — members public despite non-trivial ctor/dtor
struct StderrRedirect {
  std::streambuf *backup{nullptr};
  std::ofstream null_stream;
  explicit StderrRedirect(bool suppress) { /* ... */ }
  ~StderrRedirect() { /* ... */ }
};

// CORRECT — class with private members
class StderrRedirect {
public:
  explicit StderrRedirect(bool suppress);
  ~StderrRedirect();
  StderrRedirect(const StderrRedirect &) = delete;
  StderrRedirect &operator=(const StderrRedirect &) = delete;
  StderrRedirect(StderrRedirect &&) = delete;
  StderrRedirect &operator=(StderrRedirect &&) = delete;
private:
  std::streambuf *backup_{nullptr};
  std::ofstream null_stream_;
};
```

For raw C-API handles that need to be passed to C functions taking `Handle **`
(e.g. `sqlite3_open(path, &db)`), expose both an accessor and a slot function:

```cpp
class RunContext {
public:
  [[nodiscard]] sqlite3 *db() noexcept { return db_; }
  [[nodiscard]] sqlite3 **db_slot() noexcept { return &db_; }
private:
  sqlite3 *db_{nullptr};
};

// Usage:
sqlite3_open(path.c_str(), ctx.db_slot());
query::query_lots_from_db(ctx.db(), filters);
```

> **Rule**: any struct with non-trivial ctor/dtor becomes a `class` with private
> members. For C-API handle ownership, expose `handle()` getter + `slot()`
> `T**` accessor. Do not NOLINT (see §68).

---

## 88. Extracting Display Helpers to Reduce Cognitive Complexity

**Triggered by**: `readability-function-cognitive-complexity` on
`display_diamond_result` (complexity 80).

Large stream-emission functions accumulate complexity from conditional
sub-sections. Each independent sub-section becomes a free function in a
`*_detail` sub-namespace:

```cpp
namespace query::compact_detail {
void print_header_line(std::ostream &, const DiamondResult &);
void print_color_section(std::ostream &, const db::MerchantLot &);
void print_cut_polish_symmetry(std::ostream &, const db::MerchantLot &);
void print_measurements_and_proportions(std::ostream &, const db::MerchantLot &);
void print_pricing_block(std::ostream &, const DiamondResult &);
void print_value_metrics(std::ostream &, const DiamondResult &);
void print_intervals_block(std::ostream &, const DiamondResult &);
void print_component_scores(std::ostream &, const DiamondResult &);
}
```

The public entry point becomes a linear sequence of calls with complexity ~6.

For N identical `if (score) { os << label << *score; ... }` blocks, bundle the
arguments into an aggregate and define a single emitter:

```cpp
struct ComponentLine {
  std::string_view label;
  const std::optional<double> &score;
  bool used_flag;
  std::string_view used_suffix;
};
void emit_component(std::ostream &os, const ComponentLine &line);
```

> **Rule**: any stream-emission function over complexity 25 must be split into
> `*_detail` sub-namespace helpers. Use aggregate structs for repetitive emit
> patterns (extends §53).

---

## 89. `else after return` — Dispatch-Table Fix

**Triggered by**: `readability-else-after-return`.

Long `if/else if` chains with `return` inside each branch are also flagged by
this check. The dispatch table refactor (§84) eliminates the chain entirely,
resolving `else after return` as a side effect.

> **Rule**: dispatch tables are the preferred fix for `readability-else-after-return`
> in argument parsing. Do not manually rewrite each branch.

---

## 90. Object-Size Sanitizer at `-O0`

**Triggered by**: `clang-diagnostic-invalid-command-line-argument` —
"the object size sanitizer has no effect at -O0".

`-fsanitize=object-size` requires optimizations to compute the sizes of
allocated objects at compile time. Clang emits a warning at `-O0` because
the check is silently disabled.

**Fix in CMake**: gate the flag behind the build type:

```cmake
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-fsanitize=object-size)
  add_link_options(-fsanitize=object-size)
endif()

---

## 83. Parallel-Array Initialisation Must Route Through a Single Helper

**Triggered by**: manual code review — duplicated init loops drift silently.

When two or more code paths construct a `std::vector<Result>` from a parallel
`std::vector<Source>` (e.g. `results[i]` fields derived from `lots[i]`), the
initialisation **must** be centralised in a single inline helper. Hand-rolled
per-site loops drift whenever a new field is added to `Result`: one site gets
the field set, the other does not, and the bug is undetectable at compile time.

```cpp
// WRONG — two call sites, two copies of the init loop. Adding a new field
// (e.g. lot_index) to one site leaves the other silently broken.
// site A (query_v2_main.cpp)
for (std::size_t i = 0; i < lots.size(); ++i) {
  results[i].lot = lots[i];
  results[i].lot_index = i;                // added here
  results[i].is_white_diamond = is_white_diamond_color(lots[i].color);
}
// site B (query_v2_core.hpp execute_and_serialize)
for (std::size_t i = 0; i < lots.size(); ++i) {
  results[i].lot = lots[i];
  // lot_index NOT set — silent coupling bug
  results[i].is_white_diamond = is_white_diamond_color(lots[i].color);
}

// CORRECT — single helper, called from every entrypoint.
inline void init_results_from_lots(std::vector<DiamondResult> &results,
                                   std::span<const db::MerchantLot> lots) {
  results.assign(lots.size(), DiamondResult{});
  for (std::size_t i = 0; i < lots.size(); ++i) {
    const auto &lot = lots[i];
    auto &result = results[i];
    result.lot = lot;
    result.lot_index = i;
    result.is_white_diamond = is_white_diamond_color(lot.color);
    result.lab = lot.lab;
    result.price_per_carat_calculated =
        (lot.weight > 0.0) ? (lot.total_price / lot.weight) : 0.0;
  }
  assert(lot_index_coupling_intact(results));
}
```

> **Rule**: when a result vector is constructed from a parallel source vector
> at two or more call sites, extract a single `init_*_from_*` helper and route
> every call site through it. Per-field drift is otherwise undetectable. Pair
> the helper with a `[[nodiscard]] noexcept` coupling predicate
> (e.g. `lot_index_coupling_intact`) and assert it at the end of the helper so
> debug builds catch any future regression.

## 84. Coupling Predicates — Index Loop, Not Pointer Arithmetic

**Triggered by**: `-Wsign-conversion` + review of `P0122R7` guidance.

When asserting that `results[i].lot_index == i` for a contiguous container,
prefer an explicit index loop over pointer arithmetic. `&result - results.data()`
is a `ptrdiff_t` and triggers `-Wsign-conversion` when compared to `std::size_t`.

```cpp
// WRONG — ptrdiff_t / size_t mismatch under -Wsign-conversion
assert(result.lot_index == static_cast<std::size_t>(&result - results.data()));

// CORRECT — loop-local index, sign-clean
[[nodiscard]] inline bool
lot_index_coupling_intact(std::span<const DiamondResult> results) noexcept {
  for (std::size_t i = 0; i < results.size(); ++i) {
    if (results[i].lot_index != i)
      return false;
  }
  return true;
}
```

> **Rule**: coupling assertions over contiguous containers use an explicit
> `std::size_t` index loop. Pointer-difference idioms are banned.


## 86. Presence-Based Field Tracking for Graded Attributes (Fluorescence Pattern)

**Triggered by**: rarity/sparkle fields-used profile divergence for
`Fluorescence = None` graded diamonds — the same grade was counted as *used*
by one pipeline and *missing* by the other, producing mirror-image inconsistent
grouping on the same dataset (see commit fixing §86 example).

### Symptom

Diamonds with `Fluorescence = N` (graded "None") appeared in groups where:

- Rarity fields-used profile marked **Fluorescence as MISSING** (because
  `query_fluorescence_rarity` early-returns `0.0` for normalized `"None"`, and
  `apply_score_if` only appends the profile code when `*target > 0.0`); while
- Sparkle fields-used profile marked **Fluorescence as USED** (because for
  fancy color — `color_grade::UNKNOWN` — the interaction matrix path returned
  a small perturbation, or the legacy fallback scaled above the `1e-6`
  magnitude gate).

And simultaneously, diamonds with `Fluorescence ∈ {F, M, STG, VST}` appeared in
groups where the inverse held (Sparkle → missing, Rarity → used), because the
sparkle magnitude gate collapsed for fancy-color + the UNKNOWN matrix row.

### Root Cause — Two Independent Gates Misaligned

Two independent "is this field used" gates encoded the same semantic question
with different — and both incorrect — proxies:

1. **Rarity side** (`query_v2_core.hpp::apply_score_if`): *"code this field iff
   the computed rarity score is strictly positive."* Wrong because `"None"` is
   a **real, gradable** category with a finite occurrence frequency in the seed
   distribution. A log-rarity of `0.0` means "no marginal contribution", not
   "no data".
2. **Sparkle side** (`sparkle_integration.hpp::apply_grade_factors`): *"flag
   this field iff the multiplicative factor deviates from 1.0 by more than
   1e-6."* Wrong for the same reason: `factor == 1.000` for
   `fluorescence_grade::none` is **correct physics** (no optical modifier),
   not **absent data**.

Both gates conflated **"contributed a non-trivial numeric delta"** with
**"the underlying graded datum was observed and fed into the model"**. Fields
used is the latter question.

### Rule

> **Presence-based flagging rule.** For any graded categorical attribute whose
> *baseline* value (e.g. `None`, `Round`, `0 inclusions`) is a legitimate,
> observer-assignable grade — not a data-absence sentinel — the fields-used
> flag MUST be set on *presence of the grade*, gated only by exclusion state
> and availability, **never** by the magnitude of the computed contribution.

Equivalent pseudocode for each side:

```cpp
// Rarity side — template variant that flags on data presence only.
template <typename F>
static inline void apply_score_if_present(bool criteria_flag,
                                          bool has_data,
                                          std::optional<double> &target,
                                          std::string &profile_codes,
                                          std::string_view code,
                                          F &&scorer) {
  if (criteria_flag && has_data) {
    target = std::forward<F>(scorer)();
    profile_codes += code;                    // unconditional — no `> 0.0`
  }
}

// Sparkle side — drop the magnitude gate on the usage flag.
if (auto fluo_f = fluorescence_color_interaction_factor(
        spec.fluorescence, spec.color, cfg)) {   // nullopt iff grade == unknown
  if (!excl.exclude_fluorescence) {
    brilliance_final = brilliance_final * (*fluo_f);
    usage.used_fluorescence = true;             // unconditional — not `|f-1|>1e-6`
  }
}
```

### Distinguishing "Grade Present" from "Grade Unknown"

Presence-based flagging is **not** "flag unconditionally". A genuine
absent-data sentinel (`fluorescence_grade::unknown`, empty string,
uninterpretable token) must still suppress the flag:

- `fluorescence_color_interaction_factor()` returns `std::nullopt` for
  `fluorescence_grade::unknown` — the caller's `if (auto f = ...)` already
  excludes the unknown path.
- `query_v2_core.hpp::calculate_rarity_scores` derives `has_fluorescence`
  from `!merchant_lot.fluorescence_intensity.empty()` — empty-string means
  "no data on this lot" and must not be flagged.

### Exclusion Handling

The exclusion flag (`RatingExclusions::exclude_fluorescence`,
`sparkle_exclusions::exclude_fluorescence`) always suppresses the usage
flag — an excluded factor is, by user intent, **not a used field**.

For `rarity_engine_calc.hpp::get_calculation_profile`, the exclusion state
must be propagated into the function:

```cpp
[[nodiscard]] inline CalculationProfile
get_calculation_profile(const db::Lot &lot,
                        const rarity::RarityReport &report,
                        const RatingExclusions &exclusions = {}) {
  // ...
  profile.has_fluorescence = (!lot.Fluoresence_Intensity.empty() &&
                              !exclusions.exclude_fluorescence);
  // ...
}
```

The default-argument preserves source compatibility with existing call sites
that pre-date the rule; new call sites should pass the exclusions explicitly.

### Scope and Expansion

Only **fluorescence** was migrated to presence-based flagging in the current
fix, because only fluorescence exhibited the observable symptom in production
output (the "None" grade is the only attribute whose baseline is common enough
to collapse the score gate for a large fraction of the dataset).

The same reasoning applies to **any** categorical grade whose most-common
value is a legitimate non-null baseline. Candidates to audit next:

- `Inclusions` — a graded "no visible inclusions" (IF-equivalent) is a real
  observation, not data absence.
- `Symmetry / Polish` — grade `EX` in a population where `EX` is baseline;
  currently gated by `|f-1|>1e-6` in `apply_grade_factors`.
- `Shape` — not at risk (shape enum has a distinct `unknown` sentinel that
  `parse_shape` returns for absent data).

These are **not** included in the current fix because changing them would
shift the fields-used grouping for every diamond in the database, which is
beyond the scope of the reported symptom and requires separate user
confirmation. File an issue before expanding the rule.

### Anti-Patterns (banned)

```cpp
// WRONG — uses score magnitude as proxy for presence.
if (target && *target > 0.0) profile_codes += code;

// WRONG — uses factor deviation as proxy for presence.
if (std::abs(f.to_double() - 1.0) > 1e-6) usage.used_X = true;

// WRONG — silently drops a graded baseline value by early-returning zero
// and relying on the downstream score-gate to exclude it from the profile.
inline double query_fluorescence_rarity(sqlite3 *db, const db::Lot &lot) {
  const std::string norm = normalize_fluorescence(lot.Fluoresence_Intensity);
  if (norm.empty() || norm == "None") return 0.0;   // <-- couples two concerns
  // ...
}
```

The read-op early-return in `rarity_read_ops.hpp::query_fluorescence_rarity`
is retained for now (changing it shifts numeric rarity scores, out of scope
for the symptom fix), but is flagged as a latent risk: if the downstream
`apply_score_if_present` helper is ever reverted to a score-gated variant,
the bug re-emerges silently. A future patch should either (a) remove the
early-return and let the SQL query return the real "None" occurrence row,
or (b) document the coupling in a `static_assert`-style comment next to
both sites.

### Testing

Any regression test for this rule must cover **all four cells** of the
`{None, Graded} × {White, Fancy}` matrix and assert that `"F"` appears in
both the rarity and sparkle `fields_used_profile` strings for every cell
except the one where exclusion is active:

| Color   | Fluorescence | exclude_fluorescence | expect "F" in rarity | expect "F" in sparkle |
|---------|--------------|----------------------|----------------------|-----------------------|
| White D | None         | false                | YES                  | YES                   |
| White D | Strong       | false                | YES                  | YES                   |
| Fancy Y | None         | false                | YES                  | YES                   |
| Fancy Y | Strong       | false                | YES                  | YES                   |
| any     | empty/unknown| false                | no                   | no                    |
| any     | any          | true                 | no                   | no                    |

> **Rule**: presence-based fields-used flagging for graded categorical
> attributes. Never use score magnitude or factor deviation as a proxy for
> data presence. The `unknown` / empty-string sentinel is the only legitimate
> suppressor besides explicit exclusion.


### 86.2 Removing the `query_fluorescence_rarity` "None" Early-Return

**Status**: applied as the second stage of the §86 fix.

The original `rarity_read_ops.hpp::query_fluorescence_rarity` contained:

```cpp
const std::string norm = normalize_fluorescence(lot.Fluoresence_Intensity);
if (norm.empty() || norm == "None")
  return 0.0;
```

This was a shortcut hiding two independent concerns behind a single branch:

1. *"The graded value is 'None', so it has no marginal rarity contribution"* —
   **false**. "None" is ~67% of the population (Moses 1997, G&G 33(4), 244–259)
   so its log₁₀ rarity contribution is `log₁₀(100/67) ≈ 0.174` — small, but
   finite and non-zero.
2. *"The input is empty/garbage, so there's no data to look up"* — this is
   already handled upstream by the caller's `has_fluorescence` presence gate
   in `query_v2_core.hpp::calculate_rarity_scores`, and by the empty-string
   check in `get_calculation_profile`.

The `normalize_fluorescence` catch-all `return "None"` for unrecognized input
is a separate, pre-existing latent issue: it means a garbage intensity string
is silently treated as "None". This is **not** fixed by §86.2 because
changing `normalize_fluorescence` would shift scoring for any unknown-format
entries (scope drift). Document the coupling here and flag for a future
cleanup that either (a) returns a distinct `""` for unrecognized input and
lets the downstream SQL lookup miss, or (b) routes unknown input to an
explicit error-log path.

#### Fix applied

```cpp
inline double query_fluorescence_rarity(sqlite3 *db, const db::Lot &lot) {
  // §86.2: "None" is a legitimate graded baseline (~67% occurrence per
  // Moses 1997) with a finite log10 rarity contribution. Do NOT early-
  // return zero here — feed the normalized value into the SQL lookup so
  // the seed `fluorescence_rarity` row for "None" is used.
  const std::string norm = normalize_fluorescence(lot.Fluoresence_Intensity);
  if (norm.empty())
    return 0.0;
  const auto &fluor = norm;
  // ... SQL lookup as before.
}
```

The `norm.empty()` guard is dead code with the current `normalize_fluorescence`
implementation (which always returns a non-empty canonical string), but is
retained as defensive depth in case that function is ever refactored.

#### Numeric impact

With `kFluorescence = 0.08` (8% of combined rarity weight), a "None"-graded
diamond now contributes `~0.174 × 0.08 ≈ 0.014` log₁₀ to the combined rarity
score vs zero before. For a typical combined score in the range 3–5, this
is a relative shift of ~0.3–0.5% — well below the ±20% per-attribute
uncertainty radius documented in `rarity_interval_math.hpp` for
fluorescence (±5% per-attribute → combined contribution well under the
combined-score interval width). The shift is semantically correct: None is
no longer "not counted", it is "counted with a small contribution matching
its ~67% population frequency".

#### Downstream copula adjustment

`rarity_copula_adjustment.hpp::score_to_z` gates on `log10_score <= 0.0`:

```cpp
double score_to_z(double log10_score) noexcept {
  if (log10_score <= 0.0) return 0.0;
  // ...
}
```

Previously `None → 0.0 → z = 0` (no copula contribution). After §86.2,
`None → 0.174 → z = Φ⁻¹(1 − 10⁻⁰·¹⁷⁴) = Φ⁻¹(0.330) ≈ −0.44`. This is a
small negative z-score contributing via the quadratic form
`(½) Σ ρᵢⱼ zᵢ zⱼ wᵢ wⱼ / ln(10)`. Effect magnitude is bounded by the
fluorescence ρ with other attributes (per `rarity_copula_adjustment.hpp`
the ρ matrix entries for fluorescence are ≈ 0.05 ± 0.05), so the resulting
δ_copula on the combined score is below 1e-3 log₁₀ for None-graded
diamonds. No correction needed.

#### Test update

The fixed-value assertion in `test_integration.cpp::test_fluorescence_rarity`
was:

```cpp
ctx.check(none_score == 0.0, "None returns 0.0 (most common)");
```

This is now wrong and is replaced by a monotonic-ordering assertion that
expresses the underlying intent ("None is the most common grade, so it
has the lowest rarity score") without hard-coding a specific numeric
value:

```cpp
ctx.check(scores.at(0) > 0.0,
          "None has strictly positive rarity (§86.2: no longer 0.0)");
ctx.check(scores.at(0) < scores.at(1), "None < Faint (None is most common)");
ctx.check(scores.at(1) < scores.at(2), "Faint < Medium");
ctx.check(scores.at(2) < scores.at(3), "Medium < Strong");
ctx.check(scores.at(3) < scores.at(4), "Strong < Very Strong");
```

Monotonic assertions are preferred over absolute-value assertions for seed
data tests because they are robust to future seed data revisions (e.g., if
the population percentages are recalibrated from a newer GIA study). They
still catch the original bug (zero score for None) and additionally catch
any future regression that breaks the ordering invariant.

> **Rule**: when asserting rarity scores against seed data, prefer monotonic
> ordering invariants over absolute-value equalities. The invariant captures
> the semantic contract (common → low rarity); the absolute value does not.

## §87 JSON key drift — always prefer `jget_first` with candidate lists for merchant-feed fields

### Rule
Any field read from a third-party merchant JSON feed MUST use
`update_db::jget_first<T>` with an explicit candidate list, never the
single-key `jget<T>` form. This applies even when the canonical key
"looks obviously correct" — merchant feeds drift across versions and
between vendors, and a single-key miss degrades silently to the default
value (`""` or `0.0`) with zero diagnostic signal.

### Rationale
- §86.2 (fluorescence typo `"Fluoresence"`) and P0.1 (Midonline
  `"Total"` / `"Price"` instead of `"Total Price"` / `"Price Per Carat"`)
  are two instances of the same failure class: a silently-defaulted
  numeric or string field that looks identical to a legitimately empty
  record in the output.
- `jget<double>` returning `0.0` on a key miss is indistinguishable from
  `0.0` as a valid reading. The only defence is a candidate list that
  covers every observed spelling.

### Positive pattern
```cpp
mlot.total_price = jget_first(
    obj,
    {"Total Price", "Total", "TotalPrice", "total_price"},
    0.0);
```

### Negative pattern
```cpp
mlot.total_price = jget(obj, "Total Price", 0.0);
```

### Candidate-list ordering
1. Canonical documented spelling first (what the spec says).
2. Compact / joined variants next (`"TotalPrice"`, `"total_price"`).
3. Observed real-world spellings last, including known typos and
   shortened forms (`"Total"`, `"Price"`, `"Disc%"`).

The first non-empty (for strings) or first present (for numerics) hit
wins; downstream consumers are unaffected.

### Test obligation
Every field migrated to `jget_first` MUST gain a matching assertion in
`src/test/test_json_key_tolerance.cpp` (Group 8: real-world fragment
parse). A migration without a test entry is incomplete.

### Caveat on numeric `jget_first`
`jget_first<double>` returns on the first present key even if the value
parses as `0.0`. That is acceptable only because `obj->has(k)` gates
each candidate — a missing key is skipped, but a present-but-zero key
terminates the search. Do NOT rely on `jget_first<double>` to fall
through zeros across candidates. If a feed legitimately reports `0.0`
in a populated field, put it first; if it reports absence as `0.0`,
handle it upstream before the call.

### Scope
Applies to `src/update_db_main/update_db_main.cpp` and any future
parse helpers extracted under P2.2 (`update_db_parse.hpp`).

### §86.3 — Shape Rarity Field Presence Tracking + Alias Normalization

The shape attribute is subject to the same presence-flagging rule as
fluorescence (§86) **and** an additional aliasing problem: merchant feeds
emit shape names like `"Round"` while the seed table stores
`"Round Brilliant"`. The exact-match SQL lookup returns no row →
`query_shape_rarity` returns `0.0` → `apply_score_if` drops the `"S"`
profile code → `get_calculation_profile.has_shape` returns `false` → the
fields-used grouping reports shape as missing for every all-Round group.

**Rule (additive to §86):** Shape rarity lookups MUST normalize the input
through `rarity::shape_name::normalize_shape_name()` before the SQL
lookup, and the rarity-side presence flag MUST come from
`apply_score_if_present` (presence of `Shape_Full_Name`), not from
`shape_rarity_score > 0.0`. The exclusion flag
`RatingExclusions::exclude_shape` is the only valid suppressor.

**Anti-pattern:**
```cpp
profile.has_shape =
    (!lot.Shape_Full_Name.empty() && report.shape_rarity_score > 0.0);
```

**Correct:**
```cpp
profile.has_shape =
    (!lot.Shape_Full_Name.empty() && !exclusions.exclude_shape);
```

The sparkle side (`compute_physics_sparkle`) is already presence-based
(`if (!excl.exclude_shape) usage.used_shape = true;`) and requires no
change — but the verification script asserts this as a regression guard.

The alias table in `shape_name_normalize.hpp` is intentionally
case-insensitive and additive: unrecognized inputs pass through
unchanged so that downstream `other_unidentified` handling is preserved.

## §86.4 — Cut / Polish / Symmetry Rarity Field Presence Tracking

Same anti-pattern as §86 (fluorescence) and §86.3 (shape), three more
fields. The grade-to-rarity multiplier table for `cut`, `polish`, and
`symmetry` contains entries that map to exactly `1.0` (e.g. `polish_grade::
excellent`, `symmetry_grade::excellent`, the `EX` baseline that dominates
modern inventory). `log10(1.0) == 0.0` → the score-gated `apply_score_if`
helper drops the profile code → the result-grouping key splits on a
phantom field, producing two otherwise-identical groups that differ only
on whether the baseline grade was "counted as used".

**Rule (additive to §86):** every rarity field, without exception, must
flow through `apply_score_if_present` (presence of the merchant lot
field) and `get_calculation_profile.has_X` must read
`(!lot.X.empty() && !exclusions.exclude_X)`. The
`report.X_rarity_score > 0.0` conjunct is forbidden — it conflates
"contributed a non-trivial numeric delta" with "the underlying graded
datum was observed", and the §86 rule says these are different
questions. Score-gating is only valid for fields that have no
zero-multiplier baseline — there are no such fields in this codebase.

## §86.5 — Sparkle Field Presence Tracking (cut / polish / symmetry / color / inclusions / proportions)

Mirror of §86.4 on the sparkle pipeline. The same `1.0` multipliers and
the same magnitude-deviation gates (`std::abs(factor - 1.0) > 1e-6`,
`transmission_val < 0.9999`, `loss_val > 1e-6`,
`std::abs(crown_actual - crown_default) > 0.1`) drop the usage flag for
the most common grades. Replace each magnitude/deviation gate with an
unconditional `usage.used_X = true;` inside the existing
`if (!excl.exclude_X)` block.

### Anti-patterns (banned, additive to §86)
```cpp
// §86.4 — banned: rarity score-gating on a baseline-1.0 field
profile.has_polish =
    (!lot.Polish.empty() && report.polish_rarity_score > 0.0);

// §86.5 — banned: sparkle magnitude gating on a baseline-1.0 multiplier
if (auto polish_f = polish_factor(spec.polish, cfg)) {
  if (!excl.exclude_polish) {
    brilliance_final = brilliance_final * (*polish_f);
    if (std::abs(polish_f->to_double() - 1.0) > 1e-6) {  // WRONG
      usage.used_polish = true;
    }
  }
}

// §86.5 — banned: sparkle deviation gating on geometric measurement
if (!excl.exclude_depth) {
  if (std::abs(actual_crown - default_crown) > 0.1 ||         // WRONG
      std::abs(actual_pavilion - default_pavilion) > 0.1) {
    components.usage.used_proportions = true;
  }
}
```

### Correct form
```cpp
profile.has_polish = (!lot.Polish.empty() && !exclusions.exclude_polish);

if (auto polish_f = polish_factor(spec.polish, cfg)) {
  if (!excl.exclude_polish) {
    brilliance_final = brilliance_final * (*polish_f);
    usage.used_polish = true;
  }
}

if (!excl.exclude_depth) {
  components.usage.used_proportions = true;
}
```

### Latent risk: `compute_physics_sparkle` does not carry clarity grade

`used_inclusions` cannot perfectly distinguish "FL/IF graded with no
scattering centres" from "no clarity reading at all" without carrying
the parsed clarity grade into `diamond_specification`. Current fix gates
on `!spec.inclusions.empty() && !excl.exclude_inclusions`, which still
drops the flag for FL/IF lots. Tracked as TODO P2.6. Same applies to
`used_proportions` (TODO P2.7) and `used_color` for fancy diamonds where
`spec.color == UNKNOWN` and `fancy_color_full_name` is the only colour
signal.

### §87 expansion — `cert.color` is a key-drift hazard

The "white-only diamond search returns zero results" symptom is the same
class of bug as the fluorescence key-typo of §86.2 / §87. Single-key
`jget<std::string>(obj, "Color", "")` returns `""` for any feed that
spells the field `"Color Grade"`, `"ColorGrade"`, `"Colour"`, etc.
Every white diamond then imports with `mlot.color == ""` and is rejected
by `is_white_diamond_color()` for the rest of its life in the database.

**Rule:** every merchant-feed string field with semantic meaning to a
downstream filter (color, fancy color, fluorescence, shape, clarity, cut,
polish, symmetry) MUST use `jget_first` with an explicit candidate list,
even if the canonical key looks obviously correct. The audit list:
```bash
grep -n 'jget(obj,' src/update_db_main/update_db_main.cpp
```

— every hit on a downstream-filtered field is a §87 violation.
