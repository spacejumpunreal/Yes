#pragma once

#include "Runtime/Public/Yes.h"

#include "Runtime/Public/Concurrency/Concurrency.h"
#include <mutex>

namespace Yes
{
	struct InitAsMainThread
	{};
	class Thread
	{
		/*
		NOTE: can't be moved because thread_local ThisThread will be set, you can't move that
		*/
	public:
		Thread(InitAsMainThread);
		Thread();
		~Thread();

		void Run(ThreadFunctionPrototype func, void* param, const wchar_t* name = L"YesThread", size_t stackSize = 256 * 1024);
		void Join();

		static void SetCurrentThreadName(const wchar_t* name);
		static Thread* GetThisThread();
		static Thread* GetMainThread();
		static bool CurrentThreadIsMainThread();

		static void Sleep(float seconds);
	private:
		static void ThreadBody(void* p);
	private:
		static Thread* MainThread;
	private:
		std::mutex mJoinLock;
	};
}