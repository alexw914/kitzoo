# kitzoo

kitzoo is a modular C++20 foundation library for Linux, macOS, and Windows.
It provides common utilities and integrations with established C++ libraries.
Applications can link the modules they need.

## Modules

| Module | Functionality |
| --- | --- |
| `core` | Version information and compiler/platform utilities. |
| `utilities` | Random values, UUIDs, singletons, and general-purpose helpers. |
| `string` | String processing, numeric conversion, and encoding. |
| `time` | Clocks, calendar conversion, timers, and duration measurement. |
| `os` | System information, thread operations, and filesystem utilities. |
| `memory` | Allocation, byte buffers, containers, and shared memory. |
| `queue` | Blocking, concurrent, and single-producer/single-consumer queues. |
| `thread` | Thread pools, object pools, locks, and synchronized values. |
| `log` | Synchronous and asynchronous logging. |
| `cli` | Command-line argument parsing. |
| `json` | JSON loading, access, and serialization. |
| `crypto` | AES encryption and decryption; requires OpenSSL. |

CMake targets use the `kitzoo::<module>` naming convention.

## Build and test

Requirements: CMake 3.25+, Ninja, Git, and a C++20 compiler (GCC, Clang, or MSVC).
Dependencies are fetched during configuration. For MSVC, use an x64 Visual Studio
Developer shell with the C++ desktop workload installed.

```sh
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DKITZOO_BUILD_TESTS=ON
cmake --build build/debug --parallel
ctest --test-dir build/debug --output-on-failure
```

Tests and examples are enabled by default in standalone builds. Add
`-DKITZOO_BUILD_BENCHMARKS=ON` to build benchmarks, or `-DKITZOO_WITH_OPENSSL=ON`
to enable crypto with an installed OpenSSL development package.

Clang and Ninja users can also use the Debug preset:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

See [examples](examples/), [tests](tests/), and [platform build scripts](scripts/)
for module usage and platform-specific build commands.

## Use in another CMake project

### FetchContent

Replace `<release-or-commit>` with the revision you want to use:

```cmake
include(FetchContent)
FetchContent_Declare(kitzoo
    GIT_REPOSITORY https://github.com/alexw914/kitzoo.git
    GIT_TAG <release-or-commit>)
FetchContent_MakeAvailable(kitzoo)

target_link_libraries(app PRIVATE kitzoo::core kitzoo::string)
```

### Installed package

```sh
cmake --install build/debug --prefix /path/to/prefix
```

Configure your application with `-DCMAKE_PREFIX_PATH=/path/to/prefix`, then use:

```cmake
find_package(kitzoo CONFIG REQUIRED)
target_link_libraries(app PRIVATE kitzoo::core kitzoo::string)
```

## Reference

| Project | Purpose |
| --- | --- |
| [spdlog](https://github.com/gabime/spdlog) | Logging. |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON. |
| [cxxopts](https://github.com/jarro2783/cxxopts) | Command-line parsing. |
| [ConcurrentQueue](https://github.com/cameron314/concurrentqueue) | Concurrent queues. |
| [BS::thread_pool](https://github.com/bshoshany/thread-pool) | Thread pools. |
| [mimalloc](https://github.com/microsoft/mimalloc) | Memory allocation. |
| [OpenSSL](https://github.com/openssl/openssl) | Cryptography. |
| [GoogleTest](https://github.com/google/googletest) | Unit tests. |
| [Google Benchmark](https://github.com/google/benchmark) | Benchmarks. |
