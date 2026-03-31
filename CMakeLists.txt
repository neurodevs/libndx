cmake_minimum_required(VERSION 3.21)
project(ndx VERSION 0.1.0 LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(nlohmann_json REQUIRED)

set(NDX_SOURCES
  src/ndx_ffi.cpp
  src/acquisition_backend.cpp
  src/ble/ble_backend.cpp
  src/ftdi/ftdi_backend.cpp
)

if(APPLE)
  list(APPEND NDX_SOURCES src/ble/macos/core_bluetooth_provider.mm)
else()
  list(APPEND NDX_SOURCES src/ble/ble_provider.cpp)
endif()

add_library(ndx STATIC ${NDX_SOURCES})

target_include_directories(ndx PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

add_library(lsl SHARED IMPORTED)
set_target_properties(lsl PROPERTIES
  IMPORTED_LOCATION "/opt/local/lib/liblsl.dylib"
  INTERFACE_INCLUDE_DIRECTORIES "/opt/local/include"
)

target_link_libraries(ndx PUBLIC
  lsl
  nlohmann_json::nlohmann_json
)

if(APPLE)
  target_link_libraries(ndx PUBLIC
    "-framework CoreBluetooth"
    "-framework Foundation"
  )
else()
  message(FATAL_ERROR "libndx: unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

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
  tests/test_ble_ffi.cpp
  tests/test_ble_provider.cpp
  tests/test_ftdi_backend.cpp
  tests/test_ftdi_ffi.cpp
)

target_compile_definitions(ndx_tests PRIVATE NDX_TESTING)

target_link_libraries(ndx_tests PRIVATE
  ndx
  Catch2::Catch2WithMain
  nlohmann_json::nlohmann_json
)

include(CTest)
include(Catch)
catch_discover_tests(ndx_tests)

if(APPLE)
  add_executable(run_ble_test tools/run_ble_test.mm)
  target_link_libraries(run_ble_test PRIVATE
    lsl
    "-framework CoreBluetooth"
    "-framework Foundation"
  )

  add_executable(run_govee tools/run_govee.mm)
  target_link_libraries(run_govee PRIVATE
    "-framework CoreBluetooth"
    "-framework Foundation"
  )

  add_executable(scan_ble tools/scan_all_ble.mm)
  target_link_libraries(scan_ble PRIVATE
    "-framework CoreBluetooth"
    "-framework Foundation"
  )
endif()