cmake_minimum_required(VERSION 3.22.1)
project("kenlm-jni")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(KENLM_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/kenlm)
set(BOOST_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/include)

include_directories(
    ${KENLM_ROOT}
    ${BOOST_ROOT}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(KENLM_LM_SOURCES
    ${KENLM_ROOT}/lm/bhiksha.cc
    ${KENLM_ROOT}/lm/binary_format.cc
    ${KENLM_ROOT}/lm/build_binary_stub.cc
    ${KENLM_ROOT}/lm/config.cc
    ${KENLM_ROOT}/lm/lm_exception.cc
    ${KENLM_ROOT}/lm/model.cc
    ${KENLM_ROOT}/lm/quantize.cc
    ${KENLM_ROOT}/lm/read_arpa.cc
    ${KENLM_ROOT}/lm/search_hashed.cc
    ${KENLM_ROOT}/lm/search_trie.cc
    ${KENLM_ROOT}/lm/trie.cc
    ${KENLM_ROOT}/lm/trie_sort.cc
    ${KENLM_ROOT}/lm/virtual_interface.cc
    ${KENLM_ROOT}/lm/vocab.cc
)

set(KENLM_UTIL_SOURCES
    ${KENLM_ROOT}/util/bit_packing.cc
    ${KENLM_ROOT}/util/float_to_string.cc
    ${KENLM_ROOT}/util/integer_to_string.cc
    ${KENLM_ROOT}/util/double-conversion/bignum.cc
    ${KENLM_ROOT}/util/double-conversion/bignum-dtoa.cc
    ${KENLM_ROOT}/util/double-conversion/cached-powers.cc
    ${KENLM_ROOT}/util/double-conversion/double-to-string.cc
    ${KENLM_ROOT}/util/double-conversion/fast-dtoa.cc
    ${KENLM_ROOT}/util/double-conversion/fixed-dtoa.cc
    ${KENLM_ROOT}/util/double-conversion/string-to-double.cc
    ${KENLM_ROOT}/util/double-conversion/strtod.cc
    ${KENLM_ROOT}/util/ersatz_progress.cc
    ${KENLM_ROOT}/util/exception.cc
    ${KENLM_ROOT}/util/file.cc
    ${KENLM_ROOT}/util/file_piece.cc
    ${KENLM_ROOT}/util/mmap.cc
    ${KENLM_ROOT}/util/murmur_hash.cc
    ${KENLM_ROOT}/util/parallel_read.cc
    ${KENLM_ROOT}/util/pool.cc
    ${KENLM_ROOT}/util/read_compressed.cc
    ${KENLM_ROOT}/util/scoped.cc
    ${KENLM_ROOT}/util/spaces.cc
    ${KENLM_ROOT}/util/string_piece.cc
    ${KENLM_ROOT}/util/usage.cc
)

add_library(kenlm-jni SHARED
    kenlm_wrapper.cpp
    ${KENLM_LM_SOURCES}
    ${KENLM_UTIL_SOURCES}
)

target_compile_definitions(kenlm-jni PRIVATE
    KENLM_MAX_ORDER=6
    NDEBUG
    _LIBCPP_ENABLE_CXX17_REMOVED_FEATURES
)

target_compile_options(kenlm-jni PRIVATE
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-sign-compare
    -Wno-ambiguous-reversed-operator
)

find_library(log-lib log)

target_link_libraries(kenlm-jni ${log-lib})

target_link_options(kenlm-jni PRIVATE -Wl,-z,max-page-size=16384)

