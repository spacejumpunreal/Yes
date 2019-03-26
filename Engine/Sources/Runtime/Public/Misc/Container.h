#pragma once
#include "Yes.h"
#include "Misc/SharedObject.h"

#include <vector>

namespace Yes
{
	/*
	template<typename T>
	using IRef = std::shared_ptr<T>;

	using ByteBlob = std::vector<uint8_t>;
	using SharedBytes = IRef<ByteBlob>;

	inline SharedBytes CreateSharedBytes(size_t sz, void* initData = nullptr)
	{
		auto blob = new ByteBlob(sz);
		Copy(initData, blob->data(), sz);
		return IRef<ByteBlob>(blob);
	}
	*/

	class RawSharedBuffer : public ISharedBuffer
	{//hold reference of buffer mallocated elsewhere
	public:
		RawSharedBuffer(size_t size, void* ptr)
			: mSize(size)
			, mPtr(ptr)
		{}
		size_t GetSize() 
		{ 
			return mSize; 
		}
		void* GetData()
		{
			return mPtr;
		}
		~RawSharedBuffer()
		{
			free(mPtr);
		}
	protected:
		size_t mSize;
		void* mPtr;
	};

	class ArraySharedBuffer : public ISharedBuffer
	{
	public:
		ArraySharedBuffer(size_t size)
			: mSize(size)
			, mData(malloc(size))
		{}
		ArraySharedBuffer(size_t size, const void* ptr)
			: mSize(size)
			, mData(malloc(size))
		{
			memcpy(mData, ptr, mSize);
		}
		~ArraySharedBuffer()
		{
			free(mData);
		}
		size_t GetSize()
		{
			return mSize;
		}
		void* GetData()
		{
			return mData;
		}
	protected:
		size_t mSize;
		void* mData;
	};

	template<typename T = uint8>
	class VectorSharedBuffer : public ISharedBuffer, public std::vector<T>
	{
	public:
		VectorSharedBuffer(size_t size = 0)
			: std::vector<T>(size)
			, mUnitSize(sizeof(T))
		{}
		size_t GetSize()
		{
			return std::vector<T>::size() * mUnitSize;
		}
		void* GetData()
		{
			return std::vector<T>::data();
		}
		size_t GetCount()
		{
			return std::vector<T>::size();
		}
	private:
		size_t mUnitSize;
	};
}