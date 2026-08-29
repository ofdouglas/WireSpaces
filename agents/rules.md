# WireSpaces Coding Rules / Style Guide

**Background:** Embedded C++ for the WireSpaces messaging / networking stack — MCU firmware, host simulators, and agent-assisted development.

**Related:** General embedded C++ rules also live in `Design/Instructions/cpp_rules.md`. This file is the WireSpaces-specific view: repository layout, C++ core conventions, and agent workflow.

---

## Repository Layout

```
WireSpaces/
  core/         C++17 WS Core — packet header, router, dispatch, receivers
    *.h/*.cpp   Production headers and implementations
    test/       C++ GoogleTest + stub services (host-only)
  cpp/          Reusable embedded C++17 libraries (ported from Design/Firmware)
    foundation/ Span, Array, StaticString, …
    containers/ RingBuffer, …
    crc/        CRC algorithms
    <component>/test/  Optional per-component tests
    services/   Reusable firmware services (heartbeat, DID tables, …)
  platform/     Target-specific integration and toolchain compatibility
  sim/          Linux-hosted simulator shell
    *.h/*.cpp   Production headers and implementations
    test/       Simulator tests
  examples/     Buildable examples for supported targets
  tools/        Host-side tools and reusable Python package
  tests/        Repository-level integration and hardware tests
  agents/       Agent instructions (this file)
  docs/         Governed design documents
  sketches/     Experiment sketches
```

* **`core/`** — Native C++17 runtime for routing and local-domain delivery.
* **`cpp/`** — Header-first or component-directory C++ libraries. Include via `cpp/` on the include path (e.g. `#include <containers/ring_buffer.h>`).
* **`cpp/services/`** — Endpoint services and cross-cutting firmware features built on core + cpp.
* When creating **reusable** platform-independent code for WireSpaces, place it under `cpp/`, not back in `Design/Firmware`, unless the library is shared across multiple Design projects.

---

## Core C++ Rules (`core/`)

* Use C++17; core APIs live in the `wirespaces` namespace.
* No dynamic allocation, exceptions, or RTTI.
* Production headers and implementations live directly in `core/`; tests and
  test support live in `core/test/`.
* Keep packet processing non-templated. `WS_PACKET_BUFFER_DEFINE` may define
  fixed trailing storage while routers and dispatchers operate on `PacketBuffer`.
* Prefer fixed-size tables, spans, strongly typed identifiers, and small
  interfaces such as `EndpointReceiver` and `PacketForwarder`.
* Synchronization belongs inside concrete receiver implementations, not in the
  dispatcher or `EndpointReceiver` interface.

---

## C++ Rules

* Use `-std=c++17` by default.

* Write code suitable for MCUs by default:
  - No dynamic allocation (except during 1-time init at boot)
  - No exceptions
  - No RTTI
  - No std lib components that use any of the above

* Prefer conservative / safety-minded implementation choices by default: compliance with standards like MISRA or AUTOSAR C++ rules is desirable.

* The MCU platform code should be robust, simple, and portable to various hardware, OS (RTOS or bare-metal), and application types.

* Embedded C++ Abstractions:
 - Use structs for basic data containers that have at most a few simple methods (like isValid(), serialize(), deserialize(), clear(), etc).
 - Use classes (all members private) for anything more complex than a basic data container.
 - Use namespaces, but only a few layers at most. Avoid having things in the global namespace.
 - Use basic templates freely but be conservative about complex template expressions or definitions.
 - For large template classes in headers, prefer declarations in the class body and out-of-line definitions later in the same file (see Header Files).
 - Use virtual methods or pure interfaces where appropriate. Example: CanDriverInterface (for portability and mockability)
 - Inheritance hierarchies must be very simple and at most several layers deep. No multi-inheritance.
 - Link-seam injection is also an option.
 - Be conservative with the preprocessor.
 - Prefer brace initialization: `uint32_t value{0x3FFU};`

