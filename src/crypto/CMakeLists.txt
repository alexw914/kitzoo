find_package(OpenSSL REQUIRED)

kitzoo_add_module(crypto SOURCES aes.cpp)
target_compile_definitions(kitzoo_crypto PUBLIC KZ_WITH_OPENSSL=1)
target_link_libraries(kitzoo_crypto PRIVATE OpenSSL::Crypto)
