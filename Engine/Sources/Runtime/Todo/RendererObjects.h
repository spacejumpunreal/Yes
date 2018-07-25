#pragma once
#include "Demo18.h"
#include "Container.h"
#include "Math.h"
#include "Debug.h"
#include "RenderDeviceResource.h"
#include "VertexFormat.h"
namespace Demo18
{
	// from RenderDeviceModule
	extern void _ScheduleRelease(IRenderDeviceResource* resource);

	class DeviceResourceRef
	{
	public:
		DeviceResourceRef(IRenderDeviceResource* p = nullptr)
			: mPtr(p)
		{}
		DeviceResourceRef& operator=(IRenderDeviceResource* p)
		{
			DestroyCurrent();
			mPtr = p;
			return *this;
		}
		~DeviceResourceRef()
		{
			DestroyCurrent();
		}
		void DestroyCurrent()
		{
			if (mPtr)
			{
				_ScheduleRelease(mPtr);
				mPtr = nullptr;
			}
		}
		template<typename T>
		T* GetPtr()
		{
			return dynamic_cast<T*>(mPtr);
		}
		operator bool()
		{
			return mPtr != nullptr;
		}
	private:
		IRenderDeviceResource* mPtr;
	};
	//Texture2D
	struct RendererTexture2D
	{
		SharedBytes mData;
		DeviceResourceRef mDeviceObject;
	};
	RendererTexture2D* CreateRendererTexture2D(const char* path);
	//Geometry
	struct RendererGeometry
	{
		const VertexFormatInfo* mFormat;
		uint32 mVertexCount;
		SharedBytes mVertexData;

		bool mIsShortIndex;
		uint32 mIndexCount;
		SharedBytes mIndexData;

		DeviceResourceRef mDeviceGeometry;
	};
	template<typename VF, size_t VCount, size_t ICount>
	RendererGeometry* CreateRendererGeometry(VF(&vb)[VCount], uint16(&ib)[ICount])
	{
		auto ret = new RendererGeometry();
		ret->mFormat = GetVertexFormatInfo(VF::Enum);
		Check(ret->mFormat != nullptr, "unknown VertexFormat Enum:%d", VF::Enum);
		ret->mVertexCount = VCount;
		ret->mIndexCount = ICount;
		char* vbb = (char*)vb;
		char* ibb = (char*)ib;
		ret->mIsShortIndex = true;
		ret->mVertexData = IRef<ByteBlob>(new ByteBlob(vbb, vbb + sizeof(vb[0]) * VCount));
		ret->mIndexData = IRef<ByteBlob>(new ByteBlob(ibb, ibb + sizeof(ib[0]) * ICount));
		return ret;
	}
	//Shader
	struct RendererShader
	{
		std::string mPath;
		SharedBytes mShaderCode;
		std::string mEntry;
		DeviceResourceRef mDeviceShader;
	};
	RendererShader* CreateRendererShader(const char* path, const char* entry);
	//RenderTarget
	struct RendererRenderTarget
	{
		V2I mSize;
		DeviceResourceRef mDeviceRenderTarget;
		bool mIsBackBuffer;
		RendererRenderTarget(V2I sz)
			: mSize(sz)
			, mIsBackBuffer(false)
		{}
	};
	inline RendererRenderTarget* CreateRendererRenderTarget(V2I size)
	{
		auto r = new RendererRenderTarget(size);
		return r;
	}
	struct RendererConstantBuffer
	{
		SharedBytes mNewData;
		size_t mSize;
		DeviceResourceRef mDeviceConstantBuffer;
		void Update(SharedBytes block)
		{
			Check(block->size() == mSize);
			mNewData = block;
		}
		template<typename BlockData>
		void Update(BlockData& block)
		{
			Check(sizeof(block) == mSize);
			mNewData = CreateSharedBytes(mSize, &block);
		}
	};
	inline RendererConstantBuffer* CreateRendererConstantBuffer(size_t sz)
	{
		auto ret = new RendererConstantBuffer();
		ret->mSize = sz;
		return ret;
	}
	struct RendererSamplerState
	{
		DeviceResourceRef mDeviceSamplerState;
	};
	inline RendererSamplerState* CreateRendererSamplerState()
	{
		return new RendererSamplerState();
	}
	struct RendererRenderState
	{
	};

	struct RendererDrawcall
	{
		IRef<RendererShader> VertexShader;
		IRef<RendererShader> PixelShader;
		IRef<RendererGeometry> Geometry;
		std::vector<IRef<RendererTexture2D>> Textures;
		std::vector<IRef<RendererSamplerState>> Samplers;
		IRef<RendererConstantBuffer> VSConstants[1];
		IRef<RendererConstantBuffer> PSConstants[1];
		IRef<RendererRenderState> RenderState;
		RendererDrawcall* Next;
	};

	struct RendererPass
	{
		RendererDrawcall* mDrawcalls;
		IRef<RendererRenderTarget> mRT;
		V4F mClearColor;
		float mClearDepth;
		uint32 mClearStencil;
		bool mNeedClearDepth : 1;
		bool mNeedClearStencil : 1;

		RendererPass(RendererDrawcall* drawcalls, V4F clearColor, float clearDepth, uint32 clearStencil)
			: mDrawcalls(drawcalls)
			, mClearColor(clearColor)
			, mClearDepth(clearDepth)
			, mClearStencil(clearStencil)
			, mNeedClearDepth(true)
			, mNeedClearStencil(true)
		{}
		virtual ~RendererPass()
		{
			auto head = mDrawcalls;
			while (head)
			{
				auto toDel = head;
				delete toDel;
				head = head->Next;
			}
			mDrawcalls = nullptr;
		}
	};
}