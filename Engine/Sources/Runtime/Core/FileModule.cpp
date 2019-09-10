#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Core/FileModule.h"
#include "Runtime/Public/Core/System.h"
#include "Runtime/Public/Misc/Container.h"
#include "Runtime/Public/Misc/Debug.h"

#include <fstream>

namespace Yes
{
	struct FileModuleImp : public FileModule
	{
	public:
		virtual void SetBasePath(const char* path) override
		{
			mBasePath = path;
			while (
				(mBasePath.back() == '/' || mBasePath.back() == '\\') &&
				(mBasePath.size() > 0))
			{
				mBasePath.pop_back();
			}
			mBasePath.push_back('/');
		}
		virtual void InitializeModule(System* system)
		{
			auto it = system->GetArguments().equal_range("FileModuleBasePath");
			SetBasePath(it.first->second.c_str());
		}
		virtual std::string GetNativePath(const char* path)
		{
			return _GetNativePath(path);
		}
		inline std::string _GetNativePath(const char* path)
		{
			return mBasePath + "/" + path;
		}
		virtual SharedBufferRef ReadFileContent(const char* path)
		{
			auto pth = GetNativePath(path);
			std::ifstream fs(pth, std::ios::binary | std::ios::ate);
			Check(fs.is_open());
			auto ed = fs.tellg();
			fs.seekg(0, std::ios::beg);
			auto todo = ed - fs.tellg();
			auto blob = new VectorSharedBuffer<uint8>(todo);
			auto rp = (char*)&(*blob)[0];
			while (todo > 0)
			{
				fs.read(rp, todo);
				todo -= fs.gcount();
				Check(fs.good());
			}
			return blob;
		}
	protected:
		std::string mBasePath;
	DEFINE_MODULE_IN_CLASS(FileModule, FileModuleImp);
	};
	DEFINE_MODULE_REGISTRY(FileModule, FileModuleImp, -1000);
}