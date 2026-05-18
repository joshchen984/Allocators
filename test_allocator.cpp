#include "allocator.h"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <random>
#include <vector>

std::size_t alignUp(std::size_t size) {
  const std::size_t ALIGN = 8;
  return (size + ALIGN - 1) & ~(ALIGN - 1);
}

void fill_memory(void *ptr, char byte, std::size_t size) {
  std::memset(ptr, byte, size);
}

void check_pattern(void *ptr, char byte, std::size_t size) {
  unsigned char *p = static_cast<unsigned char *>(ptr);
  unsigned char expected = static_cast<unsigned char>(byte);
  for (std::size_t i = 0; i < size; i++) {
    REQUIRE(p[i] == expected);
  }
}
const int CHUNK_INFO_SIZE = 8;

struct ActiveAlloc {
  void *ptr;
  std::size_t requestedSize;
  unsigned char patternByte;
};

bool rangesOverlap(char *beginA, char *endA, char *beginB, char *endB) {
  return beginA < endB && beginB < endA;
}

void verifyActivePatterns(const std::vector<ActiveAlloc> &activeAllocs) {
  for (const ActiveAlloc &alloc : activeAllocs) {
    check_pattern(alloc.ptr, static_cast<char>(alloc.patternByte),
                  alloc.requestedSize);
  }
}

void assertNoOverlapWithActiveAllocs(
    void *newPtr, std::size_t requestedSize,
    const std::vector<ActiveAlloc> &activeAllocs) {
  char *newBegin = static_cast<char *>(newPtr);
  char *newEnd = newBegin + requestedSize;
  for (const ActiveAlloc &alloc : activeAllocs) {
    char *existingBegin = static_cast<char *>(alloc.ptr);
    char *existingEnd = existingBegin + alloc.requestedSize;
    REQUIRE_FALSE(rangesOverlap(newBegin, newEnd, existingBegin, existingEnd));
  }
}

void allocateOne(Allocator &allocator, std::mt19937 &rng,
                 std::uniform_int_distribution<int> &sizeDist,
                 std::uniform_int_distribution<int> &patternDist,
                 std::vector<ActiveAlloc> &activeAllocs) {
  std::size_t requestedSize = static_cast<std::size_t>(sizeDist(rng));
  void *newPtr = allocator.allocate(requestedSize);
  REQUIRE(reinterpret_cast<std::uintptr_t>(newPtr) % 8 == 0);
  assertNoOverlapWithActiveAllocs(newPtr, requestedSize, activeAllocs);

  unsigned char patternByte = static_cast<unsigned char>(patternDist(rng));
  fill_memory(newPtr, static_cast<char>(patternByte), requestedSize);
  activeAllocs.push_back({newPtr, requestedSize, patternByte});
}

void freeRandomOne(Allocator &allocator, std::mt19937 &rng,
                   std::vector<ActiveAlloc> &activeAllocs) {
  std::uniform_int_distribution<std::size_t> idxDist(0,
                                                     activeAllocs.size() - 1);
  std::size_t idx = idxDist(rng);
  allocator.free(activeAllocs[idx].ptr);
  activeAllocs.erase(activeAllocs.begin() + static_cast<std::ptrdiff_t>(idx));
}

