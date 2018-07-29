#pragma once
#include "Yes.h"
#include "SWRJob.h"
#include "Misc/TaskQueue.h"
#include "Misc/Sync.h"
#include <thread>

namespace Yes::SWR
{
	struct SWRJobSystemImp;
	void _WorkerThread(size_t idx, SWRJobSystemImp* jobSystem);
	
	struct SharedSyncPoint : public SyncPoint, public SharedObject
	{
		SharedSyncPoint(size_t n)
			: SyncPoint(n)
		{}
		/*
		void Destroy() override
		{
			printf("SharedSyncPoint destroyed");
			SharedObject::Destroy();
		}
		*/
	};
	struct SyncJob
	{
		void operator()()
		{
			mSyncPoint->Sync();
		}
		TRef<SharedSyncPoint> mSyncPoint;
	};

	struct SWRJobSystemImp : public SWRJobSystem, public TaskQueue
	{
	public:
		SWRJobSystemImp(const DeviceDesc* desc)
			: mThreads(desc->NumThreads)
		{
			for (size_t i = 0; i < desc->NumThreads; ++i)
			{
				mThreads[i] = std::thread(_WorkerThread, i, this);
			}
		}
		void _Put(bool atBack, std::function<void()>&& f) override
		{
			TaskQueue::Put(atBack, std::forward<std::function<void()>>(f));
		}
		void PutSyncPoint() override
		{
			size_t nThreads = mThreads.size();
			TRef<SharedSyncPoint> sp(new SharedSyncPoint(nThreads));
			auto jobs = new SyncJob[mThreads.size()];
			for (int i = 0; i < mThreads.size(); ++i)
			{
				jobs[i].mSyncPoint = sp;
			}
			TaskQueue::Put(true, &jobs[0], (&jobs[0]) + mThreads.size());
			delete[] jobs;
		}
		void Test(int x)
		{
			
		}
	protected:
		std::vector<std::thread> mThreads;

		friend void _WorkerThread(size_t idx, SWRJobSystemImp* jobSystem)
		{
			jobSystem->Execute();
		}
	};

	SWRJobSystem* CreateSWRJobSystem(const DeviceDesc* desc)
	{
		return new SWRJobSystemImp(desc);
	}
}
