#include "Runtime/Public/Core/FrameLogicModule.h"
#include "Runtime/Public/Concurrency/Fiber.h"
#include "Runtime/Public/Concurrency/Lock.h"
#include "Runtime/Public/Concurrency/JobUtils.h"
#include "Runtime/Public/Misc/Containers/CompactAA.h"
#include "Runtime/Public/Core/ConcurrencyModule.h"
#include "Runtime/Public/Misc/SharedObject.h"
#include "Runtime/Public/Memory/AllocUtils.h"
#include <limits>
#include <vector>
#include <bitset>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>


namespace Yes
{
	using JobDispatcher = JobDataBatch<32>;
	static const Name NameFrameEnded("FrameEnded");
	void FrameEvent::Wait()
	{
		if (mState.load(std::memory_order_seq_cst) == 0)
			JobWaitingList::Append(Fiber::GetCurrentFiber());
	}
	void FrameEvent::Signal()
	{
		mState.store(1, std::memory_order_seq_cst);
		JobWaitingList::Pop(std::numeric_limits<size_t>::max());
	}

	struct TaskDesc
	{
		void*				Function;
		TypeId				Output;
		std::vector<TypeId>	Inputs;
		TaskDesc()
			: Output(std::type_index(typeid(TaskDesc)))
			, Function(nullptr)
		{}
	};
	using Name2Index = std::unordered_map<Name, uint32>;
	using TypeId2Index = std::unordered_map<TypeId, uint32>;
	using RegisteredTasks = std::unordered_map<TypeId, TaskDesc>;
	using RegisteredTasksItr = RegisteredTasks::iterator;

	struct TaskGraphConfig : public SharedObject
	{
		size_t							TaskCount;
		void**							Functions;
		CompactAA<uint32>				Consumers;
		CompactAA<uint32>				Dependencies;
		TaskGraphConfig(size_t n, TaskDesc* desc[], std::vector<uint32>* consumers, std::vector<uint32>* deps);
		~TaskGraphConfig();
	};

	struct FrameTaskInvokeContext
	{
	public:
		static FrameTaskInvokeContext* Create(void* Function, IFrameContext* fctx, uint32 taskIndex, IDatum* args[], uint32 nArgs)
		{
			FrameTaskInvokeContext* ret = (FrameTaskInvokeContext*)malloc(sizeof(FrameTaskInvokeContext) + sizeof(void*) * nArgs);
			ret->Function = Function;
			ret->FrameContext = fctx;
			ret->TaskIndex = taskIndex;
			ret->NArgs = nArgs;
			void** pArgs = (void**)&ret[1];
			memcpy(pArgs, args, sizeof(void*) * nArgs);
			return ret;
		}
		static void InvokeAndCleanup(FrameTaskInvokeContext* ctx)
		{
			void* f = ctx->Function;
			size_t nArgs = ctx->NArgs;
			IFrameContext* fctx = ctx->FrameContext;
			void* ret = InvokeWithTATB<void*>(f, fctx, (void**)&ctx[1], nArgs);
			fctx->OnTaskDone(ctx->TaskIndex, (IDatum*)ret);
			free(ctx);
		}
	private:
		void*			Function;
		IFrameContext*	FrameContext;
		uint32			TaskIndex;
		uint32			NArgs;
		void*			Datum[1];
	};

	struct FrameContext : public IFrameContext
	{
	public:
		FrameContext(size_t frameIndex, IFrameContext* prev, TaskGraphConfig* config, size_t nEvents);
		FrameEvent* GetFrameEvent(const Name& name);
		IFrameContext* GetPreviousFrame() { return mPrev; }
		void StartInitialTask(IFrameContext* ctx);
		void OnTaskDone(uint32 taskIndex, IDatum* datum);
		void End();
	private:
		void ScheduleTask(uint32 taskIndex, JobDispatcher& batch);
	private:
		std::atomic<uint32>*			mWaitCounts;
		IDatum**						mDatums;
		FrameEvent*						mEvents;
		TRef<TaskGraphConfig>			mTaskConfig;
		size_t							mFraneIndex;
		IFrameContext*					mPrev;
	};

	class FrameLogicModuleImp : public FrameLogicModule
	{
	public:
		FrameLogicModuleImp();
		void StartFrame() override;
		void EndFrame(IFrameContext*);
		void RegisterFrameEvent(const Name& name) override;
		void RegisterTaskImp(void* func, TypeId* types, size_t tCount) override;
		void SetConclusionTaskImp(TypeId name) override;
	public:
		const Name2Index& EventNameMap() { return mEventName2Index; }
	private:
		void RecreateTaskGraphConfig();
	private:
		SimpleSpinLock									mLock;
		TypeId											mConclusionTask;
		RegisteredTasks									mRegisteredTasks;
		Name2Index										mEventName2Index;
		IFrameContext*									mLastFrameConctext;
		TRef<TaskGraphConfig>							mTaskGraphConfig;
		std::atomic<uint32>								mLauchedFrames;
		std::atomic<uint32>								mEndedFrames;
	DEFINE_MODULE_IN_CLASS(FrameLogicModule, FrameLogicModuleImp);
	};
	DEFINE_MODULE_REGISTRY(FrameLogicModule, FrameLogicModuleImp, -1000);