TEST_CASE("allocator basic behavior", "[allocator]") {
  std::size_t capacity = 200;
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
            static_cast<char *>(ptr1) + alignUp(50 + CHUNK_INFO_SIZE));
  }

  SECTION("Throws when allocates more memory than capacity") {
    REQUIRE_THROWS_AS(a.allocate(capacity), std::bad_alloc);
  }
  SECTION("Coalesces adjacent free memory") {
    int NUM_PTRS = 5;
    int blockSize = capacity / NUM_PTRS;
    REQUIRE(capacity % NUM_PTRS == 0);
    REQUIRE(blockSize % 8 == 0);
    blockSize -= CHUNK_INFO_SIZE;
    void *ptrs[NUM_PTRS];

    for (int i = 0; i < NUM_PTRS; i++) {
      ptrs[i] = a.allocate(blockSize);
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

TEST_CASE("allocator edge-case behavior", "[allocator][edge]") {
  SECTION("allocate(0) returns aligned pointer and can be freed") {
    Allocator a{200};
    void *ptr = a.allocate(0);
    REQUIRE(ptr != nullptr);
    REQUIRE(reinterpret_cast<std::uintptr_t>(ptr) % 8 == 0);
    a.free(ptr);
  }

  SECTION("Returned pointers are always 8-byte aligned") {
    Allocator a{256};
    std::vector<std::size_t> requestSizes = {1, 2, 3, 7, 8, 9, 15, 16, 17, 31};
    std::vector<void *> ptrs;
    ptrs.reserve(requestSizes.size());

    for (std::size_t requestSize : requestSizes) {
      void *ptr = a.allocate(requestSize);
      REQUIRE(reinterpret_cast<std::uintptr_t>(ptr) % 8 == 0);
      ptrs.push_back(ptr);
    }

    for (void *ptr : ptrs) {
      a.free(ptr);
    }
  }

  SECTION("free(nullptr) is a no-op") {
    Allocator a{200};
    REQUIRE_NOTHROW(a.free(nullptr));
    void *ptr = a.allocate(40);
    REQUIRE(ptr != nullptr);
    a.free(ptr);
  }

  SECTION("Coalesces middle then neighbors: middle-left-right order") {
    Allocator a{200};
    void *p1 = a.allocate(40);
    void *p2 = a.allocate(40);
    void *p3 = a.allocate(40);

    a.free(p2);
    a.free(p1);
    a.free(p3);

    void *big = a.allocate(120);
    REQUIRE(big == p1);
    a.free(big);
  }

  SECTION("Coalesces middle then neighbors: middle-right-left order") {
    Allocator a{200};
    void *p1 = a.allocate(40);
    void *p2 = a.allocate(40);
    void *p3 = a.allocate(40);

    a.free(p2);
    a.free(p3);
    a.free(p1);

    void *big = a.allocate(120);
    REQUIRE(big == p1);
    a.free(big);
  }

  SECTION("Does not split when remainder equals header size") {
    Allocator a{200};
    // Internal allocated chunk size is 184 (168 payload + header + footer).
    // Remainder is 16, so allocator should not split.
    void *ptr = a.allocate(200 - 16 - 16);
    REQUIRE_THROWS_AS(a.allocate(1), std::bad_alloc);
    a.free(ptr);
  }

  SECTION("Splits when remainder can fit another minimum chunk") {
    Allocator a{200};
    // Internal allocated chunk size is 176 (160 payload + header + footer).
    // Remainder is 24, so should hold another minimum chunk.
    void *first = a.allocate(200 - 16 - 24);
    void *second = a.allocate(1);
    REQUIRE(second != nullptr);
    a.free(second);
    a.free(first);
  }
}

TEST_CASE("allocator randomized stress", "[allocator][stress][.]") {
  constexpr std::size_t kHeapSize = 4096;
  constexpr int kStepCount = 2000;
  constexpr int kAllocateProbabilityPercent = 70;
  constexpr int kMaxRequestedSize = 96;
  constexpr unsigned int kSeed = 123456u;

  Allocator allocator{kHeapSize};
  std::mt19937 rng(kSeed);
  std::uniform_int_distribution<int> actionDist(0, 99);
  std::uniform_int_distribution<int> sizeDist(0, kMaxRequestedSize);
  std::uniform_int_distribution<int> patternDist(1, 255);

  std::vector<ActiveAlloc> activeAllocs;
  activeAllocs.reserve(256);

  for (int step = 0; step < kStepCount; ++step) {
    verifyActivePatterns(activeAllocs);

    bool shouldAllocate =
        activeAllocs.empty() || actionDist(rng) < kAllocateProbabilityPercent;
    if (shouldAllocate) {
      allocateOne(allocator, rng, sizeDist, patternDist, activeAllocs);
    } else {
      freeRandomOne(allocator, rng, activeAllocs);
    }
  }

  verifyActivePatterns(activeAllocs);
  for (const ActiveAlloc &alloc : activeAllocs) {
    allocator.free(alloc.ptr);
  }
}
