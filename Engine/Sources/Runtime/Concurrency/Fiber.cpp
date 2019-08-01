#include "Public/Concurrency/Fiber.h"

#include <windows.h>

namespace Yes
{
	Fiber::Fiber(ThreadFunctionPrototype func, void* param, const wchar_t* name, size_t stackSize)
		: mParam(param)
		, mName(name)
	{
		mHandle = ::CreateFiber(stackSize, func, this);
	}
	Fiber* Fiber::GetCurrentFiber()
	{
		return (Fiber*)GetFiberData();
	}
	void Fiber::SwitchTo(Fiber* dest)
	{
		SwitchToFiber(dest->mHandle);
	}
}

