#pragma once
#include "Yes.h"
#include "Public/Core/IModule.h"
#include "Public/Concurrency/JobUtils.h"

namespace Yes
{
	class ConcurrencyModule : public IModule
	{//this is for job users
	public:
		//parameters
		static const size_t MaxFreeFibers = 32;

		//flags
		static const uint32 DispatchPolicyOnCurrent = 1 << 16;
		static const uint32 DispatchPolicyLocked = 2 << 16;

		//jobs
		virtual void AddJobs(const JobData* datum, size_t count, uint32 dispatchPolicy = 0) = 0;

		//API for job function
		static size_t GetJobThrreadIndex();

		//stats
		virtual void GetFiberStats(size_t& living, size_t& everCreated) = 0;
		virtual size_t GetWorkersCount() = 0;

		//primitives
		virtual JobSyncPoint* CreateJobSyncPoint(size_t initialCount, ThreadFunctionPrototype function, void* arg) = 0;
		virtual JobLock* CreateJobLock() = 0;
		virtual JobSemaphore* CreateJobSemaphore(size_t initial) = 0;

	DECLARE_MODULE_IN_CLASS(ConcurrencyModule);
	};
}