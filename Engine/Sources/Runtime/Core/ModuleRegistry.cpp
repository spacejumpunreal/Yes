#define ADD_MODULE_DEPENDENCY(TypeName) extern void Register##TypeName(); Register##TypeName();

namespace Yes
{
	void CollectAllModules()
	{
		ADD_MODULE_DEPENDENCY(MemoryModule);
		ADD_MODULE_DEPENDENCY(FileModule);
		ADD_MODULE_DEPENDENCY(ConcurrencyModule);
		ADD_MODULE_DEPENDENCY(FrameLogicModule);
		ADD_MODULE_DEPENDENCY(TickModule);
		ADD_MODULE_DEPENDENCY(DX12DemoModule);
		ADD_MODULE_DEPENDENCY(DX12RenderDeviceModule);
		ADD_MODULE_DEPENDENCY(RenderDeviceTestDriverModule);
		ADD_MODULE_DEPENDENCY(WindowsWindowModule);
		ADD_MODULE_DEPENDENCY(ConcurrencyTestDriverModule);
		ADD_MODULE_DEPENDENCY(FrameLogicTestDriverModule);
	}
}