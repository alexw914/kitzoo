// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/core/macro.hpp
// Description: Defines portable attribute and utility macros used throughout
//              the public kitzoo API.
// -----------------------------------------------------------------------------

#ifndef KITZOO_CORE_MACRO_HPP
#define KITZOO_CORE_MACRO_HPP

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

#endif // KITZOO_CORE_MACRO_HPP
