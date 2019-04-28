#pragma once
#include "Yes.h"

#include "Public/Misc/Math.h"

namespace Yes
{
	struct Camera
	{
	public:
		static Camera BuildPerspectiveCamera(float fovY, float aspect, float near, float far);
		static Camera BuildOrthogonalCamera(float aspect, float ySize, float near, float far);
		bool IsPerspecive();
		void UpdateView(float pitch, float yaw, V3F& position);
		void UpdateView(const M44F& world2view);
		const M44F& GetViewMatrix() { return MWorld2View; }
		const M44F& GetPerspectiveMatrix() { return MPerspective; }
		const M44F& GetMVPMatrix() { return MVP; }
	private:
		M44F MWorld2View;
		M44F MPerspective;
		M44F MVP;
	};
	M44F CalcWorld2ViewMatrix(float pitch, float yaw, V3F pos);
	void BuildAxis(float pitch, float yaw, V4F xyz[3]);
	M44F CalcPerspectiveMatrix(float fovY, float aspect, float zNear, float zFar);
	M44F CalcOrthogonalMatrix(float scaleY, float aspect, float zNear, float zFar);
}