* Code that is strictly on-host (e.g. unit tests in `core/test/` or `cpp/*/test/`) may use any available C++ features.

---

## File Organization

* Use either of these structures for **software component** directories under `cpp/`:

  **Simple component** (e.g. `cpp/crc/`):
  ```
  cpp/component_name/
    test/           {test_component.cpp, optional mocks / test_infra}
    *.h / *.cpp
  ```

  **Complex component** (e.g. `services/bootloader/`):
  ```
  services/component_name/
    test/
    subdirA/        {several .cpp or .h files}
    subdirB/        {more C++, Python, data, scripts, …}
    working.md      optional notes
  ```

* **`core/`** follows the same flat production-file rule, with tests under
  `core/test/`.

* Do not commit CMake build trees (`build/`, `build_core/`, `_deps/`). See each subtree's `.gitignore`.

---

## Naming Conventions

| Kind | Style |
|------|--------|
| C functions / struct members | `snake_case` |
| C macro constants | `WS_ALL_CAPS` |
| C++ namespaces | `snake_case` |
| Enums / classes | `PascalCase` |
| Class methods | `camelCase` |
| Locals / struct members | `snake_case` |
| Class private members | `snake_case_` |
| Enum values | `kEnumValue` |

WireSpaces uses `HostId` for canonical host identity and `EndpointAddress` for the packed Namespace/Endpoint dispatch key.

---

## Documentation Comments

* Don't delete comments or TODOs when editing or refactoring code, unless you are resolving the TODO or the comment is clearly obsolete (and then update or replace it—don't leave a silent gap).
* Refactors preserve intent, not just behavior. When moving, renaming, or splitting code, carry forward existing comments, TODOs, `@brief` blocks, pre/post notes, and diagram-style comments (e.g. "Before: A→B"). Structural cleanup is not permission to drop narrative the author left on purpose.
* Relocate, don't discard. If a comment applied to an implementation body, move it to the out-of-line definition (or to the declaration if it documents the public contract). If a comment applied to the API surface, keep it on the declaration even when bodies move below the class.
* TODOs are part of the design record. Keep `TODO` / `@todo` unless the work is done in the same change (then remove or replace with a short note of what was decided). Don't "clean up" open TODOs without explicit intent.
* When merging or deduplicating comments, keep the more specific version (contract, edge cases, warnings). If two comments conflict after a refactor, resolve the conflict in text—don't delete both.
* Refactoring should improve structure and readability without erasing design history embedded in comments.
* All classes and non-trivial methods definitions should have a doxygen comment:
  - Concise description of what it is for.
  - Documentation of the API contract (in/out params, preconditions, thread-safety concerns, etc, where relevant)
  - You can write more prose if there are non-obvious things to explain, but don't document the obvious.
* Anything in implementation code that is subtle or unclear should have some documentation. But prefer self-documenting code if possible.
* Example style:
```c++

// CRC algorithm name string
// TODO: detect truncation / name overflow
static constexpr size_t kMaxNameLength{16U};
using NameString = StaticString<kMaxNameLength>;

/**
 * @brief Calculate the CRC of a given input data using the bitwise software algorithm.
 *
 * @tparam    CrcAlgorithm Class type defining a specific CRC algorithm
 * @param[in] input        The input data to calculate the CRC of.
 * @return    The CRC of the input data.
 *
 * @todo Handle reflect_in and reflect_out
 * @todo Support incremental processing (update, ... finalize)
 */
 template <typename CrcAlgorithm>
 typename CrcAlgorithm::value_type crcBitwise(util::Span<const uint8_t> input) { ... }
```

---

## Refactoring

* Before finishing a refactor, check that comments and TODOs are still present where they apply (or were intentionally updated). Restoring or relocating lost notes is part of "done."
* Behavior changes and bug fixes don't justify a comment purge. Update wording where intent changed; remove only what is wrong or redundant.
* Avoid whitespace-only churn that inserts or removes a blank line on every line—it's hard to review and makes accidental comment loss easy to miss. Prefer one coherent layout pass.
* When porting libraries from `Design/Firmware` to `WireSpaces/cpp/`, preserve behavior and comments; update include paths to the `cpp/` layout.