	TaskGraphConfig::TaskGraphConfig(
		size_t n, TaskDesc* desc[],
		std::vector<uint32>* consumers,
		std::vector<uint32>* dirctDeps)
		: TaskCount(n)
		, Consumers(consumers, n)
		, Dependencies(dirctDeps, n)
	{
		Functions = new void* [n];
		for (size_t i = 0; i < n; ++i)
		{
			Functions[i] = desc[i]->Function;
		}
	}
	TaskGraphConfig::~TaskGraphConfig()
	{
		delete[] Functions;
	}
	FrameContext::FrameContext(size_t frameIndex, IFrameContext* prev, TaskGraphConfig* config, size_t nEvents)
		: mTaskConfig(config)
		, mFraneIndex(frameIndex)
		, mPrev(prev)
	{
		uint32 n = (uint32)mTaskConfig->TaskCount;
		mWaitCounts = new std::atomic<uint32>[n];
		for (uint32 i = 0; i < n; ++i)
		{
			mWaitCounts[i].store(mTaskConfig->Dependencies.Size(i), std::memory_order_relaxed);
		}
		mDatums = new IDatum*[n];
		mEvents = new FrameEvent[nEvents];
	}
	FrameEvent* FrameContext::GetFrameEvent(const Name& name)
	{
		FrameLogicModuleImp* m = GET_MODULE_AS(FrameLogicModule, FrameLogicModuleImp);
		const Name2Index& mp = m->EventNameMap();
		auto it = mp.find(name);
		return &mEvents[it->second];
	}
	void FrameContext::StartInitialTask(IFrameContext* ctx)
	{
		JobDispatcher batch(GET_MODULE(ConcurrencyModule));
		for (uint32 i = 0; i < (uint32)mTaskConfig->TaskCount; ++i)
		{
			if (mTaskConfig->Dependencies.Size(i) == 0)
			{
				batch.PutJobData(
					(ThreadFunctionPrototype)&FrameTaskInvokeContext::InvokeAndCleanup,
					FrameTaskInvokeContext::Create(mTaskConfig->Functions[i], ctx, i, nullptr, 0));
			}
		}
	}
	void FrameContext::OnTaskDone(uint32 taskIndex, IDatum* datum)
	{
		//mTaskConfig->DirectConsumersCounts[tid];
		mDatums[taskIndex] = datum;
		uint32 cCounts = mTaskConfig->Consumers.Size(taskIndex);
		uint32* cArray = mTaskConfig->Consumers[taskIndex];
		JobDispatcher batch(GET_MODULE(ConcurrencyModule));
		for (uint32 i = 0; i < cCounts; ++i)
		{
			uint32 tidx = cArray[i];
			uint32 ov = mWaitCounts[tidx].fetch_sub(1);
			if (ov <= 1)
			{
				CheckDebug(ov == 1);
				ScheduleTask(tidx, batch);
			}
		}
	}
	void FrameContext::End()
	{
		FrameLogicModuleImp* m = GET_MODULE_AS(FrameLogicModule, FrameLogicModuleImp);
		m->EndFrame(this);
	}
	void FrameContext::ScheduleTask(uint32 taskIndex, JobDispatcher& batch)
	{
		const size_t MaxArgsOnStack = 9;
		uint32 nDeps = mTaskConfig->Dependencies.Size(taskIndex);
		uint32* depArray = mTaskConfig->Dependencies[taskIndex];
		CheckAlways(nDeps < MaxArgsOnStack);
		IDatum* args[MaxArgsOnStack];
		IDatum** pArgs = args;
		for (uint32 i = 0; i < nDeps; ++i)
		{
			*pArgs++ = mDatums[depArray[i]];
		}
		batch.PutJobData(
			(ThreadFunctionPrototype)& FrameTaskInvokeContext::InvokeAndCleanup,
			FrameTaskInvokeContext::Create(mTaskConfig->Functions[taskIndex], this, taskIndex, args, nDeps));
	}
	struct DefaultConclusionTask
	{
		static DefaultConclusionTask* Job(IFrameContext* ctx)
		{
			ctx->End();
			return new DefaultConclusionTask();
		}
	};

	FrameLogicModuleImp::FrameLogicModuleImp()
		: mConclusionTask(std::type_index(typeid(FrameLogicModuleImp)))
		, mLastFrameConctext(nullptr)
	{
		RegisterTask(DefaultConclusionTask::Job);
		SetConclusionTask<DefaultConclusionTask>();
	}

