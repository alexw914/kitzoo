// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/utilities.hpp
// Description: Provides string processing, encoding, random values, UUIDs,
//              and optional AES operations.
// -----------------------------------------------------------------------------

#ifndef KITZOO_UTILITIES_HPP
#define KITZOO_UTILITIES_HPP

#if defined(KZ_WITH_OPENSSL) && KZ_WITH_OPENSSL
#include <kitzoo/utilities/aes.hpp>
#endif

#include <kitzoo/utilities/base64.hpp>
#include <kitzoo/utilities/random.hpp>
#include <kitzoo/utilities/str.hpp>
#include <kitzoo/utilities/uuid.hpp>

#endif // KITZOO_UTILITIES_HPP