---

## Header Files

* Each header file should start with a brief documentation comment and a `#pragma once`:
```c++
    /*
     * @file  ostream_helpers.h
     * @brief Helpers for std::ostream printing, for use in host-side unit tests.
     * @note  This file is not intended for use in production code.
     */

    #pragma once
```

* Prefer **declarations first, definitions after** so readers can scan the API without wading through bodies.
  - In a class (especially templates), put types, data members, and method **declarations** in the class body.
  - Put **out-of-line definitions below the class** in the same header when needed (typical for templates), grouped under a clear banner (e.g. implementations section). Match declaration order when practical.
  - **Inline in the class only when trivial**—one-line accessors, simple `constexpr` helpers, or obvious forwarding. "Trivial" means the reader isn't helped by scrolling past it, not merely "short."
  - **Public contract on the declaration** (doxygen: behavior, preconditions, thread-safety). **Implementation detail on the definition** (algorithm steps, invariants, non-obvious edge cases).
  - For non-template code, prefer `component.h` (API) + `component.cpp` (bodies) per File Organization.
```c++
class Foo {
public:
    void bar();  // contract / @brief here
private:
    void barImpl();
};

// --- Foo implementations ---
inline void Foo::bar() { barImpl(); }
```

---

## Unit Tests (Google Test)

### Planning and traceability

* What to test: happy path, expected errors / misuse, edge cases, boundary values, adversarial inputs, and equivalence classes (group inputs that should behave the same).
* Write a concise comment block at the top of each test file listing modules covered and equivalence classes exercised.
* Each `TEST` / `TEST_F` / `TEST_P` must have a **1–2 line preceding comment** stating what it verifies. Optimize for signal-to-noise in the test body—readers should see intent before assertions.

### Structure and architecture

* **Refactor common setup** into fixtures, builders, and small support types (`test/support/` in `core/`). Test bodies should focus on arrange → act → assert, not boilerplate.
* When test infrastructure is **non-trivial** (fixtures spanning modules, recorders, spies, packet builders), treat it as real software architecture: headers in `test/support/`, clear names, single responsibility—not a blob of helpers at the bottom of one `.cpp`.
* Use **parameterized tests** for equivalence classes (enum values, boundary ids, bool flags) instead of copy-pasted cases.
* Integration tests (e.g. hello-world sender → router → dispatch → mailbox) stay separate from narrow unit tests per module.

### Style

* Try to avoid very long and dense `TEST()` definitions.
* Prefer constants and builders over magic numbers in test bodies.
* Helper structs and free functions can be lightly documented; coherence still matters.
* Host-side tests may use full C++17; production MCU code rules still apply to stubs that ship on-target.

### Design process

0. (New test file): Stub test + build target first.
1. Test planning: list cases and equivalence classes.
2. Infrastructure design when fixtures/builders genuinely reduce duplication.
3. Execute and iterate.

**WireSpaces:** Core unit tests live in `core/test/` with shared code in `core/test/support/`. Component tests live under `cpp/<component>/test/` or `services/<component>/test/` when added.

---

## Build

* **Core:** from `core/build_core` (or `core/build`):
  ```bash
  cmake .. -DWIRESPACES_BUILD_CORE_TESTS=ON
  cmake --build .
  ctest --output-on-failure -R wirespaces_core_tests
  ```
* Do not `git add` build directories or FetchContent `_deps/` trees.

---

## Agents

* Read this file before creating or modifying WireSpaces C/C++ code.
* For C++-only work in other Design repos, `Design/Instructions/cpp_rules.md` remains the canonical rules file.
* Do not edit governed docs in `docs/` unless explicitly asked; prototype code in `core/`, `cpp/`, and `services/` may diverge until promoted.
