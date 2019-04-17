#include "Graphics/ImageUtil.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace Yes
{
	RawImage::RawImage(size_t width, size_t height, size_t bpp, void* content)
		: mWidth(width)
		, mHeight(height)
		, mTexelSize(bpp)
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
		int bpp, width, height;
		unsigned char* stream = stbi_load_from_memory((unsigned char*)buffer->GetData(), (int)buffer->GetSize(), &width, &height, &bpp, 4);
		CheckAlways(stream != nullptr && bpp == 4);
		RawImage* image = new RawImage(width, height, bpp, stream);
		STBI_FREE(stream);
		return image;
	}
}