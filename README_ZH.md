# Accio

一个开箱即用的局域网文件传输小工具，提供简洁的网页界面，可在浏览器里快速上传、下载文件。

[English](./README.md) | 中文

![accio](./accio.gif)

## 安装

### Ubuntu

```sh
sudo add-apt-repository ppa:taipa-xu/stable
sudo apt update
sudo apt install accio
```

#### Debian 包

也可直接从 GitHub Releases 下载预构建的 `.deb` 安装包。请根据 Ubuntu 版本与架构选择对应文件，并使用 `sudo apt install ./<package-name>.deb` 进行安装。

| Ubuntu 版本 | 架构 | 下载 |
| ----------- | ---- | ---- |
| 22.04       | amd64 | [accio_0.0.1-0ppa1.ubuntu22.04_amd64.deb](https://github.com/TaipaXu/accio/releases/download/v0.0.1/accio_0.0.1-0ppa1.ubuntu22.04_amd64.deb) |
| 22.04       | arm64 | [accio_0.0.1-0ppa1.ubuntu22.04_arm64.deb](https://github.com/TaipaXu/accio/releases/download/v0.0.1/accio_0.0.1-0ppa1.ubuntu22.04_arm64.deb) |
| 24.04       | amd64 | [accio_0.0.1-0ppa1.ubuntu24.04_amd64.deb](https://github.com/TaipaXu/accio/releases/download/v0.0.1/accio_0.0.1-0ppa1.ubuntu24.04_amd64.deb) |
| 24.04       | arm64 | [accio_0.0.1-0ppa1.ubuntu24.04_arm64.deb](https://github.com/TaipaXu/accio/releases/download/v0.0.1/accio_0.0.1-0ppa1.ubuntu24.04_arm64.deb) |
| 25.04       | amd64 | [accio_0.0.1-0ppa1.ubuntu25.04_amd64.deb](https://github.com/TaipaXu/accio/releases/download/v0.0.1/accio_0.0.1-0ppa1.ubuntu25.04_amd64.deb) |
| 25.04       | arm64 | [accio_0.0.1-0ppa1.ubuntu25.04_arm64.deb](https://github.com/TaipaXu/accio/releases/download/v0.0.1/accio_0.0.1-0ppa1.ubuntu25.04_arm64.deb) |
| 25.10       | amd64 | [accio_0.0.1-0ppa1.ubuntu25.10_amd64.deb](https://github.com/TaipaXu/accio/releases/download/v0.0.1/accio_0.0.1-0ppa1.ubuntu25.10_amd64.deb) |
| 25.10       | arm64 | [accio_0.0.1-0ppa1.ubuntu25.10_arm64.deb](https://github.com/TaipaXu/accio/releases/download/v0.0.1/accio_0.0.1-0ppa1.ubuntu25.10_arm64.deb) |

## 特性

- 无需配置，一条命令即可启动共享目录
- 网页端可视化浏览目录，支持文件上传与下载
- 路径规范化校验，确保访问受限在共享目录之内

## 使用方法

```sh
./build/release/src/accio <共享目录>
```

- `-h, --help`：查看所有命令行参数
- `-v, --version`：显示版本信息
- `-p, --path <目录>`：设置共享根目录（未指定时默认当前目录）
- `-u, --uploads <目录>`：指定上传文件存放目录（默认使用共享目录下的 `Downloads/accio`）
- `--host <地址>`：监听地址（默认 `0.0.0.0`）
- `--port <端口>`：监听端口（默认 `13396`，传入 `0` 可使用系统分配端口）
- `--password[=<密码>]`：开启访问密码；不填值则随机生成。默认无密码。
- `--enable-upload=<on|off>`：开启或关闭上传功能（默认 `on`；传 `off` 关闭上传功能）
- `--allow-exts <扩展名...>`：仅允许这些扩展名（如 `.txt .pdf`）；不可与 `--deny-exts` 同时使用
- `--allow-files <路径...>`：允许的文件名单（相对共享根目录）；可与 `--allow-exts` 或禁用类选项组合
- `--deny-exts <扩展名...>`：阻止这些扩展名；不可与 `--allow-exts` 同时使用
- `--deny-files <路径...>`：阻止的文件名单（相对共享根目录）；可与 `--deny-exts` 或允许类选项组合

过滤优先级：`deny-files` > `allow-files` > `deny-exts` > `allow-exts`。文件名单需使用相对共享根目录的路径。

示例：

- 共享当前目录：`accio`
- 共享 `/data/shared` 并关闭上传功能：`accio -p /data/shared --enable-upload=off`

## 依赖

从源码构建前，请安装 [vcpkg](https://github.com/microsoft/vcpkg)，并设置 `VCPKG_ROOT` 环境变量指向安装目录，同时把 `vcpkg` 可执行程序加入 `PATH`。

### Ubuntu

安装 [CMake](https://cmake.org) 以及 `build-essential` 工具链：

```sh
sudo apt update
sudo apt install build-essential cmake
```

### macOS

安装 Xcode Command Line Tools，然后通过 [Homebrew](https://brew.sh) 安装 CMake 和 `pkg-config`，以便 vcpkg 能够正确检测 zlib 等依赖：

```sh
xcode-select --install
brew install cmake pkg-config
```

### Windows

安装 [Visual Studio 2022](https://visualstudio.microsoft.com)，勾选“使用 C++ 的桌面开发”工作负载。确认 `cmake` 已在 `PATH` 中，并在 “x64 Native Tools Command Prompt for VS 2022” 或开发者 PowerShell 中执行构建命令，以便加载 MSVC 工具链。

## 从源码编译

```sh
git clone https://github.com/TaipaXu/accio.git
cd accio
vcpkg install
cmake --preset=unix-release
cmake --build build/release -j $(nproc)
```

## 协议

[GPL-3.0](LICENSE)
