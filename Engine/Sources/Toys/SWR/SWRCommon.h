#pragma once
#include "Yes.h"
#include "SWR.h"
#include <array>

namespace Yes::SWR
{
	struct PixelFormatColor1F
	{
		float x;
	};

	struct PixelFormatColor4F
	{
		float r, g, b, a;
	};

	struct PixelFormatProperty
	{
		size_t Size;
		PixelFormat Format;
	};

#define REGISTER_PIXEL_FORMAT(PF) PixelFormatProperty{sizeof(PixelFormat##PF), PixelFormat::PF}

	constexpr PixelFormatProperty GRegisteredPixelFormats[] = 
	{
		REGISTER_PIXEL_FORMAT(Color1F),
		REGISTER_PIXEL_FORMAT(Color4F),
	};
#undef REGISTER_PIXEL_FORMAT
	constexpr size_t GetPixelFormatSize(PixelFormat pf)
	{
		return GRegisteredPixelFormats[(int)pf].Size;
	}

	class SWRJobSystem;
	struct DeviceCore
	{
		SWRJobSystem* JobSystem;
	};
}