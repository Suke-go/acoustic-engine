# CMake configuration for Acoustic Engine
cmake_minimum_required(VERSION 3.16)
project(acoustic_engine VERSION 0.2.0 LANGUAGES C)

# C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Options
option(AE_USE_LIBMYSOFA "Enable SOFA HRTF support (libmysofa)" ON)
option(AE_ENABLE_EXTERNAL_DECODER "Enable external decoder for non-WAV formats" ON)
option(AE_BUILD_UNITY_PLUGIN "Build Unity plugin" OFF)
option(AE_BUILD_UE_PLUGIN "Build Unreal Engine plugin" OFF)
option(AE_BUILD_VST3 "Build VST3 plugin" OFF)
option(AE_BUILD_AU "Build Audio Unit plugin" OFF)

# Symbol visibility
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /O2)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -O2)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        add_compile_options(-msse2)
    endif()
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Math library (Linux needs explicit linking)
if(UNIX AND NOT APPLE)
    link_libraries(m)
endif()

#==============================================================================
# Optional dependency: libmysofa + zlib (SOFA HRTF)
#==============================================================================
if(AE_USE_LIBMYSOFA)
    include(FetchContent)

    set(ZLIB_BUILD_SHARED_LIBS OFF)
    set(ZLIB_ENABLE_TESTS OFF)
    FetchContent_Declare(
        zlib
        GIT_REPOSITORY https://github.com/madler/zlib.git
        GIT_TAG v1.3.1
    )
    FetchContent_MakeAvailable(zlib)

    FetchContent_Declare(
        libmysofa
        GIT_REPOSITORY https://github.com/hoene/libmysofa.git
        GIT_TAG v1.3.2
    )
    FetchContent_GetProperties(libmysofa)
    if(NOT libmysofa_POPULATED)
        FetchContent_Populate(libmysofa)
    endif()

    set(LIBMYSOFA_SRC_DIR ${libmysofa_SOURCE_DIR}/src)
    set(LIBMYSOFA_BIN_DIR ${CMAKE_BINARY_DIR}/libmysofa)
    file(MAKE_DIRECTORY ${LIBMYSOFA_BIN_DIR})

    set(HAVE_ZLIB 1)
    set(HAVE_LIBZ 1)
    set(HAVE_STDLIB_H 1)
    set(HAVE_STRING_H 1)
    set(HAVE_STDINT_H 1)
    set(HAVE_INTTYPES_H 1)
    set(HAVE_MALLOC 1)
    set(HAVE_REALLOC 1)
    set(HAVE_FREE 1)
    set(HAVE_MEMSET 1)
    set(HAVE_MEMCPY 1)
    set(HAVE_MEMMOVE 1)
    set(HAVE_STRCHR 1)
    set(HAVE_STRDUP 1)
    set(HAVE_STRCMP 1)
    set(HAVE_STRCASECMP 1)
    set(HAVE_STRNCASECMP 1)
    set(HAVE_SNPRINTF 1)

    configure_file(${LIBMYSOFA_SRC_DIR}/config.h.in
                   ${LIBMYSOFA_BIN_DIR}/config.h)
    configure_file(${LIBMYSOFA_SRC_DIR}/hrtf/mysofa_export.h.in
                   ${LIBMYSOFA_BIN_DIR}/mysofa_export.h)

    file(GLOB LIBMYSOFA_SOURCES
        ${LIBMYSOFA_SRC_DIR}/*.c
        ${LIBMYSOFA_SRC_DIR}/hrtf/*.c
        ${LIBMYSOFA_SRC_DIR}/hdf/*.c
        ${LIBMYSOFA_SRC_DIR}/resampler/*.c
    )

    add_library(mysofa STATIC ${LIBMYSOFA_SOURCES})
    target_include_directories(mysofa PUBLIC
        ${LIBMYSOFA_SRC_DIR}
        ${LIBMYSOFA_SRC_DIR}/hrtf
        ${LIBMYSOFA_BIN_DIR}
    )
    if(TARGET zlibstatic)
        set(ZLIB_TARGET zlibstatic)
    else()
        set(ZLIB_TARGET zlib)
    endif()
    target_link_libraries(mysofa PUBLIC ${ZLIB_TARGET})
    target_include_directories(mysofa PRIVATE ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
    target_compile_definitions(mysofa PRIVATE HAVE_CONFIG_H)
    target_compile_definitions(mysofa PUBLIC MYSOFA_STATIC)
endif()

#==============================================================================
# Main library
#==============================================================================
add_library(acoustic_engine SHARED
    src/acoustic_engine.c
    src/ae_audio_io.c
    src/ae_auditory.c
    src/ae_presets.c
    src/ae_dsp.c
    src/ae_spatial.c
    src/ae_reverb.c
    src/ae_analysis.c
    src/ae_math.c
    src/ae_simd.c
    src/ae_slm.c
    src/ae_propagation.c
    src/ae_dynamics.c
    src/ae_version.c
    src/ae_deesser.c
    src/ae_eq.c
    src/ae_drnl.c
    src/ae_modfb.c
    src/ae_perceptual.c
)
target_compile_definitions(acoustic_engine PRIVATE AE_BUILD_DLL)
target_include_directories(acoustic_engine PUBLIC ${CMAKE_SOURCE_DIR}/include)
if(UNIX AND NOT APPLE)
    target_link_libraries(acoustic_engine m)
endif()
if(AE_USE_LIBMYSOFA)
    target_compile_definitions(acoustic_engine PRIVATE AE_USE_LIBMYSOFA)
    target_link_libraries(acoustic_engine PRIVATE mysofa)
endif()
if(AE_ENABLE_EXTERNAL_DECODER)
    target_compile_definitions(acoustic_engine PRIVATE AE_ENABLE_EXTERNAL_DECODER=1)
else()
    target_compile_definitions(acoustic_engine PRIVATE AE_ENABLE_EXTERNAL_DECODER=0)
endif()

#==============================================================================
# Tests
#==============================================================================
enable_testing()

# Test: Math utilities
add_executable(test_math tests/test_math.c)
if(UNIX AND NOT APPLE)
    target_link_libraries(test_math m)
endif()
target_link_libraries(test_math acoustic_engine)
add_test(NAME test_math COMMAND test_math)

# Test: DSP primitives
add_executable(test_dsp tests/test_dsp.c)
if(UNIX AND NOT APPLE)
    target_link_libraries(test_dsp m)
endif()
target_link_libraries(test_dsp acoustic_engine)
add_test(NAME test_dsp COMMAND test_dsp)

# Test: Analysis functions
add_executable(test_analysis tests/test_analysis.c)
if(UNIX AND NOT APPLE)
    target_link_libraries(test_analysis m)
endif()
target_link_libraries(test_analysis acoustic_engine)
add_test(NAME test_analysis COMMAND test_analysis)

# Test: Auditory modeling
add_executable(test_auditory tests/test_auditory.c)
if(UNIX AND NOT APPLE)
    target_link_libraries(test_auditory m)
endif()
target_link_libraries(test_auditory acoustic_engine)
add_test(NAME test_auditory COMMAND test_auditory)

# Test: Physical propagation models
add_executable(test_propagation tests/test_propagation.c)
if(UNIX AND NOT APPLE)
    target_link_libraries(test_propagation m)
endif()
target_link_libraries(test_propagation acoustic_engine)
add_test(NAME test_propagation COMMAND test_propagation)

# Test: SIMD accuracy verification
add_executable(test_simd tests/test_simd.c)
if(UNIX AND NOT APPLE)
    target_link_libraries(test_simd m)
endif()
target_link_libraries(test_simd acoustic_engine)
add_test(NAME test_simd COMMAND test_simd)

#==============================================================================
# Custom target: Run all tests
#==============================================================================
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_math test_dsp test_analysis test_auditory test_propagation test_simd
    COMMENT "Running all tests..."
)


#==============================================================================
# Install (for future use)
#==============================================================================
# install(TARGETS acoustic_engine
#     LIBRARY DESTINATION lib
#     ARCHIVE DESTINATION lib
#     RUNTIME DESTINATION bin
# )
# install(FILES include/acoustic_engine.h DESTINATION include)

#==============================================================================
# Summary
#==============================================================================
message(STATUS "")
message(STATUS "=== Acoustic Engine Build Configuration ===")
message(STATUS "  Version:      ${PROJECT_VERSION}")
message(STATUS "  Build type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler:   ${CMAKE_C_COMPILER}")
message(STATUS "  Install dir:  ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
