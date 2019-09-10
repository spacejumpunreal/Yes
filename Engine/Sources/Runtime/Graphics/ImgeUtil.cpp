#include "Runtime/Public/Graphics/ImageUtil.h"
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace Yes
{
	RawImage::RawImage(size_t width, size_t height, size_t bpp)
		: mWidth(width)
		, mHeight(height)
		, mTexelSize(bpp)
	{
		size_t sz = width * height * bpp;
		mData = malloc(sz);
	}
	RawImage::RawImage(size_t width, size_t height, size_t bpp, void* content)
		: RawImage(width, height, bpp)
	{
		size_t sz = width * height * bpp;
		mData = malloc(sz);
		memcpy(mData, content, sz);
	}
	RawImage::~RawImage()
	{
		free(mData);
	}
	TRef<RawImage> LoadRawImage(ISharedBuffer* buffer)
	{
		const int DesiredBPP = 4;
		int width, height;
		unsigned char* stream = stbi_load_from_memory((unsigned char*)buffer->GetData(), (int)buffer->GetSize(), &width, &height, nullptr, DesiredBPP);
		RawImage* image = new RawImage(width, height, DesiredBPP, stream);
		STBI_FREE(stream);
		return image;
	}
	TRef<RawImage> BuildSingleColorTexture(const V4F& color, size_t size)
	{
		TRef<RawImage> raw = new RawImage(size, size, 4);
		RGBA* data = (RGBA*)raw->GetData();
		RGBA fill = RGBA::FromV4F(color);
		for (size_t i = 0; i < size * size; ++i)
		{
			data[i] = fill;
		}
		return raw;
	}
	TRef<RawImage> BuildCheckTexture(const V4F & colorA, const V4F & colorB, size_t size, size_t checkSize)
	{
		RawImage* raw = new RawImage(size, size, 4);
		RGBA* data = (RGBA*)raw->GetData();
		RGBA a = RGBA::FromV4F(colorA);
		RGBA b = RGBA::FromV4F(colorB);
		for (size_t y = 0; y < size; ++y)
		{
			size_t yStep = y / checkSize;
			for (size_t x = 0; x < size; ++x)
			{
				size_t xStep = x / checkSize;
				data[y * size + x] = (((xStep ^ yStep) & 1) == 0) ? a : b;
			}
		}
		return raw;
	}
}