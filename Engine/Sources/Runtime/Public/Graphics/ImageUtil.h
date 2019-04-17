#pragma once
#include "Yes.h"
#include "Misc/SharedObject.h"

namespace Yes
{
	class RawImage : public SharedObject
	{
	public:
		RawImage(size_t width, size_t height, size_t bpp, void* content);
		~RawImage();
		size_t GetWidth() { return mWidth; }
		size_t GetRowSize() { return mWidth * mTexelSize;  }
		size_t GetSliceSize() { return mWidth * mHeight * mTexelSize; }
		size_t GetHeight() { return mHeight; }
		void* GetData() { return mData; }
	protected:
		size_t mWidth;
		size_t mHeight;
		size_t mTexelSize;
		void* mData;
	};
	TRef<RawImage> LoadRawImage(ISharedBuffer* buffer);
}