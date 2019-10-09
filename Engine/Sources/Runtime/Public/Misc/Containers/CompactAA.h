#pragma once
#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Memory/AllocUtils.h"
#include "Runtime/Public/Memory/SizeUtils.h"
#include <vector>

namespace Yes
{
	//Compact Array of Array
	template<typename T>
	class CompactAA
	{
		struct ArrayHeader
		{
			uint32 Size;
			T* Head;
		};
	public:
		CompactAA(const std::vector<T>* vectors, size_t nVectors)
		{
			size_t total = 0;
			for (size_t i = 0; i < nVectors; ++i)
			{
				total += vectors[i].size();
			}
			uint32 storageSize = sizeof(T) * (uint32)total;
			uint32 headerSizeAligned = (uint32)AlignSize(sizeof(ArrayHeader) * (uint32)nVectors, alignof(T));
			uint32 totalSize = headerSizeAligned + storageSize;
			mHeaders = (ArrayHeader*)malloc(totalSize);
			T* p = (T*)(((uint8*)mHeaders) + headerSizeAligned);
			for (size_t i = 0; i < nVectors; ++i)
			{
				uint32 c = mHeaders[i].Size = (uint32)vectors[i].size();
				mHeaders[i].Head = p;
				memcpy(p, vectors[i].data(), c * sizeof(T));
				p += c;
			}
		}
		~CompactAA()
		{
			free(mHeaders);
		}
		T* operator[](uint32 idx)
		{
			return mHeaders[idx].Head;
		}
		const T* operator[](uint32 idx) const
		{
			return mHeaders[idx].Head;
		}
		uint32 Size(uint32 idx) const
		{
			return mHeaders[idx].Size;
		}
	private:
		ArrayHeader*		mHeaders;
	};
}