#pragma once

#include "Yes.h"
#include <type_traits>

namespace Yes
{
	namespace TypeList
	{
		//definition
		struct Empty
		{};
		template<typename H, typename T>
		struct TypeList
		{
			using Head = H;
			using Tail = T;
		};
		//BuildList
		template<typename... Args>
		struct BuildList;
		template<>
		struct BuildList<>
		{
			using type = Empty;
		};
		template<typename A, typename... Args>
		struct BuildList<A, Args...>
		{
			using type = 
				TypeList<
					A, 
					typename BuildList<Args...>::type
				>;
		};

		struct ReturnEmpty
		{
			using type = Empty;
		};

		template<typename TL>
		struct IsEmpty
		{
			static const bool value = std::is_same_v<TL, Empty>;
		};

		template<typename A, typename TL>
		struct Cons
		{
			using type = TypeList<A, TL>;
		};

		template<typename TL>
		struct Head
		{
			using type = typename TL::Head;
		};

		template<typename TL>
		struct Tail
		{
			using type = typename TL::Tail;
		};
		//Last
		template<typename TL, bool = IsEmpty<typename TL::Tail>::value>
		struct Last;
		template<typename TL>
		struct Last<TL, true>
		{
			using type = typename TL::Head;
		};
		template<typename TL>
		struct Last<TL, false>
		{
			using type = typename Last<typename TL::Tail>::type;
		};
		//Init
		template<typename TL, bool = IsEmpty<typename TL::Tail>::value>
		struct Init;
		template<typename TL>
		struct Init<TL, true>
		{
			using type = Empty;
		};
		template<typename TL>
		struct Init<TL, false>
		{
			using type = typename Cons<
				typename TL::Head,
				typename Init<typename TL::Tail>::type
			>::type;
		};

		template<typename TL, bool = IsEmpty<TL>::value>
		struct Length;

		template<typename TL>
		struct Length<TL, true>
		{
			static const size_t value = 0;
		};
		template<typename TL>
		struct Length<TL, false>
		{
			static const size_t value = Length<TL::Tail>::value + 1;
		};
	}
}