# Windows 交叉编译问题分析：从 Linux 构建 SDL2 游戏的 Windows 可执行文件

## 一、问题背景

本项目 `CountDownUntilDaybreak` 是基于 SDL2 的 C 语言游戏，开发环境为 Linux，需在不修改任何源码与 CMakeLists.txt 的前提下，从 Linux 交叉编译出可在 Windows x64 运行的发行包（含 exe + DLL + 资源）。

### 1.1 项目依赖与构建系统

| 项目 | 内容 |
|------|------|
| 语言标准 | C11 |
| 构建系统 | CMake 3.10+ |
| 依赖库 | SDL2、SDL2_image、SDL2_mixer、SDL2_ttf |
| 第三方单头库 | `cute_tiled.h`（已内嵌并修改） |
| 资源 | `assets/` 全部使用相对路径（如 `assets/maps/menu.tmj`） |
| 开发平台 | Linux |
| 目标平台 | Windows x64 |

### 1.2 关键约束

代码中所有资源路径均为**相对路径**（[src/ui.c#L71](../src/ui.c#L71)、[src/audio.c#L8](../src/audio.c#L8)、[src/game_state.c#L35](../src/game_state.c#L35) 等），运行时依赖 cwd（当前工作目录）。因此最终发行包必须是「exe + 同级 assets 目录」结构，运行时 cwd 必须是 exe 所在目录。

### 1.3 目录布局

```
CountDownUntilDaybreak/
├── CMakeLists.txt              # 项目构建配置（不修改）
├── cmake/
│   └── toolchain-mingw.cmake   # 交叉编译工具链文件
├── x86_64-w64-mingw32/         # 项目内嵌的 SDL2 Windows 开发包
│   ├── bin/                    # SDL2.dll / SDL2_image.dll / ...
│   ├── include/SDL2/           # SDL2 头文件
│   └── lib/                    # libSDL2.dll.a / libSDL2main.a / ...
├── src/                        # 源码
├── include/                    # 头文件
├── assets/                     # 资源文件
└── build-windows/              # 交叉编译输出目录
```

---

## 二、遇到的问题与解决

交叉编译过程中遇到两个关键链接错误。下面分别分析原因并给出解决思路。

### 2.1 问题一：`undefined reference to 'WinMain'`

#### 错误信息

```
/usr/bin/x86_64-w64-mingw32-ld: libmingw32.a(lib64_libmingw32_a-crtexewin.o):
in function `main':
crtexewin.c:67:(.text.startup+0xbd): undefined reference to `WinMain'
collect2: error: ld returned 1 exit status
```

#### 根因分析

**1. SDL_main 宏劫持机制**

[src/main.c#L4](../src/main.c#L4) `#include <SDL2/SDL.h>` 会自动引入 `SDL_main.h`，其内部有：

```c
#define main SDL_main
```

这会把用户的 `int main(int argc, char *argv[])` 宏重命名为 `int SDL_main(int argc, char *argv[])`。

**2. Windows GUI 程序入口点**

在 Windows GUI 模式下（mingw 链接 `-mwindows`），C 运行时入口点是 `WinMainCRTStartup`（来自 `libmingw32.a`），它调用 `WinMain`。但用户代码中并没有 `WinMain`，只有被宏改名的 `SDL_main`。

**3. libSDL2main.a 的作用**

`libSDL2main.a` 提供 `WinMain` 函数，它的作用是：
- 初始化 SDL
- 调用 `SDL_GetCmdLine()` 收集命令行参数
- 调用用户的 `SDL_main`（即被宏重命名的 `main`）
- 退出时调用 `SDL_Quit()`

**4. 为什么 Linux 上没问题**

[CMakeLists.txt#L25-L31](../CMakeLists.txt#L25-L31) 只链接了 `SDL2::SDL2`，没有显式链接 `SDL2::SDL2main`：

```cmake
target_link_libraries(${PROJECT_NAME}
  SDL2::SDL2
  SDL2_image::SDL2_image
  SDL2_mixer::SDL2_mixer
  SDL2_ttf::SDL2_ttf
  m
)
```

在 Linux 上 `libSDL2main.a` 是**空库**（Linux 不需要 `WinMain` 入口转换），所以一直没暴露这个问题。交叉编译到 Windows 时，`libSDL2main.a` 是必需的，否则 `WinMain` 无定义。

#### 解决方案

**不修改 CMakeLists.txt**，通过 `CMAKE_EXE_LINKER_FLAGS` 命令行参数注入 `libSDL2main.a`：

```bash
-DCMAKE_EXE_LINKER_FLAGS="\
  -static-libgcc -static-libstdc++ \
  -L$(pwd)/x86_64-w64-mingw32/lib \
  -Wl,--whole-archive -lSDL2main -Wl,--no-whole-archive"
```

**关键点：`--whole-archive` 的作用**

`libSDL2main.a` 是**静态库**，ld 默认只从中提取**当前未定义符号**对应的对象文件。但 `WinMain` 在链接开始时不是未定义的（它是入口点），ld 不会主动从 `libSDL2main.a` 中提取它。

`--whole-archive` 强制把整个静态库的所有对象文件都包含进来，确保 `WinMain` 被链接。`--no-whole-archive` 恢复默认行为，避免后续库被全包含。

### 2.2 问题二：try_compile 链接失败

#### 错误信息

```
/usr/bin/x86_64-w64-mingw32-ld: libSDL2main.a(SDL_windows_main.o):
in function `main_getcmdline':
SDL_windows_main.c:66: undefined reference to `SDL_strlen'
SDL_windows_main.c:71: undefined reference to `SDL_memcpy'
...
CMake will not be able to correctly generate this project.
-- Configuring incomplete, errors occurred!
```

#### 根因分析

CMake 在 `project()` 命令处会执行 `try_compile` 测试编译器是否可用。`try_compile` 会：
1. 用 `CMAKE_C_FLAGS` 和 `CMAKE_EXE_LINKER_FLAGS` 编译一个简单的 `testCCompiler.c`
2. 尝试链接成可执行文件

由于我们通过 `CMAKE_EXE_LINKER_FLAGS` 注入了 `-Wl,--whole-archive -lSDL2main`，`try_compile` 也会用这个 flag 链接。但 `try_compile` 的测试程序只包含 `int main(void) { return 0; }`，**没有链接 `libSDL2.dll.a`**，导致 `libSDL2main.a` 内部对 `SDL_strlen`、`SDL_memcpy` 等的引用无法解析。

#### 解决方案

通过 `CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY` 让 `try_compile` 只编译不链接：

```bash
-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
```

该变量取值：
- `EXECUTABLE`（默认）：编译并链接成可执行文件
- `STATIC_LIBRARY`：只编译成静态库，不链接

跳过链接后，`try_compile` 只验证编译器能工作，不会因为 SDL2 符号缺失而失败。

### 2.3 最终的完整配置命令

```bash
cmake -S . -B build-windows \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw.cmake \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
  -DSDL2_DIR=$(pwd)/x86_64-w64-mingw32/lib/cmake/SDL2 \
  -DCMAKE_C_FLAGS="-I$(pwd)/x86_64-w64-mingw32/include -include stdio.h" \
  -DCMAKE_EXE_LINKER_FLAGS="\
    -static-libgcc -static-libstdc++ \
    -L$(pwd)/x86_64-w64-mingw32/lib \
    -Wl,--whole-archive -lSDL2main -Wl,--no-whole-archive"
```

---

## 三、涉及的知识点

### 3.1 交叉编译基础

#### 什么是交叉编译

在**主机平台**（Host）上编译出**目标平台**（Target）的可执行文件。本项目：Host = Linux x64，Target = Windows x64。

#### 交叉编译工具链

| 组件 | Linux 包名 | 作用 |
|------|-----------|------|
| `x86_64-w64-mingw32-gcc` | `mingw-w64` | C 编译器（生成 Windows PE 格式 .obj / .exe） |
| `x86_64-w64-mingw32-g++` | `mingw-w64` | C++ 编译器 |
| `x86_64-w64-mingw32-ld` | `mingw-w64` | 链接器 |
| `x86_64-w64-mingw32-ar` | `mingw-w64` | 静态库归档工具 |
| `x86_64-w64-mingw32-windres` | `mingw-w64` | Windows 资源编译器（.rc → .res） |
| `x86_64-w64-mingw32-objdump` | `mingw-w64` | PE 文件分析工具 |

安装：`sudo apt install mingw-w64 cmake ninja-build`

### 3.2 CMake 工具链文件（Toolchain File）

#### 作用

告诉 CMake：
1. 目标平台是什么（`CMAKE_SYSTEM_NAME`）
2. 编译器在哪（`CMAKE_C_COMPILER`）
3. 在哪里找库和头文件（`CMAKE_FIND_ROOT_PATH`）

#### 本项目的工具链文件

```cmake
# cmake/toolchain-mingw.cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH
    /usr/x86_64-w64-mingw32                              # 系统 mingw runtime
    ${CMAKE_CURRENT_LIST_DIR}/../x86_64-w64-mingw32      # 项目内嵌的 SDL2 dev 包
)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)    # 程序用主机版（如 cmake 本身）
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)     # 库只在目标平台找
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)     # 头文件只在目标平台找
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)     # 包配置只在目标平台找
```

#### `CMAKE_FIND_ROOT_PATH_MODE_*` 的含义

| 模式 | 含义 |
|------|------|
| `NEVER` | 从不在目标平台根路径下找（用主机版） |
| `ONLY` | 只在目标平台根路径下找 |
| `BOTH` | 主机和目标都找（默认） |

典型配置：程序（如 cmake、ninja）用 `NEVER`（用主机版），库和头文件用 `ONLY`（必须用目标平台的）。

### 3.3 MinGW 链接机制

#### 静态库 vs 动态库（导入库）

| 文件类型 | 扩展名 | 作用 |
|---------|--------|------|
| 静态库 | `.a` | 编译时全部链接进 exe |
| 动态库 | `.dll` | 运行时加载 |
| 导入库 | `.dll.a` 或 `.lib` | 编译时提供符号表，运行时对应 `.dll` |

MinGW 的 SDL2 dev 包同时提供：
- `libSDL2.a` — 静态库（含全部 SDL2 实现）
- `libSDL2.dll.a` — 导入库（运行时对应 `SDL2.dll`）
- `libSDL2main.a` — main 入口转换的静态库

本项目使用**动态链接**模式（链接 `libSDL2.dll.a`，运行时加载 `SDL2.dll`），因此发行包必须包含 4 个 DLL。

#### `-Wl,--whole-archive` 的工作原理

ld 链接器扫描静态库时，默认**按需提取**：只提取能解析当前未定义符号的对象文件。这种策略对普通库有效，但对以下场景会出问题：

1. **入口点符号**（如 `WinMain`）：链接开始时它不是"未定义符号"，ld 不会从库中提取
2. **静态初始化器**：C++ 模板实例化、构造函数注册等，可能没有任何代码显式引用它们

`--whole-archive` 强制把整个静态库的所有对象文件都包含进来，无论是否被引用。`--no-whole-archive` 恢复默认行为。

```bash
-Wl,--whole-archive -lSDL2main -Wl,--no-whole-archive
```

### 3.4 CMake 命令行注入参数

在不修改 `CMakeLists.txt` 的前提下，通过命令行 `-D` 参数注入配置：

| 参数 | 作用 | 本项目用途 |
|------|------|-----------|
| `CMAKE_TOOLCHAIN_FILE` | 指定工具链文件 | 启用交叉编译 |
| `CMAKE_BUILD_TYPE` | 构建类型 | `Release` 启用 `-O3 -DNDEBUG` |
| `CMAKE_TRY_COMPILE_TARGET_TYPE` | try_compile 的目标类型 | `STATIC_LIBRARY` 跳过链接测试 |
| `CMAKE_C_FLAGS` | C 编译选项 | 注入 include 路径 |
| `CMAKE_EXE_LINKER_FLAGS` | 可执行文件链接选项 | 注入 `libSDL2main.a` 和静态化选项 |
| `SDL2_DIR` | SDL2 CMake 配置目录 | 指向项目内嵌的 `sdl2-config.cmake` |

### 3.5 SDL2 的 CMake 配置文件

SDL2 dev 包提供 `sdl2-config.cmake`（位于 `lib/cmake/SDL2/`），定义了以下 imported targets：

| Target | 类型 | 对应文件 |
|--------|------|---------|
| `SDL2::SDL2` | SHARED IMPORTED | `libSDL2.dll.a` + `SDL2.dll` |
| `SDL2::SDL2-static` | STATIC IMPORTED | `libSDL2.a` |
| `SDL2::SDL2main` | STATIC IMPORTED | `libSDL2main.a` |
| `SDL2::SDL2test` | STATIC IMPORTED | `libSDL2_test.a` |

通过 `DSDL2_DIR=...lib/cmake/SDL2` 让 CMake 的 `find_package(SDL2)` 能找到这个配置文件。

### 3.6 MinGW Runtime 静态化

mingw 编译的程序默认依赖以下 runtime DLL：
- `libgcc_s_seh-1.dll` — GCC 运行时
- `libstdc++-6.dll` — C++ 标准库
- `libwinpthread-1.dll` — POSIX 线程

通过链接选项静态化，避免分发这些 DLL：

```bash
-static-libgcc        # 静态链接 libgcc
-static-libstdc++     # 静态链接 libstdc++
```

静态化后，`CDUD.exe` 只依赖 4 个 SDL2 DLL + Windows 系统库（`KERNEL32.dll`、`msvcrt.dll`、`SHELL32.dll`），大幅简化分发。

### 3.7 `-include stdio.h` 的作用

本项目 `CMAKE_C_FLAGS` 中有 `-include stdio.h`，强制在所有源文件开头包含 `<stdio.h>`。

**原因**：mingw 的 `stdio.h` 定义了 `FILE`、`stdin`、`stdout`、`stderr` 等，SDL2 的某些头文件（如 `SDL_log.h`）在 Windows 上依赖这些定义。如果不强制包含，可能在某些包含顺序下出现编译错误。

### 3.8 发行包结构

```
dist-windows/
├── CDUD.exe              (922 KB)  主程序
├── SDL2.dll              (1.7 MB)  SDL2 核心
├── SDL2_image.dll        (170 KB)  图片加载（PNG/JPG）
├── SDL2_mixer.dll        (300 KB)  音频混音（MP3/WAV）
├── SDL2_ttf.dll          (1.7 MB)  TrueType 字体渲染
└── assets/                         资源目录
    ├── Tiled/                      Tiled 地图工程文件（可选分发）
    ├── audio/                      音频（menu.mp3, playing.wav）
    ├── fonts/                      字体（DejaVuSans-Bold.ttf）
    ├── images/                     图片（背景、角色、道具、tiles）
    ├── maps/                       Tiled 导出的 JSON 地图（.tmj）
    └── text/                       文本（backstory, help, license）
