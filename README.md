# kitzoo

kitzoo is a cross-platform C++20 foundation library that brings common utilities
and third-party libraries together behind modular headers and CMake targets.
It supports Linux, macOS, and Windows. Applications can link only the modules
they need.

Repository coding conventions are documented in [AGENTS.md](AGENTS.md), including
file headers, include guards, clang-format, and naming rules.

## Repository structure

```text
kitzoo/
|-- include/kitzoo/          Public module headers
|   |-- kitzoo.hpp          Common module entry point
|   |-- os.hpp              Unified OS module entry point
|   |-- os/
|   |   |-- osadaptor.hpp   Singleton system and thread operations
|   |   |-- fsadaptor.hpp   Singleton file, directory, and path operations
|   |   `-- filesystem.hpp Compatibility filesystem functions
|   `-- utilities/         Random, UUID, singleton, and utility headers
|-- src/                    Implementations and module CMake targets
|   `-- os/
|       |-- CMakeLists.txt  Defines kitzoo::os
|       |-- osadaptor.cpp   Linux, macOS, and Windows backends
|       |-- fsadaptor.cpp   Filesystem adaptor implementation
|       `-- filesystem.cpp Compatibility forwarding functions
|-- tests/                  Unit tests grouped by module
|   `-- os/                 OSAdaptor, FsAdaptor, and compatibility tests
|-- examples/               Executable module usage examples
|   |-- os.cpp              System queries and filesystem operations
|   `-- utilities.cpp       Random, UUID, and singleton inheritance
|-- benchmarks/             Performance and comparison benchmarks
|-- cmake/                  Build, dependency, and installation helpers
|-- scripts/                Linux, macOS, and Windows build/test scripts
|-- .github/workflows/      CI configuration
`-- .gitattributes          LF text files; CRLF Windows command scripts
```

The former system and filesystem modules are combined in `os`. Applications
link `kitzoo::os`, include `<kitzoo/os.hpp>`, and use the `kitzoo::os` namespace.
Public headers and implementation directories follow the same module names;
header-only modules keep their implementation in `include/kitzoo/`.

## Modules

| Target | Contents |
|---|---|
| `kitzoo::core` | Version and build information, compiler/platform macros |
| `kitzoo::utilities` | Random helpers, UUID, singleton, `ScopeGuard`, `unique_function` |
| `kitzoo::lock` | Spin locks, `Synchronized<T>` |
| `kitzoo::string` | String helpers, number conversion, hex and Base64 |
| `kitzoo::time` | Clocks, calendar conversion, injectable timelines, named measurements, Stopwatch, Deadline and Timer |
| `kitzoo::os` | FsAdaptor file, directory and path operations; OSAdaptor system and thread operations |
| `kitzoo::queue` | `BlockingQueue<T>`, `SPSCQueue<T>` |
| `kitzoo::thread` | Built-in and BS thread pools, reusable and concurrent object pools |
| `kitzoo::log` | Synchronous and asynchronous logging backed by spdlog |
| `kitzoo::cli` | cxxopts command-line parsing |
| `kitzoo::json` | JSON input loading backed by nlohmann/json |
| `kitzoo::crypto` | OpenSSL AES-CBC; enabled with `KITZOO_WITH_OPENSSL` |
| `kitzoo::memory` | mimalloc allocation, allocation budgets and shared mappings, containers and object helpers |
| `kitzoo::queue_concurrentqueue` | moodycamel ConcurrentQueue integration |
| `kitzoo::thread_concurrent_pool` | ConcurrentQueue-backed object pool |
| `kitzoo::thread_bs_pool` | Compatibility target linking `kitzoo::thread` |

Include a module header such as `<kitzoo/string.hpp>` or
`<kitzoo/os.hpp>`. `<kitzoo/kitzoo.hpp>` includes the common modules;
CLI, JSON, crypto, mimalloc, and the additional integrations have separate
headers and targets.

Link `kitzoo::os` and include `<kitzoo/os.hpp>` for `FsAdaptor` and `OSAdaptor`.
Filesystem operations use `FsAdaptor::instance()`; existing free functions such
as `kitzoo::os::read_file()` remain compatibility entry points to the same implementation.

```cpp
auto& fs = kitzoo::os::FsAdaptor::instance();
fs.mkdir("results/nested", true);  // create missing parents
fs.write_text("results/frame.txt", "frame=7\n");
fs.append_text("results/frame.txt", "status=processed\n");
auto files = fs.listdir("results", true);  // sorted recursive listing
bool folder = fs.is_folder("results");
fs.rm("results/frame.txt");  // remove one file or empty directory
```

`mkdir`, `rm`, and `listdir` default to nonrecursive operations. Pass `true` to
create parents, remove a tree, or list recursively. Recursive operations do not
follow directory symlinks. Error-code overloads place `std::error_code&` before
the optional flag. See [FsAdaptor semantics](docs/fsadaptor.md).

The header-only JSON module provides `Json` (`nlohmann::json`) and a small `Reader`
through `<kitzoo/json.hpp>` or `<kitzoo/json/reader.hpp>`. Reader only loads input and
records load failures; access, conversion, defaults, mutation, and serialization use
nlohmann's native API.

```cpp
kitzoo::json::Reader reader(R"({"services":[{"workers":4}]})");
if (reader.is_parse_success()) {
    auto& json = reader.raw();
    auto workers = json.at(kitzoo::json::Json::json_pointer("/services/0/workers")).get<int>();
    auto timeout = json.value("timeout", 30);
    json["mode"] = "default";
    auto text = json.dump(2);
}

