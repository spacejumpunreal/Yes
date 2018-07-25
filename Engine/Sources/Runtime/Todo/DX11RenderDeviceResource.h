#include "Demo18.h"
#include "RendererObjects.h"
#include "RenderDeviceResource.h"
#include "Debug.h"
#include "ThirdParty/DDSTextureLoader/DDSTextureLoader.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <windows.h>
#include "VertexFormat.h"

namespace Demo18
{
	class DX11Texture2D : public IRenderDeviceResource
	{
	public:
		DX11Texture2D(ID3D11Device* dev, const RendererTexture2D& res)
		{
			auto sz = res.mData->size();
			auto data = res.mData->data();
			ID3D11ShaderResourceView* p;
			HRESULT hr = DirectX::CreateDDSTextureFromMemory(
				dev, data, sz, nullptr, &p);
			Check(SUCCEEDED(hr));
			mShaderResourceView = p;
		}
	public:
		COMRef<ID3D11ShaderResourceView> mShaderResourceView;
	};
	enum EPipelineStage {VSStage, PSStage};

	template<EPipelineStage stage>
	inline void ApplyTexture2D(ID3D11Device* dev, ID3D11DeviceContext* ctx, RendererTexture2D& res, uint slot)
	{
		if (!res.mDeviceObject)
			res.mDeviceObject = new DX11Texture2D(dev, res);
		res.mData = nullptr;
		auto v = res.mDeviceObject.GetPtr<DX11Texture2D>();
		switch (stage)
		{
		case VSStage:
			ctx->VSSetShaderResources(slot, 1, &v->mShaderResourceView);
			break;
		case PSStage:
			ctx->PSSetShaderResources(slot, 1, &v->mShaderResourceView);
			break;
		default:
			Check(false, "unknown stage");
			break;
		}
	}

