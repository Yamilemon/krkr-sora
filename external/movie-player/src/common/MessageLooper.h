#pragma once

#include <cstdint>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>

struct Message
{
  int32_t what;
  int64_t arg;
  void *obj;
  bool quit;
};

class MessageLooper
{
public:
  MessageLooper();
  virtual ~MessageLooper();

  // MEMO PostMessageだとwindows.hを導入する環境でマクロに荒らされるので変名した
  void Post(int32_t what, int64_t arg = 0, void *data = nullptr, bool flush = false);
  void QuitLoop();

  // return: メッセージを処理した場合はtrue
  virtual void HandleMessage(int32_t what, int64_t arg, void *data);

protected:
  static const int32_t MSG_SPECIAL = INT32_MAX;

protected:
  void StartThread();
  void StopThread();
  void PostQuitMessage();
  void AddMessage(Message &&msg, bool flush);
  void MessageLoop();
  bool IsRunning() const { return mIsRunning; }

protected:
  bool mIsRunning;
  std::thread mWorker;

  // メッセージキュー管理
  std::queue<Message> mMessageQueue;
  std::mutex mMessageMutex;
  std::condition_variable mMessageCond;
};