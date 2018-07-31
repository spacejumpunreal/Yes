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
			_PutBack(std::move(ff));
		}
		template<typename Functor, typename... Args>
		void PutFront(Functor&& f, Args&&... args)
		{
			auto ff = std::bind(std::forward<Functor>(f), std::forward<Args>(args)...);
			_PutFront(std::move(ff));
		}
		virtual void _PutBack(std::function<void()>&& f) = 0;
		virtual void _PutFront(std::function<void()>&& f) = 0;
		virtual void PutSyncPoint() = 0;

		virtual void Test(int x) = 0;
		virtual ~SWRJobSystem() {}
	};

	SWRJobSystem* CreateSWRJobSystem(const DeviceDesc* desc);
}
