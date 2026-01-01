# Accio

Drop-in file transfer server for local networks, offering a modern web UI for quick uploads and downloads.

English | [中文](./README_ZH.md)

![accio](./accio.gif)

## Features

- Zero-config startup with a single command-line entry point
- Directory browser with one-click download links and upload support through the web UI
- Path normalization safeguards to keep requests inside the shared folder

## Usage

```sh
./build/release/src/accio <shared-directory>
```

- `-h, --help`: show the full list of options
- `-v, --version`: print the version string
- `-p, --path <dir>`: set the shared root directory (defaults to current directory if omitted)
- `-u, --uploads <dir>`: set the uploads directory (defaults to `Downloads/accio` inside the shared root)
- `--host <addr>`: listening host (default `0.0.0.0`)
- `--port <number>`: listening port (default `13396`, use `0` for an ephemeral port)
- `--password[=<value>]`: enable password protection; omit value to generate one. Default: no password.
- `--enable-upload=<on|off>`: enable or disable uploads (default `on`; use `off` to disable the upload feature)
- `--allow-exts <ext...>`: allow only these extensions (e.g., `.txt .pdf`); cannot be combined with `--deny-exts`
- `--allow-files <path...>`: allowlisted files (relative to the shared root); can be combined with `--allow-exts` or deny options
- `--deny-exts <ext...>`: block these extensions; cannot be combined with `--allow-exts`
- `--deny-files <path...>`: blocklisted files (relative to the shared root); can be combined with `--deny-exts` or allow options

Filtering priority: `deny-files` > `allow-files` > `deny-exts` > `allow-exts`. File paths for allow/deny lists must be relative to the shared root.

Examples:

- Serve the current directory: `accio`
- Serve `/data/shared` with the upload feature disabled: `accio -p /data/shared --enable-upload=off`

## Dependencies

Before building from source, install [vcpkg](https://github.com/microsoft/vcpkg) and set the `VCPKG_ROOT` environment variable to the directory where `vcpkg` is located. Add the `vcpkg` executable to your `PATH`.

### Ubuntu

Install [CMake](https://cmake.org) and the `build-essential` toolchain:

```sh
sudo apt update
sudo apt install build-essential cmake
```

### macOS

Install the Xcode Command Line Tools, then use [Homebrew](https://brew.sh) to install CMake and `pkg-config` so vcpkg can locate zlib and other dependencies:

```sh
xcode-select --install
brew install cmake pkg-config
```

### Windows

Install [Visual Studio 2022](https://visualstudio.microsoft.com) with the "Desktop development with C++" workload. Ensure `cmake` is on your `PATH`, and run the build commands from the "x64 Native Tools Command Prompt for VS 2022" (or an equivalent developer PowerShell prompt) so the MSVC toolchain is configured.

## Build from Source

```sh
git clone https://github.com/TaipaXu/accio.git
cd accio
vcpkg install
cmake --preset=unix-release
cmake --build build/release -j $(nproc)
```

## License

[GPL-3.0](LICENSE)
