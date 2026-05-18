#include "allocator.h"
#include <cstdint>
#include <new>
#include <stdexcept>
#include <sys/mman.h>

std::size_t Allocator::ChunkInfo::getSize() { return meta & ~(ALIGN - 1); }

void Allocator::ChunkInfo::setSize(std::size_t size) {
  meta = (meta & (ALIGN - 1)) | (size & ~(ALIGN - 1));
}

bool Allocator::ChunkInfo::getAllocated() { return meta & 1; }
void Allocator::ChunkInfo::setAllocated(bool allocated) {
  meta = (meta & ~1UL) | allocated;
}

Allocator::ChunkInfo *Allocator::ChunkInfo::getFooter() {
  return this + getSize() / sizeof(ChunkInfo) - 1;
}

void Allocator::ChunkInfo::writeChunk(ChunkInfo *memory, std::size_t size,
                                      bool allocated) {
  ChunkInfo value{size | allocated};
  *memory = value;
  ChunkInfo *footer = memory->getFooter();
  footer->setSize(size);
  footer->setAllocated(allocated);
}

// No bounds check
Allocator::ChunkInfo *Allocator::ChunkInfo::next() {
  return this + getSize() / sizeof(ChunkInfo);
}
Allocator::ChunkInfo *Allocator::ChunkInfo::prev(void *heap) {
  if (this == static_cast<ChunkInfo *>(heap)) {
    return nullptr;
  }
  return this - (this - 1)->getSize() / sizeof(ChunkInfo);
}

bool Allocator::ChunkInfo::shouldSplit(std::size_t allocateSize) {
  return getSize() > allocateSize + sizeof(ChunkInfo);
}

Allocator::Allocator(std::size_t initial_capacity)
    : capacity{alignUp(initial_capacity)},
      heap{mmap(NULL, alignUp(initial_capacity), PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)} {
  if (heap == MAP_FAILED) {
    throw std::runtime_error("mmap failed");
  }
  ChunkInfo::writeChunk(static_cast<ChunkInfo *>(heap), capacity, false);
  free_list = static_cast<ChunkInfo *>(heap);
}

Allocator::~Allocator() { munmap(heap, capacity); }

void *Allocator::allocate(std::size_t size) {
  size = alignUp(size + sizeof(ChunkInfo));
  ChunkInfo *cur = next_free_chunk(free_list, size);
  if (!chunkInHeap(cur)) {
    throw std::bad_alloc();
  }
  cur->setAllocated(true);
  if (cur->shouldSplit(size)) {
    splitChunk(cur, size);
  }
  if (free_list->getAllocated()) {
    free_list = next_free_chunk(cur->next());
  }
  return reinterpret_cast<char *>(cur) + sizeof(ChunkInfo);
}

void Allocator::free(void *ptr) {
  if (ptr == nullptr) {
    return;
  }
  ChunkInfo *chunk = getChunkFromPointer(ptr);
  chunk->setAllocated(false);
  if (shouldMergePrevChunk(chunk)) {
    mergePrevFreeChunk(chunk);
    chunk = chunk->prev(heap);
  }
  if (shouldMergeNextChunk(chunk)) {
    mergeNextFreeChunk(chunk);
  }
  if (!chunkInHeap(free_list) || chunk < free_list) {
    free_list = chunk;
  }
}

Allocator::ChunkInfo *Allocator::getChunkFromPointer(void *ptr) {
  return reinterpret_cast<ChunkInfo *>(static_cast<char *>(ptr) -
                                       sizeof(ChunkInfo));
}

bool Allocator::shouldMergeNextChunk(ChunkInfo *chunk) {
  ChunkInfo *next = chunk->next();
  return chunkInHeap(next) && !next->getAllocated();
}

bool Allocator::shouldMergePrevChunk(ChunkInfo *chunk) {
  ChunkInfo *prev = chunk->prev(heap);
  return chunkInHeap(prev) && !prev->getAllocated();
}

void Allocator::mergePrevFreeChunk(ChunkInfo *chunk) {
  ChunkInfo *prev = chunk->prev(heap);
  std::size_t newSize = prev->getSize() + chunk->getSize();
  prev->setSize(newSize);
  prev->getFooter()->setSize(newSize);
}

void Allocator::mergeNextFreeChunk(ChunkInfo *chunk) {
  ChunkInfo *next = chunk->next();
  std::size_t newSize = next->getSize() + chunk->getSize();
  chunk->setSize(newSize);
  next->getFooter()->setSize(newSize);
}

void Allocator::splitChunk(ChunkInfo *chunk, std::size_t size) {
  std::size_t remainingFreeSize = chunk->getSize() - size;
  ChunkInfo::writeChunk(chunk, size, true);
  ChunkInfo::writeChunk(chunk->next(), remainingFreeSize, false);
}

Allocator::ChunkInfo *Allocator::next_free_chunk(ChunkInfo *startChunk) {
  ChunkInfo *next_free = startChunk;
  while (chunkInHeap(next_free) && next_free->getAllocated()) {
    next_free = next_free->next();
  }
  return next_free;
}

Allocator::ChunkInfo *Allocator::next_free_chunk(ChunkInfo *startChunk,
                                                 std::size_t size) {
  ChunkInfo *cur = startChunk;
  while (chunkInHeap(cur) && (cur->getAllocated() || cur->getSize() < size)) {
    cur = cur->next();
  }
  return cur;
}

std::size_t Allocator::alignUp(std::size_t size) {
  const std::size_t ALIGN = 8;
  return (size + ALIGN - 1) & ~(ALIGN - 1);
}

bool Allocator::chunkInHeap(ChunkInfo *chunk) {
  return heap <= chunk && chunk < static_cast<ChunkInfo *>(heap) + capacity / 8;
}
