# CLAUDE.md

rjk-duck is a C++26 type-erasure library built on static reflection (P2996). It needs GCC trunk with `-freflection`.

## Layout

- `src/`: library sources. Edit these.
- `include/rjk/duck.hpp`: **generated** single header, which the tests and users include. After editing `src/`, run `python3 scripts/single_header.py` and commit both. CI fails on drift.
- `src/generated/`: produced by `scripts/generate_operators.py`. Don't edit by hand.
- `docs/`: user docs. Update them in the same PR as any behaviour change.

## Building

```sh
cmake -G Ninja -B build -DCMAKE_CXX_COMPILER=<gcc-trunk>/bin/g++ -DRJK_DUCK_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --target rjk_duck_tests --parallel
ctest --test-dir build/tests --output-on-failure
```

- **Never compile with `-g`:** GCC trunk crashes on it. For Debug, pass `-DCMAKE_CXX_FLAGS_DEBUG=-O0`.
- **Compiler bugs:** known GCC trunk bugs are tracked on #28.
- **Builds are slow:** while iterating, compile only the test file you changed.

## Code and tests

- Follow `.clang-format` and match the surrounding code.
- Prefer brace initialization: `Widget w{1};`, `int count{};`, `return T{x};`.
- Every bug fix or feature comes with a test. Give each negative `static_assert` a nearby positive control, so it can't pass vacuously.
- TODOs reference an issue: `// TODO(#NN): ...`.

## Workflow

- **Branches and PRs:** one branch and PR per issue, named `<type>/<issue>-<slug>`. Put `Closes #N` in the body.
- **Labels:** add fitting labels to every issue and PR from the existing set (`gh label list`).
- **Scope:** if you find a separate problem, open an issue for it instead of fixing it in the same PR.
- **Ask first** before merging, force-pushing, pushing to `master`, or closing someone else's issue or PR.

## Writing

Keep PR descriptions, issues, code comments and GitHub comments short.
- **PRs:** what changed, why, and how it was tested.
- **Issues:** a minimal repro, the cause if known, and the expected behaviour.
- **Code comments:** explain *why*, not *what*.
