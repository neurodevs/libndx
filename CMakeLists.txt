cmake_minimum_required(VERSION 3.21)
project(ndx VERSION 0.1.0 LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Sources ────────────────────────────────────────────────────────────────────

set(NDX_SOURCES
  src/ble/ble_backend.cpp
)

# ── Library target ─────────────────────────────────────────────────────────────

add_library(ndx STATIC ${NDX_SOURCES})

target_include_directories(ndx PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# ── liblsl ─────────────────────────────────────────────────────────────────────

add_library(lsl SHARED IMPORTED)
set_target_properties(lsl PROPERTIES
  IMPORTED_LOCATION "/opt/local/lib/liblsl.dylib"
  INTERFACE_INCLUDE_DIRECTORIES "/opt/local/include"
)

target_link_libraries(ndx PUBLIC lsl)

# ── Platform frameworks ────────────────────────────────────────────────────────

if(APPLE)
  target_link_libraries(ndx PUBLIC
    "-framework CoreBluetooth"
    "-framework Foundation"
  )
else()
  message(FATAL_ERROR "libndx: unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

# ── Tests ──────────────────────────────────────────────────────────────────────

include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.5.2
)
FetchContent_MakeAvailable(Catch2)

enable_testing()

add_executable(ndx_tests
  tests/test_ble_backend.cpp
)

target_link_libraries(ndx_tests PRIVATE ndx Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(ndx_tests)