// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: examples/os.cpp
// Description: Demonstrates system queries, thread naming, and filesystem operations.
// -----------------------------------------------------------------------------

#include <kitzoo/os.hpp>

#include <cstdio>
#include <string>

using namespace kitzoo;

auto main() -> int {
  // System queries and current-thread operations.
  std::printf("host: %s, user: %s\n", os::hostname().c_str(), os::username().c_str());
  std::printf("cpus: %u, page size: %zu, pid: %ld\n", os::cpu_count(), os::page_size(), os::current_pid());
  std::printf("physical memory: %llu bytes\n", static_cast<unsigned long long>(os::total_memory()));
  std::printf("home: %s\n", os::home_dir().c_str());
  if (const auto path = os::get_env("PATH")) {
    std::printf("PATH contains %zu bytes\n", path->size());
  }
  std::printf("thread named: %s\n", os::set_current_thread_name("main", "example") ? "yes" : "no");
  std::printf("process CPU time: %llu ns\n", static_cast<unsigned long long>(os::get_cpu_timestamp_ns()));

  // Filesystem paths, file updates, and directory cleanup.
  const auto original_dir = std::filesystem::current_path();
  std::printf("working directory: %s\n", original_dir.string().c_str());

  // Unique temp dir; the caller owns it and cleans up.
  const auto dir = os::temp_directory();
  std::filesystem::current_path(dir);
  const auto config = std::filesystem::path{"app.conf"};

  os::write_file(config, "mode=fast\n");
  std::printf("wrote: %s", os::read_file(config).c_str());

  // Atomic overwrite: a temp file is renamed over the target.
  const std::string updated{"mode=safe\n"};
  os::atomic_write(config, updated);
  std::printf("now:   %s", os::read_file(config).c_str());

  const auto entries = os::list_directory(".");
  std::printf("temp dir holds %zu entr%s\n", entries.size(), entries.size() == 1 ? "y" : "ies");
  const auto size = std::filesystem::file_size(config);
  std::printf("config size: %llu bytes\n", static_cast<unsigned long long>(size));

  os::make_directory("results/nested", true);
  os::copy_file_to(config, "results/copy.conf");
  os::append_file("results/copy.conf", "frames=12\n");
  std::filesystem::rename("results/copy.conf", "results/renamed.conf");
  std::printf("results folder: %s, renamed file: %s, recursive entries: %zu\n", os::is_dir("results") ? "yes" : "no",
              os::is_regular("results/renamed.conf") ? "yes" : "no", os::list_directory(".", true).size());
  os::remove_path("results/renamed.conf"); // Default removal does not recurse.

  std::filesystem::current_path(original_dir);
  os::remove_path(dir, true);
  return 0;
}
