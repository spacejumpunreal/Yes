#include "Yes.h"
#include "Core/System.h"
#include "Core/TickModule.h"
#include "Misc/Debug.h"
#include "Misc/Time.h"
#include "Misc/StringUtils.h"

namespace Yes
{
	class TickModuleImp : public TickModule
	{
	public:
		void Start(System* system) override
		{
			mStartTime = TimeStamp::Now();
		}
		void AddTickable(ITickable* tickable)
		{
			DelTickable(tickable);
			mTickables.emplace_back(tickable);
		}
		void DelTickable(ITickable* tickable)
		{
			mTickables.remove(tickable);
		}
		void Tick()
		{
			mElapsedSeconds = (float)(TimeStamp::Now() - mStartTime).ToSeconds();
			for (auto i = mTickables.begin(); i != mTickables.end(); ++i)
			{
				(**i).Tick();
			}
			auto it = mCallbacks.begin();
			std::vector <std::function<void()>> pending;
			for (; it != mCallbacks.end(); ++it)
			{
				if (it->first > mElapsedSeconds)
				{
					break;
				}
				pending.emplace_back(std::move(it->second));
			}
			mCallbacks.erase(mCallbacks.begin(), it);
			for (auto& cb : pending)
			{
				cb();
			}
		}
		void _AddCallback(float delay, std::function<void()>&& f)
		{
			Check(delay > 0, "deal with non-positive callbacks yourself");

			mCallbacks.emplace(delay + mElapsedSeconds, std::forward<std::function<void()>>(f));
			
		}
	private:
		TimeStamp mStartTime;
		float mElapsedSeconds = 0;
		std::list<ITickable*> mTickables;
		std::multimap<float, std::function<void()>> mCallbacks;
		std::vector<std::pair<float, std::function<void()>>> mNewAdd;

	DEFINE_MODULE_IN_CLASS(TickModule, TickModuleImp);
	};
	DEFINE_MODULE_REGISTRY(TickModule, TickModuleImp, -800)
}
