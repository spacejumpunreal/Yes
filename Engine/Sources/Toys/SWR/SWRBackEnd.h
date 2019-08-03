#pragma once
#include "Yes.h"
#include "SWR.h"
#include "Concurrency/Sync.h"

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