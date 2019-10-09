#pragma once
#include "Runtime/Public/Misc/Debug.h"
namespace Yes
{
	namespace TATBInvokeSignatures
	{
        typedef void* (*VPVP0)(void*);
        typedef void* (*VPVP1)(void*,void*);
        typedef void* (*VPVP2)(void*,void*,void*);
        typedef void* (*VPVP3)(void*,void*,void*,void*);
        typedef void* (*VPVP4)(void*,void*,void*,void*,void*);
        typedef void* (*VPVP5)(void*,void*,void*,void*,void*,void*);
        typedef void* (*VPVP6)(void*,void*,void*,void*,void*,void*,void*);
        typedef void* (*VPVP7)(void*,void*,void*,void*,void*,void*,void*,void*);


    }
    template<typename TReturn>
	TReturn InvokeWithTATB(void* Func, void* argA, void** argB, size_t nArgs)
	{
		switch (nArgs)
		{
        case 0: return ((TATBInvokeSignatures::VPVP0)Func)(argA);
        case 1: return ((TATBInvokeSignatures::VPVP1)Func)(argA, argB[0]);
        case 2: return ((TATBInvokeSignatures::VPVP2)Func)(argA, argB[0], argB[1]);
        case 3: return ((TATBInvokeSignatures::VPVP3)Func)(argA, argB[0], argB[1], argB[2]);
        case 4: return ((TATBInvokeSignatures::VPVP4)Func)(argA, argB[0], argB[1], argB[2], argB[3]);
        case 5: return ((TATBInvokeSignatures::VPVP5)Func)(argA, argB[0], argB[1], argB[2], argB[3], argB[4]);
        case 6: return ((TATBInvokeSignatures::VPVP6)Func)(argA, argB[0], argB[1], argB[2], argB[3], argB[4], argB[5]);
        case 7: return ((TATBInvokeSignatures::VPVP7)Func)(argA, argB[0], argB[1], argB[2], argB[3], argB[4], argB[5], argB[6]);

		default:
			CheckAlways(false, "Invoke called with too many arguments");
			return {};
		}
	}
}

