#pragma once
#include "Yes.h"
#include "Utils.h"

namespace Yes
{
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
}