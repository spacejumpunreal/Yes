#include "Concurrency/Thread.h"

#include "Misc/Debug.h"
#include "Windows.h"

namespace Yes
{
	std::thread::id Thread::MainThreadID;

	Thread::Thread(ThreadFunctionPrototype func, void* param, const wchar_t* name, size_t stackSize)
		: mLock(new std::mutex())
	{
		mLock->lock();
		HANDLE thisThreadHandle = ::CreateThread(nullptr, stackSize, (LPTHREAD_START_ROUTINE)func, param, 0, NULL);
		CheckSucceeded(SetThreadDescription(thisThreadHandle, name));
	}
	Thread::Thread()
		: mLock(nullptr)
	{}
	void Thread::SetCurrentThreadName(const wchar_t* name)
	{
		CheckSucceeded(SetThreadDescription(GetCurrentThread(), name));
	}
	Thread& Thread::operator=(Thread&& other)
	{
		mLock = other.mLock;
		other.mLock = nullptr;
		return *this;
	}
	Thread::~Thread()
	{
		if (mLock)
		{
			delete mLock;
		}
	}
	void Thread::Join()
	{
		mLock->lock();
		mLock->unlock();
	}
	std::thread::id Thread::GetThreadID()
	{
		return std::this_thread::get_id();
	}
	std::thread::id Thread::GetMainThreadID()
	{
		return MainThreadID;
	}
	void Thread::SetAsMainThread()
	{
		SetCurrentThreadName(L"EngineMainThread");
		MainThreadID = GetThreadID();
	}
	bool Thread::CurrentThreadIsMainThread()
	{
		return MainThreadID == GetThreadID();
	}
	void Thread::ThreadBody(void* p)
	{
		Thread* self = (Thread*)p;
		self->mLock->unlock();
	}
}