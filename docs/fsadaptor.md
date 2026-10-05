# FsAdaptor

Include `<kitzoo/os.hpp>` or `<kitzoo/os/fsadaptor.hpp>`, link `kitzoo::os`,
and access filesystem operations through `kitzoo::os::FsAdaptor::instance()`.
The former `<kitzoo/os/filesystem.hpp>` forwarding API has been removed.

## Operations

| Method | Behavior |
| --- | --- |
| read_file / read_text | Read the complete file, preserving binary bytes. |
| write_file / write_text | Create or truncate a file; parents must already exist. |
| atomic_write | Create missing parents, write a temporary sibling, then replace the destination. |
| temp_directory | Create a unique directory; callers own its cleanup. |
| current_path / set_current_path | Query or change the process-wide working directory. |
| exists / is_file / is_folder | Follow symlinks; missing paths return false. |
| is_symlink | Inspect the link itself, including dangling links. |
| is_empty | Check an empty file or directory. |
| mkdir(path, parents=false) | Create one directory; true creates missing parents. |
| rm(path, recursive=false) | Remove a file or empty directory; true removes a tree. |
| listdir(path, recursive=false) | Return sorted entries; true includes descendants. |
| append_text | Create a file if needed and append bytes; parents must exist. |
| copy_file(source, destination, overwrite=false) | Copy a file; replacement requires true. |
| rename | Rename a file or directory with platform filesystem semantics. |
| absolute / canonical | Resolve paths; canonical requires an existing target. |
| file_size | Return file size in bytes. |

`create_directories`, `remove_all`, and `list_directory` are also adaptor methods.
`list_directory` provides a sorted nonrecursive listing.

Error-code overloads place `std::error_code&` before optional flags:
`fs.mkdir(path, ec, true)`, `fs.rm(path, ec, true)`,
`fs.listdir(path, ec, true)`, and `fs.copy_file(source, destination, ec, true)`.
Overloads without an error code throw `std::filesystem::filesystem_error`
for filesystem failures. Allocation failures may still throw from either form.
Recursive listing and removal do not follow directory symlinks.
Failed recursive removal can leave a partially removed tree.

Atomic replacement uses a sibling temporary file, including Unicode paths on
Windows. It does not provide a power-loss durability guarantee through fsync.
Changing the working directory affects every thread in the process.

The implementation targets Linux, macOS, and Windows using C++20 filesystem APIs.
Symlink tests require permission to create symlinks, which may be unavailable on
Windows. Platform verification for each change is recorded in its delivery report.
