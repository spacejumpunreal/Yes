#pragma once
#include <utility>

namespace Yes
{
	class IBenchmark
	{
	public:
		virtual void Run(int) = 0;
	};

	template<int Index>
	class BubbleSortLinkedList : public IBenchmark
	{
		static const int N = 512;
	public:
		BubbleSortLinkedList()
			: mScale(N)
		{
			mMemory = new Node[N];
		}
		~BubbleSortLinkedList()
		{
			delete[] mMemory;
		}
		void Run(int scale)
		{
			for (int time = 0; time < scale; ++time)
			{
				for (int i = 0; i < mScale; ++i)
				{
					int d = mScale - i;
					mMemory[i].NodeData.Data = double(d);
					mMemory[i].NodeData.Next = &mMemory[i + 1];
				}
				mMemory[mScale - 1].NodeData.Next = nullptr;
				Node* stop = nullptr;
				for (int i = 0; i < mScale; ++i)
				{
					Node* p = mMemory;
					for (; p->NodeData.Next != stop; p = p->NodeData.Next)
					{
						if (p->NodeData.Data > p->NodeData.Next->NodeData.Data)
						{
							std::swap(p->NodeData.Data, p->NodeData.Next->NodeData.Data);
						}
					}
					stop = p;
				}
			}
		}
	private:
		struct Node
		{
			union
			{
				struct
				{
					double		Data;
					Node*		Next;
				} NodeData;
				double Padding[32];
			};
		};
		Node*	mMemory;
		int		mScale;
	};

	template<int Index>
	class FixedPointSqrt2 : public IBenchmark
	{
	public:
		FixedPointSqrt2()
			: mLast(0)
			, mAcc(666)
		{
		}
		void Run(int scale)
		{
			for (int i = 0; i < scale; ++i)
			{
				mLast = 0;
				double next = 0;
				do
				{
					next = mLast * mLast + mLast - 2;
					mLast = (next + mLast) * 0.5;
				} while (std::abs(mLast - next) > 1e-8);
				mAcc += mLast;
				mAcc *= 0.5;
			}
		}
		double GetAcc() { return mAcc; }
	private:
		double				mLast;
		volatile double		mAcc;
	};
}

#define RUN_REF_BENCHMARK(Type, Scale) \
{\
	static IBenchmark* benchmark = new Type();\
	benchmark->Run(Scale);\
}