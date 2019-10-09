import os

def print_defs(c):
    frags = []
    def gen(n):
        return "        typedef void* (*VPVP%d)(%s);\n" % (n, ",".join(["void*"] * (1 + n)))
    for i in xrange(c):
        frags.append(gen(i))
    return "".join(frags)


def print_invokes(c):
    frags = []
    def gen(n):
        arg_list = ["argA"] + ["argB[%d]" % i for i in xrange(n)]
        return "        case %d: return ((TATBInvokeSignatures::VPVP%d)Func)(%s);\n" % (n, n, ", ".join(arg_list))
    for i in xrange(c):
        frags.append(gen(i))
    return "".join(frags)


FILE_TEMPLATE = """#pragma once
#include "Runtime/Public/Misc/Debug.h"
namespace Yes
{
	namespace TATBInvokeSignatures
	{
%s

    }
    template<typename TReturn>
	TReturn InvokeWithTATB(void* Func, void* argA, void** argB, size_t nArgs)
	{
		switch (nArgs)
		{
%s
		default:
			CheckAlways(false, "Invoke called with too many arguments");
		}
	}
}

"""

if __name__ == "__main__":
    c = 8
    t = FILE_TEMPLATE % (print_defs(c), print_invokes(c))
    cp = os.path.dirname(os.path.realpath(__file__))
    fp = os.path.join(cp, "../Sources/Runtime/Public/Misc/Functional/Invoke.h")
    with open(fp , "wb") as wf:
        wf.write(t)
