#pragma once

namespace Yes
{
	template<typename Func>
	struct FunctionInfo;

	template<typename... ATypes>
	struct ArgTypeList;

	template<>
	struct ArgTypeList<>
	{
		static const size_t Count = 0;
		template<typename TVisitor>
		static void Traverse(TVisitor&)
		{}
	};

	template<typename Head, typename... Tail>
	struct ArgTypeList<Head, Tail...> : ArgTypeList<Tail...>
	{
		using Current = Head;
		using TailList = ArgTypeList<Tail...>;
		static const size_t Count = TailList::Count + 1;
		template<typename TVisitor>
		static void Traverse(TVisitor& visitor)
		{
			visitor.template Visit<Head>();
			TailList::Traverse(visitor);
		}
	};

	template<typename RType, typename... ATypes>
	struct FunctionInfo<RType(*)(ATypes... args)> : ArgTypeList<ATypes...>
	{
		using ReturnValueType = RType;
		using TailList = ArgTypeList<ATypes...>;
		static const size_t Count = TailList::Count + 1;
		template<typename TVisitor>
		static void Traverse(TVisitor& visitor)
		{
			visitor.template Visit<ReturnValueType>();
			TailList::Traverse(visitor);
		}
	};
}