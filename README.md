# Onlooker

Onlooker is a simple memory profiler for Windows. It allows you to record memory statistics for a process tree similar to the Linux `time` command.

Cutelooker is a GUI for Onlooker traces written in Qt. It also allows you to link the memory trace with a log file.

## Building (Windows)

You need Visual Studio 2019 or higher and [CMake 3.15](https://cmake.org/download/) or higher. Then run:

```
cmake -B build
```

And open `build\Onlooker.sln` to build the project. This project uses [cmkr](https://github.com/build-cpp/cmkr), to modify the configuration look at `cmake.toml`. You can find the documentation [here](https://build-cpp.github.io/cmkr/cmake-toml/). After modifying `cmake.toml` just build the solution and everything will be regenerated automatically.

## Building (other platforms)

You should be able to build the Qt GUI on other platforms. `Onlooker` is Windows specific, so you will only be able to view traces.

You need a compiler supporting C++17 (tested with clang 12.0). Then run:

```
cmake -B build -DCMAKE_C_COMPILER=clang-12 -DCMAKE_CXX_COMPILER=clang++-12
cmake --build build
```

Replace `clang-12` and `clang++-12` with a path to your compiler.

You can use `-DCMAKE_BUILD_TYPE=Release` to build in release mode.

## Log file format

TODO: discuss how to write a log converter