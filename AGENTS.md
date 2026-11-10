# Repository Guidelines

## Project Structure & Module Organization

RTAD is a C library for appending payloads to executables and reading them at runtime.

- `include/rtad.h`: public API and error codes.
- `src/rtad.c`, `src/rtad_def.h`: implementation and internal declarations.
- `test/unit_test.c`: direct function tests, including internal helpers.
- `test/integration_test.c`, `test/cjk_path_test.c`: public API, executable lifecycle, and Unicode path integration tests.
- `example/`: executable-copy and self-update examples.
- `cmake/PreCommit.cmake`: local build/test hook; `.github/workflows/test.yml`: cross-platform CI.
- `.devcontainer/`: development environment configuration. Keep generated builds under `out/`.

## Build, Test, and Development Commands

Use CMake 3.14+, a C compiler, Python, and pre-commit. Tests fetch cmocka 2.0.0 during initial configuration.

```sh
cmake -S . -B out/dev -DRTAD_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build out/dev --config Debug --parallel 2
(cd out/dev && ctest --output-on-failure -C Debug)
```

Tests default to OFF; explicitly enable them. Build examples with:

```sh
cmake -S example -B out/examples
cmake --build out/examples
```

Run `pre-commit install` to enable commit hooks and `pre-commit run --all-files` before submitting. Hooks format C/YAML and build/test in `out/pre-commit`. Python hook environments install their own formatter dependencies; CMake and the compiler must be available separately.

## Coding Style & Naming Conventions

Follow `.clang-format`: LLVM style, two-space indentation, 80-column limit, and preserved include ordering. YAML uses `.yamlfix.toml`. Prefer descriptive `snake_case`, public `rtad_` names, and explicit error-handling branches.

Declare internal helpers in `src/rtad_def.h` using `RTAD_PRIVATE`, which exposes them under `RTAD_TEST`. Reuse existing helpers. Prefer direct patches for focused edits over transformation scripts.

## Testing Guidelines

Use cmocka and name cases `test_<behavior>`. Place function-level tests in `unit_test.c`; keep subprocess scenarios in integration tests. Add regression coverage for changed behavior, failure paths, and preservation of existing file contents. No numerical coverage threshold is configured.

Keep CTest driver names ASCII; create CJK paths at runtime. Verify payloads byte-for-byte, including embedded NULs. Windows paths use UTF-8, converted internally to UTF-16. Report which platforms were actually tested; Linux success does not establish Windows/macOS compatibility.

## Commit & Pull Request Guidelines

Follow existing semantic prefixes: `fix:`, `feat:`, and `ci:`, with optional scopes such as `feat(pre-commit):`. Use `fix` for corrected behavior and `feat` for new capabilities.

PR descriptions should explain the problem, resulting behavior, relevant issues, and validation performed. Document API or encoding changes in `include/rtad.h` and `readme.md`. Keep unrelated changes separate and preserve existing user edits.
