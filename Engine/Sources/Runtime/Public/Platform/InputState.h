#pragma once

#include "Runtime/Public/Yes.h"

#include <unordered_map>

namespace Yes
{
	enum class KeyCode
	{
		Left, Right,
		Up, Down,
		Forward, Backward,
		MouseLeft, MouseRight,
		Other,
		KeyCodeCount,
	};
	struct InputState
	{
		bool KeyStates[(int)KeyCode::KeyCodeCount];
		float NormalizedMousePosition[2];
		int AbsoluteMousePosition[2];
		float DeltaNormalizedMousePosition[2];
		int DeltaAbsoluteMousePosition[2];
		//void Diff(std::unordered_map<KeyCode, bool>& diffResult);
	};
}