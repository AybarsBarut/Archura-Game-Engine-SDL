#pragma once

#include "MemoryTracker.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace Archura::Memory {

constexpr size_t kDefaultAlignment = 16;
constexpr uint32_t kInvalidPoolIndex = std::numeric_limits<uint32_t>::max();

constexpr size_t ConstexprMax(size_t a, size_t b) { return a > b ? a : b; }

inline size_t NormalizeAlignment(size_t alignment) {
  if (alignment < kDefaultAlignment)
    alignment = kDefaultAlignment;

  size_t result = 1;
  while (result < alignment)
    result <<= 1;
  return result;
}

inline uintptr_t AlignForward(uintptr_t address, size_t alignment) {
  const uintptr_t mask = static_cast<uintptr_t>(alignment - 1);
  return (address + mask) & ~mask;
}

inline void *AllocateAligned(size_t sizeBytes, size_t alignment) {
  return ::operator new(sizeBytes,
                        std::align_val_t(NormalizeAlignment(alignment)));
}

inline void FreeAligned(void *ptr, size_t alignment) noexcept {
  if (!ptr)
    return;
  ::operator delete(ptr, std::align_val_t(NormalizeAlignment(alignment)));
}

class LinearAllocator {
public:
  explicit LinearAllocator(size_t capacityBytes,
                           const char *name = "LinearAllocator",
                           size_t alignment = kDefaultAlignment)
      : m_Capacity(capacityBytes), m_Alignment(NormalizeAlignment(alignment)),
        m_Name(name ? name : "LinearAllocator") {
    m_Start = static_cast<std::byte *>(AllocateAligned(m_Capacity, m_Alignment));
    MemoryTracker::RegisterAllocator(this, m_Name, m_Capacity);
  }

  ~LinearAllocator() {
    MemoryTracker::UnregisterAllocator(this);
    FreeAligned(m_Start, m_Alignment);
  }

  LinearAllocator(const LinearAllocator &) = delete;
  LinearAllocator &operator=(const LinearAllocator &) = delete;

  void *Allocate(size_t sizeBytes, size_t alignment = kDefaultAlignment) {
    if (sizeBytes == 0)
      return nullptr;

    alignment = NormalizeAlignment(alignment);
    const uintptr_t start = reinterpret_cast<uintptr_t>(m_Start);
    const uintptr_t current = start + m_Offset;
    const uintptr_t aligned = AlignForward(current, alignment);
    const size_t padding = static_cast<size_t>(aligned - current);
    const size_t nextOffset = m_Offset + padding + sizeBytes;

    if (nextOffset > m_Capacity)
      return nullptr;

    m_Offset = nextOffset;
    MemoryTracker::RecordAllocation(this, padding + sizeBytes);
    MemoryTracker::RecordState(this, m_Offset, 0.0f);
    return reinterpret_cast<void *>(aligned);
  }

  template <typename T> T *AllocateArray(size_t count) {
    static_assert(!std::is_void<T>::value, "Cannot allocate void arrays");
    return static_cast<T *>(Allocate(sizeof(T) * count, alignof(T)));
  }

  template <typename T, typename... Args> T *Construct(Args &&...args) {
    void *memory = Allocate(sizeof(T), alignof(T));
    if (!memory)
      return nullptr;
    return new (memory) T(std::forward<Args>(args)...);
  }

  void Reset() {
    m_Offset = 0;
    MemoryTracker::RecordReset(this);
  }

  size_t GetCapacity() const { return m_Capacity; }
  size_t GetUsedMemory() const { return m_Offset; }
  size_t GetRemainingMemory() const { return m_Capacity - m_Offset; }

private:
  std::byte *m_Start = nullptr;
  size_t m_Capacity = 0;
  size_t m_Offset = 0;
  size_t m_Alignment = kDefaultAlignment;
  const char *m_Name = "LinearAllocator";
};

class StackAllocator {
public:
  using Marker = size_t;

  explicit StackAllocator(size_t capacityBytes,
                          const char *name = "StackAllocator",
                          size_t alignment = kDefaultAlignment)
      : m_Capacity(capacityBytes), m_Alignment(NormalizeAlignment(alignment)),
        m_Name(name ? name : "StackAllocator") {
    m_Start = static_cast<std::byte *>(AllocateAligned(m_Capacity, m_Alignment));
    MemoryTracker::RegisterAllocator(this, m_Name, m_Capacity);
  }

