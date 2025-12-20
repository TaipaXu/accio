# Accio

一个开箱即用的局域网文件传输小工具，提供简洁的网页界面，可在浏览器里快速上传、下载文件。

[English](./README.md) | 中文

![accio](./accio.gif)

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
