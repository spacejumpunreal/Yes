#include "Concurrency/Thread.h"
#include "Concurrency/Sync.h"
#include "Misc/Debug.h"
#include <atomic>
#include "Windows.h"

namespace Yes
{
	Thread* Thread::MainThread;
	thread_local Thread* ThisThread;

	struct ThreadInitParams
	{
		Semaphore<> InitV;
		Thread* ThisThread;
		ThreadFunctionPrototype Function;
		void* Context;
		const wchar_t* Name;
	};

	Thread::Thread(InitAsMainThread)
	{
		Check(ThisThread == nullptr);
		Check(MainThread == nullptr);
		SetCurrentThreadName(L"EngineMainThread");
		MainThread = ThisThread = this;
	}
	Thread::Thread()
	{}
	Thread::~Thread()
	{
		Join();
	}

	void Thread::Run(ThreadFunctionPrototype func, void* param, const wchar_t* name, size_t stackSize)
	{
		ThreadInitParams d;
		d.ThisThread = this;
		d.Function = func;
		d.Context = param;
		d.Name = name;
		HANDLE thisThreadHandle = ::CreateThread(nullptr, stackSize, (LPTHREAD_START_ROUTINE)ThreadBody, &d, 0, NULL);
		d.InitV.Decrease();
	}
	void Thread::Join()
	{
		mJoinLock.lock();
		mJoinLock.unlock();
	}
	void Thread::SetCurrentThreadName(const wchar_t* name)
	{
		CheckSucceeded(::SetThreadDescription(::GetCurrentThread(), name));
	}
	Thread* Thread::GetThisThread()
	{
		return ThisThread;
	}
	Thread* Thread::GetMainThread()
	{
		return MainThread;
	}
	bool Thread::CurrentThreadIsMainThread()
	{
		return MainThread == ThisThread;
	}
	void Thread::Sleep(float seconds)
	{
		seconds = seconds < 0.001f ? 0 : seconds;
		::Sleep((DWORD)(seconds * 1000));

	}
	void Thread::ThreadBody(void* _)
	{
		ThreadInitParams* p = (ThreadInitParams*)_;
		ThisThread = p->ThisThread;
		auto fn = p->Function;
		auto ctx = p->Context;
		::SetThreadDescription(::GetCurrentThread(), p->Name);
		p->InitV.Increase();
		fn(ctx);
		ThisThread->mJoinLock.unlock();
	}
}