  ~StackAllocator() {
    MemoryTracker::UnregisterAllocator(this);
    FreeAligned(m_Start, m_Alignment);
  }

  StackAllocator(const StackAllocator &) = delete;
  StackAllocator &operator=(const StackAllocator &) = delete;

  void *Allocate(size_t sizeBytes, size_t alignment = kDefaultAlignment) {
    if (sizeBytes == 0)
      return nullptr;

    alignment = NormalizeAlignment(alignment);
    const uintptr_t start = reinterpret_cast<uintptr_t>(m_Start);
    const uintptr_t current = start + m_Offset;
    const uintptr_t userAddress =
        AlignForward(current + sizeof(AllocationHeader), alignment);
    const uintptr_t headerAddress = userAddress - sizeof(AllocationHeader);
    const size_t nextOffset =
        static_cast<size_t>((userAddress + sizeBytes) - start);

    if (nextOffset > m_Capacity)
      return nullptr;

    auto *header = reinterpret_cast<AllocationHeader *>(headerAddress);
    header->previousOffset = m_Offset;
    header->allocationBytes = nextOffset - m_Offset;
    m_Offset = nextOffset;

    MemoryTracker::RecordAllocation(this, header->allocationBytes);
    MemoryTracker::RecordState(this, m_Offset, 0.0f);
    return reinterpret_cast<void *>(userAddress);
  }

  template <typename T, typename... Args> T *Construct(Args &&...args) {
    void *memory = Allocate(sizeof(T), alignof(T));
    if (!memory)
      return nullptr;
    return new (memory) T(std::forward<Args>(args)...);
  }

  void Free(void *ptr) {
    if (!ptr)
      return;

    auto *header = reinterpret_cast<AllocationHeader *>(
        reinterpret_cast<std::byte *>(ptr) - sizeof(AllocationHeader));
    const size_t freedBytes = m_Offset - header->previousOffset;
    m_Offset = header->previousOffset;
    MemoryTracker::RecordFree(this, freedBytes);
    MemoryTracker::RecordState(this, m_Offset, 0.0f);
  }

  Marker GetMarker() const { return m_Offset; }

  void FreeToMarker(Marker marker) {
    if (marker > m_Offset)
      return;
    const size_t freedBytes = m_Offset - marker;
    m_Offset = marker;
    MemoryTracker::RecordFree(this, freedBytes);
    MemoryTracker::RecordState(this, m_Offset, 0.0f);
  }

  void Reset() {
    m_Offset = 0;
    MemoryTracker::RecordReset(this);
  }

  class ScopedMarker {
  public:
    explicit ScopedMarker(StackAllocator &allocator)
        : m_Allocator(allocator), m_Marker(allocator.GetMarker()) {}
    ~ScopedMarker() { m_Allocator.FreeToMarker(m_Marker); }

    ScopedMarker(const ScopedMarker &) = delete;
    ScopedMarker &operator=(const ScopedMarker &) = delete;

  private:
    StackAllocator &m_Allocator;
    Marker m_Marker = 0;
  };

  size_t GetCapacity() const { return m_Capacity; }
  size_t GetUsedMemory() const { return m_Offset; }
  size_t GetRemainingMemory() const { return m_Capacity - m_Offset; }

private:
  struct AllocationHeader {
    size_t previousOffset = 0;
    size_t allocationBytes = 0;
  };

  std::byte *m_Start = nullptr;
  size_t m_Capacity = 0;
  size_t m_Offset = 0;
  size_t m_Alignment = kDefaultAlignment;
  const char *m_Name = "StackAllocator";
};

