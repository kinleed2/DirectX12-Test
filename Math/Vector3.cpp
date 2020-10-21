//
//  Vector3.cpp
//  iex
//
//  Created by virtual machine on 2013/02/13.
//  Copyright (c) 2013年 virtual machine. All rights reserved.
//

#include "Vector3.h"
#include "Matrix.h"

void Vector3::Transform( Vector3& vec, Matrix& mat )
{
	x = vec.x * mat._11 + vec.y * mat._21 + vec.z * mat._31 + mat._41;
	y = vec.x * mat._12 + vec.y * mat._22 + vec.z * mat._32 + mat._42;
	z = vec.x * mat._13 + vec.y * mat._23 + vec.z * mat._33 + mat._43;
}

void Vector3::TransformCoord( Vector3& vec, Matrix& mat )
{
	float w = vec.x * mat._14 + vec.y * mat._24 + vec.z * mat._34 + mat._44;
	x = (vec.x * mat._11 + vec.y * mat._21 + vec.z * mat._31 + mat._41) / w;
	y = (vec.x * mat._12 + vec.y * mat._22 + vec.z * mat._32 + mat._42) / w;
	z = (vec.x * mat._13 + vec.y * mat._23 + vec.z * mat._33 + mat._43) / w;
}

void Vector3::Transform3x3( Vector3& vec, Matrix& mat )
{
	x = vec.x * mat._11 + vec.y * mat._21 + vec.z * mat._31;
	y = vec.x * mat._12 + vec.y * mat._22 + vec.z * mat._32;
	z = vec.x * mat._13 + vec.y * mat._23 + vec.z * mat._33;
}

float Vector3::dot( const Vector3& v1, const Vector3& v2 )
{
	return v1.x*v2.x + v1.y*v2.y + v1.z*v2.z;
}

Vector3 Vector3::cross( Vector3& out, const Vector3& v1, const Vector3& v2 )
{
	out.x = v1.y*v2.z - v1.z*v2.y;
	out.y = v1.z*v2.x - v1.x*v2.z;
	out.z = v1.x*v2.y - v1.y*v2.x;
	return out;
}
