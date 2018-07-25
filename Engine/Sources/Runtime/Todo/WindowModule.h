#pragma once
#include "Demo18.h"
#include "IModule.h"
#include "Math.h"

namespace Demo18
{
	using KeyCode = unsigned long;
	using KeyMap = std::unordered_map<KeyCode, bool>;
	struct MouseState
	{
		bool LButtonDown;
		bool RButtonDown;
		V2I Pos;
		V2F Coord;
	};
	class WindowModule : public IModule
	{
	public:
		virtual void SetTitle(const std::wstring& title) = 0;
		virtual bool CheckWindowResized(bool clearState) = 0;
		virtual V2I GetWindowSize() = 0;
		virtual V2I GetClientSize() = 0;
		virtual void SetKeepAspect(bool keep) = 0;
		//inputs
		virtual void CaptureState(KeyMap& keyMap, MouseState& mouseState) = 0;
		//handle
		virtual void GetWindowHandle(void* out) = 0;
		
	};
	extern WindowModule* CreateWindowModule();
}