template <typename T, size_t N> class PoolAllocator {
public:
  static_assert(N > 0, "PoolAllocator capacity must be greater than zero");

  static constexpr size_t ObjectAlignment =
      ConstexprMax(alignof(T), kDefaultAlignment);
  static constexpr size_t ObjectSize =
      ((sizeof(T) + ObjectAlignment - 1) / ObjectAlignment) * ObjectAlignment;

  explicit PoolAllocator(const char *name = "PoolAllocator")
      : m_Name(name ? name : "PoolAllocator") {
    ResetFreeList();
    MemoryTracker::RegisterAllocator(this, m_Name, N * ObjectSize);
  }

  ~PoolAllocator() { MemoryTracker::UnregisterAllocator(this); }

  PoolAllocator(const PoolAllocator &) = delete;
  PoolAllocator &operator=(const PoolAllocator &) = delete;

  T *Allocate() {
    if (m_FreeHead == kInvalidPoolIndex)
      return nullptr;

    const uint32_t index = m_FreeHead;
    m_FreeHead = m_NextFree[index];
    m_NextFree[index] = kInvalidPoolIndex;
    m_UsedCount++;

    MemoryTracker::RecordAllocation(this, ObjectSize);
    MemoryTracker::RecordState(this, GetUsedMemory(), GetFragmentation());
    return PointerAt(index);
  }

  template <typename... Args> T *Create(Args &&...args) {
    T *slot = Allocate();
    if (!slot)
      return nullptr;
    return new (slot) T(std::forward<Args>(args)...);
  }

  void Deallocate(T *ptr) {
    if (!ptr)
      return;
    const uint32_t index = IndexOf(ptr);
    assert(index < N && "Pointer does not belong to this PoolAllocator");

    m_NextFree[index] = m_FreeHead;
    m_FreeHead = index;
    if (m_UsedCount > 0)
      m_UsedCount--;

    MemoryTracker::RecordFree(this, ObjectSize);
    MemoryTracker::RecordState(this, GetUsedMemory(), GetFragmentation());
  }

  void Destroy(T *ptr) {
    if (!ptr)
      return;
    ptr->~T();
    Deallocate(ptr);
  }

  void Reset() {
    ResetFreeList();
    MemoryTracker::RecordReset(this);
    MemoryTracker::RecordState(this, 0, GetFragmentation());
  }

  bool Owns(const T *ptr) const {
    const auto address = reinterpret_cast<uintptr_t>(ptr);
    const auto first = reinterpret_cast<uintptr_t>(&m_Storage[0]);
    const auto last = reinterpret_cast<uintptr_t>(&m_Storage[N]);
    return address >= first && address < last;
  }

  size_t GetCapacity() const { return N; }
  size_t GetUsedCount() const { return m_UsedCount; }
  size_t GetFreeCount() const { return N - m_UsedCount; }
  size_t GetUsedMemory() const { return m_UsedCount * ObjectSize; }

  float GetFragmentation() const {
    return static_cast<float>(GetFreeCount()) / static_cast<float>(N);
  }

private:
  struct alignas(ObjectAlignment) Storage {
    std::byte bytes[ObjectSize];
  };

  void ResetFreeList() {
    for (uint32_t i = 0; i < static_cast<uint32_t>(N - 1); ++i)
      m_NextFree[i] = i + 1;
    m_NextFree[N - 1] = kInvalidPoolIndex;
    m_FreeHead = 0;
    m_UsedCount = 0;
  }

  T *PointerAt(uint32_t index) {
    return reinterpret_cast<T *>(m_Storage[index].bytes);
  }

  uint32_t IndexOf(const T *ptr) const {
    const auto base = reinterpret_cast<uintptr_t>(&m_Storage[0]);
    const auto address = reinterpret_cast<uintptr_t>(ptr);
    return static_cast<uint32_t>((address - base) / sizeof(Storage));
  }

  Storage m_Storage[N];
  uint32_t m_NextFree[N] = {};
  uint32_t m_FreeHead = 0;
  size_t m_UsedCount = 0;
  const char *m_Name = "PoolAllocator";
};

template <size_t BlockSize, size_t BlockCount,
          size_t Alignment = kDefaultAlignment>
class FixedBlockPool {
public:
  static_assert(BlockSize > 0, "BlockSize must be greater than zero");
  static_assert(BlockCount > 0, "BlockCount must be greater than zero");

  static constexpr size_t BlockAlignment =
      ConstexprMax(Alignment, kDefaultAlignment);
  static constexpr size_t AlignedBlockSize =
      ((BlockSize + BlockAlignment - 1) / BlockAlignment) * BlockAlignment;

  explicit FixedBlockPool(const char *name = "FixedBlockPool")
      : m_Name(name ? name : "FixedBlockPool") {
    ResetFreeList();
    MemoryTracker::RegisterAllocator(this, m_Name,
                                     AlignedBlockSize * BlockCount);
  }

  ~FixedBlockPool() { MemoryTracker::UnregisterAllocator(this); }

  FixedBlockPool(const FixedBlockPool &) = delete;
  FixedBlockPool &operator=(const FixedBlockPool &) = delete;

  void *AllocateBlock() {
    if (m_FreeHead == kInvalidPoolIndex)
      return nullptr;

    const uint32_t index = m_FreeHead;
    m_FreeHead = m_NextFree[index];
    m_NextFree[index] = kInvalidPoolIndex;
    m_UsedCount++;

    MemoryTracker::RecordAllocation(this, AlignedBlockSize);
    MemoryTracker::RecordState(this, GetUsedMemory(), GetFragmentation());
    return m_Blocks[index].bytes;
  }

