#include "Yes.h"
#include "Core/TickModule.h"
#include "Misc/Debug.h"
#include "Misc/Time.h"

namespace Yes
{
	DEFINE_MODULE(TickModule)
	{
	public:
		void Start()
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
			//mTicking = true;
			for (auto& cb : pending)
			{
				cb();
			}
			//mTicking = false;
		}
		void _AddCallback(float delay, std::function<void()>&& f)
		{
			Check(delay > 0, "deal with non-positive callbacks yourself");
			/*
			if (mTicking)
			{
				mNewAdd.emplace_back(delay, std::move(f));
			}
			else
			{
				mCallbacks.emplace(delay + mElapsedSeconds, std::forward<std::function<void()>>(f));
			}
			*/
			mCallbacks.emplace(delay + mElapsedSeconds, std::forward<std::function<void()>>(f));
			
		}
	private:
		//bool mTicking = false;
		TimeStamp mStartTime;
		float mElapsedSeconds = 0;
		std::list<ITickable*> mTickables;
		std::multimap<float, std::function<void()>> mCallbacks;
		std::vector<std::pair<float, std::function<void()>>> mNewAdd;
	};
	DEFINE_MODULE_CREATOR(TickModule);
}
