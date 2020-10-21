//Matrix.h
#pragma once
#include <stdio.h>
#include "Vector3.h"
#include <string.h>

class Quaternion;
//*****************************************************************************
//
//
//
//*****************************************************************************
class Matrix
{
public:
	union
	{
		struct
		{
			float _11, _12, _13, _14;
			float _21, _22, _23, _24;
			float _31, _32, _33, _34;
			float _41, _42, _43, _44;
		};
		float m[16];
	};
	
	float* getArray(){ return m; }
		
	void identity();
	void multiply(const Matrix& mat1, const Matrix& mat2);
	void multiply(float val);
	void inverse();
	void Interporate(Matrix& target, float rate);

    Matrix() {};
	Matrix( const float* p ){ memcpy( &_11, p, sizeof(float)*16); }
    Matrix( float f11, float f12, float f13, float f14,
           float f21, float f22, float f23, float f24,
           float f31, float f32, float f33, float f34,
           float f41, float f42, float f43, float f44 )
	{
		_11 = f11; _12 = f12; _13 = f13; _14 = f14;
		_21 = f21; _22 = f22; _23 = f23; _24 = f24;
		_31 = f31; _32 = f32; _33 = f33; _34 = f34;
		_41 = f41; _42 = f42; _43 = f43; _44 = f44;
	}
          
    Matrix& operator *= ( const Matrix& mat )
	{
		multiply(*this, mat);
		return *this;
	}

	Matrix operator * (const Matrix& mat) const
	{
		Matrix matT;
		matT.multiply(*this, mat);
		return matT;
	}
	
	Matrix operator + ( const Matrix& mat ) const
	{
		Matrix matT;
		for (int i = 0; i < 16; i++) {
			matT.m[i] = this->m[i] + mat.m[i];
		}
		return matT;
	}

	Matrix operator * (float val) const
	{
		Matrix matT;
		for (int i = 0; i < 16; i++) {
			matT.m[i] = this->m[i] * val;
		}
		return matT;
	}


	void LookAt(const Vector3& position, const Vector3& target, const Vector3& up = Vector3(0, 1, 0));
	void PerspectiveFov(float fovY, float aspect, float znear, float zfar);
	void Ortho( float w, float h, float zn, float zf);

	void TRS(Vector3& T, Vector3& R, Vector3& S);
	void TRSdegree(Vector3& T, Vector3& R, Vector3& S);

	void RotationZXY(float x, float y, float z);

};