```

**关键点**：`assets/` 必须与 `CDUD.exe` 同级，因为代码中所有资源路径都是相对路径。

### 3.9 验证 DLL 依赖

用 `objdump` 检查 exe 依赖的 DLL：

```bash
x86_64-w64-mingw32-objdump -p build-windows/CDUD.exe | grep "DLL Name:"
```

本项目输出：
```
DLL Name: KERNEL32.dll    # Windows 系统库，无需分发
DLL Name: msvcrt.dll      # Windows 系统库，无需分发
DLL Name: SDL2.dll        # 需分发
DLL Name: SDL2_image.dll  # 需分发
DLL Name: SDL2_mixer.dll  # 需分发
DLL Name: SDL2_ttf.dll    # 需分发
DLL Name: SHELL32.dll     # Windows 系统库，无需分发
```

### 3.10 POST_BUILD 自动复制资源

[CMakeLists.txt#L40-L44](../CMakeLists.txt#L40-L44) 配置了 POST_BUILD 命令，每次构建后自动把 `assets/` 复制到构建目录：

```cmake
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/assets" "${CMAKE_BINARY_DIR}/assets"
    COMMENT "清除旧 assets 并重新复制"
)
```

因此构建完成后 `build-windows/` 下直接就有 `assets/` 可用，打包时只需复制 exe + DLL + assets。

---

## 四、完整构建流程

### 4.1 一次性配置

```bash
cd /path/to/CountDownUntilDaybreak

