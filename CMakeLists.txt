cmake_minimum_required(VERSION 3.20)

project(
  PatchMeshToBrushes
  VERSION 0.1.0
  DESCRIPTION "Convert Quake III Arena era patchDef meshes into convex brushDef output"
  LANGUAGES CXX
)

option(PMTB_BUILD_TESTS "Build PatchMeshToBrushes tests" ON)
option(PMTB_BUILD_BENCHMARKS "Build PatchMeshToBrushes benchmark tools" ON)

configure_file(
  cmake/PatchMeshToBrushesVersion.hpp.in
  "${PROJECT_BINARY_DIR}/generated/PatchMeshToBrushesVersion.hpp"
  @ONLY
)

add_library(mtb_core
  src/cli/Arguments.cpp
  src/conversion/BrushBuilder.cpp
  src/conversion/BrushPlanner.cpp
  src/conversion/ConversionPlanner.cpp
  src/conversion/PatchTopology.cpp
  src/geometry/BezierPatch.cpp
  src/geometry/PlaneFit.cpp
  src/geometry/UvProjection.cpp
  src/geometry/Winding.cpp
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

add_executable(patch-mesh-to-brushes src/main.cpp)
target_link_libraries(patch-mesh-to-brushes PRIVATE mtb_core)

if(PMTB_BUILD_BENCHMARKS)
  add_executable(benchmark_large_map benchmarks/benchmark_large_map.cpp)
  target_link_libraries(benchmark_large_map PRIVATE mtb_core)
endif()

if(PMTB_BUILD_TESTS)
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

  add_executable(test_geometry_core tests/test_geometry_core.cpp)
  target_link_libraries(test_geometry_core PRIVATE mtb_core)
  add_test(NAME geometry_core COMMAND test_geometry_core)

  add_executable(test_patch_topology tests/test_patch_topology.cpp)
  target_link_libraries(test_patch_topology PRIVATE mtb_core)
  add_test(NAME patch_topology COMMAND test_patch_topology)

  add_executable(test_brush_planner tests/test_brush_planner.cpp)
  target_link_libraries(test_brush_planner PRIVATE mtb_core)
  add_test(NAME brush_planner COMMAND test_brush_planner)

  add_executable(test_brush_builder tests/test_brush_builder.cpp)
  target_link_libraries(test_brush_builder PRIVATE mtb_core)
  add_test(NAME brush_builder COMMAND test_brush_builder)

  add_executable(test_golden_conversion tests/test_golden_conversion.cpp)
  target_link_libraries(test_golden_conversion PRIVATE mtb_core)
  add_test(
    NAME golden_conversion
    COMMAND test_golden_conversion
      "${PROJECT_SOURCE_DIR}/tests/golden"
  )

  add_executable(test_malformed_fuzz tests/test_malformed_fuzz.cpp)
  target_link_libraries(test_malformed_fuzz PRIVATE mtb_core)
  add_test(NAME malformed_fuzz COMMAND test_malformed_fuzz)

  add_executable(test_q3dm1_fixture tests/test_q3dm1_fixture.cpp)
  target_link_libraries(test_q3dm1_fixture PRIVATE mtb_core)
  add_test(
    NAME q3dm1_fixture
    COMMAND test_q3dm1_fixture
      "${PROJECT_SOURCE_DIR}/tests/map/q3dm1sample.map"
  )

  add_executable(test_q3dm1_conversion tests/test_q3dm1_conversion.cpp)
  target_link_libraries(test_q3dm1_conversion PRIVATE mtb_core)
  add_test(
    NAME q3dm1_conversion
    COMMAND test_q3dm1_conversion
      "${PROJECT_SOURCE_DIR}/tests/map/q3dm1sample.map"
  )

  add_executable(test_deterministic_output tests/test_deterministic_output.cpp)
  target_link_libraries(test_deterministic_output PRIVATE mtb_core)
  add_test(
    NAME deterministic_output
    COMMAND test_deterministic_output
      "${PROJECT_SOURCE_DIR}/tests/map/q3dm1sample.map"
  )
endif()
