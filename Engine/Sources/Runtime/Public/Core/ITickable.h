#pragma once

namespace Yes
{
	class FrameState;
	struct ITickable
	{
		virtual void Tick() = 0;
	};

}