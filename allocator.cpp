#include "allocator.h"
#include <cstdint>
#include <new>
#include <stdexcept>
#include <sys/mman.h>

Allocator::Allocator(std::uint32_t initial_capacity)
    : heap{mmap(NULL, initial_capacity, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)} {
  if (heap == MAP_FAILED) {
    throw std::runtime_error("mmap failed");
  }
  free_list = new (heap)
      ChunkInfo{nullptr, nullptr, initial_capacity - sizeof(ChunkInfo), false};
}

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
  ChunkInfo *chunk = reinterpret_cast<ChunkInfo *>(static_cast<char *>(ptr) -
                                                   sizeof(ChunkInfo));
  chunk->allocated = false;
  if (chunk->prev != nullptr && !chunk->prev->allocated) {
    chunk->prev->size += chunk->size + sizeof(ChunkInfo);
    chunk->prev->next = chunk->next;
    if (chunk->next != nullptr) {
      chunk->next->prev = chunk->prev;
    }
    chunk = chunk->prev;
  }
  if (chunk->next != nullptr && !chunk->next->allocated) {
    chunk->size += chunk->next->size + sizeof(ChunkInfo);
    if (chunk->next->next != nullptr) {
      chunk->next->next->prev = chunk;
    }
    chunk->next = chunk->next->next;
  }
  if (free_list == nullptr || chunk < free_list) {
    free_list = chunk;
  }
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
  return chunk + chunk->size + sizeof(ChunkInfo);
}
