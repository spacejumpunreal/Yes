#include "InputModule.h"
#include "TickModule.h"
#include "WindowModule.h"
#include "Debug.h"
#include "Utils.h"
#include "System.h"

namespace Demo18
{
	struct InputModuleImp : public InputModule, public ITickable
	{
		InputModuleImp()
			: mCurr(0)
		{}
		virtual void Start() override
		{
			mWindowModule = GET_MODULE(Window);
			ZeroFill(mStates);
			GET_MODULE(Tick)->AddTickable(this);
		}
		inline bool _IsPressed(char c)
		{
			auto itr = mTmpKeyMap.find(c);
			if (itr == mTmpKeyMap.end())
				return false;
			return itr->second;
		}
		virtual void Tick() override
		{
			InputState& last = mStates[mCurr];
			mCurr = 1 - mCurr;
			InputState & curr = mStates[mCurr];
			ZeroFill(curr);
			MouseState mouseState;
			mWindowModule->CaptureState(mTmpKeyMap, mouseState);
			
			curr.LeftPad.x += _IsPressed('A') ? -1.0f : 0.0f;
			curr.LeftPad.x += _IsPressed('D') ? +1.0f : 0.0f;
			curr.LeftPad.y += _IsPressed('Q') ? -1.0f : 0.0f;
			curr.LeftPad.y += _IsPressed('E') ? +1.0f : 0.0f;
			curr.LeftPad.z += _IsPressed('S') ? -1.0f : 0.0f;
			curr.LeftPad.z += _IsPressed('W') ? +1.0f : 0.0f;

			curr.LastMousePos = last.CurrentMousePos;
			curr.LastMouseCoord = last.CurrentMouseCoord;
			curr.CurrentMousePos = mouseState.Pos;
			curr.CurrentMouseCoord = mouseState.Coord;
			curr.LButtonPressed = mouseState.LButtonDown;
			curr.RButtonPressed = mouseState.RButtonDown;
			LogState();
		}
		void LogState()
		{
			InputState & curr = mStates[mCurr];
			/*
			printf(">>>>>>>\n");
			printf("LeftPad:(%.1f,%.1f,%.1f)\n", curr.LeftPad.x, curr.LeftPad.y, curr.LeftPad.z);
			printf("CurrentMousePos:(%d,%d)\n", curr.CurrentMousePos.x, curr.CurrentMousePos.y);
			printf("CurrentMouseCoord:(%.3f,%.3f)\n", curr.CurrentMouseCoord.x, curr.CurrentMouseCoord.y);
			printf("<<<<<<<\n");
			*/
		}
		InputState& GetInputState()
		{
			return mStates[mCurr];
		}
		KeyMap mTmpKeyMap;
		InputState mStates[2];
		int mCurr;
		WindowModule* mWindowModule;
	};
	InputModule* CreateInputModule()
	{
		return new InputModuleImp();
	}
}