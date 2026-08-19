# Kuznix Terminal

A Qt 6 + C++20 + Meson developer terminal focused on Linux development workflows.

## Features

- Multi-tab interactive shell sessions.
- Configurable shell and persistent project directory.
- Developer build settings window.
- Build-system profiles: Meson/Ninja, CMake/Ninja, CMake/Make, Make, Ninja, Cargo and custom.
- Configurable Make/CMake parallelism.
- Separate Ninja and Cargo job limits.
- Fixed `-jX` or automatic `$(nproc)` parallelism.
- C/C++ build flags and CMake/Meson/Cargo arguments.
- Extra build environment variables.
- Optional compiler-cache setting for ccache/sccache workflows.
- Verbose build mode.
- Configure, Build, Install and toolchain-info actions.
- Persistent settings through Qt QSettings.
- Keyboard shortcuts: Ctrl+Shift+T, Ctrl+Shift+W, Ctrl+Shift+B and F6.

## Build

Dependencies: Qt 6 Core/Gui/Widgets, Meson, Ninja, a C++20 compiler and pkg-config.

```sh
meson setup build
meson compile -C build
meson install -C build
```

Run:

```sh
./build/kuznix-terminal
```

## Build tuning examples

The application can drive common Linux developer workflows such as:

```text
make -j$(nproc)
ninja -j16
cmake --build build --parallel 16
cargo build -j8 --release
meson setup build --buildtype=release
```

Build settings are saved automatically and can be changed without editing shell configuration files.

## Notes

The terminal currently uses Qt's `QProcess` shell backend. It is intentionally lightweight; a future version can replace it with a PTY backend for full terminal emulation, including robust ANSI/VT input, resize signaling, alternate screen buffers and applications such as vim, tmux and htop.
