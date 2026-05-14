cmake_minimum_required(VERSION 3.28)

project(Allocators)

add_executable(MyExe
    main.cpp allocator.h allocator.cpp)

Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.8.1 # or a later release
)

FetchContent_MakeAvailable(Catch2)

add_executable(tests test_allocator.cpp allocator.cpp)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)
