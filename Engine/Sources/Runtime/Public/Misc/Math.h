#pragma once
#include "Yes.h"
#include <cmath>

namespace Yes
{
	// constants
	const float PI = 3.14159f;
	const float HalfPI = PI / 2.0f;


	// miscellaneous functions
	template<typename T>
	T Abs(const T x)
	{
		if (x < 0)
			return -x;
		return x;
	}
#define BODY_FOR_FUNCTION(funcname)\
	if constexpr (std::is_same_v<T, float>)\
	{\
		return std::funcname##f(x);\
	}\
	if constexpr (std::is_same_v<T, double>)\
	{\
		return std::funcname(x);\
	}
	template<typename T>
	inline T Cos(T x)
	{
		BODY_FOR_FUNCTION(cos)
	}
	template<typename T>
	inline T Sin(T x)
	{
		BODY_FOR_FUNCTION(sin)
	}
	template<typename T>
	inline T Tan(T x)
	{
		BODY_FOR_FUNCTION(tan)
	}
	template<typename T>
	inline T Cot(T x)
	{
		return 1.0f / Tan(x);
	}
#undef BODY_FOR_FUNCTION

	template<typename T> struct Vector3;

	// Geometry types
	template<typename T>
	struct Vector4
	{
		T x, y, z, w;
		Vector3<T> xyz()
		{
			return Vector3<T>(x, y, z);
		}
		Vector4()
		{}
		Vector4(const Vector4& a)
			: x(a.x)
			, y(a.y)
			, z(a.z)
			, w(a.w)
		{}
		Vector4(T x, T y, T z, T w)
			: x(x)
			, y(y)
			, z(z)
			, w(w)
		{}
		explicit Vector4(T a)
			: x(a)
			, y(a)
			, z(a)
			, w(a)
		{}
		T* Data()
		{
			return &x;
		}
		bool HasNaNs() const
		{
			return std::isnan(x) || std::isnan(y) || std::isnan(z) || std::isnan(w);
		}
		Vector4<T>& operator+=(const Vector4<T>& r)
		{
			x += r.x;
			y += r.y;
			z += r.z;
			w += r.w;
			return *this;
		}
		friend Vector4<T> operator+(const Vector4<T>& l, const Vector4<T>& r)
		{
			return Vector4<T>(l.x + r.x, l.y + r.y, l.z + r.z, l.w + r.w);
		}
		friend Vector4<T> operator-(const Vector4<T>& l, const Vector4<T>& r)
		{
			return Vector4<T>(l.x - r.x, l.y - r.y, l.z - r.z, l.w - r.w);
		}
		friend Vector4<T> operator*(const Vector4<T>& a, T b)
		{
			return Vector4<T>(a.x * b, a.y * b, a.z * b, w * b);
		}
		friend Vector4<T> operator*(T b, const Vector4<T>& a)
		{
			return Vector4<T>(a.x * b, a.y * b, a.z * b, a.w * b);
		}
		friend T Dot(const Vector4<T>& l, const Vector4<T>& r)
		{
			return l.x * r.x + l.y * r.y + l.z * r.z + l.w * r.w;
		}
		T SquareLength() const
		{
			return x * x + y * y + z * z + w * w;
		}
		T Length() const
		{
			return std::sqrt(SquareLength());
		}
		Vector4<T>& Normalize()
		{
			T rl = 1 / this->Length();
			x *= rl;
			y *= rl;
			z *= rl;
			w *= rl;
		}
		Vector4<T> Normalized() const
		{
			T rl = 1 / this->Length();
			return Vector4<T>(x * rl, y * rl, z * rl, w * rl);
		}
	};
	typedef Vector4<float> V4F;
	typedef Vector4<int32> V4I;
	static_assert(sizeof(V4F) == 16, "sizeof(V4F) == 16");
	static_assert(sizeof(V4I) == 16, "sizeof(V4I) == 16");

