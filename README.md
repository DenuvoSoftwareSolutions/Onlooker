# Onlooker

Onlooker is a simple memory profiler for Windows. It allows you to record memory statistics for a process tree similar to the Linux `time` command. For example:
```
> Onlooker.exe my.exe arguments
```

Onlooker acts like a wrapper for `my.exe` and keeps track of the memory usage of all child processes. When `my.exe` terminates, the time series is saved in a JSON trace file.

Additionally you can attach Onlooker to an existing process:

```
> Onlooker.exe :attach <pid>
```

Cutelooker is a GUI for Onlooker traces written in Qt. It also allows you to link the memory trace with a log file.

An [post introducing Onlooker and Cutelooker](https://denuvosoftwaresolutions.github.io/Onlooker/intro.html) was published September 16, 2022.

## Building (Windows)

You need Visual Studio 2019 or higher and [CMake 3.15](https://cmake.org/download/) or higher. Then run:

```
cmake -B build
```

And open `build\Onlooker.sln` to build the project. This project uses [cmkr](https://github.com/build-cpp/cmkr), to modify the configuration look at `cmake.toml`. You can find the documentation [here](https://build-cpp.github.io/cmkr/cmake-toml/). After modifying `cmake.toml` just build the solution and everything will be regenerated automatically.

## Building (other platforms)

You should be able to build the Qt GUI on other platforms. `Onlooker` is Windows specific, so you will only be able to view traces.

You need a compiler supporting C++17 (tested with clang 12.0 and GCC 11.2) and the Qt5 development files installed (on Debian/Ubuntu: `apt install qtbase5-dev qt5-qmake qtbase5-dev-tools qtchooser`). Then run:

```
cmake -B build -DCMAKE_C_COMPILER=clang-12 -DCMAKE_CXX_COMPILER=clang++-12
cmake --build build
```

Replace `clang-12` and `clang++-12` with a path to your compiler, or omit if using an up-to-date default compiler.

You can use `-DCMAKE_BUILD_TYPE=Release` to build in release mode.

## Log file format

A key feature is that you can link your application's logs to the timeline Cutelooker visualizes. When you update the selection in Cutelooker, you can see immediately see what your application was doing at that time.

The log format is JSON:

```json
{
  "1643811426000": ["first message", "second message"],
  "1643811427000": ["a second later"]
}
```

The key is the number of _milliseconds_ since epoch (UTC) as a string and the value is a list of log lines that happened at this time.

After loading this JSON log in Cutelooker UI, the selection of the graph automatically scrolls to the relevant log line and vice versa.

## License

Onlooker is available under the permissive [BSL-1.0](https://choosealicense.com/licenses/bsl-1.0/) license.
Cutelooker is available under [GPLv3](https://choosealicense.com/licenses/gpl-3.0/) license.

Cutelooker uses [QCustomPlot](http://www.qcustomplot.com/) version 2.0.1 by Emanuel Eichhammer.
