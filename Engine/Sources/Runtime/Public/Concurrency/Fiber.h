#pragma once

#include "Yes.h"

#include "Public/Concurrency/Concurrency.h"
namespace Yes
{
	using FiberHandle = void*;
	class Fiber
	{
	public:
		Fiber(ThreadFunctionPrototype func, void* param, const wchar_t* name = L"YesFiber", size_t stackSize = 16 * 1024);
		static Fiber* GetCurrentFiber();
		void* GetParam() { return mParam; }
		const wchar_t* GetName() { return mName; }
		void SwitchTo(Fiber* dest);
	private:
		void* mParam;
		FiberHandle mHandle;
		const wchar_t* mName;
	};
}