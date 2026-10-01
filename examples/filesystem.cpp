// ---------------------------------------------------------------------------
// kitzoo example: filesystem helpers
//
// Demonstrates:
//   - current_path / set_current_path + relative paths
//   - temp_directory + write_text / read_text
//   - atomic_write (write-to-temp + rename: readers never see a torn write)
//   - list_directory / file_size with standard filesystem errors
// ---------------------------------------------------------------------------

#include <kitzoo/filesystem/filesystem.hpp>

#include <cstdio>
#include <string>

using namespace kitzoo;

int main() {
    auto const original_dir = fs::current_path();
    std::printf("working directory: %s\n", original_dir.string().c_str());

    // Unique temp dir; the caller owns it and cleans up.
    auto const dir = fs::temp_directory();
    fs::set_current_path(dir);
    auto const config = std::filesystem::path{"app.conf"};

    fs::write_text(config, "mode=fast\n");
    std::printf("wrote: %s", fs::read_text(config).c_str());

    // Atomic overwrite: a temp file is renamed over the target.
    std::string const updated{"mode=safe\n"};
    fs::atomic_write(config, std::span{updated.data(), updated.size()});
    std::printf("now:   %s", fs::read_text(config).c_str());

    auto const entries = fs::list_directory(".");
    std::printf("temp dir holds %zu entr%s\n", entries.size(), entries.size() == 1 ? "y" : "ies");
    auto const size = fs::file_size(config);
    std::printf("config size: %llu bytes\n", static_cast<unsigned long long>(size));

    fs::set_current_path(original_dir);
    fs::remove_all(dir);
    return 0;
}
