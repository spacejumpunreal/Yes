#include "Core/ConcurrencyModule.h"
#include "Public/Core/System.h"
#include "Public/Concurrency/Fiber.h"
#include "Public/Concurrency/Lock.h"
#include "Public/Concurrency/Thread.h"
#include "Public/Concurrency/JobUtils.h"
#include <deque>
#include <atomic>
#include <mutex>

namespace Yes
{
	struct InternalJobData
	{
		union
		{
			JobData JobData;
			Fiber* Fiber;
		} Data;
		uint32 IsFiber: 1;
		InternalJobData()
		{
			Data.JobData = {};
			IsFiber = 0;
		}
		InternalJobData(const JobData& d)
		{
			Data.JobData = d;
			IsFiber = 0;
		}
		InternalJobData(Fiber* fiber)
		{
			Data.Fiber = fiber;
			IsFiber = 1;
		}
		InternalJobData(const InternalJobData& d) = default;
	};

	struct PerThreadSharedData
	{
		std::vector<InternalJobData> Queue;
		SimpleSpinLock Lock;
	};
	enum class FiberCleanupAction
	{
		NoAction,
		FreeFiber,
		HangOnWaitList,
	};
	struct PerThreadPrivateData
	{
		uint32 ThreadIndex;
		FiberCleanupAction Action;
		Fiber* PreviousFiber;
		PerThreadPrivateData()
			: ThreadIndex(0)
			, Action(FiberCleanupAction::NoAction)
			, PreviousFiber(nullptr)
		{}
	};

	static thread_local PerThreadPrivateData JobThreadPrivateData;
	size_t ConcurrencyModule::GetJobThreadIndex()
	{
		return JobThreadPrivateData.ThreadIndex;
	}
	/*
	void AddJobs(ConcurrencyModule* module, const JobData* datum, size_t count)
	{
		module->AddJobs(datum, count);
	}
	*/

	struct JobSystemWorkerThread : public Thread
	{
	public:
		uint32 ThreadIndex;
		uint32 StealState;
		ConcurrencyModuleImp* Module;
	public:
		void Run(size_t threadIndex);
		static void WorkerThreadFunction(void*);
	};
	void JobSystemWorkerThread::Run(size_t threadIndex)
	{
		ThreadIndex = (uint32)threadIndex;
		StealState = 0;
		Module = (ConcurrencyModuleImp*)GET_MODULE(ConcurrencyModule);
		Thread::Run(WorkerThreadFunction, nullptr, L"JobSystemWorkerThread", 32 * 1024);
	}
	void JobSystemWorkerThread::WorkerThreadFunction(void*)
	{
		//setup things
		JobSystemWorkerThread* thisThread = (JobSystemWorkerThread*)Thread::GetThisThread();
		JobThreadPrivateData.ThreadIndex = thisThread->ThreadIndex;
		new Fiber(thisThread, nullptr, nullptr);
		//todo
	}

	struct ConcurrencyModuleImp : public ConcurrencyModule
	{
	private:
		//readonly global data
		size_t mThreadCount;
		std::atomic<bool> mShouldQuit;

		//shared per-thread
		PerThreadSharedData* mPerThreadSharedData;

		//shared global
		std::condition_variable mWaitingList;
		std::mutex mGlobalLock;
		uint mPendingJobs;

