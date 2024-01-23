cmake_minimum_required(VERSION 3.18)

project(benchmark-distributions LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
#endif()

if (NOT MSVC)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()

add_subdirectory(Catch2)

add_executable(benches
    benches-part1.cpp
    benches-part2.cpp
    distributions-lemire.hpp
    distributions-others.hpp
    emul.hpp
    pcg.hpp
    inlining-blocker.cpp
    inlining-blocker.hpp
)

target_link_libraries(benches
  PRIVATE
    Catch2::Catch2WithMain
)
