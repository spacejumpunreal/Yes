#pragma once
#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Misc/Utils.h"
#include "Toys/SWR/Public/SWR.h"
#include "Toys/SWR/SWRTexture.h"


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