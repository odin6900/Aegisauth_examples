cmake_minimum_required(VERSION 3.16)
project(aegis_sdk VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)

add_library(aegis_sdk src/aegis.cpp)
target_include_directories(aegis_sdk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(aegis_sdk PUBLIC CURL::libcurl)

option(AEGIS_BUILD_EXAMPLES "Build the quickstart example" ON)
if(AEGIS_BUILD_EXAMPLES)
  add_executable(aegis_quickstart examples/quickstart.cpp)
  target_link_libraries(aegis_quickstart PRIVATE aegis_sdk)
endif()