# PC-s (Linux/Windows/macOS) fordítás: unit tesztek, ISOBUS-szimulátor, object pool ellenőrzés.
# Az ESP32 firmware-t NEM ez fordítja, hanem a PlatformIO (platformio.ini).
#
#   cmake -S host -B build-host -DCMAKE_BUILD_TYPE=Release
#   cmake --build build-host -j
#   ctest --test-dir build-host --output-on-failure
cmake_minimum_required(VERSION 3.16)
project(seeder_host CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

option(SEEDER_WITH_ISOBUS "AgIsoStack++ alapú ISOBUS réteg és szimulátor fordítása" ON)
set(AGISOSTACK_DIR "" CACHE PATH "Helyi AgIsoStack-plus-plus forrás (üres = letöltés)")

file(GLOB CORE_SRC ${ROOT}/src/core/*.cpp)
add_library(seeder_core STATIC ${CORE_SRC})
target_include_directories(seeder_core PUBLIC ${ROOT}/include)
if(MSVC)
  target_compile_options(seeder_core PRIVATE /W4)
else()
  target_compile_options(seeder_core PRIVATE -Wall -Wextra -Wpedantic -Werror=return-type)
endif()

enable_testing()
add_executable(test_core ${ROOT}/tests/host/test_core.cpp)
target_link_libraries(test_core seeder_core)
add_test(NAME core COMMAND test_core)

if(SEEDER_WITH_ISOBUS)
  include(FetchContent)
  if(AGISOSTACK_DIR)
    FetchContent_Declare(isobus SOURCE_DIR ${AGISOSTACK_DIR})
  else()
    # ugyanaz a rögzített verzió, mint a platformio.ini-ben
    FetchContent_Declare(isobus
      GIT_REPOSITORY https://github.com/Open-Agriculture/AgIsoStack-plus-plus.git
      GIT_TAG 3e37ba629b53b565fa80c63feaed027a9156d3b9)
  endif()
  set(CAN_DRIVER "VirtualCAN" CACHE STRING "" FORCE)
  set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(isobus)

  file(GLOB APP_SRC ${ROOT}/src/app/*.cpp)
  add_library(seeder_app STATIC ${APP_SRC})
  target_include_directories(seeder_app PUBLIC ${ROOT}/include ${ROOT}/src/app ${ROOT}/generated)
  target_link_libraries(seeder_app PUBLIC seeder_core isobus::Isobus isobus::HardwareIntegration isobus::Utility)

  add_executable(test_app ${ROOT}/tests/host/test_app.cpp)
  target_link_libraries(test_app seeder_app isobus::Isobus isobus::HardwareIntegration)
  target_compile_definitions(test_app PRIVATE SEEDER_POOL_FILE="${ROOT}/generated/object_pool.iop")
  add_test(NAME app COMMAND test_app)

  # Object pool ellenőrzése az AgIsoStack++ saját VT-szerver parserével
  add_executable(iop_check ${ROOT}/tests/host/iop_check.cpp)
  target_link_libraries(iop_check isobus::Isobus isobus::HardwareIntegration isobus::Utility)
  add_test(NAME object_pool COMMAND iop_check ${ROOT}/generated/object_pool.iop)

  # Teljes ISOBUS-szimuláció virtuális CAN-en: saját ECU + TC szerver + VT szerver-teszt
  add_executable(isobus_sim ${ROOT}/tests/host/isobus_sim.cpp)
  target_link_libraries(isobus_sim seeder_app)
  add_test(NAME isobus_sim_basic_400 COMMAND isobus_sim ${ROOT}/generated/object_pool.iop 400 120)
  add_test(NAME isobus_sim_480 COMMAND isobus_sim ${ROOT}/generated/object_pool.iop 480 80)
  # Müller / Dammann HD12, Touch1200: négyzetes (80×80) virtuális softkey-k; és egy széles-alacsony változat
  add_test(NAME isobus_sim_hd12_square COMMAND isobus_sim ${ROOT}/generated/object_pool.iop 480 80 80)
  add_test(NAME isobus_sim_wide_keys COMMAND isobus_sim ${ROOT}/generated/object_pool.iop 600 100 64)
endif()

# Webes felület demószerver (opcionális, a cpp-httplib fejlécet letölti)
option(SEEDER_WEB_DEMO "Webes demószerver fordítása" OFF)
set(HTTPLIB_DIR "" CACHE PATH "Helyi cpp-httplib könyvtár (üres = letöltés)")
if(SEEDER_WITH_ISOBUS AND SEEDER_WEB_DEMO)
  if(HTTPLIB_DIR)
    set(httplib_SOURCE_DIR ${HTTPLIB_DIR})
  else()
    FetchContent_Declare(httplib GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git GIT_TAG v0.18.3)
    FetchContent_Populate(httplib)
  endif()
  add_executable(web_demo ${ROOT}/tests/host/web_demo.cpp)
  target_include_directories(web_demo PRIVATE ${httplib_SOURCE_DIR})
  target_compile_definitions(web_demo PRIVATE WEB_DIR="${ROOT}/web")
  target_link_libraries(web_demo seeder_app isobus::Isobus isobus::HardwareIntegration)
endif()
