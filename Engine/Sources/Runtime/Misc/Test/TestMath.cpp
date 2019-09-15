#include "Runtime/Public/Yes.h"
#include "Runtime/Public/Misc/Debug.h"
#include "Runtime/Public/Misc/Math.h"
namespace Yes
{
	void TestMath()
	{
		const float Error = 0.001f;
		const M44F Identity = M44F::Identity();
		{
			M44F scale0 = M44F::Scale(V3F(2, 3, 4));
			M44F scale0Inv = scale0.InverseSRT();
			M44F ret = scale0 * scale0Inv - Identity;
			auto err = ret.AbsSumComponents();
			Check(err < Error);
		}
		{
			M44F scale0 = M44F::Translate(V3F(2, 3, 4));
			M44F scale0Inv = scale0.InverseSRT();
			M44F ret = scale0 * scale0Inv - Identity;
			auto err = ret.AbsSumComponents();
			Check(err < Error);
		}
		{
			M44F scale0 = M44F::RotateX(1);
			M44F scale0Inv = scale0.InverseSRT();
			M44F ret = scale0 * scale0Inv - Identity;
			auto err = ret.AbsSumComponents();
			Check(err < Error);
		}
		{
			M44F scale0 = M44F::RotateY(1);
			M44F scale0Inv = scale0.InverseSRT();
			M44F ret = scale0 * scale0Inv - Identity;
			auto err = ret.AbsSumComponents();
			Check(err < Error);
		}
		{
			//M44F scale0 = M44F::Scale(V3F(1, 2, 3)) * M44F::RotateY(2) * M44F::Translate(V3F(2, 4, 6));
			M44F scale0 = M44F::Scale(V3F(10, 20, 30)) * M44F::Translate(V3F(2, 4, 6));
			M44F scale0Inv = scale0.InverseSRT();
			M44F ret = scale0 * scale0Inv - Identity;
			auto err = ret.AbsSumComponents();
			Check(err < Error);
		}
		{
			M44F a = M44F::Scale(V3F(2, 3, 4));
			float d = a.Determinant();
			Check(d == 24);
		}


		{
			M44F scale0 = M44F::Scale(V3F(2, 3, 4));
			M44F scale0Inv = scale0.Inverse();
			M44F ret = scale0 * scale0Inv - Identity;
			auto err = ret.AbsSumComponents();
			Check(err < Error);
		}
		{
			M44F scale0 = M44F::Translate(V3F(2, 3, 4));
			M44F scale0Inv = scale0.Inverse();
			M44F ret = scale0 * scale0Inv - Identity;
			auto err = ret.AbsSumComponents();
			Check(err < Error);
		}
		{
			M44F scale0 = M44F::RotateX(1);
			M44F scale0Inv = scale0.Inverse();
			M44F ret = scale0 * scale0Inv - Identity;
			auto err = ret.AbsSumComponents();
			Check(err < Error);
		}
		{
			M44F scale0 = M44F::RotateY(1);
			M44F scale0Inv = scale0.Inverse();
			M44F ret = scale0 * scale0Inv - Identity;
			auto err = ret.AbsSumComponents();
			Check(err < Error);
		}
		{
			//M44F scale0 = M44F::Scale(V3F(1, 2, 3)) * M44F::RotateY(2) * M44F::Translate(V3F(2, 4, 6));
			M44F scale0 = M44F::Scale(V3F(10, 20, 30)) * M44F::Translate(V3F(2, 4, 6));
			M44F scale0Inv = scale0.Inverse();
			M44F ret = scale0 * scale0Inv - Identity;
			auto err = ret.AbsSumComponents();
			Check(err < Error);
		}
	}
}