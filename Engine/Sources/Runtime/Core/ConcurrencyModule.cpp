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
		uint32 IsLocked: 1;
		uint32 IsStolen : 1;
		InternalJobData()
		{
			Data.JobData = {};
			IsFiber = IsLocked = IsStolen = 0;
		}
		InternalJobData(const JobData& d, bool locked)
		{
			Data.JobData = d;
			IsFiber = 0;
			IsLocked = locked ? 1 : 0;
			IsStolen = 0;
		}
		InternalJobData(const InternalJobData& d) = default;
	};

	struct PerThreadData
	{
		std::deque<InternalJobData> Queue;
		SimpleSpinLock Lock;
		std::atomic<int> StealableCount;
		PerThreadData()
			: StealableCount(0)
		{}
	};

	static thread_local size_t JobSystemWorkerThreadIndex = -1;
	size_t ConcurrencyModule::GetJobThrreadIndex()
	{
		return JobSystemWorkerThreadIndex;
	}

	struct ConcurrencyModuleImp : public ConcurrencyModule
	{
	private:

		struct WorkerThread : public Thread
		{
		public:
			uint32 ThreadIndex;
			uint32 StealState;
			uint32 LockFailTolerance;
			ConcurrencyModuleImp* Module;
			void Run(size_t threadIndex)
			{
				ThreadIndex = (uint32)threadIndex;
				StealState = 0;
				LockFailTolerance = 0;
				Thread::Run(WorkerThreadFunction, nullptr, L"JobSystemWorkerThread", 32 * 1024);
			}
			static void WorkerThreadFunction(void*)
			{
				WorkerThread* thisThread = (WorkerThread*)Thread::GetThisThread();
				JobSystemWorkerThreadIndex = thisThread->ThreadIndex;
				thisThread->Module = (ConcurrencyModuleImp*)GET_MODULE(ConcurrencyModule);
				InternalJobData ijd;
				while (true)
				{
					thisThread->Get1Job(&ijd);
					if (ijd.IsFiber)
					{//free current fiber, use that fiber
						Fiber::SwitchTo(ijd.Data.Fiber);
					}
					else
					{//directly call on this
						ijd.Data.JobData.Function(ijd.Data.JobData.Context);
					}
				}
			}
			void Get1Job(InternalJobData* jdata)
			{
				int failCount = 0;
				while (true)
				{
					if (Module->TryTakeJobFromQueue<false>(jdata, ThreadIndex))
					{
						return;
					}
					size_t queueIndex = ThreadIndex;
					size_t thisQueueIndex = queueIndex;
					size_t distQueueIndex = ~queueIndex;
					size_t invSelector = StealState;
					queueIndex = (invSelector & thisQueueIndex) | (StealState & distQueueIndex);
					queueIndex = queueIndex % Module->mThreadCount;
					if (Module->TryTakeJobFromQueue<true>(jdata, queueIndex))
					{
						return;
					}
					else
					{
						++failCount;
					}
					if (failCount == Module->mThreadCount)
					{
						Thread::Sleep(0.001f);
					}
					StealState = (StealState + 1) % Module->mThreadCount;
				}
			}

		};
	private:
		//readonly global data
		size_t mThreadCount;
		std::atomic<bool> mShouldQuit;
		PerThreadData* mPerThreadData;

		//fiber management
		SimpleSpinLock mFreeFiberLock;
		std::vector<Fiber*> mFreeFibers;
#if DEBUG
		std::atomic<size_t> mCreatedFibers;
#endif
		std::atomic<size_t> mLivingFibers;

		//things almost not accessed
		WorkerThread* mThreads;

	public:
		virtual void AddJobs(const JobData* datum, size_t count, uint32 dispatchPolicy) override
		{
			uint32 queueId = dispatchPolicy & 0xffff;
			CheckAlways(queueId < mThreadCount);
			bool locked = (dispatchPolicy & DispatchPolicyLocked) != 0;
			if ((dispatchPolicy & DispatchPolicyOnCurrent) != 0)
			{
				EnqueueJobs(&mPerThreadData[queueId], datum, count, locked);
			}
			else
			{
				std::vector<JobData> tmpDatum;
				tmpDatum.reserve((count + mThreadCount - 1) / mThreadCount);
				for (size_t tid = 0; tid < mThreadCount; ++tid)
				{
					for (size_t jid = tid; jid < count; jid += mThreadCount)
					{
						tmpDatum.push_back(datum[jid]);
					}
					EnqueueJobs(&mPerThreadData[tid], tmpDatum.data(), tmpDatum.size(), locked);
					tmpDatum.clear();
				}
			}
		}
		virtual void GetFiberStats(size_t& living, size_t& everCreated) override
		{
			living = mLivingFibers.load(std::memory_order_relaxed);
			everCreated = 0;
#if DEBUG
			everCreated = mCreatedFibers.load(std::memory_order_relaxed);
#endif
		}
		virtual size_t GetWorkersCount() override
		{
			return mThreadCount;
		}
		virtual JobSyncPoint* CreateJobSyncPoint(size_t initialCount, ThreadFunctionPrototype function, void* arg) override
		{
			return new JobSyncPoint(initialCount, function, arg);
		}
		virtual JobLock* CreateJobLock() override
		{
			return nullptr;
		}
		virtual JobSemaphore* CreateJobSemaphore(size_t initial) override
		{
			return nullptr;
		}

	private:
		//fiber management
		void FreeFiber(Fiber* fb)
		{
			
			mFreeFiberLock.Lock();
			if (mFreeFibers.size() > MaxFreeFibers)
			{
				mFreeFiberLock.Unlock();
				mLivingFibers.fetch_sub(1, std::memory_order_relaxed);
				delete fb;
			}
			else
			{
				mFreeFibers.push_back(fb);
				mFreeFiberLock.Unlock();
			}
		}
		Fiber* AllocFiber()
		{
			mFreeFiberLock.Lock();
			if (mFreeFibers.size() > 0)
			{
				Fiber* fb = mFreeFibers.back();
				mFreeFibers.pop_back();
				mFreeFiberLock.Unlock();
				return fb;
			}
			else
			{
				new Fiber(WorkerThread::WorkerThreadFunction, this);
				mFreeFiberLock.Unlock();
				mLivingFibers.fetch_add(1, std::memory_order_relaxed);
			}
#if DEBUG
			++mCreatedFibers;
#endif
		}
		// queue management
		void EnqueueJobs(PerThreadData* d, const JobData* datum, size_t count, bool locked)
		{
			d->Lock.Lock();
			if (!locked)
			{
				d->StealableCount.fetch_add((int)count, std::memory_order_relaxed);
			}
			for (size_t i = 0; i < count; ++i)
			{
				d->Queue.emplace_back(datum[i], locked);
			}
			d->Lock.Unlock();
		}
		template<bool steal>
		bool TryTakeJobFromQueue(InternalJobData* jdata, size_t queueId)
		{
			PerThreadData& d = mPerThreadData[queueId];
			if constexpr (steal)
			{
				if (d.StealableCount.load(std::memory_order_relaxed) > 0)
				{
					d.Lock.Lock();
					while (d.Queue.size() != 0 && d.Queue.front().IsStolen)
					{
						d.Queue.pop_front();
					}
					bool found = false;
					for (auto it = d.Queue.begin(); it != d.Queue.end(); ++it)
					{
						if ((!it->IsStolen) && (!it->IsLocked))
						{
							it->IsStolen = true;
							*jdata = *it;
							found = true;
							break;
						}
					}
					if (found)
					{
						d.StealableCount.fetch_sub(1, std::memory_order_relaxed);
					}
					else
					{
						d.StealableCount.store(0, std::memory_order_relaxed);
					}
					d.Lock.Unlock();
					return found;
				}
				return false;
			}
			else
			{
				bool found = false;
				d.Lock.Lock();
				while (d.Queue.size() != 0 && d.Queue.front().IsStolen)
				{
					d.Queue.pop_front();
				}
				if (d.Queue.size() != 0)
				{
					*jdata = d.Queue.front();
					d.Queue.pop_front();
					found = true;
				}
				d.Lock.Unlock();
				return found;
			}
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
		}
		virtual void Start(System* system)
		{
			mPerThreadData = new PerThreadData[mThreadCount];
			mThreads = new WorkerThread[mThreadCount];
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
}