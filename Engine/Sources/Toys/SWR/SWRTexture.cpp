#pragma once
#include "Yes.h"
#include "SWR.h"
#include "SWRTexture.h"
#include "Misc/Utils.h"

namespace Yes::SWR
{

	SWRHandle CreateSWRTexture2D(const Texture2DDesc* desc)
	{
		return new SWRTexture2D(desc);
	}
	void SWRTexture2D::Fill(void* fillColor, size_t colorSize)
	{
		FillN(mData, mWidth * mHeight, fillColor, colorSize);
	}
}