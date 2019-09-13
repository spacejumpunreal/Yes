#pragma once
#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Misc/Container.h"
#include "Toys/SWR/Public/SWR.h"
#include "Toys/SWR/SWRCommon.h"
#include "Toys/SWR/SWRFrontEnd.h"
#include "Toys/SWR/SWRTiles.h"
#include "Toys/SWR/SWRShader.h"
#include "Toys/SWR/SWRPipelineState.h"




namespace Yes::SWR
{
	void SWRFrontEndJob::operator()()
	{
		auto vs = dynamic_cast<SWRVertexShader*>(mState->VSShader.GetPtr());
		//0. allocate intermediate buffer
		auto vb = mState->VertexBuffer.GetPtr();
		auto vbSize = mState->VertexBuffer->GetSize();
		auto vCount = vbSize / vs->InputStride;
		auto imbSize = vCount * vs->OutputStride;
		//TODO: split this buffer to make this parallel
		TRef<ISharedBuffer> imb(new ArraySharedBuffer(imbSize));
		//auto buf = imb->GetData();
		//1. translate vertex buffer using vertex shader
		auto vbd = (uint8*)vb->GetData();
		auto imbd = (uint8*)imb->GetData();
		for (int i = 0; i < vCount; ++i)
		{
			vs->Function(vbd, imbd);
			//do the divide by w thing
			auto pos = (float*)imbd;
			pos[0] /= pos[4];
			pos[1] /= pos[4];
			pos[2] /= pos[4];
			vbd += vs->InputStride;
			imbd += vs->OutputStride;
		}
		//2. foreach triangle, discard out of ndc ones, sort it to tiles
		mDeviceCore->TileBuffer->BinTriangles(imb, mState, vs->OutputStride);
	}
}