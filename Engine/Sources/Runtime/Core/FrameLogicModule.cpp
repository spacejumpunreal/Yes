#include "Runtime/Public/Core/FrameLogicModule.h"
#include "Runtime/Public/Concurrency/Fiber.h"
#include "Runtime/Public/Concurrency/Lock.h"
#include <limits>
#include <vector>
#include <unordered_map>


namespace Yes
{
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
		FrameTaskFunction Function;
		Name Output;
		std::vector<Name> Inputs;
	};

	struct TaskDetail
	{
		FrameTaskFunction Function;
		uint32 DependencyCount;
		std::vector<uint16> DirectConsumers;
	};

	class FrameLogicModuleImp : FrameLogicModule
	{
	public:
		void StartFrame()
		{}
		void RegisterFrameEvent(const Name& name)
		{}
		void RegisterTask(const Name& outName, FrameTaskFunction func, const Name dependencies[], size_t dependencyCount)
		{}
		void SetRootTasks(const Name& names, size_t count)
		{}
	private:
		typedef std::unordered_map<Name, TaskDesc>	RegisterTaskRequestMap;
		typedef std::unordered_map<Name, uint32>	Name2Index;
	private:
		SimpleSpinLock									mLock;
		//queued
		std::vector<Name>								mQueuedSetRootTasks;
		RegisterTaskRequestMap							mQueuedRegisterTaskRequests;
		std::vector<Name>								mQueuedRegisterEventRequests;
		//current
		Name2Index										mCurrentTaskName2Index;
		std::vector<TaskDetail>							mCurrentTaskArray;
		Name2Index										mCurrentEventName2Index;
		std::vector<FrameEvent>							mCurrentEventArray;
		std::vector<TaskDetail*>						mCurrentRootTasks;
	};
}