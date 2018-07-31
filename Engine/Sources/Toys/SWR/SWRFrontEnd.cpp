#pragma once
#include "Yes.h"
#include "SWR.h"
#include "SWRCommon.h"
#include "SWRFrontEnd.h"
#include "SWRTiles.h"
#include "SWRShader.h"
#include "SWRPipelineState.h"


#include "Misc/Container.h"

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
		auto buf = imb->GetData();
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