	struct DeviceVertexInfo
	{
		const uint Count;
		const D3D11_INPUT_ELEMENT_DESC* Desc;
		DeviceVertexInfo(unsigned long long c, const D3D11_INPUT_ELEMENT_DESC* d)
			: Count(uint(c))
			, Desc(d)
		{}
	};
	inline void RegisterVertexFormats()
	{
		{
			static const D3D11_INPUT_ELEMENT_DESC desc[] = {
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			};
			static const DeviceVertexInfo x(sizeof(desc) / sizeof(desc[0]), desc);
			RegisterVertexFormat<VF_P3F_N3F_T2F>(&x);
		}
		{
			static const D3D11_INPUT_ELEMENT_DESC desc[] = {
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			};
			static const DeviceVertexInfo x(sizeof(desc) / sizeof(desc[0]), desc);
			RegisterVertexFormat<VF_P3F_T2F>(&x);
		}
		
	}
	class DX11DeviceGeometry : public IRenderDeviceResource
	{
	public:
		DX11DeviceGeometry(ID3D11Device* dev, const RendererGeometry& geom)
		{
			{
				D3D11_BUFFER_DESC vbd;
				vbd.ByteWidth = UINT(geom.mVertexData->size());
				vbd.Usage = D3D11_USAGE_DEFAULT;
				vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
				vbd.CPUAccessFlags = 0;
				vbd.MiscFlags = 0;
				vbd.StructureByteStride = 0; //no meaning for vertex buffer
				D3D11_SUBRESOURCE_DATA data;
				data.pSysMem = geom.mVertexData->data();
				ID3D11Buffer* buf;
				auto hr = dev->CreateBuffer(&vbd, &data, &buf);
				Check(SUCCEEDED(hr));
				mVertexBuffer = buf;
			}
			{
				D3D11_BUFFER_DESC ibd;
				ibd.ByteWidth = UINT(geom.mIndexData->size());
				ibd.Usage = D3D11_USAGE_DEFAULT;
				ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
				ibd.CPUAccessFlags = 0;
				ibd.MiscFlags = 0;
				ibd.StructureByteStride = 0; //no meaning for index buffer
				D3D11_SUBRESOURCE_DATA data;
				data.pSysMem = geom.mIndexData->data();
				ID3D11Buffer* buf;
				auto hr = dev->CreateBuffer(&ibd, &data, &buf);
				Check(SUCCEEDED(hr));
				mIndexBuffer = buf;
			}
		}
	public:
		COMRef<ID3D11Buffer> mVertexBuffer;
		COMRef<ID3D11Buffer> mIndexBuffer;
	};
	inline void ApplyGeometry(ID3D11Device* dev, ID3D11DeviceContext* ctx, RendererGeometry& res, uint slot)
	{
		if (!res.mDeviceGeometry)
			res.mDeviceGeometry = new DX11DeviceGeometry(dev, res);
		res.mIndexData = nullptr;
		res.mVertexData = nullptr;
		auto v = res.mDeviceGeometry.GetPtr<DX11DeviceGeometry>();
		uint zero = 0;
		uint vsize = uint(res.mFormat->VertexSize);
		ctx->IASetVertexBuffers(slot, 1, &v->mVertexBuffer, &vsize, &zero);
		ctx->IASetIndexBuffer(v->mIndexBuffer.GetPtr(), (res.mIsShortIndex ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT), 0);
		ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	class DX11DeviceVertexShader : public IRenderDeviceResource
	{
	public:
		DX11DeviceVertexShader(ID3D11Device* dev, const RendererShader& shader)
		{
			HRESULT hr;
			ID3DBlob* blob;
			ID3DBlob* errMsg;
			hr = D3DCompile(
				shader.mShaderCode->data(), shader.mShaderCode->size(),
				shader.mPath.c_str(), nullptr, nullptr, shader.mEntry.c_str(), "vs_5_0",
				D3D10_SHADER_DEBUG | D3D10_SHADER_SKIP_OPTIMIZATION,
				0, &blob, &errMsg);
			if (errMsg)
				Check(!errMsg, errMsg->GetBufferPointer());
			ID3DBlob* sig;
			hr = D3DGetBlobPart(
				blob->GetBufferPointer(), blob->GetBufferSize(), 
				D3D_BLOB_INPUT_SIGNATURE_BLOB, 0, &sig);
			mSignatureBlob = sig;
			Check(SUCCEEDED(hr));
			ID3D11VertexShader* vs;
			hr = dev->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &vs);
			Check(SUCCEEDED(hr));
			mShader = vs;
			ReleaseCOM(blob);
		}
		ID3D11InputLayout* GetLayout(const VertexFormatInfo* vfi, ID3D11Device* dev)
		{
			auto itr = mInputLayoutCache.find(vfi);
			ID3D11InputLayout* layout = nullptr;
			if (itr == mInputLayoutCache.end())
			{
				auto dd = vfi->DeviceDetail;
				auto hr = dev->CreateInputLayout(
					dd->Desc, dd->Count,
					mSignatureBlob->GetBufferPointer(), mSignatureBlob->GetBufferSize(),
					&layout);
				Check(SUCCEEDED(hr));
				mInputLayoutCache[vfi] = layout;
			}
			else
			{
				layout = itr->second.GetPtr();
			}
			return layout;
		}
	public:
		COMRef<ID3D11VertexShader> mShader;
		COMRef<ID3DBlob> mSignatureBlob;
		std::unordered_map<const VertexFormatInfo*, COMRef<ID3D11InputLayout>> mInputLayoutCache;
	};
	inline void ApplyLayout(ID3D11Device* dev, ID3D11DeviceContext* ctx, RendererShader* vs, RendererGeometry* geom)
	{
		auto l = vs->mDeviceShader.GetPtr<DX11DeviceVertexShader>()->GetLayout(geom->mFormat, dev);
		ctx->IASetInputLayout(l);
	}
	class DX11DevicePixelShader : public IRenderDeviceResource
	{
	public:
		DX11DevicePixelShader(ID3D11Device* dev, const RendererShader& shader)
		{
			HRESULT hr;
			ID3DBlob* blob;
			ID3DBlob* errMsg;
			hr = D3DCompile(
				shader.mShaderCode->data(), shader.mShaderCode->size(),
				shader.mPath.c_str(), nullptr, nullptr, shader.mEntry.c_str(), "ps_5_0",
				D3D10_SHADER_DEBUG | D3D10_SHADER_SKIP_OPTIMIZATION,
				0, &blob, &errMsg);
			if (errMsg)
				Check(!errMsg, errMsg->GetBufferPointer());
			ID3D11PixelShader* ps;
			hr = dev->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &ps);
			Check(SUCCEEDED(hr));
			mShader = ps;
			ReleaseCOM(blob);
		}
	public:
		COMRef<ID3D11PixelShader> mShader;
	};
	template<EPipelineStage stage>
	inline void ApplyShader(ID3D11Device* dev, ID3D11DeviceContext* ctx, RendererShader& res)
	{
		if constexpr(stage == EPipelineStage::VSStage)
		{
			if (!res.mDeviceShader)
			{
				res.mDeviceShader = new DX11DeviceVertexShader(dev, res);
				res.mShaderCode = nullptr;
			}
			auto v = res.mDeviceShader.GetPtr<DX11DeviceVertexShader>();
			ctx->VSSetShader(v->mShader.GetPtr(), nullptr, 0);
		}
		else if constexpr(stage == EPipelineStage::PSStage)
		{
			if (!res.mDeviceShader)
			{
				res.mDeviceShader = new DX11DevicePixelShader(dev, res);
				res.mShaderCode = nullptr;
			}
			auto v = res.mDeviceShader.GetPtr<DX11DevicePixelShader>();
			ctx->PSSetShader(v->mShader.GetPtr(), nullptr, 0);
		}
		else
		{
			Check(false, "ApplyShader: unknown stage");
		}
	}
	class DX11DeviceConstantBuffer : public IRenderDeviceResource
	{
	public:
		DX11DeviceConstantBuffer(ID3D11Device* dev, const RendererConstantBuffer& c)
		{
			D3D11_BUFFER_DESC cbd;
			cbd.ByteWidth = uint(c.mSize);
			cbd.Usage = D3D11_USAGE_DYNAMIC;
			cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			cbd.MiscFlags = 0;
			cbd.StructureByteStride = 0;
			D3D11_SUBRESOURCE_DATA data;
			data.pSysMem = c.mNewData->data();
			data.SysMemPitch = 0;
			data.SysMemSlicePitch = 0;
			ID3D11Buffer* buf;
			auto hr = dev->CreateBuffer(&cbd, &data, &buf);
			Check(SUCCEEDED(hr));
			mBuffer = buf;
		}
		void Update(ID3D11DeviceContext* ctx, RendererConstantBuffer& c)
		{
			if (c.mNewData)
			{
				D3D11_MAPPED_SUBRESOURCE res;
				auto buf = mBuffer.GetPtr();
				auto hr = ctx->Map(buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &res);
				Check(SUCCEEDED(hr));
				Copy(c.mNewData->data(), res.pData, c.mNewData->size());
				ctx->Unmap(buf, 0);
			}
		}
	public:
		COMRef<ID3D11Buffer> mBuffer;
	};
	template<EPipelineStage stage>
	inline void ApplyConstant(ID3D11Device* dev, ID3D11DeviceContext* ctx, RendererConstantBuffer& res, uint slot)
	{
		if (!res.mDeviceConstantBuffer)
		{
			res.mDeviceConstantBuffer = new DX11DeviceConstantBuffer(dev, res);
			res.mNewData = nullptr;
		}
		else
		{
			if (res.mNewData)
			{
				auto v = res.mDeviceConstantBuffer.GetPtr<DX11DeviceConstantBuffer>();
				v->Update(ctx, res);
				res.mNewData = nullptr;
			}
		}
		auto v = res.mDeviceConstantBuffer.GetPtr<DX11DeviceConstantBuffer>();
		switch (stage)
		{
		case VSStage:
			ctx->VSSetConstantBuffers(slot, 1, &v->mBuffer);
			break;
		case PSStage:
			ctx->PSSetConstantBuffers(slot, 1, &v->mBuffer);
			break;
		default:
			Check(false, "unknown stage");
			break;
		}
	}
	class DX11RenderTarget : public IRenderDeviceResource
	{
	public:
		DX11RenderTarget(ID3D11Device* dev, RendererRenderTarget& res)
		{
			ID3D11Texture2D* texture;
			HRESULT hr;
			D3D11_TEXTURE2D_DESC td;
			td.Width = res.mSize.x;
			td.Height = res.mSize.y;
			td.MipLevels = 1;
			td.ArraySize = 1;
			td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			td.SampleDesc.Count = 1;
			td.SampleDesc.Quality = 0;
			td.Usage = D3D11_USAGE_DEFAULT;
			td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			td.CPUAccessFlags = 0;
			td.MiscFlags = 0;
			hr = dev->CreateTexture2D(&td, nullptr, &texture);
			Check(SUCCEEDED(hr));
		}
	public:
		COMRef<ID3D11RenderTargetView> mRenderTargetView;
		COMRef<ID3D11ShaderResourceView> mShaderResourceView;
	};
	inline void ApplyRenderTarget(ID3D11Device* dev, ID3D11DeviceContext* ctx, ID3D11DepthStencilView* dsv, RendererRenderTarget& res)
	{
		if (!res.mDeviceRenderTarget)
			res.mDeviceRenderTarget = new DX11RenderTarget(dev, res);
		auto v = res.mDeviceRenderTarget.GetPtr<DX11RenderTarget>();
		ctx->OMSetRenderTargets(1, &v->mRenderTargetView, dsv);
	}
	class DX11SamplerState : public IRenderDeviceResource
	{
	public:
		DX11SamplerState(ID3D11Device* dev, RendererSamplerState& res)
		{
			D3D11_SAMPLER_DESC sdesc;
			ZeroFill(sdesc);
			sdesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			sdesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
			sdesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
			sdesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
			sdesc.MipLODBias = 0;
			auto hr = dev->CreateSamplerState(&sdesc, &mSamplerState);
			Check(SUCCEEDED(hr));
		}
	public:
		ID3D11SamplerState* mSamplerState;
	};
	template<EPipelineStage stage>
	inline void ApplySamplerState(ID3D11Device* dev, ID3D11DeviceContext* ctx, RendererSamplerState& res, uint slot)
	{
		if (!res.mDeviceSamplerState)
			res.mDeviceSamplerState = new DX11SamplerState(dev, res);
		auto v = res.mDeviceSamplerState.GetPtr<DX11SamplerState>();
		switch (stage)
		{
		case VSStage:
			ctx->VSSetSamplers(slot, 1, &v->mSamplerState);
			break;
		case PSStage:
			ctx->PSSetSamplers(slot, 1, &v->mSamplerState);
			break;
		default:
			Check(false, "unknown stage");
			break;
		}
	}
}