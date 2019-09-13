#pragma once
#include <chrono>
#include <thread>

namespace Yes
{
	class TimeDuration
	{
		typedef std::chrono::system_clock::duration DurationType;
	public:
		double ToSeconds() const
		{
			auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(mDuration).count();
			return double(ns / 1000000000.0);
		}
		double ToMilliSeconds() const
		{
			auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(mDuration).count();
			return double(ns / 1000000.0);
		}
	public:
		TimeDuration(double seconds)
			: mDuration(std::chrono::milliseconds((long long)(seconds * 1000)))
		{}
		TimeDuration(const DurationType& d)
			: mDuration(d)
		{}
		TimeDuration& operator+=(const TimeDuration& rhs)
		{
			mDuration += rhs.mDuration;
			return *this;
		}
		TimeDuration& operator-=(const TimeDuration& rhs)
		{
			mDuration -= rhs.mDuration;
			return *this;
		}
	private:
		DurationType mDuration;

		friend TimeDuration operator-(const TimeDuration& lhs, const TimeDuration& rhs)
		{
			return TimeDuration(lhs.mDuration - rhs.mDuration);
		}
		friend TimeDuration operator<(const TimeDuration& lhs, const TimeDuration& rhs)
		{
			return lhs.mDuration < rhs.mDuration;
		}
		friend TimeDuration operator>(const TimeDuration& lhs, const TimeDuration& rhs)
		{
			return lhs.mDuration > rhs.mDuration;
		}
		friend TimeDuration operator==(const TimeDuration& lhs, const TimeDuration& rhs)
		{
			return lhs.mDuration > rhs.mDuration;
		}
	};

	class TimeStamp
	{
		typedef std::chrono::system_clock::time_point StampType;
	public:
		TimeStamp() = default;
		static TimeStamp Now()
		{
			return TimeStamp(std::chrono::system_clock::now());
		}
	private:
		TimeStamp(const StampType& ts)
			: mStamp(ts)
		{}
		StampType mStamp;
		friend TimeDuration operator-(const TimeStamp& lhs, const TimeStamp& rhs)
		{
			return TimeDuration(lhs.mStamp - rhs.mStamp);
		}
	};
	
	template<int N = 60>
	class FPSCalculator
	{
	public:
		FPSCalculator()
			: mFrames(0)
			, mFPS(0.0f)
		{}
		void StartFrame(const TimeStamp& st)
		{
			++mFrames;
			auto i = mFrames % N;
			if (mFrames <= N)
				return;
			auto last = mStamps[i];
			auto diff = (st - last).ToSeconds();
			mFPS = float(N / diff);
			mStamps[i] = st;
		}
		void StartFrame()
		{
			StartFrame(TimeStamp::Now());
		}
		float GetFPS()
		{
			return mFPS;
		}
		int GetFrameCount()
		{
			return mFrames;
		}
	private:
		TimeStamp mStamps[N];
		int mFrames;
		float mFPS;
	};

	static inline void Sleep(TimeDuration t)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(int(t.ToMilliSeconds())));
	}
}