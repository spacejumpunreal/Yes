#include "SWRTiles.h"
#include "SWRPipelineState.h"
#include "SWRJob.h"
#include "Misc/Math.h"
#include "Misc/Container.h"
#include <algorithm>
#include <deque>
#include <mutex>

namespace Yes::SWR
{
	const int32 SWRTileQueueMaxPendingTriangles = 1024;
	class SWRTileBufferImp;
	class SWRTileQueue
	{
	public:
		//need a lock or something, better if can avoid it
		std::mutex QueueLock;
		std::deque<BinnedTriangles> Queue;
		//some statistics for scheduling
		int32 PendingTriangles;
		bool IsBusy;
	};

	struct SWFlushTileQueueSTask
	{
	public:
		void operator()()
		{
			std::deque<BinnedTriangles> q;
			std::unique_lock<std::mutex> lk(mQueue->QueueLock);
			if (mQueue->IsBusy)
			{
				return;
			}
			else
			{
				bool qEmpty;
				do
				{
					mQueue->IsBusy = true;
					std::swap(q, mQueue->Queue);
					lk.unlock();
					//go through queue
					qEmpty = q.empty();
					q.clear();
				} while (!qEmpty);
				

			}
		}
	public:
		SWRTileBufferImp* mTiles;
		SWRTileQueue* mQueue;
	};

	class SWRTileBufferImp : public SWRTileBuffer
	{
	public:
		SWRTileBufferImp(const DeviceDesc* desc, SWRJobSystem* jobSystem)
			: mTileCountU((int32)desc->TileCountU)
			, mTileCountV((int32)desc->TileCountV)
			, mQueues(new SWRTileQueue[mTileCountU * mTileCountV])
			, mJobSystem(jobSystem)
		{
			for (int32 i = 0; i < mTileCountU * mTileCountV; ++i)
			{
				mQueues[i].IsBusy = false;
			}
		}
		~SWRTileBufferImp()
		{
			delete[] mQueues;
		}
		void BinTriangles(TRef<ISharedBuffer>& imb, TRef<SWRPipelineState>& ps, size_t stride) override
		{
			auto d = (uint8*)imb->GetData();
			auto ed = d + imb->GetSize();
			auto nTriangles = (uint16)((ed - d) / (stride * 3));
			//TODO:use some sort of static thread local storage?
			//each tile may have a triangle index buffer(contain triangle indexes that belong to this tile)
			auto p2v = new VectorSharedBuffer<uint16>*[mTileCountU * mTileCountV];
			auto fp = (float*)d;
			for (uint16 tri = 0; tri < nTriangles; ++tri)
			{
				V2F pos[3];
				for (int i = 0; i < 3; ++i)
				{
					pos[i].x = fp[0];
					pos[i].y = fp[2];
					fp += stride;
				}
				float minU = pos[0].x;
				float maxU = pos[0].x;
				float minV = pos[0].y;
				float maxV = pos[0].y;
				for (int i = 1; i < 3; ++i)
				{
					minU = std::min(minU, pos[i].x);
					maxU = std::max(maxU, pos[i].x);
					minV = std::min(minV, pos[i].y);
					maxV = std::max(maxV, pos[i].y);
				}
				//min max UV to min max tile index
				int32 startTileU = std::max(0, (int32)(minU * mTileCountU));
				int32 endTileU = std::min(mTileCountU - 1, (int32)(maxU * mTileCountU));
				int32 startTileV = std::max(0, (int32)(minV * mTileCountV));
				int32 endTileV = std::min(mTileCountV - 1, (int32)(maxV * mTileCountV));
				for (int32 v = startTileV; v <= endTileV; ++v)
				{
					for (int32 u = startTileU; u <= endTileU; ++u)
					{
						//TODO: check if this tile is completely outside the triangle: yet it won't be that bad if we keep it here
						auto vp = p2v[u + v * mTileCountU];
						if (!vp)
						{
							vp = new VectorSharedBuffer<uint16>(0);
							p2v[u + v * mTileCountU] = vp;
						}
						vp->push_back(tri);
					}
				}
			}
			for (int tile = 0; tile < mTileCountU * mTileCountV; ++tile)
			{
				if (p2v[tile])
				{
					std::lock_guard<std::mutex> lk(mQueues[tile].QueueLock);
					mQueues[tile].Queue.emplace_back(imb, ps, p2v[tile], (int32)stride);
					mQueues[tile].PendingTriangles += (int32)p2v[tile]->GetCount();
					bool flushQueue = false;
					if (mQueues[tile].PendingTriangles > SWRTileQueueMaxPendingTriangles)
					{
						mQueues[tile].PendingTriangles = 0;
						flushQueue = true;
					}
					if (flushQueue)
					{
						mJobSystem->PutFront(SWFlushTileQueueSTask{ this, &mQueues[tile] });
					}
				}
			}
			delete p2v;
		}
	protected:
		int32 mTileCountU;
		int32 mTileCountV;
		int32 mTileSize;
		SWRTileQueue* mQueues;
		SWRJobSystem* mJobSystem;

		friend class SWRRSTask;
	};
	SWRTileBuffer * CreateSWRTileBuffer(const DeviceDesc * desc, SWRJobSystem* jobSystem)
	{
		return new SWRTileBufferImp(desc, jobSystem);
	}
}