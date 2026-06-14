cmake_minimum_required(VERSION 4.1)
project(olympia-g2 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# =============================================================================
# Compiler-flag conventions (read before "tidying up" this file)
# =============================================================================
# The 32->64-bit modernization ladder (Phases 1-8) is complete; every warning
# class below was triaged to ZERO and locked as an error. Issue #13
# (implicit-int-conversion) is likewise driven to zero and locked, as a
# separate code-quality track -- NOT a 64-bit hazard. See CLAUDE.md
# "Modernization status" for the per-phase history and "Warning policy" for how
# new warnings are triaged.
#
# 1. Enforced classes are written as an explicit pair, one class per line:
#        -Wfoo -Werror=foo
#    The bare -Wfoo is technically redundant (-Werror=foo already enables it),
#    but the pair is deliberate: it records that the class was driven to zero
#    and locked. Do NOT collapse these to bare -Werror=foo -- the redundancy is
#    a record of completed work, not clutter.
#
# 2. -Wno-foo suppressions are legacy idioms this ~60K-line codebase still
#    relies on and that are NOT being chased down right now.
#
# The shared set is applied identically to every target via
# olympia_compile_flags(), with optional sanitizers via
# olympia_enable_sanitizers(). -Wformat -Werror=format is enforced project-wide
# on both targets: the full format class is an error, no sub-suppression (the
# non-literal-format sites were fixed in issue #6). There is no per-target
# warning difference.
#
# Optimization level is owned by the build type / preset (debug -> default -O0
# -g, release -> -O3 -DNDEBUG, asan-ubsan -> -O1 -g ... via its preset override),
# NOT pinned per target. A hardcoded target -O used to be appended after the
# build-type flags and clobber the preset's choice (release silently built at
# -Og/-O1, never -O3); removed in issue #14 (mirrors ../olympia-g1@b149508).
# =============================================================================

option(OLYMPIA_SANITIZE "Enable sanitizer instrumentation" OFF)
set(OLYMPIA_SANITIZERS "address,undefined" CACHE STRING "Comma-separated sanitizers, e.g. address,undefined")

# All shared engine warning flags, applied identically to every target.
# Optimization is owned by the build type / preset (see the conventions block).
function(olympia_compile_flags tgt)
    target_compile_options(${tgt} PRIVATE
        # --- Suppressions: legacy idioms left as-is ---
        -Wno-comment
        -Wno-compare-distinct-pointer-types
        -Wno-dangling-else
        -Wno-deprecated-declarations
        -Wno-extra-tokens
        -Wno-incompatible-library-redeclaration
        -Wno-logical-not-parentheses
        -Wno-macro-redefined
        -Wno-multichar
        -Wno-non-literal-null-conversion
        -Wno-parentheses
        -Wno-tautological-constant-out-of-range-compare

        # --- Format/vararg checking enforced (issues #5/#6) ---
        # The full class incl. format-security / format-nonliteral is an error;
        # the non-literal sites were fixed in #6, so no sub-suppression.
        -Wformat -Werror=format

        # --- Modernization classes triaged to 0 and locked (CLAUDE.md) ---
        -Wint-to-pointer-cast -Werror=int-to-pointer-cast                # Phase 1
        -Wpointer-to-int-cast -Werror=pointer-to-int-cast                # Phase 1
        -Wincompatible-pointer-types -Werror=incompatible-pointer-types  # Phase 2
        -Wint-conversion -Werror=int-conversion                          # Phase 3
        -Wstrict-prototypes -Werror=strict-prototypes                    # Phase 4
        -Wmissing-prototypes -Werror=missing-prototypes                  # Phase 4
        -Wimplicit-function-declaration -Werror=implicit-function-declaration  # Phase 4
        -Wmissing-declarations -Werror=missing-declarations              # Phase 5
        # A sizeof(ptr) where sizeof(*ptr) is meant grows 4->8 bytes on LP64
        # (a real corruption risk). The MD5 bzero bug is fixed; audited clean.
        -Wsizeof-pointer-memaccess -Werror=sizeof-pointer-memaccess      # Phase 6 (#10)
        # Signed<->unsigned implicit conversions (arch-independent, large class).
        -Wsign-conversion -Werror=sign-conversion                        # Phase 7 (#11)
        # A non-void function that falls off the end leaves the caller reading a
        # garbage register -- on LP64 an 8-byte garbage long/pointer. This is the
        # class that caused the fact/100 "st -32" flicker (#9) via i_use().
        # Portable: -Wreturn-type catches fall-off-the-end on every compiler and
        # only warns by default, so the -Werror pair is needed everywhere. The
        # companion bare-`return;`-in-non-void class is Clang-guarded below
        # (-Wreturn-mismatch), since GCC enforces it without the flag. (#18)
        -Wreturn-type -Werror=return-type                                # Phase 8 (#12)
    )

    # Clang-only diagnostic spellings; GCC folds each into a broader class
    # (-Wconversion) not enabled here.
    if (CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options(${tgt} PRIVATE
            # The LP64 width-truncation class (long/size_t/ssize_t into int).
            -Wshorten-64-to-32 -Werror=shorten-64-to-32                  # Phase 6 (#10)
            # Clang splits bare `return;` in a non-void function out of
            # -Wreturn-type as its own class; GCC enforces it anyway (default
            # permerror on >=14, folded into -Wreturn-type on <=13). Guarding the
            # Clang-only spelling avoids a fatal "unrecognized option" on GCC <=13
            # (the option did not exist before GCC 14) and loses no coverage.
            -Wreturn-mismatch -Werror=return-mismatch                    # Phase 8 (#18)
            # Code-quality, NOT a 64-bit hazard: int->short/char narrowing into
            # entity struct fields. Identical on ILP32/LP64.
            -Wimplicit-int-conversion -Werror=implicit-int-conversion    # issue #13
        )
    endif()
endfunction()

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
add_executable(mapgen-g2)
target_sources(mapgen-g2 PRIVATE
    mapgen/mapgen.c mapgen/rnd.c mapgen/z.c
    lib/ilist.c lib/roads.c lib/tiles.c
)
target_sources(mapgen-g2 PRIVATE
    FILE_SET HEADERS
    BASE_DIRS
        mapgen lib
    FILES
        mapgen/z.h
        mapgen/proto.h
        lib/lists.h
)
target_include_directories(mapgen-g2 PRIVATE mapgen/ lib/)
# Require C11 at the target level (project-level CMAKE_C_STANDARD remains the
# default for future targets). See issue #4.
target_compile_features(mapgen-g2 PRIVATE c_std_11)
olympia_compile_flags(mapgen-g2)

# ------------------------------------------------------------------------------
# G2 olympia (main game engine)
# ------------------------------------------------------------------------------
add_executable(olympia-g2)
target_sources(olympia-g2 PRIVATE
    olympia/add.c olympia/adv.c olympia/alchem.c olympia/art.c olympia/basic.c
    olympia/beast.c olympia/build.c olympia/buy.c olympia/check.c olympia/c1.c
    olympia/c2.c olympia/cloud.c olympia/code.c olympia/combat.c olympia/day.c
    olympia/dir.c olympia/display.c olympia/eat.c olympia/faery.c olympia/garr.c
    olympia/gate.c olympia/glob.c olympia/gm.c olympia/hades.c olympia/immed.c
    olympia/input.c olympia/io.c olympia/loc.c olympia/lore.c olympia/main.c
    olympia/make.c olympia/move.c olympia/necro.c olympia/npc.c olympia/order.c
    olympia/perm.c olympia/produce.c olympia/quest.c olympia/relig.c
    olympia/report.c olympia/rnd.c olympia/savage.c olympia/scry.c olympia/seed.c
    olympia/sout.c olympia/stack.c olympia/stealth.c olympia/storm.c
    olympia/summary.c olympia/swear.c olympia/tunnel.c olympia/u.c olympia/use.c
    olympia/z.c

    lib/accept_ents.c lib/admits.c lib/cstrings.c lib/exit_views.c lib/fights.c lib/flag_ents.c
    lib/ilist.c lib/item_ents.c
    lib/orders.c lib/req_ents.c lib/roads.c lib/skill_ents.c lib/tiles.c lib/trades.c lib/wait_args.c
    lib/checked_alloc.c
)
target_sources(olympia-g2 PRIVATE
    FILE_SET HEADERS
    BASE_DIRS
        olympia lib
    FILES
        olympia/code.h olympia/dir.h olympia/display.h olympia/etc.h olympia/garr.h
        olympia/loc.h olympia/loop.h olympia/order.h olympia/sout.h olympia/stack.h
        olympia/swear.h olympia/u.h olympia/use.h
        olympia/z.h
        olympia/proto.h
        olympia/legacy.h

        lib/lists.h lib/checked_alloc.h
)
target_include_directories(olympia-g2 PRIVATE olympia/ lib/)
# Require C11 at the target level (project-level CMAKE_C_STANDARD remains the
# default for future targets). See issue #4.
target_compile_features(olympia-g2 PRIVATE c_std_11)
olympia_compile_flags(olympia-g2)

# Optional ASan/UBSan via the asan-ubsan preset (OLYMPIA_SANITIZE=ON).
olympia_enable_sanitizers(olympia-g2)
