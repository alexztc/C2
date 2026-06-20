cmake_minimum_required(VERSION 3.13)
project(c2_trie LANGUAGES CXX)

# Quiet warnings during third-party builds; our own code is -Wall -pedantic.
add_definitions(-w)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -Wall -pedantic")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# Third-party header + library setup
configure_file(
  lib/ds2i/ds2i_config.hpp.in
  lib/ds2i/ds2i_config.hpp
  ESCAPE_QUOTES)

find_package(Boost REQUIRED)

include_directories(
  include
  lib/ds2i
  lib/sux
  baseline_marisa/marisa/lib
  baseline_marisa/marisa/include)

add_subdirectory(lib/ds2i/succinct)
add_subdirectory(lib/sdsl-lite)
add_subdirectory(lib/fsst)

# -----------------------------------------------------------------------------
# benchmark — the main driver; consult benchmark.cpp::main for case IDs.
# -----------------------------------------------------------------------------
add_executable(benchmark
  benchmark.cpp
  baseline_ctriepp/zfast_impl.cpp)
target_include_directories(benchmark PRIVATE
  ${Boost_INCLUDE_DIR}
  baseline_coco
  baseline_fst
  baseline_ctriepp
  baseline_ctriepp/ctriepp/ctriepp
  baseline_ctriepp/ctriepp/ZFastTrie)
target_link_libraries(benchmark
  sdsl succinct fsst
  ${Boost_LIBRARIES}
  ${CMAKE_SOURCE_DIR}/baseline_marisa/libmarisa.a)

# -----------------------------------------------------------------------------
# profile — same translation units but built with __PROFILE__ + debug symbols
# for perf_event-based LLC miss measurement (see __PROFILE__ blocks in
# benchmark.cpp).
# -----------------------------------------------------------------------------
add_executable(profile
  benchmark.cpp
  baseline_ctriepp/zfast_impl.cpp)
target_include_directories(profile PRIVATE
  ${Boost_INCLUDE_DIR}
  baseline_coco
  baseline_fst
  baseline_ctriepp
  baseline_ctriepp/ctriepp/ctriepp
  baseline_ctriepp/ctriepp/ZFastTrie)
target_link_libraries(profile
  sdsl succinct fsst
  ${Boost_LIBRARIES}
  ${CMAKE_SOURCE_DIR}/baseline_marisa/libmarisa.a)
target_compile_options(profile PRIVATE -ggdb3)
target_compile_definitions(profile PRIVATE __PROFILE__)