  void FreeBlock(void *ptr) {
    if (!ptr)
      return;

    const uint32_t index = IndexOf(ptr);
    assert(index < BlockCount && "Pointer does not belong to this pool");

    m_NextFree[index] = m_FreeHead;
    m_FreeHead = index;
    if (m_UsedCount > 0)
      m_UsedCount--;

    MemoryTracker::RecordFree(this, AlignedBlockSize);
    MemoryTracker::RecordState(this, GetUsedMemory(), GetFragmentation());
  }

  void Reset() {
    ResetFreeList();
    MemoryTracker::RecordReset(this);
    MemoryTracker::RecordState(this, 0, GetFragmentation());
  }

  size_t GetUsedMemory() const { return m_UsedCount * AlignedBlockSize; }
  size_t GetUsedCount() const { return m_UsedCount; }
  size_t GetCapacity() const { return BlockCount; }

  float GetFragmentation() const {
    return static_cast<float>(BlockCount - m_UsedCount) /
           static_cast<float>(BlockCount);
  }

private:
  struct alignas(BlockAlignment) BlockStorage {
    std::byte bytes[AlignedBlockSize];
  };

  void ResetFreeList() {
    for (uint32_t i = 0; i < static_cast<uint32_t>(BlockCount - 1); ++i)
      m_NextFree[i] = i + 1;
    m_NextFree[BlockCount - 1] = kInvalidPoolIndex;
    m_FreeHead = 0;
    m_UsedCount = 0;
  }

  uint32_t IndexOf(const void *ptr) const {
    const auto base = reinterpret_cast<uintptr_t>(&m_Blocks[0]);
    const auto address = reinterpret_cast<uintptr_t>(ptr);
    return static_cast<uint32_t>((address - base) / sizeof(BlockStorage));
  }

  BlockStorage m_Blocks[BlockCount];
  uint32_t m_NextFree[BlockCount] = {};
  uint32_t m_FreeHead = 0;
  size_t m_UsedCount = 0;
  const char *m_Name = "FixedBlockPool";
};

template <typename T, size_t BlockSize, size_t BlockCount,
          size_t Alignment = kDefaultAlignment>
class PoolStdAllocator {
public:
  using value_type = T;
  using PoolType = FixedBlockPool<BlockSize, BlockCount, Alignment>;

  template <typename, size_t, size_t, size_t> friend class PoolStdAllocator;

  PoolStdAllocator()
      : m_Pool(std::make_shared<PoolType>("PoolStdAllocator")) {}

  explicit PoolStdAllocator(std::shared_ptr<FixedBlockPool<BlockSize, BlockCount,
                                                           Alignment>>
                                pool)
      : m_Pool(std::move(pool)) {}

  template <typename U>
  PoolStdAllocator(const PoolStdAllocator<U, BlockSize, BlockCount, Alignment>
                       &other) noexcept
      : m_Pool(other.m_Pool) {}

  T *allocate(std::size_t count) {
    if (count == 0)
      return nullptr;
    if (count * sizeof(T) > BlockSize)
      throw std::bad_alloc();

    void *block = m_Pool->AllocateBlock();
    if (!block)
      throw std::bad_alloc();
    return static_cast<T *>(block);
  }

  void deallocate(T *ptr, std::size_t) noexcept { m_Pool->FreeBlock(ptr); }

  template <typename U> struct rebind {
    using other = PoolStdAllocator<U, BlockSize, BlockCount, Alignment>;
  };

  std::shared_ptr<PoolType> GetPool() const { return m_Pool; }

private:
  std::shared_ptr<PoolType> m_Pool;
};

template <typename T, typename U, size_t BlockSize, size_t BlockCount,
          size_t Alignment>
bool operator==(
    const PoolStdAllocator<T, BlockSize, BlockCount, Alignment> &a,
    const PoolStdAllocator<U, BlockSize, BlockCount, Alignment> &b) {
  return a.GetPool() == b.GetPool();
}

template <typename T, typename U, size_t BlockSize, size_t BlockCount,
          size_t Alignment>
bool operator!=(
    const PoolStdAllocator<T, BlockSize, BlockCount, Alignment> &a,
    const PoolStdAllocator<U, BlockSize, BlockCount, Alignment> &b) {
  return !(a == b);
}

} // namespace Archura::Memory
