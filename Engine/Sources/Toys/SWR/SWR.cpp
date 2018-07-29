#include "Yes.h"
#include "SWR.h"
#include "SWRCommon.h"
#include "SWRTexture.h"
#include "SWRShader.h"
#include "SWRPipelineState.h"
#include "SWRFrontEnd.h"
#include "SWRBackEnd.h"
#include "SWRJob.h"

#include "Misc/Container.h"
#include "Misc/Time.h"
#include "Misc/Sync.h"

#include <cstdio>


namespace Yes::SWR
{
	struct SWRDeviceImp : public SWRDevice
	{
	public:
		//0 common
		virtual SWRHandle CreateBuffer(const BufferDesc* desc)
		{
			return new ArraySharedBuffer(desc->Size, desc->InitialData);
		}
		virtual SWRHandle CreateTexture2D(const Texture2DDesc* desc)
		{
			return CreateSWRTexture2D(desc);
		}
		virtual SWRHandle CreateRenderTarget(const Texture2DDesc* desc)
		{
			return CreateSWRTexture2D(desc);
		}
		virtual SWRHandle CreateDepthStencil(const Texture2DDesc* desc)
		{
			CheckAlways(desc->PixelFormat == PixelFormat::Color1F, "only support 1F depth");
			return CreateSWRTexture2D(desc);
		}
		virtual SWRHandle CreateVertexShader(const VertexShaderDesc* desc)
		{
			return CreateSWRVertexShader(desc);
		}
		virtual SWRHandle CreatePixelShader(const PixelShaderDesc* desc)
		{
			return CreateSWRPixelShader(desc);
		}
		//1 IA
		virtual void IASetVertexBuffer(SWRHandle vb)
		{
			_FlushStateIfInUse();
			mCurrentState->VertexBuffer = vb.GetPtr();
		}
		//2 VS
		virtual void VSSetConstant(size_t slot, SWRHandle cb)
		{
			_FlushStateIfInUse();
			mCurrentState->VSConstants[slot] = cb.GetPtr();
		}
		virtual void VSSetTexture(size_t slot, SWRHandle tex)
		{
			_FlushStateIfInUse();
			mCurrentState->VSTextures[slot] = tex.GetPtr();
		}
		virtual void VSSetShader(SWRHandle vs)
		{
			_FlushStateIfInUse();
			mCurrentState->VSShader = vs;
		}
		//3 RS
		virtual void RSSetRasterizationState(SWRHandle rsstate){}
		//4 PS
		virtual void PSSetConstant(size_t slot, SWRHandle cb)
		{
			_FlushStateIfInUse();
			mCurrentState->PSConstants[slot] = cb.GetPtr();
		}
		virtual void PSSetTexture(size_t slot, SWRHandle tex)
		{
			_FlushStateIfInUse();
			mCurrentState->PSTextures[slot] = tex.GetPtr();
		}
		virtual void PSSetShader(SWRHandle ps)
		{
			_FlushStateIfInUse();
			mCurrentState->PSShader = ps;
		}
		//5 OM
		virtual void OMSetRenderTarget(size_t slot, SWRHandle rt)
		{
			_FlushStateIfInUse();
			mCurrentState->OMRenderTargets[slot] = rt.GetPtr();
		}
		virtual void OMSetDepthStencil(SWRHandle ds)
		{
			_FlushStateIfInUse();
			mCurrentState->OMDepthStencil = ds.GetPtr();
		}
		virtual void OMSetOutputMergerState(OutputMergerSignature omstate)
		{
			_FlushStateIfInUse();
			mCurrentState->OMShader = omstate;
		}
		//2. commands
		virtual void Clear(float clearColor[4], float depth)
		{
			mCurrentStateInUse = true;
			//put into queue
		}
		virtual void Draw()
		{
			mCurrentStateInUse = true;
			mDeviceCore.JobSystem->PutBack(SWRFrontEndJob(mCurrentState, &mDeviceCore));
		}
		virtual void Present()
		{
			mPLimit.Wait();
			mDeviceCore.JobSystem->PutBack(SWRPresentJob(&mPLimit));
		}
		//3. state management
		void _FlushStateIfInUse()
		{
			if (mCurrentStateInUse)
			{
				mCurrentState = new SWRPipelineState(*mCurrentState.GetPtr());
				mCurrentStateInUse = false;
			}
		}
		//-1. misc
		SWRDeviceImp(const DeviceDesc* desc)
			: mCurrentState(new SWRPipelineState())
			, mCurrentStateInUse(false)
			//present related
			, mPLimit(2)
		{
			mDeviceCore = DeviceCore
			{ 
				CreateSWRJobSystem(desc),
			};
		}

#pragma region TEST
		void Test(int x)
		{
			if (x < 0)
			{
				mDeviceCore.JobSystem->PutSyncPoint();
			}
			else
			{
				mDeviceCore.JobSystem->PutBack([](int c)
				{
					for (int i = 0; i < c; ++i)
					{
						printf("JobProgress:%d/%d\n", i, c);
						Sleep(TimeDuration(0.5));
					}
				}, x);
			}
		}
#pragma endregion TEST
	protected:
		DeviceCore mDeviceCore;
		// piplinestate related
		TRef<SWRPipelineState> mCurrentState;
		bool mCurrentStateInUse;
		//present related
		Semaphore<> mPLimit;
	};
	SWRDevice* CreateSWRDevice(const DeviceDesc * desc)
	{
		return new SWRDeviceImp(desc);
	}
}