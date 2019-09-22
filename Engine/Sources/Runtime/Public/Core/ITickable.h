#pragma once

namespace Yes
{
	class FrameState;
	struct ISequentialTickable
	{
		virtual void Tick(FrameState* state) = 0;
	};

	struct ITickable
	{
		virtual void Tick() = 0;
	};

}