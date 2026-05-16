#include "allocator.h"
#include <cstdint>
#include <new>
#include <stdexcept>
#include <sys/mman.h>

Allocator::Allocator(std::uint32_t initial_capacity)
    : capacity{initial_capacity},
      heap{mmap(NULL, initial_capacity, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)} {
  if (heap == MAP_FAILED) {
    throw std::runtime_error("mmap failed");
  }
  free_list = new (heap) ChunkInfo{
      nullptr, nullptr,
      initial_capacity - static_cast<std::uint32_t>(sizeof(ChunkInfo)), false};
}

Allocator::~Allocator() { munmap(heap, capacity); }

void *Allocator::allocate(std::uint32_t size) {
  ChunkInfo *cur = next_free_chunk(free_list, size);
  if (cur == nullptr) {
    throw std::bad_alloc();
  }
  cur->allocated = true;
  if (should_split(cur->size, size)) {
    splitChunk(cur, size);
  }
  if (free_list->allocated) {
    free_list = next_free_chunk(cur->next);
  }
  return reinterpret_cast<char *>(cur) + sizeof(ChunkInfo);
}

void Allocator::free(void *ptr) {
  if (ptr == nullptr) {
    return;
  }
  ChunkInfo *chunk = getChunkFromPointer(ptr);
  chunk->allocated = false;
  if (shouldMergePrevChunk(chunk)) {
    mergePrevFreeChunk(chunk);
    chunk = chunk->prev;
  }
  if (shouldMergeNextChunk(chunk)) {
    mergeNextFreeChunk(chunk);
  }
  if (free_list == nullptr || chunk < free_list) {
    free_list = chunk;
  }
}

Allocator::ChunkInfo *Allocator::getChunkFromPointer(void *ptr) {
  return reinterpret_cast<ChunkInfo *>(static_cast<char *>(ptr) -
                                       sizeof(ChunkInfo));
}

bool Allocator::shouldMergeNextChunk(Allocator::ChunkInfo *chunk) {
  return chunk->next != nullptr && !chunk->next->allocated;
}

bool Allocator::shouldMergePrevChunk(Allocator::ChunkInfo *chunk) {
  return chunk->prev != nullptr && !chunk->prev->allocated;
}

void Allocator::mergePrevFreeChunk(Allocator::ChunkInfo *chunk) {
  chunk->prev->size += chunk->size + sizeof(ChunkInfo);
  chunk->prev->next = chunk->next;
  if (chunk->next != nullptr) {
    chunk->next->prev = chunk->prev;
  }
}

void Allocator::mergeNextFreeChunk(Allocator::ChunkInfo *chunk) {
  chunk->size += chunk->next->size + sizeof(ChunkInfo);
  if (chunk->next->next != nullptr) {
    chunk->next->next->prev = chunk;
  }
  chunk->next = chunk->next->next;
}

void Allocator::splitChunk(Allocator::ChunkInfo *chunk, std::uint32_t size) {
  std::uint32_t remainingFreeSize = chunk->size - size - sizeof(ChunkInfo);
  chunk->size = size;
  ChunkInfo *remainingChunk = new (nextChunkLocation(chunk))
      ChunkInfo{chunk, chunk->next, remainingFreeSize, false};
  if (chunk->next) {
    chunk->next->prev = remainingChunk;
  }
  chunk->next = remainingChunk;
}

bool Allocator::should_split(std::uint32_t chunkSize,
                             std::uint32_t allocateSize) {
  return chunkSize > allocateSize + sizeof(ChunkInfo);
}

Allocator::ChunkInfo *
Allocator::next_free_chunk(Allocator::ChunkInfo *startChunk) {
  ChunkInfo *next_free = startChunk;
  while (next_free != nullptr && next_free->allocated) {
    next_free = next_free->next;
  }
  return next_free;
}

Allocator::ChunkInfo *
Allocator::next_free_chunk(Allocator::ChunkInfo *startChunk,
                           std::uint32_t size) {
  ChunkInfo *cur = startChunk;
  while (cur != nullptr && (cur->allocated || cur->size < size)) {
    cur = cur->next;
  }
  return cur;
}

void *Allocator::nextChunkLocation(Allocator::ChunkInfo *chunk) {
  return reinterpret_cast<char *>(chunk) + chunk->size + sizeof(ChunkInfo);
}
