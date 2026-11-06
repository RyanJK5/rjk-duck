# CLAUDE.md

rjk-duck is a C++26 type-erasure library built on static reflection (P2996). It needs GCC trunk with `-freflection`.

## Layout

- `src/`: library sources. Edit these.
- `include/rjk/duck.hpp`: **generated** single header, which the tests and users include. After editing `src/`, run `python3 scripts/single_header.py` and commit both. CI fails on drift (`--check`).
- `src/generated/`: produced by `scripts/generate_operators.py`. Don't edit by hand.
- `tests/`: GoogleTest. Negative conformance checks go in `tests/test_rejection.cpp`, compile-time checks in `tests/test_constexpr.cpp`.
- `docs/`: user docs. Update them in the same PR as any behaviour change.

## Building

```sh
cmake -G Ninja -B build -DCMAKE_CXX_COMPILER=<gcc-trunk>/bin/g++ -DRJK_DUCK_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --target rjk_duck_tests --parallel
ctest --test-dir build/tests --output-on-failure
```

- **Never compile with `-g`.** GCC trunk crashes on it. For Debug builds pass `-DCMAKE_CXX_FLAGS_DEBUG=-O0`.
- **Sanitizers:** `-DRJK_DUCK_SANITIZE=ON`. Some UBSan checks are disabled because of GCC bugs.
- **GCC trunk bugs:** known ones are tracked on #28. Prefer a small code workaround over fighting the compiler, and note new ones there.
- **Builds are slow.** While iterating, compile only the test file you changed. Run the full build once before pushing.

## Code

- Formatting follows `.clang-format`.
- **Lambdas:** call them directly, `[] consteval { ... }()` for compile-time-only bodies. Don't use `std::invoke([] { ... })`.
- **Compile-time evaluation:** calling trait members through a duck, view or pointer isn't supported (#65). Only construction and lifetime operations work.
- **TODOs** reference an issue: `// TODO(#NN): ...`.

## Tests

- Every bug fix or feature comes with a test.
- Each negative `static_assert` needs a nearby positive control, so it can't pass vacuously.
- Tests must not rely on undefined behaviour. Asserts catch it in Debug builds.
- Guard death tests with `#if !defined(NDEBUG) && GTEST_HAS_DEATH_TEST`.

## Workflow

- **Branches and PRs:** one branch and PR per issue, named `<type>/<issue>-<slug>`, e.g. `fix/75-duck-ptr-const-return`. Put `Closes #N` in the body. PRs are squash-merged.
- **Labels:** add fitting labels to every issue and PR from the existing set (`gh label list`). Don't invent new ones.
- **Scope:** stay within the issue. When you find a separate problem, open a new issue for it instead of fixing it in the same PR.
- **Ask first** before merging, force-pushing, pushing to `master`, or closing someone else's issue or PR. API changes get the `change` label.
- **Editing PR bodies:** `gh pr edit` fails on this repo (Projects classic error). Use `gh api -X PATCH repos/RyanJK5/rjk-duck/pulls/<n> -F body=@file`.

## Writing

Keep everything short: PR descriptions, issues, code comments and GitHub comments.
- **PRs:** what changed, why, and how it was tested. Nothing more.
- **Issues:** a minimal repro, the cause if known, and the expected behaviour.
- **Code comments:** explain *why*, not *what*.
