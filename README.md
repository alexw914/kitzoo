# kitzoo

kitzoo is a modular C++20 foundation library for Linux, macOS, and Windows.
It provides common utilities and integrations with established C++ libraries.
Applications can link the modules they need.

## Modules

| Module | Functionality |
| --- | --- |
| `core` | Version information, command-line parsing, singletons, move-only callables, and scope guards. |
| `utilities` | String processing, encoding, random values, UUIDs, and optional AES, SHA-256, and HMAC. |
| `time` | Clocks, calendar conversion, elapsed measurements, timelines, and periodic callbacks. |
| `os` | System queries, thread configuration, file/directory operations, and graceful shutdown. |
| `memory` | Memory allocation, buffers, containers, and shared memory. |
| `queue` | Blocking (optionally bounded) and concurrent queues. |
| `thread` | Thread pools, object pools, and synchronization. |
| `log` | Synchronous/asynchronous logging, file rollover, and retention. |
| `json` | JSON parsing, file loading, and serialization. |

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
to enable AES, SHA-256, and HMAC utilities with an installed OpenSSL development package.

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

Pin a release tag or commit:

```cmake
include(FetchContent)
FetchContent_Declare(kitzoo
    GIT_REPOSITORY https://github.com/alexw914/kitzoo.git
    GIT_TAG v0.3.0)
FetchContent_MakeAvailable(kitzoo)

target_link_libraries(app PRIVATE kitzoo::core kitzoo::utilities)
```

### Installed package

```sh
cmake --install build/debug --prefix /path/to/prefix
```

Configure your application with `-DCMAKE_PREFIX_PATH=/path/to/prefix`, then use:

```cmake
find_package(kitzoo CONFIG REQUIRED)
target_link_libraries(app PRIVATE kitzoo::core kitzoo::utilities)
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
