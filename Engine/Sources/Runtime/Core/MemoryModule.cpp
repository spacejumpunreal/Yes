#include "Yes.h"
#include "Core/MemoryModule.h"
#include "Core/System.h"

namespace Yes
{
	const int MaxAllocators = 64;
	struct MemoryModuleImp : public MemoryModule
	{
	public:
		void InitializeModule(System* system)
		{
			std::fill_n(mAllocators, MaxAllocators, nullptr);
		}
		void Start(System* system)
		{
			
		}
		void Tick()
		{}

		virtual IAllocator* GetAllocator(AllocatorTag tag)
		{
			return nullptr;
		}
		virtual IAllocator* CreateAllocator(AllocatorTag tag)
		{
			return nullptr;
		}

	protected:
		IAllocator* mAllocators[MaxAllocators];
		DEFINE_MODULE_IN_CLASS(MemoryModule, MemoryModuleImp);
	};
	DEFINE_MODULE_REGISTRY(MemoryModule, MemoryModuleImp, -1100);
}
