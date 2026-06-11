cmake_minimum_required(VERSION 4.1)
project(olympia-g2 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

function(legacy_build_flags tgt)
    # Flags to hide legacy code warnings.
    # We'll need to roll some of these back when we're updating the code to run as 64-bit.
    target_compile_options(${tgt} PRIVATE
        -Wno-comment
        -Wno-compare-distinct-pointer-types
        -Wno-dangling-else
        -Wno-deprecated-declarations
        -Wno-deprecated-non-prototype
        -Wno-extra-tokens
        -Wno-format
        -Wno-format-security
        -Wno-implicit-function-declaration
        -Wno-incompatible-library-redeclaration
        -Wno-incompatible-pointer-types
        -Wno-int-conversion
        -Wno-int-to-pointer-cast
        -Wno-logical-not-parentheses
        -Wno-macro-redefined
        -Wno-multichar
        -Wno-non-literal-null-conversion
        -Wno-parentheses
        -Wno-pointer-to-int-cast
        -Wno-return-mismatch
        -Wno-return-type
        -Wno-sizeof-pointer-memaccess
        -Wno-tautological-constant-out-of-range-compare
    )
endfunction()

function(phase_1_build_flags tgt)
    # i/pointer lists triage - only pointer-cast errors.
    target_compile_options(${tgt} PRIVATE
        -Wint-to-pointer-cast -Werror=int-to-pointer-cast
        -Wpointer-to-int-cast -Werror=pointer-to-int-cast
    )
endfunction()

function(phase_2_build_flags tgt)
    # incompatible pointer types
    target_compile_options(${tgt} PRIVATE
        -Wincompatible-pointer-types -Werror=incompatible-pointer-types
    )
endfunction()

function(phase_3_build_flags tgt)
    # integer conversions
    target_compile_options(${tgt} PRIVATE
        -Wint-conversion -Werror=int-conversion
    )
endfunction()

function(phase_4_build_flags tgt)
    # forward declarations; start turning on errors
    target_compile_options(${tgt} PRIVATE
        -Wmissing-prototypes -Werror=missing-prototypes
        -Wstrict-prototypes -Werror=strict-prototypes
        -Wimplicit-function-declaration -Werror=implicit-function-declaration
    )
endfunction()

function(phase_5_build_flags tgt)
    # C11's enabled, sanitizers enabled
    target_compile_options(${tgt} PRIVATE
        -Wmissing-declarations -Werror=missing-declarations
        # string literal to char* is a classic legacy footgun
        #-Wwrite-strings #-Werror=write-strings
        -fno-common
        -fno-omit-frame-pointer
        -fno-optimize-sibling-calls
        -fsanitize=address,undefined
        -fno-sanitize-recover=all
    )
    target_link_options(${tgt} PRIVATE
        -fno-common
        -fno-omit-frame-pointer
        -fno-optimize-sibling-calls
        -fsanitize=address,undefined
        -fno-sanitize-recover=all
    )
endfunction()

option(OLYMPIA_SANITIZE "Enable sanitizer instrumentation" OFF)
set(OLYMPIA_SANITIZERS "" CACHE STRING "Comma-separated sanitizers, e.g. address,undefined"
    address,undefined
)

function(olympia_enable_sanitizers tgt)
    if(NOT OLYMPIA_SANITIZE)
        return()
    endif()
    if(NOT OLYMPIA_SANITIZERS)
        message(FATAL_ERROR "OLYMPIA_SANITIZE is ON but OLYMPIA_SANITIZERS is empty")
    endif()

    # Sanitizers are supported on Clang/GCC; MSVC uses different flags.
    if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        # Compile and Link flags must match!
        target_compile_options(${tgt} PRIVATE
            -fsanitize=${OLYMPIA_SANITIZERS}
            -fno-omit-frame-pointer
            -fno-optimize-sibling-calls
            -fno-sanitize-recover=all
            -fno-common
        )
        target_link_options(${tgt} PRIVATE
            -fsanitize=${OLYMPIA_SANITIZERS}
            -fno-omit-frame-pointer
            -fno-optimize-sibling-calls
            -fno-sanitize-recover=all
            -fno-common
        )
        if (CMAKE_C_COMPILER_ID MATCHES "Clang")
            # -fsanitize-address-use-after-scope is Clang-only
            target_compile_options(${tgt} PRIVATE
                -fsanitize-address-use-after-scope
            )
            target_link_options(${tgt} PRIVATE
                -fsanitize-address-use-after-scope
            )
        endif()
    else()
        message(WARNING "OLYMPIA_SANITIZE is ON but compiler is not Clang/GNU; sanitizers not enabled.")
    endif()
endfunction()

# Engine-specific flags to hide legacy code warnings. We'll need to roll
# some of these back when we're updating the code to run as 64-bit.
set(LEGACY_C_FLAGS
        -Wno-comment
        -Wno-compare-distinct-pointer-types
        -Wno-dangling-else
        -Wno-deprecated-declarations
        -Wno-deprecated-non-prototype
        -Wno-extra-tokens
        -Wno-format
        -Wno-format-security
        -Wno-implicit-function-declaration
        -Wno-incompatible-library-redeclaration
        -Wno-incompatible-pointer-types
        -Wno-int-conversion
        -Wno-int-to-pointer-cast
        -Wno-logical-not-parentheses
        -Wno-macro-redefined
        -Wno-multichar
        -Wno-non-literal-null-conversion
        -Wno-parentheses
        -Wno-pointer-to-int-cast
        -Wno-return-mismatch
        -Wno-return-type
        -Wno-sizeof-pointer-memaccess
        -Wno-tautological-constant-out-of-range-compare
)

# Create a stricter set of compiler flags to catch
set(LEGACY_C_FLAGS_STRICT
        ${LEGACY_C_FLAGS}
)

# Remove suppressions that hide 64-bit hazards
list(REMOVE_ITEM LEGACY_C_FLAGS_STRICT
        -Wno-int-conversion
        -Wno-int-to-pointer-cast
        -Wno-pointer-to-int-cast
        -Wno-implicit-function-declaration
        -Wno-return-mismatch
        -Wno-return-type
)

# ------------------------------------------------------------------------------
# 32-bit build support
# ------------------------------------------------------------------------------
# Enable 32-bit builds on 64-bit Linux systems for generating golden test files
# from the original legacy code before 64-bit refactoring.
#
# Usage:
#   mkdir build32 && cd build32
#   cmake -DBUILD_32BIT=ON ..
#   cmake --build .
#
option(BUILD_32BIT "Build 32-bit binaries (requires gcc-multilib on Linux)" OFF)

if (BUILD_32BIT)
    message(STATUS "32-bit build enabled (-g -m32)")
    add_compile_options(-g -m32)
    add_link_options(-g -m32)
endif()

# Platform-specific defines
if (LINUX)
    add_compile_definitions(OLYMPIA_CC_LINUX)
endif ()
if (WIN32)
    add_compile_definitions(OLYMPIA_CC_WINDOWS)
endif ()

# ------------------------------------------------------------------------------
# G2 mapgen
# ------------------------------------------------------------------------------
add_executable(g2-mapgen)
target_sources(g2-mapgen PRIVATE
    g2/mapgen/mapgen.c g2/mapgen/rnd.c g2/mapgen/z.c
    lib/ilist.c lib/roads.c lib/tiles.c
)
target_sources(g2-mapgen PRIVATE
    FILE_SET HEADERS
    BASE_DIRS
        g2/mapgen lib
    FILES
        g2/mapgen/z.h
        lib/lists.h
)
target_include_directories(g2-mapgen PRIVATE g2/mapgen/ lib/)
target_compile_options(g2-mapgen PRIVATE
    ${LEGACY_C_FLAGS} -O1 -g
    -Wint-conversion -Werror=int-conversion
    -Wint-to-pointer-cast -Werror=int-to-pointer-cast
    -Wpointer-to-int-cast -Werror=pointer-to-int-cast
    -Wincompatible-pointer-types -Werror=incompatible-pointer-types
)

# ------------------------------------------------------------------------------
# G2 olympia (main game engine)
# ------------------------------------------------------------------------------
add_executable(g2-olympia)
target_sources(g2-olympia PRIVATE
    g2/olympia/add.c g2/olympia/adv.c g2/olympia/alchem.c g2/olympia/art.c g2/olympia/basic.c
    g2/olympia/beast.c g2/olympia/build.c g2/olympia/buy.c g2/olympia/check.c g2/olympia/c1.c
    g2/olympia/c2.c g2/olympia/cloud.c g2/olympia/code.c g2/olympia/combat.c g2/olympia/day.c
    g2/olympia/dir.c g2/olympia/display.c g2/olympia/eat.c g2/olympia/faery.c g2/olympia/garr.c
    g2/olympia/gate.c g2/olympia/glob.c g2/olympia/gm.c g2/olympia/hades.c g2/olympia/immed.c
    g2/olympia/input.c g2/olympia/io.c g2/olympia/loc.c g2/olympia/lore.c g2/olympia/main.c
    g2/olympia/make.c g2/olympia/move.c g2/olympia/necro.c g2/olympia/npc.c g2/olympia/order.c
    g2/olympia/perm.c g2/olympia/produce.c g2/olympia/quest.c g2/olympia/relig.c
    g2/olympia/report.c g2/olympia/rnd.c g2/olympia/savage.c g2/olympia/scry.c g2/olympia/seed.c
    g2/olympia/sout.c g2/olympia/stack.c g2/olympia/stealth.c g2/olympia/storm.c
    g2/olympia/summary.c g2/olympia/swear.c g2/olympia/tunnel.c g2/olympia/u.c g2/olympia/use.c
    g2/olympia/z.c

    lib/accept_ents.c lib/admits.c lib/cstrings.c lib/exit_views.c lib/fights.c lib/flag_ents.c
    lib/ilist.c lib/item_ents.c
    lib/orders.c lib/req_ents.c lib/roads.c lib/skill_ents.c lib/tiles.c lib/trades.c lib/wait_args.c
    lib/checked_alloc.c
)
target_sources(g2-olympia PRIVATE
    FILE_SET HEADERS
    BASE_DIRS
        g2/olympia lib
    FILES
        g2/olympia/code.h g2/olympia/dir.h g2/olympia/display.h g2/olympia/etc.h g2/olympia/garr.h
        g2/olympia/loc.h g2/olympia/loop.h g2/olympia/order.h g2/olympia/sout.h g2/olympia/stack.h
        g2/olympia/swear.h g2/olympia/u.h g2/olympia/use.h
        g2/olympia/z.h
        g2/olympia/legacy.h

        lib/lists.h lib/checked_alloc.h
)
target_include_directories(g2-olympia PRIVATE g2/olympia/ lib/)
target_compile_options(g2-olympia PRIVATE
    ${LEGACY_C_FLAGS} -Og -g
    # Phase 1 - list triage - only pointer-cast errors
    -Wint-to-pointer-cast -Werror=int-to-pointer-cast
    -Wpointer-to-int-cast -Werror=pointer-to-int-cast
    # Phase 2 - incompatible pointer types
    -Wincompatible-pointer-types -Werror=incompatible-pointer-types
    # Phase 3 - integer conversions
    -Wint-conversion -Werror=int-conversion
)

# Optional ASan/UBSan via the asan-ubsan preset (OLYMPIA_SANITIZE=ON).
olympia_enable_sanitizers(g2-olympia)
