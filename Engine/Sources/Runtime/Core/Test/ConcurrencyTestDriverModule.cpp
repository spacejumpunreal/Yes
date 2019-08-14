#include "Core/Test/ConcurrencyTestDriverModule.h"
#include "Public/Core/System.h"
#include "Public/Core/TickModule.h"
#include "Public/Core/ConcurrencyModule.h"
#include "Public/Concurrency/Thread.h"

#include <atomic>

namespace Yes
{
	static std::atomic<int> TakenJobs[8];
	static std::atomic<int> DoneJobs;
	static const size_t NJobs = 256;
	struct ConcurrencyTestDriverModuleImp : public ConcurrencyTestDriverModule, public ITickable
	{
	private:
		static JobSyncPoint* SyncPoint;
	public:
		virtual void Start(System* sys) override
		{
			ConcurrencyModule* m = GET_MODULE(ConcurrencyModule);
			SyncPoint = m->CreateJobSyncPoint(NJobs, UniteAction, nullptr);
			JobData j
			{
				ConcurrencyTestDriverModuleImp::Spawn,
				reinterpret_cast<void*>(1),
			};
			m->AddJobs(&j, 1);
		}
		static void UniteAction(void* data)
		{
			ConcurrencyModule* m = GET_MODULE(ConcurrencyModule);
			printf("****************************************************************\n");
			for (size_t i = 0; i < m->GetWorkersCount(); ++i)
			{
				printf("thread(%zd):%d\n", i, (int)TakenJobs[i]);
			}
		}
		static int fab(int x)
		{
			if (x == 0 || x == 1)
				return x;
			return fab(x - 1) + fab(x - 2);
		}
		static void Spawn(void* data)
		{
			//need to be able to get job system worker thread index
			size_t index = reinterpret_cast<size_t>(data);
			size_t cthread = ConcurrencyModule::GetJobThrreadIndex();
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
					ConcurrencyTestDriverModuleImp::Spawn,
					reinterpret_cast<void*>(index * 2 + i)
				};
			}
			m->AddJobs(jd, todo);
			++DoneJobs;
			fab(20);
			SyncPoint->Sync();
		}
		virtual void Tick() override
		{}

	public:
		DEFINE_MODULE_IN_CLASS(ConcurrencyTestDriverModule, ConcurrencyTestDriverModuleImp);
	};
	JobSyncPoint* ConcurrencyTestDriverModuleImp::SyncPoint;

	DEFINE_MODULE_REGISTRY(ConcurrencyTestDriverModule, ConcurrencyTestDriverModuleImp, 1000);
}