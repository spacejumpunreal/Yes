#include "Runtime/Public/Graphics/MeshUtil.h"
#include "Runtime/Public/Misc/Math.h"
#include "Runtime/Public/Misc/Container.h"
#include <unordered_map>

namespace Yes
{
	struct P3FN3FT2F
	{
		V3F Position;
		V3F Normal;
		V2F Texcoord;
		P3FN3FT2F(const V3F& p, const V3F& n, const V2F& t)
			: Position(p)
			, Normal(n)
			, Texcoord(t)
		{}
	};
	void CreateUnitSphereMesh(SharedBufferRef& vb, SharedBufferRef& ib, int longitudeCount, int latitudeCount)
	{
		int vCount = 0;
		{//add polars
			std::vector<P3FN3FT2F> vertices;
			vertices.emplace_back(V3F(0, 1, 0), V3F(0, 1, 0), V2F(0, 0));
			//add the rest vertices
			for (int latitude = 0; latitude < latitudeCount; ++latitude)
			{
				float theta = PI / (latitudeCount + 1) * (latitude + 1);
				for (int longitude = 0; longitude < longitudeCount; ++longitude)
				{
					float phi = 2 * PI / longitudeCount * longitude;
					float sinTheta = Sin(theta);
					V3F v{ sinTheta * Cos(phi), Cos(theta), sinTheta * Sin(phi) };
					vertices.emplace_back(v, v, V2F(phi / (2 * PI), theta / PI));
				}
			}
			vertices.emplace_back(V3F(0, -1, 0), V3F(0, -1, 0), V2F(0, 1));
			vb = new ArraySharedBuffer(sizeof(P3FN3FT2F) * vertices.size(), vertices.data());
			vCount = (int)vertices.size();
		}
		{//top fans
			std::vector<int32> indices;
			for (int longitude = 0; longitude < longitudeCount; ++longitude)
			{//counter clockwise
				indices.push_back(0);
				indices.push_back(longitude + 1);
				indices.push_back((longitude + 2) % longitudeCount);
			}
			//middle strips
			for (int latitude = 0; latitude < latitudeCount; ++latitude)
			{
				int latitudeStart = 1 + latitude * longitudeCount;
				for (int longitude = 0; longitude < longitudeCount; ++longitude)
				{
					int bottomLeftIndex = latitudeStart + longitude;
					int topLeftIndex = bottomLeftIndex - longitudeCount;
					indices.push_back(topLeftIndex);
					indices.push_back(bottomLeftIndex);
					indices.push_back(bottomLeftIndex + 1);
					indices.push_back(bottomLeftIndex + 1);
					indices.push_back(topLeftIndex + 1);
					indices.push_back(topLeftIndex);
				}
			}
			//bottom fans
			int lastVerticeIndex = vCount - 1;
			int lastLatitudeStart = lastVerticeIndex - longitudeCount;
			for (int longitude = 0; longitude < longitudeCount; ++longitude)
			{//counter clockwise
				indices.push_back(0);
				indices.push_back(longitude + lastLatitudeStart);
				indices.push_back((longitude + lastLatitudeStart + 1) % longitudeCount);
			}
			ib = new ArraySharedBuffer(sizeof(int32) * indices.size(), indices.data());
		}
	}
}