cmake -S . -B build-windows \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw.cmake \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
  -DSDL2_DIR=$(pwd)/x86_64-w64-mingw32/lib/cmake/SDL2 \
  -DCMAKE_C_FLAGS="-I$(pwd)/x86_64-w64-mingw32/include -include stdio.h" \
  -DCMAKE_EXE_LINKER_FLAGS="\
    -static-libgcc -static-libstdc++ \
    -L$(pwd)/x86_64-w64-mingw32/lib \
    -Wl,--whole-archive -lSDL2main -Wl,--no-whole-archive"
```

### 4.2 增量编译

```bash
cmake --build build-windows -j
```

### 4.3 打包

```bash
rm -rf dist-windows && mkdir -p dist-windows
cp build-windows/CDUD.exe dist-windows/
cp -r build-windows/assets dist-windows/
cp x86_64-w64-mingw32/bin/*.dll dist-windows/
tar -czf CDUD-windows-x64.tar.gz -C dist-windows .
```

---

## 五、常见问题排查

### 5.1 `undefined reference to 'WinMain'`

**原因**：未链接 `libSDL2main.a`，或链接顺序错误。

**解决**：用 `-Wl,--whole-archive -lSDL2main -Wl,--no-whole-archive` 强制包含。

### 5.2 try_compile 失败（`undefined reference to 'SDL_strlen'` 等）

**原因**：`CMAKE_EXE_LINKER_FLAGS` 注入了 SDL2 库，但 try_compile 测试程序没链接 SDL2。

**解决**：加 `-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY`。

### 5.3 `find_package(SDL2)` 找不到

**原因**：CMake 不知道 SDL2 的 `sdl2-config.cmake` 在哪。

**解决**：加 `-DSDL2_DIR=/path/to/x86_64-w64-mingw32/lib/cmake/SDL2`。

### 5.4 运行时找不到 DLL

**原因**：DLL 没和 exe 放在同一目录。

**解决**：用 `objdump -p` 检查依赖，把所有非系统 DLL 复制到 exe 同级目录。

### 5.5 运行时找不到资源

**原因**：资源用了相对路径，但 cwd 不是 exe 所在目录。

**解决**：从 exe 所在目录启动程序（双击或快捷方式设置"起始位置"）。

### 5.6 Windows Defender 误报

**原因**：mingw 编译的 exe 无数字签名，可能被杀软误报。

**解决**：用 `osslsigncode` 自签名，或申请代码签名证书。

---

## 六、方案对比

| 方案 | 工具链准备 | 依赖获取 | 编译速度 | 复杂度 | 推荐度 |
|------|-----------|---------|---------|--------|--------|
| **A. MinGW-w64 + 手动 SDL2 dev 包** | apt 一键安装 | 从 libsdl.org 下载 zip | 快 | 中 | ⭐⭐⭐⭐⭐ |
| B. MXE | 编译几小时 | 自动 | 慢首次 | 高 | ⭐⭐⭐ |
| C. MSYS2 (Windows 原生) | 需 Windows 机器 | pacman | 快 | 低 | ⭐⭐⭐⭐ |
| D. Docker 镜像 | docker pull | 镜像内置 | 快 | 中 | ⭐⭐⭐⭐ |

本项目采用**方案 A**：在 Linux 上用 `mingw-w64` 直接交叉编译，最快且可控。

---

## 七、参考链接

- [MinGW-w64 官方文档](https://www.mingw-w64.org/)
- [CMake 交叉编译指南](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html)
- [CMAKE_TRY_COMPILE_TARGET_TYPE 文档](https://cmake.org/cmake/help/latest/variable/CMAKE_TRY_COMPILE_TARGET_TYPE.html)
- [ld --whole-archive 文档](https://ftp.gnu.org/old-gnu/Manuals/ld-2.9.1/html_node/ld_3.html)
- [SDL2 开发包下载](https://github.com/libsdl-org/SDL/releases)
- [SDL2_main 机制说明](https://wiki.libsdl.org/SDL2/SDL_main)

---

## 八、修改记录

| 日期 | 修改文件 | 修改内容 |
|------|---------|---------|
| 2026-07-26 | 无（不修改任何项目代码） | 仅通过 CMake 命令行参数和新增工具链文件完成交叉编译配置 |
| 2026-07-26 | cmake/toolchain-mingw.cmake | 新增工具链文件（非项目业务代码） |
