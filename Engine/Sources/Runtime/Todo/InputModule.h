#pragma once
#include "Demo18.h"
#include "WindowModule.h"
#include "IModule.h"
namespace Demo18
{
	struct InputState
	{
		V3F LeftPad;
		V2I LastMousePos;
		V2F LastMouseCoord;
		V2I CurrentMousePos;
		V2F CurrentMouseCoord;
		bool LButtonPressed;
		bool RButtonPressed;
	};
	class InputModule : public IModule
	{
	public:
		virtual InputState& GetInputState() = 0;
	};
	extern InputModule* CreateInputModule();
}