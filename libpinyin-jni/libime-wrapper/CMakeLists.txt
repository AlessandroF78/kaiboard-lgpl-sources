cmake_minimum_required(VERSION 3.18)
project(libime-android VERSION 1.1.14)

add_link_options("LINKER:--hash-style=gnu,--build-id=none")

# Skip install rules to avoid kenlm export issue
set(CMAKE_SKIP_INSTALL_RULES ON)

# ECM
find_package(ECM REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_MODULE_PATH})

# LibIntl (must be before Fcitx5Utils)
find_package(LibIntl REQUIRED CONFIG)

# DL (needed by Fcitx5Utils)
find_library(DL_LIBRARY dl)
if(DL_LIBRARY AND NOT TARGET DL::DL)
  add_library(DL::DL INTERFACE IMPORTED)
  set_target_properties(DL::DL PROPERTIES
    INTERFACE_LINK_LIBRARIES "${DL_LIBRARY}")
endif()

# Pthread (needed by Fcitx5Utils)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
if(NOT TARGET Pthread::Pthread)
  add_library(Pthread::Pthread INTERFACE IMPORTED)
  set_target_properties(Pthread::Pthread PROPERTIES
    INTERFACE_LINK_LIBRARIES "Threads::Threads")
endif()

# Fcitx5Utils
find_package(Fcitx5Utils REQUIRED CONFIG)

# Boost (cmake-based)
list(APPEND CMAKE_FIND_ROOT_PATH "${PREBUILT_DIR}/boost/arm64-v8a/lib/cmake")
find_package(Boost 1.61.0 REQUIRED COMPONENTS iostreams CONFIG)
add_compile_definitions("BOOST_ALL_NO_EMBEDDED_GDB_SCRIPTS")

# ZSTD alias
set(zstd_DIR "${PREBUILT_DIR}/zstd/arm64-v8a/lib/cmake/zstd")
find_package(zstd REQUIRED CONFIG)
if(NOT TARGET PkgConfig::ZSTD)
  add_library(PkgConfig::ZSTD ALIAS zstd::libzstd_static)
endif()

# Marisa alias
set(marisa_DIR "${PREBUILT_DIR}/marisa/arm64-v8a/lib/cmake/marisa")
find_package(marisa CONFIG)
if(TARGET marisa::marisa AND NOT TARGET PkgConfig::Marisa)
  add_library(PkgConfig::Marisa ALIAS marisa::marisa)
endif()

# libime options
option(ENABLE_TEST "" OFF)
option(ENABLE_GUI_TOOLS "" OFF)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "")

add_subdirectory("${LIBIME_SRC_DIR}" libime-build)

# kenlm path fix
target_compile_options(kenlm PRIVATE
  "-ffile-prefix-map=${LIBIME_SRC_DIR}/src/libime/core/kenlm=.")
