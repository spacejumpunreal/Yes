#include "Public/Concurrency/Fiber.h"
#include "Public/Misc/Debug.h"
#include <windows.h>

namespace Yes
{
	Fiber::Fiber(ThreadFunctionPrototype func, void* param, const wchar_t* name, size_t stackSize)
		: mParam(param)
		, mName(name)
		, mOriginThread(nullptr)
	{
		mHandle = ::CreateFiber(stackSize, func, this);
	}
	Fiber::Fiber(Thread* thread, void* param, const wchar_t* name)
		: mParam(param)
		, mName(name)
		, mOriginThread(thread)
	{
		mHandle = ::ConvertThreadToFiber(this);
	}
	Fiber* Fiber::GetCurrentFiber()
	{
		return (Fiber*)::GetFiberData();
	}
	void Fiber::SwitchTo(Fiber* dest)
	{
		::SwitchToFiber(dest->mHandle);
	}
	Fiber::~Fiber()
	{
		HANDLE h = mHandle;
		mHandle = nullptr;
		::DeleteFiber(h);
	}
}

