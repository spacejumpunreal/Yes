#pragma once
#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Concurrency/Sync.h"
#include "Toys/SWR/Public/SWR.h"


namespace Yes::SWR
{
	class SWRPresentJob
	{
	public:
		SWRPresentJob(Semaphore<>* s)
			: mSemaphore(s)
		{}
		void operator()()
		{
			//do present
			mSemaphore->Increase();
		}
	private:
		Semaphore<>* mSemaphore;
	};
}