#include "Runtime/Public/Core/FrameLogicModule.h"
#include "Runtime/Public/Concurrency/Fiber.h"
#include <limits>


namespace Yes
{
	void FrameEvent::Wait()
	{
		JobWaitingList::Append(Fiber::GetCurrentFiber());
	}
	void FrameEvent::Signal()
	{
		JobWaitingList::Pop(std::numeric_limits<size_t>::max());
	}

	struct FrameLogicModuleImp : FrameLogicModule
	{

	};
}