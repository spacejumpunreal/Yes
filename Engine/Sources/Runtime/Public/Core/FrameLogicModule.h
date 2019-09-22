#pragma once
#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Core/IModule.h"
#include "Runtime/Public/Misc/Name.h"
#include "Runtime/Public/Concurrency/JobUtils.h"

namespace Yes
{
	class IDatum
	{
	public:
		virtual void Clear() = 0;
	};

	class FrameEvent : private JobWaitingList
	{
	public:
		void Wait();
		void Signal();
	};

	class FrameContext
	{
	public:
		//events
		FrameEvent* GetFrameEvent(const Name& name);
		//previous frame
		FrameContext* GetPreviousFrame();
	};

	using FrameTaskFunction = IDatum* (*)(IDatum* [], size_t n);

	class FrameLogicModule : public IModule
	{
	public:
		virtual void StartFrame() = 0;
		virtual void RegisterDatum(const Name& name) = 0;
		virtual void RegisterFrameEvent(const Name& name) = 0;
		virtual void RegisterTask(const Name& outName, FrameTaskFunction func, const Name dependencies[], size_t dependencyCount) = 0;
		virtual void SetRootTasks(const Name& names, size_t count) = 0;
	};
}