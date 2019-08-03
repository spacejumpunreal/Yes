#include "Public/Misc/Functional.h"
#include <cstdio>
#include <iostream>
#include <typeinfo>
#include <functional>

using namespace Yes::TypeList;

#define TestType(A, B, COMMENT) static_assert(std::is_same_v<A, B>, COMMENT)
#define TestListOperationType(OP, ExpectedResult, ...) \
	static_assert(std::is_same_v<ExpectedResult, OP<__VA_ARGS__>::type>, #OP)

#define TestListOperationValue(OP, ExpectedResult, ...) \
	static_assert(ExpectedResult == OP<__VA_ARGS__>::value, #OP)

namespace Yes
{
	template<typename R>
	R Func0()
	{
		R r{};
		std::cout << "Func0:"
			<< typeid(R).name() << "(" << r << ")" 
			<< std::endl;
		return r;
	}
	template<typename R, typename A>
	R Func1(A a)
	{
		R r{};
		std::cout 
			<< "Func1:" 
			<< typeid(R).name() << "(" << r << "):" 
			<< typeid(A).name() << "(" << a << "),"
			<< std::endl;
		return r;
	}
	int TestFunctional()
	{
		class A
		{};
		class B
		{};
		class C
		{};

		using LEmpty = Empty;
		TestListOperationType(BuildList, LEmpty);
		using LA = BuildList<A>::type;
		using LB = BuildList<B>::type;
		using LC = BuildList<C>::type;
		TestListOperationType(BuildList, LA, A);

		using LAB = BuildList<A, B>::type;
		using LBC = BuildList<B, C>::type;
		using LABC = BuildList<A, B, C>::type;
		
		TestType(LAB, LAB, "BuildList");

		TestListOperationType(Cons, LABC, A, LBC);

		TestListOperationType(Head, A, LABC);

		TestListOperationType(Tail, LBC, LABC);

		TestListOperationType(Last, A, LA);
		TestListOperationType(Last, B, LAB);
		TestListOperationType(Last, C, LABC);

		TestListOperationType(Init, Empty, LA);
		TestListOperationType(Init, LA, LAB);
		TestListOperationType(Init, LAB, LABC);

		TestListOperationValue(Length, 0, Empty);
		TestListOperationValue(Length, 1, LA);
		TestListOperationValue(Length, 3, LABC);

		return 0;
		
	}
}