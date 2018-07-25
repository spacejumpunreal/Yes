#pragma once
#include "Yes.h"
#include "IModule.h"
#include "Misc/Container.h"

namespace Yes
{
	DECLARE_MODULE(FileModule)
	{
		virtual void SetBasePath(const char* path) = 0;
		virtual std::string GetNativePath(const char* path) = 0;
		virtual ByteBlob ReadFileContent(const char* path) = 0;
	};
}