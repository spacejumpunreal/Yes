#pragma once
#include "Yes.h"
#include "SWR.h"
#include <functional>

namespace Yes::SWR
{
	class SWRJobSystem
	{
	public:
		template<typename Functor, typename... Args>
		void PutBack(Functor&& f, Args&&... args)
		{
			std::function<void()> ff = std::bind(std::forward<Functor>(f), std::forward<Args>(args)...);
			_Put(true, std::move(ff));
		}
		template<typename Functor, typename... Args>
		void PutFront(Functor&& f, Args&&... args)
		{
			auto ff = std::bind(std::forward<Functor>(f), std::forward<Args>(args)...);
			_Put(false, std::move(ff));
		}
		virtual void _Put(bool atBack, std::function<void()>&& f) = 0;
		virtual void PutSyncPoint() = 0;

		virtual void Test(int x) = 0;
	};

	SWRJobSystem* CreateSWRJobSystem(const DeviceDesc* desc);
}
