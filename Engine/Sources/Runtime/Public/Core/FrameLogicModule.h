#pragma once
#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Core/IModule.h"
#include "Runtime/Public/Misc/Name.h"
#include "Runtime/Public/Misc/Functional/FunctionInfo.h"
#include "Runtime/Public/Concurrency/JobUtils.h"
#include "Runtime/Public/Misc/Functional/Invoke.h"
#include <typeindex>
#include <vector>
#include <atomic>

namespace Yes
{
	class IDatum
	{
	public:
		template<typename T>
		T& As()
		{
			return *(T*)GetData();
		}
		virtual void* GetData() { return this; }
	};

	class FrameEvent : private JobWaitingList
	{
	public:
		void Wait();
		void Signal();
	private:
		std::atomic<int>	mState;
	};

	class IFrameContext
	{
	public:
		virtual FrameEvent* GetFrameEvent(const Name& name) = 0;
		virtual IFrameContext* GetPreviousFrame() = 0;
		virtual void OnTaskDone(uint32 tid, IDatum* datum) = 0;
		virtual void End() = 0;
	};

	using TypeId = std::type_index;

	class FrameLogicModule : public IModule
	{
	public:
		virtual void StartFrame() = 0;
		virtual void RegisterFrameEvent(const Name& name) = 0;
		template<typename Func>
		void RegisterTask(Func func);
		template<typename TDatum>
		void SetConclusionTask();
	protected:
		virtual void RegisterTaskImp(void* func, TypeId* types, size_t tCount) = 0;
		virtual void SetConclusionTaskImp(TypeId task) = 0;
	DECLARE_MODULE_IN_CLASS(FrameLogicModule);
	};

	struct TypeIdRecorder
	{
	public:
		TypeIdRecorder(TypeId* buffer)
			: mBuffer(buffer)
		{}
		template<typename T>
		void Visit()
		{
			*mBuffer++ = std::type_index(typeid(T));
		}
	private:
		TypeId* mBuffer;
	};

	template<typename Func>
	void FrameLogicModule::RegisterTask(Func func)
	{
		const size_t tCount = FunctionInfo<std::decay<Func>::type>::Count;
		static_assert(sizeof(TypeId) <= sizeof(long long), "use larger backing memory");
		long long mem[tCount];
		TypeId* typeIds = (TypeId*)& mem[0];
		TypeIdRecorder v(typeIds);
		FunctionInfo<decltype(func)>::Traverse(v);
		RegisterTaskImp(func, typeIds, tCount);
	}
	template<typename TDatum>
	void FrameLogicModule::SetConclusionTask()
	{
		SetConclusionTaskImp(std::type_index(typeid(TDatum*)));
	}
}