	template<typename T>
	struct Vector3
	{
		T x, y, z;
		Vector3()
		{}
		Vector3(const Vector3& a)
			: x(a.x)
			, y(a.y)
			, z(a.z)
		{}
		Vector3(T x, T y, T z)
			: x(x)
			, y(y)
			, z(z)
		{}
		explicit Vector3(T a)
			: x(a)
			, y(a)
			, z(a)
		{}
		const T* Data() const
		{
			return &x;
		}
		bool HasNaNs() const
		{
			return std::isnan(x) || std::isnan(y) || std::isnan(z);
		}
		Vector3& operator=(const Vector3<T>& other)
		{
			x = other.x;
			y = other.y;
			z = other.z;
			return *this;
		}
		friend Vector3<T> operator+(const Vector3<T>& l, const Vector3<T>& r)
		{
			return Vector3<T>(l.x + r.x, l.y + r.y, l.z + r.z);
		}
		friend Vector3<T> operator-(const Vector3<T>& v)
		{
			return Vector3<T>(-v.x, -v.y, -v.z);
		}
		Vector3<T>& operator+=(const Vector3<T>& r)
		{
			x += r.x;
			y += r.y;
			z += r.z;
			return *this;
		}
		friend Vector3<T> operator-(const Vector3<T>& l, const Vector3<T>& r)
		{
			return Vector3<T>(l.x - r.x, l.y - r.y, l.z - r.z);
		}
		Vector3<T> operator-=(const Vector3<T>& r)
		{
			x -= r.x;
			y -= r.y;
			z -= r.z;
			return *this;
		}
		friend Vector3<T> operator*(const Vector3<T>& a, T b)
		{
			return Vector3<T>(a.x * b, a.y * b, a.z * b);
		}
		friend Vector3<T> operator*(T b, const Vector3<T>& a)
		{
			return Vector3<T>(a.x * b, a.y * b, a.z * b);
		}
		friend T Dot(const Vector3<T>& l, const Vector3<T>& r)
		{
			return l.x * r.x + l.y * r.y + l.z * r.z;
		}
		friend Vector3<T> Cross(const Vector3<T>& v1, const Vector3<T>& v2)
		{
			double v1x = v1.x, v1y = v1.y, v1z = v1.z;
			double v2x = v2.x, v2y = v2.y, v2z = v2.z;
			return Vector3<T>(T((v1y * v2z) - (v1z * v2y)), T((v1z * v2x) - (v1x * v2z)),
				T((v1x * v2y) - (v1y * v2x)));
		}
		T SquareLength() const
		{
			return x * x + y * y + z * z;
		}
		T Length() const
		{
			return std::sqrt(SquareLength());
		}
		Vector3<T>& Normalize()
		{
			T rl = 1 / this->Length();
			x *= rl;
			y *= rl;
			z *= rl;
		}
		Vector3<T> Normalized() const
		{
			T rl = 1 / this->Length();
			return Vector3<T>(x * rl, y * rl, z * rl);
		}
	};
	typedef Vector3<float> V3F;
	typedef Vector3<int32> V3I;
	static_assert(sizeof(V3F) == 12, "sizeof(V3F) == 12");
	static_assert(sizeof(V3I) == 12, "sizeof(V3I) == 12");

	template<typename T>
	struct Vector2
	{
		T x, y;
		Vector2()
		{}
		Vector2(const Vector2& a)
			: x(a.x)
			, y(a.y)
		{}
		Vector2(T x, T y)
			: x(x)
			, y(y)
		{}
		explicit Vector2(T a)
			: x(a)
			, y(a)
		{}
		T* Data()
		{
			return &x;
		}
		bool HasNaNs() const
		{
			return std::isnan(x) || std::isnan(y);
		}
		friend bool operator==(const Vector2<T>&l, const Vector2<T>& r)
		{
			return l.x == r.x && l.y == r.y;
		}
		friend bool operator!=(const Vector2<T>&l, const Vector2<T>& r)
		{
			return !(l == r);
		}
		friend Vector2<T> operator+(const Vector2<T>& l, const Vector2<T>& r)
		{
			return Vector2<T>(l.x + r.x, l.y + r.y);
		}
		Vector2<T>& operator+=(const Vector2<T>& r) const
		{
			return Vector2<T>(x + r.x, y + r.y);
		}
		friend Vector2<T> operator-(const Vector2<T>& l, const Vector2<T>& r)
		{
			return Vector2<T>(l.x - r.x, l.y - r.y);
		}
		Vector2<T> operator-=(const Vector2<T>& r) const
		{
			return Vector2<T>(x - r.x, y - r.y);
		}
		friend Vector2<T> operator*(const Vector2<T>& a, T b)
		{
			return Vector2<T>(a.x * b, a.y * b);
		}
		friend Vector2<T> operator*(T b, const Vector2<T>& a)
		{
			return Vector2<T>(a.x * b, a.y * b);
		}
		friend T Dot(const Vector2<T>& l, const Vector2<T>& r)
		{
			return l.x * r.x + l.y * r.y;
		}
		T SquareLength() const
		{
			return x * x + y * y;
		}
		T Length() const
		{
			return std::sqrt(SquareLength());
		}
		Vector2<T>& Normalize()
		{
			T rl = 1 / this->Length();
			x *= rl;
			y *= rl;
		}
		Vector2<T> Normalized() const
		{
			T rl = 1 / this->Length();
			return Vector2<T>(x * rl, y * rl);
		}
	};
	typedef Vector2<float> V2F;
	typedef Vector2<int32> V2I;
	typedef Vector2<uint32> V2U;
	static_assert(sizeof(V2F) == 8, "sizeof(V2F) == 8");
	static_assert(sizeof(V2I) == 8, "sizeof(V2I) == 8");

