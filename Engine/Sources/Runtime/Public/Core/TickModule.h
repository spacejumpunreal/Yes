#pragma once
#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Core/IModule.h"
#include "Runtime/Public/Core/ITickable.h"
#include <functional>

namespace Yes
{

	class TickModule : public IModule
	{
	public:
		template<typename Functor, typename... Args>
		void AddCallback(float delay, Functor&& f, Args&&... args)
		{
			_AddCallback(delay, std::bind(std::forward<Functor>(f), std::forward<Args>(args)...));
		}
		virtual void Tick() = 0;
	private:
		virtual void _AddCallback(float delay, std::function<void()>&& f) = 0;

	DECLARE_MODULE_IN_CLASS(TickModule)
	};
}
