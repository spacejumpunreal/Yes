#pragma once
#include "Yes.h"
#include "IModule.h"
#include "Misc/SharedObject.h"

namespace Yes
{
	class FileModule: public IModule
	{
	public:
		virtual void SetBasePath(const char* path) = 0;
		virtual std::string GetNativePath(const char* path) = 0;
		virtual SharedBufferRef ReadFileContent(const char* path) = 0;
	DECLARE_MODULE_IN_CLASS(FileModule);
	};
}