#include "Runtime/Public/Core/ConcurrencyModule.h"
#include "Runtime/Public/Core/System.h"
#include "Runtime/Public/Concurrency/Fiber.h"
#include "Runtime/Public/Concurrency/Lock.h"
#include "Runtime/Public/Concurrency/Thread.h"
#include "Runtime/Public/Concurrency/JobUtils.h"
#include <deque>
#include <atomic>
#include <mutex>

namespace Yes
{
	struct ConcurrencyModuleImp;

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
		RelesaeLock,
	};
	struct PerThreadPrivateData
	{
		uint32 ThreadIndex;
		FiberCleanupAction Action;
		Fiber* PreviousFiber;
		union
		{
			struct
			{
				JobLock* Lock;
			} ReleaseParams;
		};
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

	struct ConcurrencyModuleImp : public ConcurrencyModule
	{
	public:
		//readonly global data
		size_t mThreadCount;
		std::atomic<bool> mShouldQuit;

		//shared per-thread
		PerThreadSharedData* mPerThreadSharedData;

		//shared global
		std::condition_variable mWaitingList;
		std::mutex mGlobalLock;
		size_t mPendingJobs;

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
		//for job system users
		virtual void AddJobs(const JobData* datum, size_t count) override;
		virtual void RescheduleFibers(Fiber* fibers[], size_t count) override;
		//API for fiber worker
		static void JobFiberBody(void*);
		void FreeCurrentAndSwitchTo(Fiber* fiber);//used by fiber when they find fiber job
		virtual void SwitchOutAndReleaseLock(JobLock* lock) override;
		
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
	private:
		void AddInternalJobs(const InternalJobData* datum, size_t count);
		void EnqueueJobsOnPerThreadQueue(PerThreadSharedData* sdata, InternalJobData* jobs, size_t count);
		void HandleJob(InternalJobData& job);
		bool WaitFor1Job(InternalJobData& job);
		void CleanupPreviousFiber();
		void FreeFiber(Fiber* fiber);
		Fiber* AllocFiber();
	public:
		virtual void InitializeModule(System* system);
		virtual void Start(System* system);
	public:
		DEFINE_MODULE_IN_CLASS(ConcurrencyModule, ConcurrencyModuleImp);
	};
	DEFINE_MODULE_REGISTRY(ConcurrencyModule, ConcurrencyModuleImp, -900);

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
		new Fiber(thisThread, nullptr, L"JobWorkerFiber");
		ConcurrencyModuleImp::JobFiberBody(nullptr);
	}

	//--------------------job management------------------------------------------
	void ConcurrencyModuleImp::AddJobs(const JobData* datum, size_t count)
	{
		std::vector<InternalJobData> ijd;
		ijd.reserve(count);
		for (size_t i = 0; i < count; ++i)
		{
			ijd.emplace_back(InternalJobData(datum[i]));
		}
		AddInternalJobs(ijd.data(), count);
	}
	void ConcurrencyModuleImp::AddInternalJobs(const InternalJobData* datum, size_t count)
	{
		std::vector<InternalJobData> tmpDatum;
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
	void ConcurrencyModuleImp::EnqueueJobsOnPerThreadQueue(PerThreadSharedData* sdata, InternalJobData* jobs, size_t count)
	{
		sdata->Lock.Lock();
		sdata->Queue.reserve(sdata->Queue.size() + count);
		for (size_t i = 0; i < count; ++i)
		{
			sdata->Queue.push_back(jobs[i]);
		}
		sdata->Lock.Unlock();
	}
	bool ConcurrencyModuleImp::WaitFor1Job(InternalJobData& job)
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
				return true;
			}
			else
			{
				sd.Lock.Unlock();
			}
		}
		return false;
		CheckAlways(false, "should never reach here, should be able to find a job");
	}
	void ConcurrencyModuleImp::HandleJob(InternalJobData& job)
	{
		if (job.IsFiber)
		{
			FreeCurrentAndSwitchTo(job.Data.Fiber);
		}
		else
		{
			ThreadFunctionPrototype f = job.Data.JobData.Function;
			void* a = job.Data.JobData.Context;
			f(a);
		}
	}
	void ConcurrencyModuleImp::CleanupPreviousFiber()
	{
		auto act = JobThreadPrivateData.Action;
		JobThreadPrivateData.Action = FiberCleanupAction::NoAction;
		switch (act)
		{
			case FiberCleanupAction::FreeFiber:
			{
				FreeFiber(JobThreadPrivateData.PreviousFiber);
				break;
			}
			case FiberCleanupAction::RelesaeLock:
			{
				JobThreadPrivateData.ReleaseParams.Lock->Unlock();
				break;
			}
			default:
				break;
		}
	}
	//--------------------fiber management----------------------------------------
	Fiber* CreateOriginalFiber()
	{
		return new Fiber(Thread::GetThisThread(), nullptr, L"OriginalFiber");
	}
	Fiber* CreateJobFiber()
	{
		return new Fiber(ConcurrencyModuleImp::JobFiberBody, nullptr, L"JobFiber");
	}
	void DestroyJobFiber(Fiber* f)
	{
		delete f;
	}
	void ConcurrencyModuleImp::FreeFiber(Fiber* fiber)
	{
		mFreeFiberLock.Lock();
		if (mFreeFibers.size() < MaxFreeFibers)
		{
			mFreeFibers.push_back(fiber);
			fiber = nullptr;
		}
		mFreeFiberLock.Unlock();
		if (fiber != nullptr)
		{
			DestroyJobFiber(fiber);
		}
	}
	Fiber* ConcurrencyModuleImp::AllocFiber()
	{
		Fiber* ret = nullptr;
		mFreeFiberLock.Lock();
		if (mFreeFibers.size() > 0)
		{
			ret = mFreeFibers.back();
			mFreeFibers.pop_back();
		}
		mFreeFiberLock.Unlock();
		if (ret == nullptr)
			ret = CreateJobFiber();
		return ret;
	}
	void ConcurrencyModuleImp::FreeCurrentAndSwitchTo(Fiber* fiber)
	{
		JobThreadPrivateData.Action = FiberCleanupAction::FreeFiber;
		JobThreadPrivateData.PreviousFiber = Fiber::GetCurrentFiber();
		//newly created will cleanup for us
		Fiber::SwitchTo(fiber);
		//when we are back, cleanup previous
		CleanupPreviousFiber();
	}
	void ConcurrencyModuleImp::SwitchOutAndReleaseLock(JobLock* lock)
	{
		JobThreadPrivateData.Action = FiberCleanupAction::RelesaeLock;
		JobThreadPrivateData.PreviousFiber = Fiber::GetCurrentFiber();
		JobThreadPrivateData.ReleaseParams.Lock = lock;
		Fiber::SwitchTo(AllocFiber());
	}
	void ConcurrencyModuleImp::RescheduleFibers(Fiber* fibers[], size_t count)
	{
		std::vector<InternalJobData> datum(count);
		for (size_t i = 0; i < count; ++i)
		{
			datum[i].IsFiber = true;
			datum[i].Data.Fiber = fibers[i];
		}
		AddInternalJobs(datum.data(), count);
	}
	void ConcurrencyModuleImp::JobFiberBody(void*)
	{
		ConcurrencyModuleImp* m = (ConcurrencyModuleImp*)GET_MODULE(ConcurrencyModule);
		JobSystemWorkerThread* thread = (JobSystemWorkerThread*)Thread::GetThisThread();
		m->CleanupPreviousFiber();
		while (!m->mShouldQuit.load(std::memory_order_relaxed))
		{
			InternalJobData data;
			m->WaitFor1Job(data);
			m->HandleJob(data);
		}
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
	//--------------------module convensions--------------------------------------
	void ConcurrencyModuleImp::InitializeModule(System* system)
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
	void ConcurrencyModuleImp::Start(System* system)
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
}