kitzoo::json::Reader file(std::filesystem::path{"config.json"});
kitzoo::json::Reader object(kitzoo::json::Json{{"workers", 4}});
kitzoo::json::Reader reload;
auto loaded = reload.load_file("config.json");
```

String constructors parse JSON text; `std::filesystem::path` selects file input.
`load_file()` also accepts paths stored as strings, without guessing input kinds.
The `Json` constructor owns its input, copying lvalues and moving rvalues.
`parse()` and `load_file()` return `bool`; check `is_parse_success()` and `error_info()`
after construction. Failed loads preserve the previous document. `raw()` provides
mutable or const access to that document, including after a failed load; callers decide
whether to keep using it. Direct mutation does not change the status of the last load.
Parsing uses nlohmann's nonthrowing mode with a generic error message; file reads use
filesystem error codes. Allocation failures can still throw. Native JSON access and
conversion retain nlohmann's behavior and exceptions. [examples/json.cpp](examples/json.cpp)
demonstrates reads and writes of integers, 64-bit signed/unsigned integers, float/double,
booleans, strings, null, arrays, objects, and nested fields. Run it without arguments
for the built-in sample, or pass `input.json [output.json]` to read and optionally save
a file using the sample schema.

Use `kitzoo::os::OSAdaptor` from `<kitzoo/os.hpp>` for system queries
(`hostname()`, `cpu_count()`, `get_env()`, memory, process and stacktrace queries).
All OSAdaptor operations are instance methods accessed through `OSAdaptor::instance()`.
The declarations and implementations live in `osadaptor.hpp` and `osadaptor.cpp`.
OSAdaptor inherits `util::Singleton<OSAdaptor>` and uses its `instance()` accessor.
The singleton base disables copying and moving; OSAdaptor's constructor is private.
Scheduling uses `enum class OSAdaptorShedPolicy { Other, Rr, Fifo }`.

| Operation | Linux | macOS | Windows |
|---|---|---|---|
| System queries and process CPU time | Native APIs | POSIX / sysctl | Win32 |
| Thread name | Any live thread | Current thread only | Thread description API |
| Scheduling | POSIX Other/Rr/Fifo; Other forces zero priority | POSIX policy and priority ranges | Other only; native Windows priority values |
| Thread nice | Linux TID, range -20..19 | Returns false | Returns false |
| CPU binding | CPU indices | Returns false | Indices within the thread's processor group |
| Socket path placeholder | /dev/shm | /tmp | Windows temporary directory |

Thread handles must identify live threads; a Linux kernel TID is distinct from
a pthread handle. Names retain the reconstructed `mosa/` prefix and 15-byte
limit. Permission failures and unsupported operations return `false`.
CPU time reports process user + kernel time in nanoseconds, not wall-clock time.
Socket path allocation creates a closed placeholder: unlink it before binding
and remove the socket afterward. Check the returned path against your platform's
Unix socket path length limit. Allocation failure returns an empty string.
The implementation is adapted from reconstructed imosadaptor OSAdaptor,
with corrected time units, descriptor handling and validation; it is not an
ABI-compatible replacement.

### Time utilities

The time module remains independent: link `kitzoo::time` and include
`<kitzoo/time.hpp>`. `Stopwatch`, `Deadline` and `Timer` retain their interfaces.
`TimeStamp` is a signed nanosecond `std::chrono::sys_time`, and `TimeDuration`
is `std::chrono::nanoseconds`; use standard chrono conversions for other units.
`utc_timestamp()` reads wall time; `steady_timestamp()` has an unspecified epoch
and is for elapsed time, not calendar dates. `DateTime`, `to_date_time` and
`from_date_time` support validated UTC/local calendar conversion, including
nanosecond fractions and UTC dates before 1970 or beyond 2038. Local conversion
uses OS timezone/DST rules and its supported date range. Ambiguous local times
use mktime's choice; normalized dates such as DST gaps are rejected.
`format_timestamp` defaults to local milliseconds and also offers UTC and
second/microsecond/nanosecond precision.

`SystemTimeline`, `FeederTimeline`, `CallbackTimeline` and custom `Timeline`
implementations support live time, replay and keyed external clocks.
`OffsetTimeline` adds a checked signed correction to another timeline.
`Time::instance()` selects its timeline once: call `init(shared_ptr<Timeline>)`
before the first query, otherwise it selects system time. Independent timeline
objects do not require the singleton. A feeder becomes valid after any feed,
including zero; callbacks must support concurrent calls. Timeline values are
nanoseconds in their own domain, not necessarily Unix UTC. `sleep_for` and
`sleep_until` follow that timeline using roughly 1 ms polling, support stop-token
cancellation, and use a steady clock for timeout (zero means unlimited). A paused
or backward replay cannot defeat a nonzero real timeout; invalid timelines return
false. Negative timeout and target arithmetic overflow throw exceptions.

`TimeWatcher` records named intervals and caches completed results. Its move-only
`scope()` measures to explicit `finish()` or destruction, and optionally invokes
a callback outside the internal lock. Scopes can outlive their watcher; destructor
callback exceptions are suppressed, while explicit finish propagates them.
Watchers use the steady clock even when the global timeline is simulated.
`ptp_timestamp(device)` reads Linux PTP clocks optionally; device failures and
unsupported platforms return nullopt. Hardware configuration determines its
timescale. Real PTP hardware was not available for validation.

See [`examples/time.cpp`](examples/time.cpp) and
[`docs/time_reference_analysis.md`](docs/time_reference_analysis.md) for examples,
reference comparison and external framework integration boundaries.

### Memory allocation

Mimalloc is a required dependency. Link `kitzoo::memory`, and include
`<kitzoo/memory.hpp>`. The mmemory-inspired APIs use repository naming conventions:

| Reference interface | kitzoo interface |
|---|---|
| MFBasicMemory / MFBasicMemoryConfig | BasicMemory / BasicMemoryConfig |
| MFSharedBasicMemoryConfig / MFMemoryType | SharedBasicMemoryConfig / MemoryType |
| MFAllocator / MFVector / MFMap and related aliases | MiAllocator / Vector / Map and related aliases |
| MFMemory::alloc_object / alloc_basic_array | Memory::alloc_object / alloc_basic_array |
| MFMakeShared / MFMakePair | make_shared / make_pair |

`MiAllocator<T>` is a stateless STL allocator calling mimalloc directly.
Ordinary types use `mi_malloc`; over-aligned types use `mi_malloc_aligned`, selected
at compile time. All frees use `mi_free`. There is no PMR resource pointer,
virtual dispatch, per-allocation tracking or application mutex on this path.
`Vector`, `Map`, `String` and related container aliases use MiAllocator by default.
Instances compare equal, so container moves can transfer storage without
resource checks. `Allocator<T>` has been removed; the old `MimallocAllocator`
name/header is replaced by `<kitzoo/memory/mi_allocator.hpp>`.

`make_unique<T>(...)` constructs a single object with mimalloc and returns
`UniquePtr<T>` with a stateless `MiDeleter<T>`. It supports over-aligned objects,
constructor rollback, move, reset and release. Array and derived-to-base ownership
conversions are not supported. Use `std::unique_ptr<T>` for objects allocated with
ordinary new/delete; do not mix these deleters.
Smart-pointer helpers and container aliases are declared in
`<kitzoo/memory/advanced_types.hpp>`. Use `make_unique` for mimalloc objects;
the separate raw-object construction/destruction helpers have been removed.
For custom deleters and control-block allocators, use the standard `std::shared_ptr`
constructor directly.

`ByteBuffer(size, alignment)` owns uninitialized raw bytes through mimalloc.
It is movable and noncopyable, with `data`, `size`, `capacity` and `alignment`.
`reserve` preserves logical size, `resize` grows capacity geometrically and
preserves existing bytes, `clear` retains storage, and `reset` releases it.
`shrink_to_fit` reduces capacity to size (or frees an empty buffer). Growth may
invalidate pointers; allocation failure preserves the original buffer.
Newly exposed bytes are uninitialized. Alignment must be a nonzero power of two;
invalid alignment throws invalid_argument, excessive size throws length_error,
and allocation failure throws bad_alloc. Buffer alignment is retained on growth. `append(span)` and `append(pointer, size)`
add bytes, including this buffer's own logical contents or subranges, safely across
reallocation. Empty input is a no-op; null nonempty input or an internal source
outside the logical size throws invalid_argument. Length overflow throws
length_error before allocation. `view()` returns a mutable or const `std::span`
over the logical bytes; it does not own storage and follows the buffer pointer's
lifetime and invalidation rules.

Special allocation remains explicit and optional. Use standard `std::pmr`
containers/strings with `LimitedResource`, `BasicMemory::resource()` or
`shared_resource()`. MiAllocator does not accept resource pointers. LimitedResource
adds a thread-safe logical requested-byte limit and reports live bytes, peak
bytes and live allocation count; it does not bound RSS or allocator overhead.
PMR's virtual calls, tracking and synchronization are paid only by resource users.
PMR assignment retains the destination resource; copy construction uses the
process-wide PMR default unless an explicit resource is supplied. Swap requires
equal resources. Use PMR element types for nested resource propagation; our
String alias always allocates through direct mimalloc.

`BasicMemory::instance()` retains one-time `init_virtual()` and `init_shared()`.
`init_virtual()` now configures a logical requested-byte budget instead of a
contiguous 16-byte-block virtual pool, eliminating that pool's scanning and
fragmentation behavior. `allocate()` returns null on zero size, invalid alignment
or budget exhaustion; before initialization it uses mimalloc directly.
`resource()` exposes this route to standard PMR allocators. `virtual_stats()` reports budget
usage. `allocate_shared()` / `shared_resource()` try the shared mapping first,
then follow the ordinary budget/direct fallback. `prefix_address` is reserved
and ignored. Raw frees require the original allocation-start pointer and byte
size. `MemoryType::Virtual` identifies live budget-backed allocations, not an
OS address-range reservation.

Shared storage uses POSIX `shm_open`/`mmap` on Linux and macOS, and named file
mappings on Windows. Initialization never replaces or unlinks an existing mapping.
POSIX names require one leading `/`; Windows names use native conventions such
as `Local\\kitzoo_memory`. The owner releases the mapping on singleton destruction
and unlinks its POSIX name. Only shared mappings retain the 16-byte-block region
allocator: their process-local bookkeeping cannot be replaced by ordinary
mimalloc allocation. Other processes may map bytes and use offsets, but cannot
independently allocate/free through this resource. Raw pointers, STL containers
and shared_ptr control blocks are not cross-process data formats.

`make_shared`, `make_unique` and default `Memory` use direct mimalloc allocation.
Select another resource with `make_shared_with_resource(resource, ...)`,
`Memory{resource}`, or standard `std::pmr::polymorphic_allocator::new_object` /
`delete_object` for custom-resource raw objects. Objects, arrays and control
blocks use the chosen allocator; constructor and control-block failures reclaim
storage. Arrays support allocator-aware elements and over-aligned types.
When using an explicit resource, it must outlive dependent containers, objects and weak-pointer control
blocks; free their allocations before destroying them. Explicit resource pointers must
be non-null. Do not change the process-wide PMR default to select a resource.

See [`examples/memory.cpp`](examples/memory.cpp) for direct and bounded allocation
using direct MiAllocator and optional PMR resources, including native shared mapping. Build the
`kitzoo_example_memory` target to run it.
The example separates containers, smart pointers, byte buffers, budgets and shared mappings.
Tests in `tests/memory` are grouped by allocator, advanced types, object/array ownership,
byte buffers, resources and the BasicMemory singleton. Weak-reference tests verify
that resource-backed control blocks remain allocated until the last weak reference is released.
`ObjectPool<T>` uses `memory::Vector` for its slot chunks and `memory::SharedPtr`
for shared leases, with mimalloc-backed control blocks. Its unique leases retain
the pool-specific deleter that returns slots. The pool must outlive all leases;
see [`examples/thread.cpp`](examples/thread.cpp).
`ConcurrentObjectPool<T>::add` takes `memory::UniquePtr<T>` created by
`memory::make_unique<T>`. Shared leases also use mimalloc-backed control blocks;
their pool-specific deleter returns objects to the queue.
Both `ObjectPool` and `ConcurrentObjectPool` are declared in
`<kitzoo/thread/object_pool.hpp>` and available through `kitzoo::thread`.

| Pool | Synchronization | Object lifecycle | Empty pool | Shutdown and counts |
| --- | --- | --- | --- | --- |
| `ObjectPool` | Caller synchronizes | Constructs on acquire; destroys on return; reuses storage | Grows storage | Live-object count |
| `ConcurrentObjectPool` | Concurrent queue | Reuses objects supplied through `add` | Blocking, try or timed acquire | Approximate available count; no close operation |

`ConcurrentObjectPool` retains object state between leases. It provides all three acquire
modes without a separate blocking pool implementation. It has no `close()` operation;
use timed acquire when a worker must periodically check for shutdown. Both pools must
outlive their leases and any threads accessing them.

Built-in `ThreadPool` and BS aliases (`BSThreadPool`, `BSLightThreadPool`, priority,
pause and wait-deadlock-check variants) are declared in `<kitzoo/thread/thread_pool.hpp>`.
Linking `kitzoo::thread` supplies their dependencies; a separate BS header is no longer needed.

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
| `KITZOO_WARNINGS_AS_ERRORS` | `ON` | Treat project compiler warnings as errors |

Enable crypto by adding `-DKITZOO_WITH_OPENSSL=ON` when configuring.
Mimalloc, spdlog and JSON are always available. Set `OPENSSL_ROOT_DIR` if OpenSSL
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

[`examples/os.cpp`](examples/os.cpp) demonstrates system queries, thread naming,
and filesystem operations through the unified OS module (`kitzoo_example_os`).
[`examples/utilities.cpp`](examples/utilities.cpp) demonstrates random helpers,
UUIDs, and inheriting `Singleton<T>` (`kitzoo_example_utilities`).

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
| [mimalloc](https://github.com/microsoft/mimalloc) | Allocator integration; required |

Development builds also use [GoogleTest](https://github.com/google/googletest)
for unit tests and [Google Benchmark](https://github.com/google/benchmark) for
benchmarks. Comparison benchmarks use spdlog and BS::thread_pool.

??????????????? flush???????? MiAllocator ?????
[?????????](docs/log_reference_analysis.md)? [logging ??](examples/logging.cpp)?
