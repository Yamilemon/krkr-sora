#pragma once

#include "CommonUtils.h"
#include "Constants.h"

#include <vector>
#include <mutex>

// -----------------------------------------------------------------------------
// generic buffer queue
// -----------------------------------------------------------------------------
struct BufferQueueEntryBase
{
  uint8_t *data;
  size_t dataSize;
  size_t capacity;

  uint8_t *adddata;
  size_t adddataSize;
  size_t addcapacity;

  BufferQueueEntryBase()
  : data(nullptr)
  , dataSize(0)
  , capacity(0)
  , adddata(nullptr)
  , adddataSize(0)
  , addcapacity(0)
  {
    InitBuffer();
  }
  virtual ~BufferQueueEntryBase()
  {
    Release();
    ReleaseAdd();
  }

  void CopyFrom(BufferQueueEntryBase *from)
  {
    Resize(from->dataSize);
    dataSize = from->dataSize;
    memcpy(data, from->data, dataSize);

    ResizeAdd(from->adddataSize);
    adddataSize = from->adddataSize;
    memcpy(adddata, from->adddata, adddataSize);
  }

  virtual void Init(int32_t bufIdx) = 0;
  virtual void Clear()              = 0;

  void InitBuffer()
  {
    data        = nullptr;
    dataSize    = 0;
    capacity    = 0;
    adddata     = nullptr;
    adddataSize = 0;
    addcapacity = 0;
  }

  virtual void Alloc(size_t allocSize)
  {
    data     = new uint8_t[allocSize];
    capacity = allocSize;
  }

  virtual void Resize(size_t newSize)
  {
    if (capacity < newSize) {
      Realloc(newSize);
    }
  }

  virtual void Realloc(size_t newSize)
  {
    Release();
    Alloc(newSize);
  }

  virtual void Release()
  {
    if (data != nullptr) {
      delete[] data;
      data     = nullptr;
      dataSize = 0;
      capacity = 0;
    }
  }

  virtual void AllocAdd(size_t allocSize)
  {
    adddata     = new uint8_t[allocSize];
    addcapacity = allocSize;
  }

  virtual void ResizeAdd(size_t newSize)
  {
    if (addcapacity < newSize) {
      ReallocAdd(newSize);
    }
  }

  virtual void ReallocAdd(size_t newSize)
  {
    ReleaseAdd();
    AllocAdd(newSize);
  }

  virtual void ReleaseAdd()
  {
    if (adddata != nullptr) {
      delete[] adddata;
      adddata     = nullptr;
      adddataSize = 0;
      addcapacity = 0;
    }
  }
};

template<class T>
class BufferQueue
{
public:
  BufferQueue()
  {
    // バッファオブジェクトにインタフェースを強制する
    static_assert(std::is_base_of<BufferQueueEntryBase, T>::value,
                  "Template parameter T must be subclass of BufferQueueEntryBase");
  }
  ~BufferQueue() { Done(); }

  void Init(size_t poolSize)
  {
    INLINE_ASSERT(mBuffers.empty() == true, "BufferQueue: invalid initialize.\n");

    mReadableQueue.Clear();
    mWritableQueue.Clear();
    for (size_t i = 0; i < poolSize; i++) {
      mWritableQueue.Enqueue(i);
    }
    mBuffers.resize(poolSize);
  }

  void Done()
  {
    INLINE_ASSERT(mBuffers.empty() != true, "BufferQueue: invalid shutdown.\n");

    mReadableQueue.Clear();
    mWritableQueue.Clear();
    for (T &buf : mBuffers) {
      buf.Release();
    }
    mBuffers.clear();
  }

  void Clear()
  {
    mReadableQueue.Clear();
    mWritableQueue.Clear();
    for (size_t i = 0; i < mBuffers.size(); i++) {
      mWritableQueue.Enqueue(i);
      mBuffers[i].Init(i);
    }
  }

  bool CheckIndex(int32_t index)
  {
    return (0 <= index && index < (int32_t)mBuffers.size());
  }

  void EnqueueBufferIndexForReader(int32_t index)
  {
    if (CheckIndex(index)) {
      mReadableQueue.Enqueue(index);
    }
  }

  void EnqueueBufferIndexForWriter(int32_t index)
  {
    if (CheckIndex(index)) {
      mWritableQueue.Enqueue(index);
    }
  }

  int32_t DequeueIndexForReader()
  {
    int32_t index = -1;
    mReadableQueue.Dequeue(index);
    return index;
  }

  int32_t DequeueIndexForWriter()
  {
    int32_t index = -1;
    mWritableQueue.Dequeue(index);
    return index;
  }

  size_t SizeForReader() const { return mReadableQueue.Size(); }
  size_t SizeForWriter() const { return mWritableQueue.Size(); }

  T *GetBuffer(int32_t index)
  {
    T *result = nullptr;
    if (CheckIndex(index)) {
      result = &mBuffers[index];
    }
    return result;
  }

  void ReleaseBuffer(int32_t index)
  {
    if (CheckIndex(index)) {
      T *buf = GetBuffer(index);
      buf->Release();
    }
  }

  std::vector<T> &Buffers() { return mBuffers; }

private:
  // std::mutex mGlobalLock;
  std::vector<T> mBuffers;
  SafeQueue<int32_t> mReadableQueue;
  SafeQueue<int32_t> mWritableQueue;
};
