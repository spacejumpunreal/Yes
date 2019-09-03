#include "Core/Test/ConcurrencyTestDriverModule.h"
#include "Public/Core/System.h"
#include "Public/Core/TickModule.h"
#include "Public/Core/ConcurrencyModule.h"
#include "Public/Concurrency/Thread.h"
#include "Public/Concurrency/JobUtils.h"

#include <atomic>

namespace Yes
{
	static const bool TestSpawn = false;
	static const bool TestLock = true;

	static JobUnitePoint* SyncPoint;
	static JobLock* aJobLock;

	static std::atomic<int> TakenJobs[8];
	static std::atomic<int> DoneJobs;
	static const size_t NJobs = 256;

	static const int LockTestN = 2048;
	static int TakenValues[LockTestN];
	static int GV;
	static std::atomic<int> Acc;

	
	struct ConcurrencyTestDriverModuleImp : public ConcurrencyTestDriverModule, public ITickable
	{
	private:

	public:
		static int fab(int x)
		{
			if (x == 0 || x == 1)
				return x;
			return fab(x - 1) + fab(x - 2);
		}
		virtual void Start(System* sys) override
		{
			ConcurrencyModule* m = GET_MODULE(ConcurrencyModule);
			if (TestSpawn)
			{
				SyncPoint = m->CreateJobUnitePoint(NJobs, SpawnTestUniteAction, nullptr);
				JobData j
				{
					ConcurrencyTestDriverModuleImp::SpawnTestSpawn,
					reinterpret_cast<void*>(1),
				};
				m->AddJobs(&j, 1);
			}
			if (TestLock)
			{
				SyncPoint = m->CreateJobUnitePoint(LockTestN, LockTestUniteAction, nullptr);
				aJobLock = m->CreateJobLock();
				JobDataBatch jb(m, 0);
				for (size_t i = 0; i < LockTestN; ++i)
				{
					jb.PutJobData(LockTestJob, reinterpret_cast<void*>(i));
				}
			}
		}
		static void SpawnTestUniteAction(void* data)
		{
			ConcurrencyModule* m = GET_MODULE(ConcurrencyModule);
			printf("****************************************************************\n");
			for (size_t i = 0; i < m->GetWorkersCount(); ++i)
			{
				printf("thread(%zd):%d\n", i, (int)TakenJobs[i]);
			}
		}
		static void SpawnTestSpawn(void* data)
		{
			//need to be able to get job system worker thread index
			size_t index = reinterpret_cast<size_t>(data);
			size_t cthread = ConcurrencyModule::GetJobThreadIndex();
			printf("job(%03zd) run on thread(%02zd)\n", index, cthread);
			TakenJobs[cthread].fetch_add(1, std::memory_order_relaxed);
			ConcurrencyModule* m = GET_MODULE(ConcurrencyModule);
			size_t todo = 0;
			JobData jd[2];
			for (int i = 0; i < 2; ++i)
			{
				size_t ii = index * 2 + i;
				if (ii > NJobs)
					continue;
				jd[todo++] =
				{
					ConcurrencyTestDriverModuleImp::SpawnTestSpawn,
					reinterpret_cast<void*>(index * 2 + i)
				};
			}
			m->AddJobs(jd, todo);
			++DoneJobs;
			fab(20);
			SyncPoint->Unite();
		}
		static void LockTestJob(void* data)
		{
			int tv = 0;
			aJobLock->Lock();
			tv = GV++;
			aJobLock->Unlock();
			Acc += tv;
			SyncPoint->Unite();
		}
		static void LockTestUniteAction(void* data)
		{
			int trueValue = LockTestN * (LockTestN - 1) / 2;
			int Accv = Acc;
			CheckAlways(trueValue == Accv);
			if (trueValue == Accv)
			{
				printf("LockTest Passed\n");
			}
			else
			{
				printf("LockTest Failed\n");
			}
		}
		virtual void Tick() override
		{}

	public:
		DEFINE_MODULE_IN_CLASS(ConcurrencyTestDriverModule, ConcurrencyTestDriverModuleImp);
	};

	DEFINE_MODULE_REGISTRY(ConcurrencyTestDriverModule, ConcurrencyTestDriverModuleImp, 1000);
}