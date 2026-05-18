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
    std::size_t getSize();
    void setSize(std::size_t size);

    bool getAllocated();
    void setAllocated(bool allocated);

    inline ChunkInfo *getFooter();
    static void writeChunk(ChunkInfo *memory, std::size_t size, bool allocated);

    // No bounds check
    ChunkInfo *next();
    ChunkInfo *prev(void *heap);
    bool shouldSplit(std::size_t allocateSize);
  };
  std::size_t capacity;
  void *heap;
  ChunkInfo *free_list;

  ChunkInfo *next_free_chunk(ChunkInfo *startChunk);
  ChunkInfo *next_free_chunk(ChunkInfo *startChunk, std::size_t size);
  void splitChunk(ChunkInfo *chunk, std::size_t size);

  bool shouldMergeNextChunk(ChunkInfo *chunk);
  bool shouldMergePrevChunk(ChunkInfo *chunk);
  void mergeNextFreeChunk(ChunkInfo *chunk);
  void mergePrevFreeChunk(ChunkInfo *chunk);
  ChunkInfo *getChunkFromPointer(void *ptr);

  std::size_t alignUp(std::size_t size);

  bool chunkInHeap(ChunkInfo *chunk);

public:
  Allocator(std::size_t initial_capacity);
  ~Allocator();
  virtual void *allocate(std::size_t size);
  virtual void free(void *ptr);
};
