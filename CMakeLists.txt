cmake_minimum_required(VERSION 3.15)
project(dylibbundler)

set(CMAKE_CXX_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

string(REPLACE "-O3" "-O2" CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})

add_compile_options(-pipe -Wall -Wextra -Wpedantic)

include_directories(src)

add_executable(dylibbundler
    src/Dependency.cpp
    src/Dependency.h
    src/DylibBundler.cpp
    src/DylibBundler.h
    src/main.cpp
    src/Settings.cpp
    src/Settings.h
    src/Utils.cpp
    src/Utils.h
)
