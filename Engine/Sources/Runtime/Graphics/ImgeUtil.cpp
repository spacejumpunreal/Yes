#include "Graphics/ImageUtil.h"
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace Yes
{
	RawImage::RawImage(size_t width, size_t height, size_t bpp, void* content, size_t actualBpp)
		: mWidth(width)
		, mHeight(height)
		, mTexelSize(bpp)
	{
		size_t sz = width * height * bpp;
		mData = malloc(sz);
		if (bpp == actualBpp)
		{
			memcpy(mData, content, sz);
		}
		else
		{
			if (actualBpp == 3)
			{
				uint8* dst = (uint8*)mData;
				uint8* src = (uint8*)content;
				size_t sz = width * height;
				for (size_t i = 0; i < sz; ++i)
				{
					dst[0] = src[0];
					dst[1] = src[1];
					dst[2] = src[2];
					dst[3] = 1;
					src += 3;
					dst += 4;
				}
			}
			else
			{
				CheckAlways(false);
			}
		}
	}
	RawImage::~RawImage()
	{
		free(mData);
	}
	TRef<RawImage> LoadRawImage(ISharedBuffer* buffer)
	{
		const int DesiredBPP = 4;
		int bpp, width, height;
		unsigned char* stream = stbi_load_from_memory((unsigned char*)buffer->GetData(), (int)buffer->GetSize(), &width, &height, &bpp, DesiredBPP);
		CheckAlways(stream != nullptr && (1 <= bpp || bpp == DesiredBPP));
		RawImage* image = new RawImage(width, height, DesiredBPP, stream, bpp);
		STBI_FREE(stream);
		return image;
	}
	TRef<RawImage> BuildSingleColorTexture(const V4F& color, size_t width, size_t height)
	{
		std::vector<RGBA> raw;
		size_t sz = width * height;
		raw.resize(sz);
		for (int i = 0; i < sz; ++i)
		{
			raw[i].r = (uint8)(color.x * 255);
			raw[i].g = (uint8)(color.y * 255);
			raw[i].b = (uint8)(color.z * 255);
			raw[i].a = (uint8)(color.w * 255);
		}
		TRef<RawImage> rimage = new RawImage(width, height, 4, raw.data(), 4);
		return rimage;
	}
}