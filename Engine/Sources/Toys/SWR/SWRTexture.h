#pragma once
#include "Yes.h"
#include "SWR.h"
#include "SWRCommon.h"
#include "SWRSampler.h"

namespace Yes::SWR
{
	class SWRTexture2D : public SWRTextureSampler
	{
	public:
		SWRTexture2D(const Texture2DDesc* desc)
			: mWidth(desc->Width)
			, mHeight(desc->Height)
			, mPixelFormat(desc->PixelFormat)
			, mPixelSize(GetPixelFormatSize(desc->PixelFormat))
		{
			mData = malloc(mPixelSize * mWidth * mHeight);
		}
		void* At(float x, float y) override
		{
			auto ix = (int32)(x * mWidth);
			auto iy = (int32)(y * mHeight);
			return At(ix, iy);
		}
		void* At(int32 ix, int32 iy) override
		{
			auto bp = (uint8*)mData;
			return bp + (ix + iy * mWidth) * mPixelSize;
		}
		void Fill(void* fillColor, size_t colorSize);
	private:
		size_t mWidth;
		size_t mHeight;
		PixelFormat mPixelFormat;
		size_t mPixelSize;
		void* mData;
	};
	SWRHandle CreateSWRTexture2D(const Texture2DDesc* desc);
}