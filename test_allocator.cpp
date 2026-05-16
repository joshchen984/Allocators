#include "allocator.h"
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <new>

void fill_memory(void *ptr, char byte, std::size_t size) {
  std::memset(ptr, byte, size);
}

void check_pattern(void *ptr, char byte, std::size_t size) {
  unsigned char *p = static_cast<unsigned char *>(ptr);
  for (int i = 0; i < size; i++) {
    REQUIRE(p[i] == byte);
  }
}
const int CHUNK_INFO_SIZE = 24;

TEST_CASE("allocator", "[allocator]") {
  std::uint32_t capacity = 200;
  Allocator a{capacity};
  SECTION("Can allocate, write, and free one chunk of memory with more memory "
          "in allocator") {
    void *ptr = a.allocate(20);
    fill_memory(ptr, 0x12, 20);
    check_pattern(ptr, 0x12, 20);
    a.free(ptr);
  }

  SECTION("Can allocate, write, and free one chunk of memory with no remaining "
          "memory in allocator") {
    void *ptr = a.allocate(170);
    fill_memory(ptr, 0x12, 170);
    check_pattern(ptr, 0x12, 170);
    a.free(ptr);
  }

  SECTION("After freeing chunk, reuses freed memory") {
    void *ptr = a.allocate(20);
    fill_memory(ptr, 0x12, 20);
    check_pattern(ptr, 0x12, 20);
    a.free(ptr);
    void *newPtr = a.allocate(20);
    REQUIRE(ptr == newPtr);
  }

  SECTION("After freeing chunk, reuses freed memory when chunk uses all of "
          "memory") {
    void *ptr = a.allocate(170);
    fill_memory(ptr, 0x12, 170);
    check_pattern(ptr, 0x12, 170);
    a.free(ptr);
    void *newPtr = a.allocate(170);
    REQUIRE(ptr == newPtr);
  }

  SECTION("Allocates multiple times") {
    void *ptr1 = a.allocate(50);
    void *ptr2 = a.allocate(50);
    REQUIRE(static_cast<char *>(ptr2) ==
            static_cast<char *>(ptr1) + 50 + CHUNK_INFO_SIZE);
  }

  SECTION("Throws when allocates more memory than capacity") {
    REQUIRE_THROWS_AS(a.allocate(capacity), std::bad_alloc);
  }
  SECTION("Coalesces adjacent free memory") {
    int NUM_PTRS = 5;
    void *ptrs[NUM_PTRS];
    for (int i = 0; i < NUM_PTRS; i++) {
      ptrs[i] = a.allocate((capacity - CHUNK_INFO_SIZE * NUM_PTRS) / NUM_PTRS);
    }
    for (void *ptr : ptrs) {
      a.free(ptr);
    }
    a.allocate(capacity - CHUNK_INFO_SIZE);
  }

  SECTION("Throws when multiple allocations use more than capacity") {
    a.allocate(capacity / 2);
    REQUIRE_THROWS_AS(a.allocate(capacity / 2), std::bad_alloc);
  }
}
