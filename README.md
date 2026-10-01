# kitzoo

kitzoo is a C++20 foundation library with modules for strings, time,
filesystems, logging, synchronization, queues, threads, and system utilities.

## Requirements

- CMake 3.25 or newer
- A C++20 compiler: GCC 12+, Clang 16+, or Apple Clang 15+

## Build

The standalone `debug` preset builds the library, tests, benchmarks, and
examples:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Other presets: `release`, `relwithdebinfo`, `asan`, `ubsan`, `tsan`,
`asan-ubsan`, and `compare`. The `compare` preset enables benchmarks against
spdlog and BS::thread_pool.

Crypto and mimalloc support are opt-in:

```sh
cmake -S . -B build/full -DKITZOO_WITH_OPENSSL=ON -DKITZOO_WITH_MIMALLOC=ON
cmake --build build/full
```

## Use in another CMake project

Install a build and find the package:

```sh
cmake --install build/release --prefix /path/to/prefix
```

```cmake
find_package(kitzoo CONFIG REQUIRED)
target_link_libraries(app PRIVATE kitzoo::core kitzoo::string)
```

Or add kitzoo with `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(kitzoo
    GIT_REPOSITORY <repository-url>
    GIT_TAG <release-or-commit>)
FetchContent_MakeAvailable(kitzoo)
target_link_libraries(app PRIVATE kitzoo::core kitzoo::string)
```

When included as a subproject, tests, benchmarks, and examples are disabled.
The standalone build defaults to `Release` if no build type is selected.

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
| `kitzoo::log` | Synchronous and asynchronous logging |
| `kitzoo::system` | Environment, host, process, memory, and stacktrace queries |
| `kitzoo::cli` | cxxopts command-line parsing |
| `kitzoo::json` | nlohmann/json integration |
| `kitzoo::crypto` | OpenSSL AES-CBC; enabled with `KITZOO_WITH_OPENSSL` |
| `kitzoo::memory` | mimalloc allocator; enabled with `KITZOO_WITH_MIMALLOC` |

Additional targets expose `ConcurrentQueue` (`kitzoo::queue_concurrentqueue`), a
ConcurrentQueue-backed object pool (`kitzoo::thread_concurrent_pool`), and
BS::thread_pool (`kitzoo::thread_bs_pool`). Link only the targets used by your
application.

Use a module header such as `<kitzoo/string.hpp>` or
`<kitzoo/filesystem.hpp>`. `<kitzoo/kitzoo.hpp>` includes the common modules;
CLI, JSON, crypto, mimalloc, and the additional integrations have their own
headers and targets.

## Examples

Standalone builds enable examples by default. They cover strings, time, JSON,
CLI, logging, queues, memory pools, filesystems, utilities, and threads. The
crypto example is included when OpenSSL support is enabled.

Third-party dependency fetch rules are in `cmake/dependencies/`.
