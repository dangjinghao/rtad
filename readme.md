# RTAD

A C library that lets a running program generate a copy of itself carrying its own data. The generated executable reads that data on its next launch and can generate another copy with updated data. Program and persistent data travel together as a single file, without a separate data file.

The core workflow is:

1. **Write a new self:** `rtad_copy_self_with_data` creates an executable copy containing the supplied data.
2. **Read self:** the generated program calls `rtad_extract_self_data` to read its embedded data.
3. **Replace self on Linux/macOS:** `rtad_set_self_data` replaces the executable at its existing path with an updated copy. Exit after success; the next launch reads the new data. Windows supports the copy-and-read workflow, but not in-place replacement.

CI covers Windows (MSVC/GCC/Clang), Linux and macOS (GCC/Clang), on x64 and ARM64.

## TODO list

- [ ] atomic file create/replace API

## Build and test

Requires CMake 3.14+ and a C compiler. Tests use cmocka, downloaded during configuration.

```sh
cmake -S . -B out/dev -DRTAD_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build out/dev --config Debug --parallel 2
(cd out/dev && ctest --output-on-failure -C Debug)
```

## API

Include [`rtad.h`](include/rtad.h) and link against `rtad`.

See [`rtad.h`](include/rtad.h) for full document.

## Examples

```sh
cmake -S example -B out/examples
cmake --build out/examples
```

[`example.c`](example/example.c) demonstrates the full cycle: the program generates a copy of itself with text, and that copy reads the text from its own executable. The copy can then generate another version with new text. With one destination argument it creates a clean copy, provided it already has a payload.

```sh
./out/examples/rtad_example out/examples/packed "Hello, RTAD!"
chmod +x out/examples/packed
./out/examples/packed
./out/examples/packed out/examples/updated "Updated data"
chmod +x out/examples/updated
./out/examples/updated
./out/examples/packed out/examples/clean
```

[`example2.c`](example/example2.c) demonstrates single-file persistence at the same path on Linux/macOS: it replaces its own executable with updated data and exits. Run it again without arguments to read the saved text:

```sh
./out/examples/rtad_example2 "Hello, RTAD!"
./out/examples/rtad_example2
```

Example 1 stores the terminating NUL; Example 2 stores only the text bytes. On Windows, use `.exe` filenames and omit `chmod`; executables may be under `Debug/` or `Release/`. Self-update is unsupported on Windows.

## Paths and update constraints

- Windows paths must be **UTF-8**, not ACP-encoded strings. RTAD converts them to UTF-16; invalid UTF-8 returns `RTAD_ERR_IO`. Executable lookup retains the Windows `MAX_PATH` limit. Linux/macOS use native filesystem bytes, normally UTF-8. Payloads are never transcoded.
- Copy operations overwrite destinations and may leave partial output on failure. Use a separate destination. `rtad_set_data` requires an existing file and is not atomic: a write failure can lose its previous payload.
- Prevent concurrent updates. After successful `rtad_set_self_data` or `rtad_truncate_self_data(NULL)`, exit promptly and restart; do not keep reading or updating through the old process.
- Self-update prepares a sibling temporary file before renaming it over the executable. Failures before rename preserve the original. The directory must be writable; owner, group and permission bits are preserved, but ACLs and extended attributes are not. Symlinks resolve to their targets; other hard links retain the old file. Crashes may leave temporary files, and replacement is not guaranteed durable across power loss.
- macOS updates do not re-sign executables or guarantee code-signature/Gatekeeper acceptance.
