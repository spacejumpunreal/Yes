#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Core/MemoryModule.h"
#include "Runtime/Public/Core/System.h"

namespace Yes
{
	const int MaxAllocators = 64;
	struct MemoryModuleImp : public MemoryModule
	{
	public:
		void InitializeModule(System*)
		{
			std::fill_n(mAllocators, MaxAllocators, nullptr);
		}
		void Start(System*)
		{
			
		}
		void Tick()
		{}

		virtual IAllocator* GetAllocator(AllocatorTag)
		{
			return nullptr;
		}
		virtual IAllocator* CreateAllocator(AllocatorTag)
		{
			return nullptr;
		}

	protected:
		IAllocator* mAllocators[MaxAllocators];
		DEFINE_MODULE_IN_CLASS(MemoryModule, MemoryModuleImp);
	};
	DEFINE_MODULE_REGISTRY(MemoryModule, MemoryModuleImp, -1100);
}
