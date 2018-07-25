#include "Yes.h"
#include "Core/TickModule.h"
#include "Misc/Debug.h"

namespace Yes
{
	DEFINE_MODULE(TickModule)
	{
	public:
		virtual void AddTickable(ITickable* tickable)
		{
			DelTickable(tickable);
			mTickables.emplace_back(tickable);
		}
		virtual void DelTickable(ITickable* tickable)
		{
			mTickables.remove(tickable);
		}
		virtual void Execute()
		{
			for (auto i = mTickables.begin(); i != mTickables.end(); ++i)
			{
				(**i).Tick();
			}
		}
		virtual void _AddCallback(float delay, std::function<void()>&& f)
		{
			Check(false, "not implemented yet");
		}
	private:
		std::list<ITickable*> mTickables;
		std::map<float, std::function<void()>> mCallbacks;
	};
	DEFINE_MODULE_CREATOR(TickModule);
}
