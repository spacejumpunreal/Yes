#pragma once

#include "Yes.h"
#include "Public/Concurrency/Concurrency.h"

#include <string>

namespace Yes
{
	class Thread;
	using OSFiberHandle = void*;
	class Fiber
	{
	public:
		Fiber(ThreadFunctionPrototype func, void* param, const wchar_t* name = L"OtherYesFiber", size_t stackSize = 16 * 1024);
		Fiber(Thread* thread, void* param, const wchar_t* name = L"ThreadYesFiber");
		Fiber(const Fiber&) = delete;
		~Fiber();
		static Fiber* GetCurrentFiber();
		static void SwitchTo(Fiber* dest);
		void* GetParam() { return mParam; }
		const wchar_t* GetName() { return mName.c_str(); }
	private:
		void* mParam;
		OSFiberHandle mHandle;
		std::wstring mName;
		Thread* mOriginThread;
	};
}