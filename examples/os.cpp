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
  auto& adaptor = os::OSAdaptor::instance();
  std::printf("host: %s, user: %s\n", adaptor.hostname().c_str(), adaptor.username().c_str());
  std::printf("cpus: %u, page size: %zu, pid: %ld\n", adaptor.cpu_count(), adaptor.page_size(), adaptor.current_pid());
  std::printf("physical memory: %llu bytes\n", static_cast<unsigned long long>(adaptor.total_memory()));
  std::printf("home: %s\n", adaptor.home_dir().c_str());
  if (auto const path = adaptor.get_env("PATH")) {
    std::printf("PATH contains %zu bytes\n", path->size());
  }
  std::printf("thread named: %s\n", adaptor.set_current_thread_name("main", "example") ? "yes" : "no");
  std::printf("process CPU time: %llu ns\n", static_cast<unsigned long long>(adaptor.get_cpu_timestamp_ns()));

  // Filesystem paths, file updates, and directory cleanup.
  auto& fs = os::FsAdaptor::instance();
  auto const original_dir = fs.current_path();
  std::printf("working directory: %s\n", original_dir.string().c_str());

  // Unique temp dir; the caller owns it and cleans up.
  auto const dir = fs.temp_directory();
  fs.set_current_path(dir);
  auto const config = std::filesystem::path{"app.conf"};

  fs.write_text(config, "mode=fast\n");
  std::printf("wrote: %s", fs.read_text(config).c_str());

  // Atomic overwrite: a temp file is renamed over the target.
  std::string const updated{"mode=safe\n"};
  fs.atomic_write(config, std::span{updated.data(), updated.size()});
  std::printf("now:   %s", fs.read_text(config).c_str());

  auto const entries = fs.listdir(".");
  std::printf("temp dir holds %zu entr%s\n", entries.size(), entries.size() == 1 ? "y" : "ies");
  auto const size = fs.file_size(config);
  std::printf("config size: %llu bytes\n", static_cast<unsigned long long>(size));

  fs.mkdir("results/nested", true);
  fs.copy_file(config, "results/copy.conf");
  fs.append_text("results/copy.conf", "frames=12\n");
  fs.rename("results/copy.conf", "results/renamed.conf");
  std::printf("results folder: %s, renamed file: %s, recursive entries: %zu\n", fs.is_folder("results") ? "yes" : "no",
              fs.is_file("results/renamed.conf") ? "yes" : "no", fs.listdir(".", true).size());
  fs.rm("results/renamed.conf"); // Default removal does not recurse.

  fs.set_current_path(original_dir);
  fs.rm(dir, true);
  return 0;
}
