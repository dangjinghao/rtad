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

## Usage

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
