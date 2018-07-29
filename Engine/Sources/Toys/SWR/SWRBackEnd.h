#pragma once
#include "Yes.h"
#include "SWR.h"
#include "Misc/Sync.h"

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
			mSemaphore->Notify();
		}
	private:
		Semaphore<>* mSemaphore;
	};
}