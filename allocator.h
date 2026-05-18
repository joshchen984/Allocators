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
    std::size_t getSize() { return meta & ~(ALIGN - 1); }
    void setSize(std::size_t size) {
      meta = (meta & (ALIGN - 1)) | (size & ~(ALIGN - 1));
    }

    bool getAllocated() { return meta & 1; }
    void setAllocated(bool allocated) { meta = (meta & ~1UL) | allocated; }

    ChunkInfo *getFooter() { return this + getSize() / sizeof(ChunkInfo) - 1; }

    static void writeChunk(ChunkInfo *memory, std::size_t size,
                           bool allocated) {
      ChunkInfo value{size | allocated};
      *memory = value;
      ChunkInfo *footer = memory->getFooter();
      footer->setSize(size);
      footer->setAllocated(allocated);
    }

    // No bounds check
    ChunkInfo *next() { return this + getSize() / sizeof(ChunkInfo); }
    ChunkInfo *prev(void *heap) {
      if (this == static_cast<ChunkInfo *>(heap)) {
        return nullptr;
      }
      return this - (this - 1)->getSize() / sizeof(ChunkInfo);
    }

    bool shouldSplit(std::size_t allocateSize) {
      return getSize() > allocateSize + sizeof(ChunkInfo);
    }
  };
  /*struct std::size_t {
    std::size_t *prev{};
    std::size_t *next{};
    std::uint32_t size{};
    bool allocated{};
  };
  */
  std::uint32_t capacity;
  void *heap;
  ChunkInfo *free_list;

  ChunkInfo *next_free_chunk(ChunkInfo *startChunk);
  ChunkInfo *next_free_chunk(ChunkInfo *startChunk, std::uint32_t size);
  void splitChunk(ChunkInfo *chunk, std::uint32_t size);

  bool shouldMergeNextChunk(ChunkInfo *chunk);
  bool shouldMergePrevChunk(ChunkInfo *chunk);
  void mergeNextFreeChunk(ChunkInfo *chunk);
  void mergePrevFreeChunk(ChunkInfo *chunk);
  ChunkInfo *getChunkFromPointer(void *ptr);

  std::size_t alignUp(std::size_t size);

  bool chunkInHeap(ChunkInfo *chunk);

public:
  Allocator(std::uint32_t initial_capacity);
  ~Allocator();
  virtual void *allocate(std::uint32_t size);
  virtual void free(void *ptr);
};
