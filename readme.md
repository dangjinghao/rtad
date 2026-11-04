# RTAD

A cross-platform library for appending data to executable files and reading it back at runtime. The executable remains fully functional after data is appended to its tail.

## Tested Platforms
- Windows+MSVC x64/arm64
- Windows+gcc/clang x64/arm64
- Linux+gcc/clang x64/arm64
- macOS+gcc/clang x64/arm64

## TODO List
- [x] Github workflow automatically test
- [ ] non-ascii file path test and support

## Error handling

Status-returning public functions return `int`: `RTAD_OK` (0) means success, and
negative values are the `rtad_error` constants declared in `include/rtad.h`.
`rtad_error_string(code)` returns a static English description for logging. Do
not free or modify it; unknown codes return `"Unknown RTAD error"`.

| Code | Meaning |
| --- | --- |
| `RTAD_ERR_INVALID_ARGUMENT` (-1) | Invalid pointer or size argument |
| `RTAD_ERR_IO` (-2) | File operation failed, including a missing file or failed flush |
| `RTAD_ERR_NO_MEMORY` (-3) | Allocation failed |
| `RTAD_ERR_INVALID_FORMAT` (-4) | RTAD magic exists but its data length is invalid |
| `RTAD_ERR_NO_DATA` (-5) | File is too short for a trailer or has no RTAD magic |
| `RTAD_ERR_TOO_LARGE` (-6) | Payload exceeds `UINT32_MAX` bytes |
| `RTAD_ERR_EXE_PATH` (-7) | Executable path lookup or conversion failed |
| `RTAD_ERR_BUFFER_TOO_SMALL` (-8) | Executable path buffer is too small |
| `RTAD_ERR_NOT_SUPPORTED` (-9) | Operation is not supported on this platform |

Use `result != RTAD_OK` to detect any failure; checks for `result == -1` must be
updated. Internal errors are propagated to the public caller. File operations
can leave incomplete output on failure.

Extraction and validation return `RTAD_ERR_NO_DATA` for files without appended
data. Truncation treats that case as success and leaves the file unchanged;
I/O failures and malformed RTAD lengths are reported as errors. The copy API
rejects oversized payloads before modifying the destination.

```c
int result = rtad_extract_self_data(&data, &data_size);
if (result == RTAD_ERR_NO_DATA) {
    /* Normal executable without bundled data. */
} else if (result != RTAD_OK) {
    /* Handle the specific failure. */
} else {
    /* Use data, then call rtad_free_extracted_data(data). */
}
```

## Usage

To add or replace data on an existing file, use `rtad_set_data(path, data, size)`.
It checks the existing trailer, removes a valid old data block and trailer, then
appends the new data and trailer. Repeated calls replace the payload rather than
stacking data blocks. Missing files, malformed trailers, invalid arguments and
oversized payloads are rejected before modification. The operation is in-place:
if writing fails after truncation, the old payload is not restored. Prevent
concurrent changes to the file while calling this API.

`rtad_get_data_size(path, &size)` validates the trailer and returns the payload
size without reading or allocating the payload. It returns `RTAD_ERR_NO_DATA`
when no trailer exists and leaves `size` unchanged on any error.

`rtad_set_self_data(data, size)` updates the executable at the current
process's path on Linux/macOS, replacing any existing RTAD payload.
It builds a new file in the original directory,
checks and syncs it, closes it, and uses `rename` to replace the original path.
It never writes to or unlinks the running executable before the replacement is
ready. An error before replacement leaves the original file intact; temporary
files are cleaned up on normal error paths. A crash can leave a temporary file.

The API preserves owner, group and permission bits. It fails if these cannot be
preserved or the containing directory is not writable. ACLs and extended
attributes (including capabilities and security labels) are not copied; use it
only where these are not required. Symlinks are resolved to the executable's
actual path; other hard links continue to reference the old file. The temporary
file is synced, but the containing directory is not synced after replacement,
so this is not a guarantee against power loss.

The caller must prevent concurrent updates to the same executable or its path,
including updates by other processes. No locking is provided. After success,
exit promptly; the next launch uses the new file. Do not repeat this API or rely
on self-reading APIs in the old process. Windows returns
`RTAD_ERR_NOT_SUPPORTED` for valid arguments; no helper-process or deferred
replacement mechanism is provided. On macOS, this API does not re-sign the
binary or guarantee code-signature/Gatekeeper acceptance.

```c
int result = rtad_set_data("application", "hello", 5);
if (result != RTAD_OK) {
    fprintf(stderr, "%s\n", rtad_error_string(result));
}
```

For some special reasons, I have to code a binary executable program, but I need to pack some data files together with it. RTAD can help me achieve this goal easily.

1. `rtad_copy_self_with_data`: Copy current executable file to a new path with appending data files.
2. Deliver or save the new generated executable file.
3. `rtad_extract_self_data`: At runtime, read back the appended data from the executable itself.

### Example 1: create and read executable copies

`example/example.c` demonstrates copying the current executable with bundled
text, reading that text on launch, and creating a copy with the data removed.
The executable being run is left unchanged; output paths are overwritten if
they already exist, so choose separate destination paths.

| Arguments | Behavior | API |
| --- | --- | --- |
| None | Read and print this executable's appended data | `rtad_extract_self_data` |
| `<new_exe_path> <text>` | Create an executable copy with the supplied text replacing any existing payload | `rtad_copy_self_with_data` |
| `<clean_exe_path>` | Print the current data, then create a copy without it | `rtad_truncate_self_data` |

The one-argument mode creates a copy only when the current executable has
appended data. Otherwise it prints `No appended data found.` and usage help.
This example stores the text's terminating NUL byte, so `Hello, RTAD!` occupies
13 bytes; Example 2 stores only the 12 text bytes.

```sh
cmake -S example -B out/examples
cmake --build out/examples

./out/examples/rtad_example
# No appended data found.
./out/examples/rtad_example out/examples/packed "Hello, RTAD!"
chmod +x out/examples/packed
./out/examples/packed
# Extracted data(13): Hello, RTAD!

./out/examples/packed out/examples/clean
# Prints the data and creates a copy without the appended payload.
chmod +x out/examples/clean
./out/examples/clean
# No appended data found.
```

The commands above use Unix shell syntax. On Windows, use `.exe` filenames and
omit `chmod`; multi-configuration generators may place `rtad_example.exe` under
`out/examples/Debug` or `out/examples/Release`.

### Example 2: update this executable

`example/example2.c` demonstrates `rtad_set_self_data`. With one non-empty text
argument it replaces its own stored data and exits. Without arguments it reads
the data stored by the previous run.

```sh
cmake -S example -B out/examples
cmake --build out/examples

./out/examples/rtad_example2
# No appended data found.
./out/examples/rtad_example2 "Hello, RTAD!"
./out/examples/rtad_example2
# Extracted data(12): Hello, RTAD!
./out/examples/rtad_example2 "Updated"
./out/examples/rtad_example2
# Extracted data(7): Updated
```

The executable and its directory must allow replacement. Run updates serially.
The example exits after updating and reads the new data only on a later launch.
Windows builds the example, but updating reports `RTAD_ERR_NOT_SUPPORTED`.
The macOS signature restrictions described above also apply. Rebuilding may
replace the executable and discard the stored data.
