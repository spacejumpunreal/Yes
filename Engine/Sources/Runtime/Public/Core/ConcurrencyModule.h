#pragma once
#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Core/IModule.h"
#include "Runtime/Public/Concurrency/JobUtils.h"

namespace Yes
{
	class Fiber;
	class ConcurrencyModule : public IModule
	{//this is for job users
	public:
		//parameters
		static const size_t MaxFreeFibers = 8;
		//job management
		virtual void AddJobs(const JobData* datum, size_t count) = 0;
		virtual void RescheduleFibers(Fiber* fibers[], size_t count) = 0;
		virtual void SwitchOutAndReleaseLock(JobLock* lock) = 0;
		//API for job function
		static size_t GetJobThreadIndex();

		//stats
		virtual void GetFiberStats(size_t& living, size_t& everCreated) = 0;
		virtual size_t GetWorkersCount() = 0;

		//primitives
		virtual JobUnitePoint* CreateJobUnitePoint(size_t initialCount, ThreadFunctionPrototype function, void* arg) = 0;
		virtual JobLock* CreateJobLock() = 0;
		virtual JobSemaphore* CreateJobSemaphore(size_t initial) = 0;

	DECLARE_MODULE_IN_CLASS(ConcurrencyModule);
	};
}