#pragma once
#include <cstdint>

class Allocator {
private:
  class ChunkInfo {
  private:
    constexpr static int ALIGN = 8;
    std::size_t meta;

    ChunkInfo(std::size_t metadata) : meta{metadata} {}

  public:
    std::size_t getSize() const noexcept;
    void setSize(std::size_t size) noexcept;

    bool getAllocated() const noexcept;
    void setAllocated(bool allocated) noexcept;

    inline ChunkInfo *getFooter() noexcept;
    static void writeChunk(ChunkInfo *memory, std::size_t size,
                           bool allocated) noexcept;

    // No bounds check
    ChunkInfo *next() noexcept;
    ChunkInfo *prev(void *heap) noexcept;
    bool shouldSplit(std::size_t allocateSize) const noexcept;
  };
  std::size_t capacity;
  void *heap;
  ChunkInfo *freeList;

  ChunkInfo *nextFreeChunk(ChunkInfo *startChunk) noexcept;
  ChunkInfo *nextFreeFittingChunk(ChunkInfo *startChunk,
                                  std::size_t size) noexcept;
  void splitChunk(ChunkInfo *chunk, std::size_t size) noexcept;

  bool shouldMergeNextChunk(ChunkInfo *chunk) const noexcept;
  bool shouldMergePrevChunk(ChunkInfo *chunk) const noexcept;
  void mergeNextFreeChunk(ChunkInfo *chunk) noexcept;
  void mergePrevFreeChunk(ChunkInfo *chunk) noexcept;
  ChunkInfo *getChunkFromPointer(void *ptr) noexcept;

  bool chunkInHeap(ChunkInfo *chunk) const noexcept;

public:
  Allocator(std::size_t initialCapacity);
  ~Allocator() noexcept;
  virtual void *allocate(std::size_t size);
  virtual void free(void *ptr) noexcept;
};
