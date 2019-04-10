#pragma once
#include "Yes.h"

#include "Public/Misc/Math.h"

namespace Yes
{
	struct PerspectiveCamera
	{
	public:
		PerspectiveCamera(float fovY, float aspect, float near, float far);
		bool IsPerspecive();
		void Update(float pitch, float yaw, V3F& position);
		const M44F& GetViewMatrix() { return MView; }
		const M44F& GetPerspectiveMatrix() { return MPerspective; }
		const M44F& GetViewPerspectiveMatrix() { return MVP; }
	private:
		M44F MView;
		M44F MPerspective;
		M44F MVP;
	};
	M44F CalcViewMatrix(float pitch, float yaw, V3F pos);
	void BuildAxis(float pitch, float yaw, V4F xyz[3]);
	M44F CalcPerspectiveMatrix(float fovY, float aspect, float zNear, float zFar);
	M44F CalcOrthogonalMatrix(float scaleY, float aspect, float zNear, float zFar);
}