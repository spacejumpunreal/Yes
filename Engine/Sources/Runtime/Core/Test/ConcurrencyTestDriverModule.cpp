#include "Runtime/Core/Test/ConcurrencyTestDriverModule.h"
#include "Runtime/Public/Core/System.h"
#include "Runtime/Public/Core/TickModule.h"
#include "Runtime/Public/Core/ConcurrencyModule.h"
#include "Runtime/Public/Concurrency/Thread.h"
#include "Runtime/Public/Concurrency/JobUtils.h"

#include <atomic>

namespace Yes
{
	static const bool TestSpawn = true;
	static const bool TestLock = true;

	static JobUnitePoint* SyncPoint0;
	static JobUnitePoint* SyncPoint1;
	static JobLock* LockTestLock;
	static JobSemaphore* TesterSemaphore;

	static int TakenJobs[8];
	static std::atomic<int> DoneJobs;
	static const size_t NJobs = 256;

	static const int LockTestN = 64;
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
			TesterSemaphore = m->CreateJobSemaphore(0);
			JobData jb { TesterJobFunction, NULL };
			m->AddJobs(&jb, 1);
		}
		static void TesterJobFunction(void* data)
		{
			ConcurrencyModule* m = GET_MODULE(ConcurrencyModule);
			for (int i = 0; i < 100; ++i)
			{
				if (TestSpawn)
				{//bomb spawn jobs, do fab(20)in each spawned job and unite
					
					SyncPoint0 = m->CreateJobUnitePoint(NJobs, SpawnTestUniteAction, nullptr);
					JobData j
					{
						ConcurrencyTestDriverModuleImp::SpawnTestSpawn,
						reinterpret_cast<void*>(1),
					};
					m->AddJobs(&j, 1);
					TesterSemaphore->Decrease();
				}
				if (TestLock)
				{
					SyncPoint1 = m->CreateJobUnitePoint(LockTestN, LockTestUniteAction, nullptr);
					LockTestLock = m->CreateJobLock();
					JobDataBatch<16> jb(m);
					for (size_t i = 0; i < LockTestN; ++i)
					{
						jb.PutJobData(LockTestJob, reinterpret_cast<void*>(i));
					}
					jb.Flush();
					TesterSemaphore->Decrease();
				}
			}
		}
		static void SpawnTestUniteAction(void* data)
		{
			ConcurrencyModule* m = GET_MODULE(ConcurrencyModule);
			printf("****************************************************************\n");
			for (size_t i = 0; i < m->GetWorkersCount(); ++i)
			{
				printf("thread(%zd):%d\n", i, TakenJobs[i]);
			}
			TesterSemaphore->Increase();
		}
		static void SpawnTestSpawn(void* data)
		{
			//need to be able to get job system worker thread index
			size_t index = reinterpret_cast<size_t>(data);
			size_t cthread = ConcurrencyModule::GetJobThreadIndex();
			//printf("job(%03zd) run on thread(%02zd)\n", index, cthread);
			++TakenJobs[cthread];
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
			SyncPoint0->Unite();
		}
		static void LockTestJob(void* data)
		{
			int tv = 0;
			LockTestLock->Lock();
			tv = GV++;
			CheckAlways(tv < LockTestN);
			Acc += tv;
			//TakenValues[tv] = Acc;
			LockTestLock->Unlock();
			SyncPoint1->Unite();
		}
		static void LockTestUniteAction(void* data)
		{
			int trueValue = LockTestN * (LockTestN - 1) / 2;
			LockTestLock->Lock();
			int Accv = Acc;
			GV = 0;
			Acc = 0;
			LockTestLock->Unlock();
			CheckAlways(trueValue == Accv);
			if (trueValue == Accv)
			{
				printf("LockTest Passed\n");
			}
			else
			{
				printf("LockTest Failed\n");
			}
			TesterSemaphore->Increase();
		}
		virtual void Tick() override
		{}

	public:
		DEFINE_MODULE_IN_CLASS(ConcurrencyTestDriverModule, ConcurrencyTestDriverModuleImp);
	};

	DEFINE_MODULE_REGISTRY(ConcurrencyTestDriverModule, ConcurrencyTestDriverModuleImp, 1000);
}