// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/core/macro.hpp
// Description: Defines portable attribute and utility macros used throughout
//              the public kitzoo API.
// -----------------------------------------------------------------------------

#pragma once

#if defined(_WIN32)
#if defined(KITZOO_SHARED_LIBRARY)
#define KZ_EXPORT __declspec(dllexport)
#define KZ_NO_EXPORT
#else
#define KZ_EXPORT
#define KZ_NO_EXPORT
#endif
#else
#if defined(KITZOO_SHARED_LIBRARY)
#define KZ_EXPORT __attribute__((visibility("default")))
#define KZ_NO_EXPORT __attribute__((visibility("hidden")))
#else
#define KZ_EXPORT
#define KZ_NO_EXPORT
#endif
#endif

#define KZ_DEPRECATED [[deprecated]]
#define KZ_DEPRECATED_MSG(msg) [[deprecated(msg)]]
#define KZ_NORETURN [[noreturn]]
#define KZ_NODISCARD [[nodiscard]]
#define KZ_MAYBE_UNUSED [[maybe_unused]]

#if defined(__GNUC__) || defined(__clang__)
#define KZ_ALWAYS_INLINE __attribute__((always_inline)) inline
#define KZ_RESTRICT __restrict__
#else
#define KZ_ALWAYS_INLINE inline
#define KZ_RESTRICT
#endif

#define KZ_STRINGIFY(value) #value
#define KZ_STRINGIFY_EXPANDED(value) KZ_STRINGIFY(value)

#define KITZOO_EXPORT KZ_EXPORT
#define KITZOO_NO_EXPORT KZ_NO_EXPORT
#define KITZOO_DEPRECATED KZ_DEPRECATED
#define KITZOO_DEPRECATED_MSG(msg) KZ_DEPRECATED_MSG(msg)
#define KITZOO_NORETURN KZ_NORETURN
#define KITZOO_NODISCARD KZ_NODISCARD
#define KITZOO_MAYBE_UNUSED KZ_MAYBE_UNUSED
#define KITZOO_ALWAYS_INLINE KZ_ALWAYS_INLINE
#define KITZOO_RESTRICT KZ_RESTRICT
#define KITZOO_STRINGIFY(value) KZ_STRINGIFY(value)
#define KITZOO_STRINGIFY_EXPANDED(value) KZ_STRINGIFY_EXPANDED(value)
