#define MYLOG_TAG "MkvIStreamReader"
#include "BasicLog.h"
#include "MkvFileReader.h"
#include "IMoviePlayer.h"

#include <cstdio>
#include <cstdlib>

class MkvIStreamReader : public IMkvFileReader
{
public:
  MkvIStreamReader();
  virtual ~MkvIStreamReader();

  bool Open(IMovieReadStream *stream);
  void Close();

  virtual int Read(void *buffer, int64_t length);
  virtual int Seek(int64_t offset, int whence);
  virtual int64_t Tell() const;

private:
  MkvIStreamReader(const MkvIStreamReader &);
  MkvIStreamReader &operator=(const MkvIStreamReader &);

  IMovieReadStream *mStream;
};

MkvIStreamReader::MkvIStreamReader()
: mStream(nullptr)
{}

MkvIStreamReader::~MkvIStreamReader()
{
  Close();
}

bool
MkvIStreamReader::Open(IMovieReadStream *stream)
{
  if (stream == nullptr) {
    return false;
  }
  mStream = stream;
  mStream->AddRef();
  return true;
}


void
MkvIStreamReader::Close()
{
  if (mStream) {
    mStream->Release();
    mStream = nullptr;
  }
}

int
MkvIStreamReader::Read(void *buffer, int64_t len)
{
  if (mStream == NULL) {
    return 0;
  }
  size_t readed = mStream->Read(buffer, static_cast<size_t>(len));
  return (readed == static_cast<size_t>(len));
}

int 
MkvIStreamReader::Seek(int64_t offset, int whence)
{
  if (mStream == NULL) {
    return -1;
  }
  mStream->Seek(offset, whence);
  return 0;
}

int64_t 
MkvIStreamReader::Tell() const
{
  if (mStream == NULL) {
    return 0;
  }
  return mStream->Tell();
}

IMkvFileReader *IMkvFileReader::Create(IMovieReadStream *stream)
{
  MkvIStreamReader *ret = new MkvIStreamReader();
  if (ret && ret->Open(stream)) {
    return ret;
  }
  delete ret;
  return nullptr;
}