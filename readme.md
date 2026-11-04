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

Here is a simple example in example directory.

1. Compile it with cmake.
2. Run `./rtad_example`, it prints "No appended data found."
3. Run `./rtad_example b.out "Hello, RTAD!"` to create a new executable file `b.out` with appended data.
4. You may need to do `chmod +x b.out` on Unix-like systems.
5. Run `./b.out`, it prints `Extracted data(13): Hello, RTAD!`
