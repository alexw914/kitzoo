# Build only mimalloc's static C allocator; kitzoo uses its explicit mi_* API.
kitzoo_fetch_dependency(
    mimalloc
    GIT_REPOSITORY https://github.com/microsoft/mimalloc.git
    GIT_TAG v3.5.3
    OPTIONS MI_BUILD_SHARED=OFF MI_BUILD_STATIC=ON MI_BUILD_OBJECT=OFF MI_BUILD_TESTS=OFF
            MI_OVERRIDE=OFF MI_OSX_ZONE=OFF MI_OSX_INTERPOSE=OFF MI_INSTALL_TOPLEVEL=ON)
