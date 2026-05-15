cmake_minimum_required(VERSION 3.20)

project(
  MeshToBrushes
  VERSION 0.1.0
  DESCRIPTION "Convert Quake III Arena era patchDef meshes into convex brushDef output"
  LANGUAGES CXX
)

option(MTB_BUILD_TESTS "Build MeshToBrushes tests" ON)

configure_file(
  cmake/MeshToBrushesVersion.hpp.in
  "${PROJECT_BINARY_DIR}/generated/MeshToBrushesVersion.hpp"
  @ONLY
)

add_library(mtb_core
  src/cli/Arguments.cpp
  src/conversion/BrushBuilder.cpp
  src/conversion/ConversionPlanner.cpp
  src/io/FileSystem.cpp
  src/map/MapDocument.cpp
)

target_include_directories(mtb_core
  PUBLIC
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_BINARY_DIR}/generated"
)
target_compile_features(mtb_core PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(mtb_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(mtb_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(mesh-to-brushes src/main.cpp)
target_link_libraries(mesh-to-brushes PRIVATE mtb_core)

if(MTB_BUILD_TESTS)
  enable_testing()

  add_executable(test_arguments tests/test_arguments.cpp)
  target_link_libraries(test_arguments PRIVATE mtb_core)
  add_test(NAME arguments COMMAND test_arguments)

  add_executable(test_patch_scanner tests/test_patch_scanner.cpp)
  target_link_libraries(test_patch_scanner PRIVATE mtb_core)
  add_test(NAME patch_scanner COMMAND test_patch_scanner)

  add_executable(test_map_parser tests/test_map_parser.cpp)
  target_link_libraries(test_map_parser PRIVATE mtb_core)
  add_test(NAME map_parser COMMAND test_map_parser)

  add_executable(test_q3dm1_fixture tests/test_q3dm1_fixture.cpp)
  target_link_libraries(test_q3dm1_fixture PRIVATE mtb_core)
  add_test(
    NAME q3dm1_fixture
    COMMAND test_q3dm1_fixture
      "${PROJECT_SOURCE_DIR}/tests/map/q3dm1sample.map"
  )
endif()
