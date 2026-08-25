#pragma once

#include <cstdint>
#include <vector>

//------------------------------------------------------
// ファイルデータ用のアロケータ
//------------------------------------------------------

extern "C" void *file_malloc(size_t size);
extern "C" void *file_realloc(void *p, size_t newSize);
extern "C" void file_free(void *p);

/**
 * @brief ファイル読み込み用バッファ（共有用にサイズ情報も保持）
 */
class tTJSBinaryStreamBuffer {
	unsigned char *mBuffer;
	size_t mReserveSize;
	size_t mSize;
public:
	tTJSBinaryStreamBuffer() : mBuffer(0), mSize(0) {
	}

	static tTJSBinaryStreamBuffer *create(size_t size) {
		tTJSBinaryStreamBuffer *buf = new tTJSBinaryStreamBuffer();
		if (buf->resize(size)) {
			return buf;
		}
		delete buf;
		return 0;
	}

	virtual ~tTJSBinaryStreamBuffer() {
		file_free((void*)mBuffer);
	}

	bool resize(size_t newSize) {
		if (mBuffer) {
			void *newBuffer = file_realloc((void*)mBuffer, newSize);
			if (newBuffer) {
				mBuffer = (unsigned char*)newBuffer;
				mSize = newSize;
				return true;
			}
		} else {
			mBuffer = (unsigned char*)file_malloc(newSize);
			if (mBuffer) {
				mSize = newSize;
				return true;
			}
		}
		return false;
	}

	const unsigned char *buffer() const { return mBuffer; }
	const size_t size() const { return mSize; }
};
