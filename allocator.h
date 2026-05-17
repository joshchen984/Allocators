#pragma once
#include <cstdint>

class Allocator {
private:
  struct ChunkInfo {
    ChunkInfo *prev{};
    ChunkInfo *next{};
    std::uint32_t size{};
    bool allocated{};
  };
  std::uint32_t capacity;
  void *heap;
  ChunkInfo *free_list;

  ChunkInfo *next_free_chunk(ChunkInfo *startChunk);
  ChunkInfo *next_free_chunk(ChunkInfo *startChunk, std::uint32_t size);
  bool should_split(std::uint32_t chunkSize, std::uint32_t allocateSize);
  void *nextChunkLocation(ChunkInfo *chunk);
  void splitChunk(ChunkInfo *chunk, std::uint32_t size);

  bool shouldMergeNextChunk(ChunkInfo *chunk);
  bool shouldMergePrevChunk(ChunkInfo *chunk);
  void mergeNextFreeChunk(ChunkInfo *chunk);
  void mergePrevFreeChunk(ChunkInfo *chunk);
  ChunkInfo *getChunkFromPointer(void *ptr);

  std::size_t alignUp(std::size_t size);

public:
  Allocator(std::uint32_t initial_capacity);
  ~Allocator();
  virtual void *allocate(std::uint32_t size);
  virtual void free(void *ptr);
};
