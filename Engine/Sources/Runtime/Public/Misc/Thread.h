#pragma once

#include "Yes.h"

#include <mutex>

namespace Yes
{
	using ThreadFunctionPrototype = void(*)(void*);
	class Thread
	{
	public:
		Thread(ThreadFunctionPrototype func, void* param, size_t stackSize = 256 * 1024);
		Thread();
		Thread& operator=(Thread&& other);
		~Thread();
		void Join();
	private:
		std::mutex* mLock;
		static void ThreadBody(void* p);
	};
}