	template<typename T>
	struct Matrix4x4
	{
		union
		{
			T s[16];
			T m[4][4];
			struct
			{
				T row0[4];
				T row1[4];
				T row2[4];
				T row3[4];
			};
		};
		Vector3<T>& Translation()
		{
			return *(Vector3<T>*)&m[3][0];
		}
		void Fill(T a)
		{
			for (int i = 0; i < 16; i++)
				s[i] = a;
		}
		T Determinant3x3()
		{
			T ret = 0;
			ret += m[0][0] * (m[1][1] * m[2][2] - m[2][1] * m[1][2]);
			ret -= m[1][0] * (m[0][1] * m[2][2] - m[0][2] * m[2][1]);
			ret += m[2][0] * (m[0][1] * m[1][2] - m[0][2] * m[1][1]);
			return ret;
		}
		T SumComponents()
		{
			T ret = 0;
			for (int i = 0; i < 16; ++i)
			{
				ret += s[i];
			}
			return ret;
		}
		T AbsSumComponents()
		{
			T ret = 0;
			for (int i = 0; i < 16; ++i)
			{
				ret += std::abs(s[i]);
			}
			return ret;
		}
		Matrix4x4 Inverse3x3()
		{
			Matrix4x4 inv;
			T invDet = 1.0f / Determinant3x3();
			for (int i = 0; i < 3; ++i)
			{
				inv.m[i][3] = inv.m[3][i] = 0;
			}
			inv.m[3][3] = 1;

			inv.m[0][0] = +invDet * (m[1][1] * m[2][2] - m[1][2] * m[2][1]);
			inv.m[0][1] = -invDet * (m[0][1] * m[2][2] - m[2][1] * m[0][2]);
			inv.m[0][2] = +invDet * (m[0][1] * m[1][2] - m[1][1] * m[0][2]);

			inv.m[1][0] = -invDet * (m[1][0] * m[2][2] - m[2][0] * m[1][2]);
			inv.m[1][1] = +invDet * (m[0][0] * m[2][2] - m[2][0] * m[0][2]);
			inv.m[1][2] = -invDet * (m[0][0] * m[1][2] - m[1][0] * m[0][2]);

			inv.m[2][0] = +invDet * (m[1][0] * m[2][1] - m[2][0] * m[1][1]);
			inv.m[2][1] = -invDet * (m[0][0] * m[2][1] - m[2][0] * m[0][1]);
			inv.m[2][2] = +invDet * (m[0][0] * m[1][1] - m[1][0] * m[0][1]);
			return inv;
		}
		Matrix4x4 InverseSRT()
		{
			Matrix4x4 cp = *this;
			auto translation = -cp.Translation();
			cp.m[3][0] = 0;
			cp.m[3][1] = 0;
			cp.m[3][2] = 0;
			Matrix4x4 r = cp.Inverse3x3();
			return Matrix4x4::Translate(translation) * r;
		}
		static Matrix4x4 Identity(T a = 1)
		{
			Matrix4x4 r;
			r.m[0][0] = a; r.m[0][1] = 0.0f; r.m[0][2] = 0.0f; r.m[0][3] = 0.0f;
			r.m[1][0] = 0.0f; r.m[1][1] = a; r.m[1][2] = 0.0f; r.m[1][3] = 0.0f;
			r.m[2][0] = 0.0f; r.m[2][1] = 0.0f; r.m[2][2] = a; r.m[2][3] = 0.0f;
			r.m[3][0] = 0.0f; r.m[3][1] = 0.0f; r.m[3][2] = 0.0f; r.m[3][3] = a;
			return r;
		}
		static Matrix4x4 Scale(const Vector3<T>& scale)
		{
			Matrix4x4 r;
			r.m[0][0] = scale.x; r.m[0][1] = 0.0f; r.m[0][2] = 0.0f; r.m[0][3] = 0.0f;
			r.m[1][0] = 0.0f; r.m[1][1] = scale.y; r.m[1][2] = 0.0f; r.m[1][3] = 0.0f;
			r.m[2][0] = 0.0f; r.m[2][1] = 0.0f; r.m[2][2] = scale.z; r.m[2][3] = 0.0f;
			r.m[3][0] = 0.0f; r.m[3][1] = 0.0f; r.m[3][2] = 0.0f; r.m[3][3] = 1.0f;
			return r;
		}
		static Matrix4x4 Empty()
		{
			return Identity(0);
		}
		static Matrix4x4 RotateY(T yaw)
		{
			auto r = Identity();
			auto cos = Cos(yaw);
			auto sin = Sin(yaw);
			r.m[0][0] = cos;
			r.m[0][2] = -sin;
			r.m[2][0] = sin;
			r.m[2][2] = cos;
			return r;
		}
		static Matrix4x4 RotateX(T pitch)
		{
			auto r = Identity();
			T cos = Cos<T>(pitch);
			T sin = Sin<T>(pitch);
			r.m[1][1] = cos;
			r.m[1][2] = -sin;
			r.m[2][1] = sin;
			r.m[2][2] = cos;
			return r;
		}
		static Matrix4x4 Translate(Vector3<T> position)
		{
			Matrix4x4 r = Identity();
			r.Translation() = position;
			return r;
		}
		static Matrix4x4 LookAt(const Vector3<T>& forward, const Vector3<T>& up, const Vector3<T>& position)
		{
			Vector3<T> right = Cross(up, forward);
			Vector3<T> y = Cross(forward, right);
			Matrix4x4 r;
			r.m[0][0] = right.Data()[0]; r.m[0][1] = right.Data()[1]; r.m[0][2] = right.Data()[2]; r.m[0][3] = position.x;
			r.m[1][0] = y.Data()[0]; r.m[1][1] = y.Data()[1]; r.m[1][2] = y.Data()[2]; r.m[1][3] = position.y;
			r.m[2][0] = forward.Data()[0]; r.m[2][1] = forward.Data()[1]; r.m[2][2] = forward.Data()[2]; r.m[2][3] = position.z;
			r.m[3][0] = r.m[3][1] = r.m[3][2] = 0; r.m[3][3] = 1;
			return r;
		}
		friend Matrix4x4 operator*(const Matrix4x4& l, const Matrix4x4& r)
		{
			Matrix4x4 ret;
			for (int y = 0; y < 4; y++)
			{
				for (int x = 0; x < 4; x++)
				{
					ret.m[y][x] = l.m[y][0] * r.m[0][x] + l.m[y][1] * r.m[1][x] + l.m[y][2] * r.m[2][x] + l.m[y][3] * r.m[3][x];
				}
			}
			return ret;
		}
		friend Matrix4x4 operator+(const Matrix4x4& l, const Matrix4x4& r)
		{
			Matrix4x4 ret;
			for (int i = 0; i < 16; i++)
				ret.s[i] = l.s[i] + r.s[i];
			return ret;
		}
		friend Matrix4x4 operator-(const Matrix4x4& l, const Matrix4x4& r)
		{
			Matrix4x4 ret;
			for (int i = 0; i < 16; i++)
				ret.s[i] = l.s[i] - r.s[i];
			return ret;
		}
		friend Vector4<T> operator*(const Vector4<T>& l, const Matrix4x4& r)
		{
			Vector4<T> ret;
			T* lv = (T*)&l;
			T* retv = (T*)&ret;
			for (int i = 0; i < 4; i++)
			{
				retv[i] = 0;
				for (int j = 0; j < 4; j++)
				{
					retv[i] += lv[j] * r.m[j][i];
				}
			}
			return ret;
		}
	};

	typedef Matrix4x4<float> M44F;
	static_assert(sizeof(M44F) == 64, "sizeof(M44F) == 64");
	// Color types
	struct RGBA
	{
	public:
		static RGBA FromFloat(float r, float g, float b, float a)
		{
			RGBA ret;
			ret.r = (uint8_t)(r * 255);
			ret.g = (uint8_t)(g * 255);
			ret.b = (uint8_t)(b * 255);
			ret.a = (uint8_t)(a * 255);
			return ret;
		}
	public:
		uint8_t r;
		uint8_t g;
		uint8_t b;
		uint8_t a;
	};
	static_assert(sizeof(RGBA) == 4, "size of RGBA should be 32bits");
}

//useful macros
#define ENCODE_UINT32_FROM_INT_RGBA(r, g, b, a) (uint32((r) | ((g) << 8) | ((b) << 16) | ((a) << 24)))
