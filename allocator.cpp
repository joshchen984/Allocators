#include "allocator.h"
#include <cstdint>
#include <new>
#include <stdexcept>
#include <sys/mman.h>

namespace {
std::size_t alignUp(std::size_t size) {
  const std::size_t ALIGN = 8;
  return (size + ALIGN - 1) & ~(ALIGN - 1);
}
} // namespace

std::size_t Allocator::ChunkInfo::getSize() const noexcept {
  return meta & ~(ALIGN - 1);
}

void Allocator::ChunkInfo::setSize(std::size_t size) noexcept {
  meta = (meta & (ALIGN - 1)) | (size & ~(ALIGN - 1));
}

bool Allocator::ChunkInfo::getAllocated() const noexcept { return meta & 1; }
void Allocator::ChunkInfo::setAllocated(bool allocated) noexcept {
  meta = (meta & ~1UL) | allocated;
}

Allocator::ChunkInfo *Allocator::ChunkInfo::getFooter() noexcept {
  return this + getSize() / sizeof(ChunkInfo) - 1;
}

void Allocator::ChunkInfo::writeChunk(ChunkInfo *memory, std::size_t size,
                                      bool allocated) noexcept {
  ChunkInfo value{size | allocated};
  *memory = value;
  ChunkInfo *footer = memory->getFooter();
  footer->setSize(size);
  footer->setAllocated(allocated);
}

// No bounds check
Allocator::ChunkInfo *Allocator::ChunkInfo::next() noexcept {
  return this + getSize() / sizeof(ChunkInfo);
}
Allocator::ChunkInfo *Allocator::ChunkInfo::prev(void *heap) noexcept {
  if (this == static_cast<ChunkInfo *>(heap)) {
    return nullptr;
  }
  return this - (this - 1)->getSize() / sizeof(ChunkInfo);
}

bool Allocator::ChunkInfo::shouldSplit(
    std::size_t allocateSize) const noexcept {
  return getSize() > allocateSize + sizeof(ChunkInfo);
}

Allocator::Allocator(std::size_t initialCapacity)
    : capacity{alignUp(initialCapacity)},
      heap{mmap(NULL, alignUp(initialCapacity), PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)} {
  if (heap == MAP_FAILED) {
    throw std::runtime_error("mmap failed");
  }
  ChunkInfo::writeChunk(static_cast<ChunkInfo *>(heap), capacity, false);
  freeList = static_cast<ChunkInfo *>(heap);
}

Allocator::~Allocator() noexcept { munmap(heap, capacity); }

void *Allocator::allocate(std::size_t size) {
  size = alignUp(size + sizeof(ChunkInfo));
  ChunkInfo *cur = nextFreeFittingChunk(freeList, size);
  if (!chunkInHeap(cur)) {
    throw std::bad_alloc();
  }
  cur->setAllocated(true);
  if (cur->shouldSplit(size)) {
    splitChunk(cur, size);
  }
  if (freeList->getAllocated()) {
    freeList = nextFreeChunk(cur->next());
  }
  return reinterpret_cast<char *>(cur) + sizeof(ChunkInfo);
}

void Allocator::free(void *ptr) noexcept {
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
  if (!chunkInHeap(freeList) || chunk < freeList) {
    freeList = chunk;
  }
}

Allocator::ChunkInfo *Allocator::getChunkFromPointer(void *ptr) noexcept {
  return reinterpret_cast<ChunkInfo *>(static_cast<char *>(ptr) -
                                       sizeof(ChunkInfo));
}

bool Allocator::shouldMergeNextChunk(ChunkInfo *chunk) const noexcept {
  ChunkInfo *next = chunk->next();
  return chunkInHeap(next) && !next->getAllocated();
}

bool Allocator::shouldMergePrevChunk(ChunkInfo *chunk) const noexcept {
  ChunkInfo *prev = chunk->prev(heap);
  return chunkInHeap(prev) && !prev->getAllocated();
}

void Allocator::mergePrevFreeChunk(ChunkInfo *chunk) noexcept {
  ChunkInfo *prev = chunk->prev(heap);
  std::size_t newSize = prev->getSize() + chunk->getSize();
  prev->setSize(newSize);
  prev->getFooter()->setSize(newSize);
}

void Allocator::mergeNextFreeChunk(ChunkInfo *chunk) noexcept {
  ChunkInfo *next = chunk->next();
  std::size_t newSize = next->getSize() + chunk->getSize();
  chunk->setSize(newSize);
  next->getFooter()->setSize(newSize);
}

void Allocator::splitChunk(ChunkInfo *chunk, std::size_t size) noexcept {
  std::size_t remainingFreeSize = chunk->getSize() - size;
  ChunkInfo::writeChunk(chunk, size, true);
  ChunkInfo::writeChunk(chunk->next(), remainingFreeSize, false);
}

Allocator::ChunkInfo *Allocator::nextFreeChunk(ChunkInfo *startChunk) noexcept {
  ChunkInfo *nextFree = startChunk;
  while (chunkInHeap(nextFree) && nextFree->getAllocated()) {
    nextFree = nextFree->next();
  }
  return nextFree;
}

Allocator::ChunkInfo *
Allocator::nextFreeFittingChunk(ChunkInfo *startChunk,
                                std::size_t size) noexcept {
  ChunkInfo *cur = startChunk;
  while (chunkInHeap(cur) && (cur->getAllocated() || cur->getSize() < size)) {
    cur = cur->next();
  }
  return cur;
}

bool Allocator::chunkInHeap(ChunkInfo *chunk) const noexcept {
  return heap <= chunk && chunk < static_cast<ChunkInfo *>(heap) + capacity / 8;
}
