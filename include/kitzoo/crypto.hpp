// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/crypto.hpp
// Description: Provides the crypto module entry point and includes APIs enabled
//              by the selected crypto integration.
// -----------------------------------------------------------------------------

#ifndef KITZOO_CRYPTO_HPP
#define KITZOO_CRYPTO_HPP

#if defined(KZ_WITH_OPENSSL)
#include <kitzoo/crypto/aes.hpp>
#endif

#endif // KITZOO_CRYPTO_HPP
