#pragma once

#include "Yes.h"

#include <mutex>

namespace Yes
{
	using ThreadFunctionPrototype = void(*)(void*);
	class Thread
	{
	public:
		Thread(ThreadFunctionPrototype func, void* param, const wchar_t* name = L"WorkerThread", size_t stackSize = 256 * 1024);
		Thread();
		static void SetCurrentThreadName(const wchar_t* name);
		Thread& operator=(Thread&& other);
		~Thread();
		void Join();
		static std::thread::id GetThreadID();
		static std::thread::id GetMainThreadID();
		static void SetAsMainThread();
		static bool CurrentThreadIsMainThread();
	private:
		std::mutex* mLock;
		static void ThreadBody(void* p);
		static std::thread::id MainThreadID;
	};
}