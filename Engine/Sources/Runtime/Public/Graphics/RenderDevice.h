#pragma once

#include "Yes.h"
#include "Misc/SharedObject.h"

namespace Yes
{
	using RenderDeviceResourceRef = RenderDeviceResourceRef;
	class RenderDeviceResource : public SharedObject
	{
	public:
	protected:
		virtual ~RenderDeviceResource() = 0;
		virtual bool IsReady() = 0; // check if is ready to use
	};
	typedef enum class VertexFormat : byte
	{
		VF_P3F_T2F,
	} VertexFormat;
	struct RenderDevicePSODesc
	{
		VertexFormat VertexFormat;
		//TODO: add parameters that needs to be passed to CreatePSOSimple
	};
	class RenderDevicePass
	{

	};

	class RenderDeviceCommand
	{
		virtual void Reset() = 0;
		virtual void Execute() = 0;
	};
	class RenderDeviceBarrier : public RenderDeviceCommand
	{};
	class  RenderDeviceDrawcall : public RenderDeviceCommand
	{
		RenderDeviceResourceRef Mesh;
		std::vector<RenderDeviceResourceRef> Textures;
		std::vector<RenderDeviceResourceRef> ConstantBuffers;
		RenderDeviceResourceRef PSO;
	};

	class RenderDevice
	{
	public:
		//Resource related
		virtual RenderDeviceResourceRef CreateMeshSimple(SharedBufferRef meshBlob);
		virtual RenderDeviceResourceRef CreatePSOSimple(VertexFormat vertexFormat, RenderDeviceResourceRef& vs, RenderDeviceResourceRef& ps);
		virtual RenderDeviceResourceRef CreateShaderSimple(SharedBufferRef textBlob);
		virtual RenderDeviceResourceRef CreateRenderTarget();
		virtual RenderDeviceResourceRef CreteTextureSimple();

		//Command related
		virtual void BeginFrame();
		virtual void EndFrame();
		virtual RenderDevicePass* AllocPass() = 0;
		virtual RenderDeviceDrawcall* AllocDrawcall() = 0;
		virtual RenderDeviceBarrier* AllocBarrier() = 0;
	};
}
