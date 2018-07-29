#pragma once
#include "Yes.h"
#include "SWR.h"
#include "SWRFrontEnd.h"
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
			vbd += vs->InputStride;
			imbd += vs->OutputStride;
		}
		//2. foreach triangle, discard out of ndc ones, sort it to tiles

	}
}