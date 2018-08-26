#include "Yes.h"
#include "Core/MemoryModule.h"

namespace Yes
{
	const int MaxAllocators = 64;
	DEFINE_MODULE(MemoryModule)
	{
	public:
		void InitializeModule()
		{
			std::fill_n(mAllocators, MaxAllocators, nullptr);
		}
		void Start()
		{
			
		}
		void Tick()
		{}
	protected:
		IAllocator* mAllocators[MaxAllocators];
	};
	DEFINE_MODULE_CREATOR(MemoryModule);
}
