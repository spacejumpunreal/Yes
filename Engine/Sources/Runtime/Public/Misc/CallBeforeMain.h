#pragma once
namespace Yes
{
	template<typename Func, Func f>
	struct CallBeforeMain
	{
		template<typename... Args>
		CallBeforeMain(Args... args)
		{
			f(args...);
		}
	};
}

#define EXECUTE_BEFORE_MAIN(name, body) static void name() body; CallBeforeMain<decltype(name), name> CallBeforeMainObject##name