		//fiber management
		SimpleSpinLock mFreeFiberLock;
		std::vector<Fiber*> mFreeFibers;
		std::atomic<size_t> mLivingFibers;
#if DEBUG
		std::atomic<size_t> mCreatedFibers;
#endif
		//things almost not accessed
		JobSystemWorkerThread* mThreads;
	public:
		//job management
		virtual void AddJobs(const JobData* datum, size_t count);
		void EnqueueJobsOnPerThreadQueue(PerThreadSharedData* sdata, JobData* jobs, size_t count);
		void WaitFor1Job(InternalJobData& job);
		//API for fibers
	public:
		//--------------------stats--------------------------------------------------
		virtual void GetFiberStats(size_t& living, size_t& everCreated) override;
		virtual size_t GetWorkersCount() override { return mThreadCount; }
		//--------------------primitives---------------------------------------------
		virtual JobUnitePoint* CreateJobUnitePoint(size_t initialCount, ThreadFunctionPrototype function, void* arg) override
		{
			return new JobUnitePoint(initialCount, function, arg);
		}
		virtual JobLock* CreateJobLock() override
		{
			return new JobLock();
		}
		virtual JobSemaphore* CreateJobSemaphore(size_t initial) override
		{
			return new JobSemaphore(initial);
		}
	public:
		virtual void InitializeModule(System* system)
		{
			mShouldQuit = false;
			const Yes::ArgMap& mp = system->GetArguments();
			auto it = mp.equal_range("NumberOfJobsThreads");
			if (it.first != it.second)
			{
				mThreadCount = atoi(it.first->second.c_str());
			}
			else
			{
				mThreadCount = (size_t)std::thread::hardware_concurrency();
			}
			mPendingJobs = 0;
		}
		virtual void Start(System* system)
		{
			mPerThreadSharedData = new PerThreadSharedData[mThreadCount];
			mThreads = new JobSystemWorkerThread[mThreadCount];
			for (size_t i = 0; i < mThreadCount; ++i)
			{
				mThreads[i].Run(i);
#if DEBUG
				++mCreatedFibers;
#endif
			}
			mLivingFibers.fetch_add(mThreadCount, std::memory_order_relaxed);
		}
	public:
		DEFINE_MODULE_IN_CLASS(ConcurrencyModule, ConcurrencyModuleImp);
	};
	DEFINE_MODULE_REGISTRY(ConcurrencyModule, ConcurrencyModuleImp, -900);

	//--------------------job and fiber management-----------------------------------------
	void ConcurrencyModuleImp::AddJobs(const JobData* datum, size_t count)
	{
		std::vector<JobData> tmpDatum;
		//TODO: should do load balancing here
		tmpDatum.reserve((count + mThreadCount - 1) / mThreadCount);
		for (size_t tid = 0; tid < mThreadCount; ++tid)
		{
			for (size_t jid = tid; jid < count; jid += mThreadCount)
			{
				tmpDatum.push_back(datum[jid]);
			}
			EnqueueJobsOnPerThreadQueue(&mPerThreadSharedData[tid], tmpDatum.data(), tmpDatum.size());
			tmpDatum.clear();
		}
		//handle wakeup
		bool needWakeup = false;
		mGlobalLock.lock();
		needWakeup = mPendingJobs == 0;
		mPendingJobs += count;
		mGlobalLock.unlock();
		if (needWakeup)
			mWaitingList.notify_all();
	}
	void ConcurrencyModuleImp::EnqueueJobsOnPerThreadQueue(PerThreadSharedData* sdata, JobData* jobs, size_t count)
	{
		sdata->Lock.Lock();
		sdata->Queue.reserve(sdata->Queue.size() + count);
		for (size_t i = 0; i < count; ++i)
		{
			sdata->Queue.push_back(jobs[i]);
		}
		sdata->Lock.Unlock();
	}
	void ConcurrencyModuleImp::WaitFor1Job(InternalJobData& job)
	{
		size_t tid = GetJobThreadIndex();
		{
			std::unique_lock lk(mGlobalLock);
			while (mPendingJobs == 0)
			{
				mWaitingList.wait(lk);
			}
			--mPendingJobs;
		}
		for (size_t i = 0; i < mThreadCount; ++i)
		{
			size_t qid = (tid ^ i) % mThreadCount;
			PerThreadSharedData& sd = mPerThreadSharedData[qid];
			sd.Lock.Lock();
			if (sd.Queue.size() > 0)
			{
				job = sd.Queue.back();
				sd.Queue.pop_back();
				sd.Lock.Unlock();
				return;
			}
			else
			{
				sd.Lock.Unlock();
			}
		}
		CheckAlways(false, "should never reach here, should be able to find a job");
	}
	//--------------------stats--------------------------------------------------
	void ConcurrencyModuleImp::GetFiberStats(size_t& living, size_t& everCreated)
	{
		living = mLivingFibers.load(std::memory_order_relaxed);
		everCreated = 0;
#if DEBUG
		everCreated = mCreatedFibers.load(std::memory_order_relaxed);
#endif
	}
}