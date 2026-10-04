// ---------------------------------------------------------------------------
// kitzoo example: OS queries, thread naming, and filesystem helpers
//
// Demonstrates:
//   - OSAdaptor singleton: host, user, CPU, memory, and process queries
//   - naming the current thread and querying process CPU time
//   - current_path / set_current_path + relative paths
//   - temp_directory + write_text / read_text
//   - atomic_write (write-to-temp + rename: readers never see a torn write)
//   - list_directory / file_size with standard filesystem errors
// ---------------------------------------------------------------------------

#include <kitzoo/os.hpp>

#include <cstdio>
#include <string>

using namespace kitzoo;

auto main() -> int {
    auto& adaptor = os::OSAdaptor::instance();
    std::printf("host: %s, user: %s\n", adaptor.hostname().c_str(), adaptor.username().c_str());
    std::printf("cpus: %u, page size: %zu, pid: %ld\n", adaptor.cpu_count(), adaptor.page_size(),
                adaptor.current_pid());
    std::printf("physical memory: %llu bytes\n",
                static_cast<unsigned long long>(adaptor.total_memory()));
    std::printf("home: %s\n", adaptor.home_dir().c_str());
    if (auto const path = adaptor.get_env("PATH")) {
        std::printf("PATH contains %zu bytes\n", path->size());
    }
    std::printf("thread named: %s\n",
                adaptor.set_current_thread_name("main", "example") ? "yes" : "no");
    std::printf("process CPU time: %llu ns\n",
                static_cast<unsigned long long>(adaptor.get_cpu_timestamp_ns()));

    auto const original_dir = os::current_path();
    std::printf("working directory: %s\n", original_dir.string().c_str());

    // Unique temp dir; the caller owns it and cleans up.
    auto const dir = os::temp_directory();
    os::set_current_path(dir);
    auto const config = std::filesystem::path{"app.conf"};

    os::write_text(config, "mode=fast\n");
    std::printf("wrote: %s", os::read_text(config).c_str());

    // Atomic overwrite: a temp file is renamed over the target.
    std::string const updated{"mode=safe\n"};
    os::atomic_write(config, std::span{updated.data(), updated.size()});
    std::printf("now:   %s", os::read_text(config).c_str());

    auto const entries = os::list_directory(".");
    std::printf("temp dir holds %zu entr%s\n", entries.size(), entries.size() == 1 ? "y" : "ies");
    auto const size = os::file_size(config);
    std::printf("config size: %llu bytes\n", static_cast<unsigned long long>(size));

    os::set_current_path(original_dir);
    os::remove_all(dir);
    return 0;
}
