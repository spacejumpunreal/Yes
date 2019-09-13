#include "SWRRasterizer.h"
#include "SWRJob.h"


namespace Yes::SWR
{

	class SWRRasterizerImp : public SWRRasterizer
	{
	public:
		SWRRasterizerImp(const DeviceDesc*, SWRJobSystem* jobSystem)
			: mJobSystem(jobSystem)
		{
		}
	protected:
		SWRJobSystem* mJobSystem;
	};

	SWRRasterizer* CreateSWRRasterizer(const DeviceDesc* desc, SWRJobSystem* jobSystem)
	{
		return new SWRRasterizerImp(desc, jobSystem);
	}
}