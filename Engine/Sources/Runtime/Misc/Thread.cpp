#include "Public/Misc/Thread.h"

#include "Public/Misc/Debug.h"
#include "Windows.h"

namespace Yes
{
	Thread::Thread(ThreadFunctionPrototype func, void* param, size_t stackSize)
		: mLock(new std::mutex())
	{
		mLock->lock();
		::CreateThread(nullptr, stackSize, (LPTHREAD_START_ROUTINE)func, param, 0, NULL);
	}
	Thread::Thread()
		: mLock(nullptr)
	{}
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
	void Thread::ThreadBody(void* p)
	{
		Thread* self = (Thread*)p;
		self->mLock->unlock();
	}
}