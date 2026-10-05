// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/core/define.h
// Description: Detects the active compiler, operating system, architecture, and
//              C++ language level for portable conditional compilation.
// -----------------------------------------------------------------------------

#ifndef KITZOO_CORE_DEFINE_H
#define KITZOO_CORE_DEFINE_H

#if defined(__clang__)
#define KZ_COMPILER_CLANG 1
#define KZ_COMPILER_GCC 0
#define KZ_COMPILER_MSVC 0
#elif defined(_MSC_VER)
#define KZ_COMPILER_CLANG 0
#define KZ_COMPILER_GCC 0
#define KZ_COMPILER_MSVC 1
#elif defined(__GNUC__)
#define KZ_COMPILER_CLANG 0
#define KZ_COMPILER_GCC 1
#define KZ_COMPILER_MSVC 0
#else
#define KZ_COMPILER_CLANG 0
#define KZ_COMPILER_GCC 0
#define KZ_COMPILER_MSVC 0
#endif

#if defined(_WIN32)
#define KZ_PLATFORM_WINDOWS 1
#define KZ_PLATFORM_LINUX 0
#define KZ_PLATFORM_MACOS 0
#elif defined(__APPLE__)
#define KZ_PLATFORM_WINDOWS 0
#define KZ_PLATFORM_LINUX 0
#define KZ_PLATFORM_MACOS 1
#elif defined(__linux__)
#define KZ_PLATFORM_WINDOWS 0
#define KZ_PLATFORM_LINUX 1
#define KZ_PLATFORM_MACOS 0
#else
#define KZ_PLATFORM_WINDOWS 0
#define KZ_PLATFORM_LINUX 0
#define KZ_PLATFORM_MACOS 0
#endif

#if defined(_MSVC_LANG) && _MSVC_LANG > __cplusplus
#define KZ_CXX_STANDARD (_MSVC_LANG)
#else
#define KZ_CXX_STANDARD (__cplusplus)
#endif

#endif // KITZOO_CORE_DEFINE_H
