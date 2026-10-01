# ---------------------------------------------------------------------------
# Compiler warning configuration
# ---------------------------------------------------------------------------
# kitzoo_apply_warnings(target)
#   Applies stringent warning flags to a target. Compiled targets get them
#   PRIVATE; header-only (INTERFACE) targets get them wrapped in
#   $<BUILD_INTERFACE:...>. Either way, warnings never propagate to
#   consumers of the INSTALLED library.
# ---------------------------------------------------------------------------

function(kitzoo_apply_warnings target)
    get_target_property(_type ${target} TYPE)
    if(_type STREQUAL "INTERFACE_LIBRARY")
        set(_scope INTERFACE)
    else()
        set(_scope PRIVATE)
    endif()

    if(MSVC)
        target_compile_options(${target} ${_scope}
            /W4 /w14242 /w14254 /w14263 /w14265 /w14287
            /w14296 /w14311 /w14545 /w14546 /w14547 /w14549
            /w14555 /w14619 /w14640 /w14826 /w14905 /w14906
            /w14928)
    else()
        set(_flags
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Wcast-align
            -Wunused
            -Woverloaded-virtual
            -Wconversion
            -Wsign-conversion
            -Wnull-dereference
            -Wdouble-promotion
            -Wimplicit-fallthrough
            -Wextra-semi
            -Wmisleading-indentation
            -Wduplicated-cond
            -Wduplicated-branches
            -Wlogical-op
            -Wuseless-cast
            $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wno-unknown-warning-option>
            $<$<BOOL:${KITZOO_WARNINGS_AS_ERRORS}>:-Werror>)

        if(_type STREQUAL "INTERFACE_LIBRARY")
            # Header-only modules compile nothing themselves; the flags exist
            # for in-project consumers (tests/benchmarks/examples). Restrict
            # them to the build tree so installed consumers are NOT forced
            # into our warning set (especially -Werror).
            list(TRANSFORM _flags PREPEND "$<BUILD_INTERFACE:")
            list(TRANSFORM _flags APPEND ">")
        endif()
        target_compile_options(${target} ${_scope} ${_flags})
    endif()
endfunction()