#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Core/TickModule.h"
#include "Runtime/Public/Core/System.h"
#include "Runtime/Public/Misc/Debug.h"
#include "Runtime/Public/Misc/StringUtils.h"
#include "Runtime/Public/Misc/Time.h"
#include "Runtime/Public/Core/FrameLogicModule.h"
#include "Runtime/Public/Core/IDatum.h"

namespace Yes
{
	class TickModuleImp : public TickModule
	{
	public:
		void Start(System*) override
		{
			mStartTime = TimeStamp::Now();
		}
		struct TickDatum : public IDatum
		{
			static TickDatum* Task(IFrameContext*)
			{
				GET_MODULE(TickModuleImp)->Tick();
				return new TickDatum();
			}
		};
		void Tick()
		{
			mElapsedSeconds = (float)(TimeStamp::Now() - mStartTime).ToSeconds();
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
		std::multimap<float, std::function<void()>> mCallbacks;
		std::vector<std::pair<float, std::function<void()>>> mNewAdd;

	DEFINE_MODULE_IN_CLASS(TickModule, TickModuleImp);
	};
	DEFINE_MODULE_REGISTRY(TickModule, TickModuleImp, -800)
}
