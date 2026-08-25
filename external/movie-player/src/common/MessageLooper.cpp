#define MYLOG_TAG "MessageLooper"
#include "BasicLog.h"

#include "CommonUtils.h"
#include "MessageLooper.h"

MessageLooper::MessageLooper()
: mIsRunning(false)
{}

MessageLooper::~MessageLooper()
{
  if (mIsRunning) {
    LOGV("MessageLooper deleted while still running. "
         "Some messages will not be processed\n");
    QuitLoop();
  }
}

void
MessageLooper::StartThread()
{
  // looperスレッド開始
  mWorker    = std::thread([this] { MessageLoop(); });
  mIsRunning = true;
}

void
MessageLooper::StopThread()
{
  // looperスレッド停止
  if (mIsRunning) {
    QuitLoop();
  }
}

void
MessageLooper::QuitLoop()
{
  PostQuitMessage();
  mWorker.join();

  mIsRunning = false;
}

void
MessageLooper::PostQuitMessage()
{
  bool doQuit  = true;
  bool doFlush = true;
  AddMessage({ MSG_SPECIAL, 0, nullptr, doQuit }, doFlush);
}

void
MessageLooper::Post(int32_t what, int64_t arg, void *data, bool flush)
{
  AddMessage({ what, arg, data, false }, flush);
}

void
MessageLooper::AddMessage(Message &&msg, bool flush)
{
  std::lock_guard<std::mutex> lk(mMessageMutex);

  if (flush) {
    // std::queue には clear() がない
    mMessageQueue = {};
  }
  mMessageQueue.push(msg);
  mMessageCond.notify_all();
}

void
MessageLooper::MessageLoop()
{
  while (true) {
    Message msg;
    {
      // condition_variableにはunique_lockが必要(lock_guardではだめ)
      std::unique_lock<std::mutex> ulk(mMessageMutex);

      mMessageCond.wait(ulk, [this] { return !mMessageQueue.empty(); });
      msg = mMessageQueue.front();
      mMessageQueue.pop();
    }

    // 終了メッセージなのでループを抜ける
    if (msg.quit) {
      // LOGV("MessageLooper: Quit message arrived.\n");
      return;
    }

    HandleMessage(msg.what, msg.arg, msg.obj);
  }
}

void
MessageLooper::HandleMessage(int32_t what, int64_t arg, void *obj)
{}
