# kitzoo

kitzoo is a cross-platform C++20 foundation library that brings common utilities
and third-party libraries together behind modular headers and CMake targets.
It supports Linux, macOS, and Windows. Applications can link only the modules
they need.

## Modules

| Target | Contents |
|---|---|
| `kitzoo::core` | Version and build information, compiler/platform macros |
| `kitzoo::utilities` | Random helpers, UUID, singleton, `ScopeGuard`, `unique_function` |
| `kitzoo::lock` | Spin locks, `Synchronized<T>` |
| `kitzoo::string` | String helpers, number conversion, hex and Base64 |
| `kitzoo::time` | `Stopwatch`, `Deadline`, `Timer`, timestamps |
| `kitzoo::filesystem` | File and directory operations |
| `kitzoo::queue` | `BlockingQueue<T>`, `SPSCQueue<T>` |
| `kitzoo::thread` | `ThreadPool`, `ObjectPool<T>`, `BlockingObjectPool<T>` |
| `kitzoo::log` | Synchronous and asynchronous logging backed by spdlog |
| `kitzoo::system` | Environment, host, process, memory, and stacktrace queries |
| `kitzoo::cli` | cxxopts command-line parsing |
| `kitzoo::json` | nlohmann/json integration |
| `kitzoo::crypto` | OpenSSL AES-CBC; enabled with `KITZOO_WITH_OPENSSL` |
| `kitzoo::memory` | mimalloc allocator; enabled with `KITZOO_WITH_MIMALLOC` |
| `kitzoo::queue_concurrentqueue` | moodycamel ConcurrentQueue integration |
| `kitzoo::thread_concurrent_pool` | ConcurrentQueue-backed object pool |
| `kitzoo::thread_bs_pool` | BS::thread_pool integration |

Include a module header such as `<kitzoo/string.hpp>` or
`<kitzoo/filesystem.hpp>`. `<kitzoo/kitzoo.hpp>` includes the common modules;
CLI, JSON, crypto, mimalloc, and the additional integrations have separate
headers and targets.

## Build and test

Requirements:

- CMake 3.25 or newer; Ninja for the commands below.
- A C++20 compiler: GCC 12+, Clang 16+, Apple Clang 15+, or MSVC from
  Visual Studio 2022 with the C++ desktop workload.
- OpenSSL development headers and libraries when crypto is enabled.

Run from a shell where the compiler and build tools are available. For MSVC,
use an x64 Visual Studio developer shell.

```sh
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
```

Standalone builds enable tests and examples by default. Benchmarks, OpenSSL,
and mimalloc are opt-in. Without an explicit build type, a single-configuration
standalone build defaults to `Release`.

| CMake option | Default | Purpose |
|---|---|---|
| `KITZOO_BUILD_TESTS` | `ON` | Build unit tests |
| `KITZOO_BUILD_EXAMPLES` | `ON` | Build module usage examples |
| `KITZOO_BUILD_BENCHMARKS` | `OFF` | Build benchmarks |
| `KITZOO_BUILD_COMPARE_BENCHMARKS` | `OFF` | Add comparisons with spdlog and BS::thread_pool; requires benchmarks |
| `KITZOO_WITH_OPENSSL` | `OFF` | Enable crypto |
| `KITZOO_WITH_MIMALLOC` | `OFF` | Enable the mimalloc allocator |
| `KITZOO_WARNINGS_AS_ERRORS` | `ON` | Treat project compiler warnings as errors |

Enable optional modules by adding `-DKITZOO_WITH_OPENSSL=ON` and
`-DKITZOO_WITH_MIMALLOC=ON` when configuring. Set `OPENSSL_ROOT_DIR` if OpenSSL
is installed outside the standard search paths.

The presets in `CMakePresets.json` use Clang and Ninja. The `debug` preset
also enables benchmarks:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Other configure presets include `release`, `relwithdebinfo`, `asan`, `ubsan`,
`tsan`, `asan-ubsan`, and `compare`. Sanitizer availability depends on the
compiler and platform. `compare` enables third-party comparison benchmarks.

Module usage examples are in [`examples/`](examples/), and unit tests are in
[`tests/`](tests/). GitHub Actions builds and tests Debug targets on Linux,
macOS, and Windows, including OpenSSL and mimalloc integrations.

## Use in another CMake project

### FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(kitzoo
    GIT_REPOSITORY https://github.com/alexw914/kitzoo.git
    GIT_TAG <release-or-commit>)
FetchContent_MakeAvailable(kitzoo)

target_link_libraries(app PRIVATE kitzoo::core kitzoo::string)
```

When included as a subproject, kitzoo disables its tests, benchmarks, and
examples. Set optional integration flags before `FetchContent_MakeAvailable()`
if your application needs them.

### Installed package

Install a configured build:

```sh
cmake --install build/debug --prefix /path/to/prefix
```

Then configure the application with `CMAKE_PREFIX_PATH=/path/to/prefix` and use:

```cmake
find_package(kitzoo CONFIG REQUIRED)
target_link_libraries(app PRIVATE kitzoo::core kitzoo::string)
```

Dependency versions and fetch rules are maintained in
[`cmake/dependencies/`](cmake/dependencies/). Sources can also be supplied through
local checkouts or archives; see [`cmake/KitzooFetch.cmake`](cmake/KitzooFetch.cmake).

## References

kitzoo directly integrates the following third-party libraries. Their upstream
projects provide the implementations used by the corresponding modules:

| Project | Use in kitzoo |
|---|---|
| [spdlog](https://github.com/gabime/spdlog) | Logging backend |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON integration |
| [cxxopts](https://github.com/jarro2783/cxxopts) | Command-line parsing |
| [ConcurrentQueue](https://github.com/cameron314/concurrentqueue) | Concurrent queues and object pools |
| [BS::thread_pool](https://github.com/bshoshany/thread-pool) | Thread-pool integration |
| [OpenSSL](https://github.com/openssl/openssl) | AES-CBC implementation; optional |
| [mimalloc](https://github.com/microsoft/mimalloc) | Allocator integration; optional |

Development builds also use [GoogleTest](https://github.com/google/googletest)
for unit tests and [Google Benchmark](https://github.com/google/benchmark) for
benchmarks. Comparison benchmarks use spdlog and BS::thread_pool.
