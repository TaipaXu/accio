# Accio

Drop-in file transfer server for local networks, offering a modern web UI for quick uploads and downloads.

English | [中文](./README_ZH.md)

![accio](./accio.gif)

## Installation

### Ubuntu

```sh
sudo add-apt-repository ppa:taipa-xu/stable
sudo apt update
sudo apt install accio
```

#### Debian Packages

Prebuilt `.deb` archives are also available on the GitHub Releases page. Download the package that matches your Ubuntu version and architecture, then install it with `sudo apt install ./<package-name>.deb`.

| Ubuntu Version | Architecture | Download |
| -------------- | ------------ | -------- |
| 22.04          | amd64        | [accio_0.0.1-0ppa1.ubuntu22.04_amd64.deb](https://github.com/TaipaXu/accio/releases/download/v0.0.1/accio_0.0.1-0ppa1.ubuntu22.04_amd64.deb) |
| 22.04          | arm64        | [accio_0.0.1-0ppa1.ubuntu22.04_arm64.deb](https://github.com/TaipaXu/accio/releases/download/v0.0.1/accio_0.0.1-0ppa1.ubuntu22.04_arm64.deb) |
| 24.04          | amd64        | [accio_0.0.1-0ppa1.ubuntu24.04_amd64.deb](https://github.com/TaipaXu/accio/releases/download/v0.0.1/accio_0.0.1-0ppa1.ubuntu24.04_amd64.deb) |
| 24.04          | arm64        | [accio_0.0.1-0ppa1.ubuntu24.04_arm64.deb](https://github.com/TaipaXu/accio/releases/download/v0.0.1/accio_0.0.1-0ppa1.ubuntu24.04_arm64.deb) |
| 25.04          | amd64        | [accio_0.0.1-0ppa1.ubuntu25.04_amd64.deb](https://github.com/TaipaXu/accio/releases/download/v0.0.1/accio_0.0.1-0ppa1.ubuntu25.04_amd64.deb) |
| 25.04          | arm64        | [accio_0.0.1-0ppa1.ubuntu25.04_arm64.deb](https://github.com/TaipaXu/accio/releases/download/v0.0.1/accio_0.0.1-0ppa1.ubuntu25.04_arm64.deb) |
| 25.10          | amd64        | [accio_0.0.1-0ppa1.ubuntu25.10_amd64.deb](https://github.com/TaipaXu/accio/releases/download/v0.0.1/accio_0.0.1-0ppa1.ubuntu25.10_amd64.deb) |
| 25.10          | arm64        | [accio_0.0.1-0ppa1.ubuntu25.10_arm64.deb](https://github.com/TaipaXu/accio/releases/download/v0.0.1/accio_0.0.1-0ppa1.ubuntu25.10_arm64.deb) |

## Features

- Zero-config startup with a single command-line entry point
- Directory browser with one-click download links and upload support through the web UI
- Path normalization safeguards to keep requests inside the shared folder

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

## Usage

```sh
./build/release/src/accio <shared-directory>
```

- `accio -h`: print the available command-line options
- `accio -v`: show the version string
- `accio <path>`: start the HTTP server rooted at the given directory (defaults to the current directory when omitted)

When the server is running, open `http://localhost:13396` in your browser to browse folders, upload files into the current view, and download any listed file.

## License

[GPL-3.0](LICENSE)