	void FrameLogicModuleImp::StartFrame()
	{
		mLock.Lock();
		if (mTaskGraphConfig.GetPtr() == nullptr)
			RecreateTaskGraphConfig();
		mLock.Unlock();
		IFrameContext* newCtx = new FrameContext(mLauchedFrames++, mLastFrameConctext, mTaskGraphConfig.GetPtr(), mEventName2Index.size());
		mLastFrameConctext = newCtx;
	}
	void FrameLogicModuleImp::EndFrame(IFrameContext* ctx)
	{
		FrameEvent* ev = ctx->GetFrameEvent(NameFrameEnded);
		ev->Signal();
		++mEndedFrames;
	}
	void FrameLogicModuleImp::RegisterFrameEvent(const Name& name)
	{//called in init time, no need for lock
		mEventName2Index.insert(std::make_pair(name, (uint32)mEventName2Index.size()));
	}
	void FrameLogicModuleImp::RegisterTaskImp(void* func, TypeId* types, size_t tCount)
	{
		mLock.Lock();
		auto& x = mRegisteredTasks[types[0]];
		x.Function = func;
		x.Output = types[0];
		CheckAlways(types[1] == std::type_index(typeid(IFrameContext*)), "invalid task signature: first argument should be IFrameContext*");
		x.Inputs.insert(x.Inputs.end(), types + 2, types + tCount - 2);
		mLock.Unlock();
	}
	void FrameLogicModuleImp::SetConclusionTaskImp(TypeId name)
	{
		mLock.Lock();
		mConclusionTask = name;
		mLock.Unlock();
	}
	static const size_t MaxActiveTasks = 64;
	typedef std::bitset<MaxActiveTasks> DepSet;
	typedef std::pair<DepSet, uint32> DepSortPair;
	static bool SortWithDependencyCount(const DepSortPair& l, const DepSortPair& r)
	{
		return l.first.count() > r.first.count();
	}
	void FrameLogicModuleImp::RecreateTaskGraphConfig()
	{//called under protection of mLock
		RegisteredTasksItr j = mRegisteredTasks.begin();
		TypeId2Index taskId2Index;
		std::vector<TaskDesc*> relevantTaskDescs;
		{//0. find all relevant tasks
			std::unordered_set<TypeId> doneSet;
			std::vector<TypeId> open;
			open.push_back(mConclusionTask);
			while (open.size() > 0)
			{
				TypeId oname = open.back();
				open.pop_back();
				auto ret = doneSet.insert(oname);
				if (!ret.second)
					continue;
				TaskDesc& desc = mRegisteredTasks[oname];
				taskId2Index[oname] = (uint32)relevantTaskDescs.size();
				relevantTaskDescs.push_back(&desc);
				open.insert(open.end(), desc.Inputs.begin(), desc.Inputs.end());
			}
		}
		size_t activeCount = relevantTaskDescs.size();
		if (activeCount > MaxActiveTasks)
		{
			CheckAlways(false, "need to raise the max number of tasks limit");
		}
		//1. find explicit and implicit dependencies for all tasks
		std::vector<DepSet> allDependencies(activeCount);
		std::vector<std::vector<uint32>> directDeps(activeCount);
		{
			std::unordered_set<TaskDesc*> doneTasks;
			std::vector<TaskDesc*> open;
			for (int i = (int)activeCount - 1; i >= 0 ; --i)
			{
				doneTasks.clear();
				open.clear();
				open.push_back(relevantTaskDescs[i]);
				while (open.size() > 0)
				{
					TaskDesc* td = open.back();
					open.pop_back();
					for (TypeId& name : td->Inputs)
					{
						uint32 idx = taskId2Index[name];
						directDeps[i].push_back(idx);
						auto ret = doneTasks.insert(relevantTaskDescs[idx]);
						if (!ret.second)
							continue;
						allDependencies[i][idx] = true;
						open.push_back(relevantTaskDescs[idx]);
					}
				}
			}
		}
		//2. for each task, sort according to all dependency cout from high fan-in to low fan-in, find minimal direct dependency set
		std::vector<std::vector<uint32>> consumers(activeCount);
		std::vector<uint32> depCounts(activeCount);
		{
			std::vector<DepSortPair> deps;
			for (size_t i = 0; i < activeCount; ++i)
			{
				deps.clear();
				for (uint32 bitIdx = 0; bitIdx < MaxActiveTasks; ++bitIdx)
				{
					if (allDependencies[i][bitIdx])
					{
						deps.emplace_back(std::make_pair(allDependencies[bitIdx], bitIdx));
						++depCounts[i];
					}
				}
				std::sort(deps.begin(), deps.end(), SortWithDependencyCount);
				DepSet leftDeps = allDependencies[i];
				for (DepSortPair& p : deps)
				{
					auto common = p.first & leftDeps;
					if (common.any())
					{
						leftDeps ^= common;
						consumers[p.second].push_back((uint32)i);
					}
					if (!leftDeps.any())
						break;
				}
			}
		}
		mTaskGraphConfig = new TaskGraphConfig(
			activeCount, 
			relevantTaskDescs.data(), 
			consumers.data(), directDeps.data());
	}
}