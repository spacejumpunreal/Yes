#include "Public/Graphics/Camera/Camera.h"

#include <cstring>
namespace Yes
{
	M44F CalcViewMatrix(float pitch, float yaw, V3F pos)
	{
		/*
			Pv * Axisv = Pw * Identity
			=>
			Pv = Pw * inv(Axisv)

			Axisv = RotateX * RotateY * Translation
			inv(Axisv) = inv(Translation) * inv(RotateY) * inv(RotateX)
		*/
		auto m =
			M44F::Translate(V3F(-pos.x, -pos.y, -pos.z)) *
			M44F::RotateY(-yaw) *
			M44F::RotateX(-pitch);
		return m;
	}
	void BuildAxis(float pitch, float yaw, V4F xyz[3])
	{
		/*
			rotate around X first
			then rotate around Y
		*/
		M44F m = M44F::RotateX(pitch) * M44F::RotateY(yaw);
		std::memcpy(xyz, &m, sizeof(V4F[3]));
	}
	M44F CalcPerspectiveMatrix(float fovYAngle, float aspect, float zNear, float zFar)
	{
		/*
			fovYAngle is full(not half) view angle in x=0 plane
			aspect is CameraWidth/CameraHeight

			2 phase:
			1. map from perspective to non-NDC orthogonal: near plane will not change, apply perspective effect, result is a AABB
			2. map from non-NDC orthogonal to NDC
		*/
		auto r = M44F::Identity(0);
		float yScale = Cot(fovYAngle / 2); // 1/tan(fovY/2),  fovY is an angle unit is radians
		float xScale = yScale / aspect;
		r.m[0][0] = xScale;
		r.m[1][1] = yScale;
		r.m[2][2] = zFar / (zFar - zNear);
		r.m[2][3] = 1.0f;
		r.m[3][2] = -zFar * zNear / (zFar - zNear);
		return r;
	}
	M44F CalcOrthogonalMatrix(float screenHeight, float aspect, float zNear, float zFar)
	{
		/*
			w = screenHeight * aspect
			h = screenHeight
			map [(-w/2, -h/2, zNear), (w/2, h/2, zFar)] to [(-1,-1,0), (1,1,1)]

			Translate(0,0,-zNear) * Scale(2/w, 2/h, 1/(zFar-zNear))
		*/
		M44F r = M44F::Empty();
		float h = screenHeight;
		float w = aspect * h;
		r.m[0][0] = 2.0f / w;
		r.m[1][1] = 2.0f / h;
		r.m[2][2] = 1.0f / (zFar - zNear);
		r.m[3][2] = -zNear / (zFar - zNear);
		r.m[3][3] = 1;
		return r;
	}
	void PerspectiveCamera::Update(float pitch, float yaw, V3F & position)
	{
		MView = CalcViewMatrix(pitch, yaw, position);
		MVP = MView * MPerspective;
	}
	PerspectiveCamera::PerspectiveCamera(float fovY, float aspect, float near, float far)
	{
		MPerspective = CalcPerspectiveMatrix(fovY, aspect, near, far);
		V4F testPoint0 = V4F{ near, near, near , 1};
		V4F testPoint1 = V4F{ far, far, far, 1 };
		MView = M44F::Identity();
		MVP = MView * MPerspective;
		/*
		auto testResult0 = testPoint0 * MPerspective;
		auto testResult1 = testPoint1 * MPerspective;
		testResult0 = testResult1;
		*/
	}
	bool PerspectiveCamera::IsPerspecive()
	{
		return
			MPerspective.m[2][2] == 1 &&
			MPerspective.m[2][3] == 0 &&
			MPerspective.m[3][2] == 0 &&
			MPerspective.m[3][3] == 1;
	}
}