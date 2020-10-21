//
//  Quaternion.cpp
//  iex
//
//  Created by virtual machine on 2013/02/13.
//  Copyright (c) 2013îN virtual machine. All rights reserved.
//

#include "Quaternion.h"

void Quaternion::toMatrix( Matrix& m)
{
	float s = sqrtf( x*x + y*y + z*z + w*w );
	s = 2.0f / (s * s);
	float vx = x * s;
	float vy = y * s;
	float vz = z * s;
	float wx = vx * w;
	float wy = vy * w;
	float wz = vz * w;
	float sx = x * vx;
	float sy = y * vy;
	float sz = z * vz;
	float cx = y * vz;
	float cy = z * vx;
	float cz = x * vy;
	
	m._11 = 1.0f - sy - sz;
	m._12 = cz + wz;
	m._13 = cy - wy;
	m._14 = 0.0f;
	m._21 = cz - wz;
	m._22 = 1.0f - sx - sz;
	m._23 = cx + wx;
	m._24 = 0.0f;
	m._31 = cy + wy;
	m._32 = cx - wx;
	m._33 = 1.0f - sx - sy;
	m._34 = 0.0f;
	m._41 = 0.0f;
	m._42 = 0.0f;
	m._43 = 0.0f;
	m._44 = 1.0f;
}

void Quaternion::slerp( Quaternion& q, Quaternion& r, float t )
{
	if( t <= 0 ){ x = q.x; y = q.y; z = q.z; w = q.w; return; }
	if( t >= 1 ){ x = r.x; y = r.y; z = r.z; w = r.w; return; }
	
	//	?Åˆ??
	float dot = q.x*r.x + q.y*r.y + q.z*r.z + q.w*r.w;
	float ss = 1.0f - (dot * dot);
	if(ss <= 0.0f){ x = q.x; y = q.y; z = q.z; w = q.w; return; }
	
	Quaternion Target;
	Target.x = r.x;
	Target.y = r.y;
	Target.z = r.z;
	Target.w = r.w;
	
	if( dot < 0.0f )
	{
		Target.x = -Target.x;
		Target.y = -Target.y;
		Target.z = -Target.z;
		Target.w = -Target.w;
		dot = -dot;
	}
	//	?Åˆ???l????
	if(dot > 1.0f ) dot = 1.0f;
	
	//	???Åe?????v?Z
	float ph = acosf(dot);
	float s = 1.0f / sinf(ph);
	float s1 = sinf(ph * (1.0f-t)) * s;
	float s2 = sinf(ph *  t      ) * s;
	//	???Åe
	x = q.x*s1 + Target.x*s2;
	y = q.y*s1 + Target.y*s2;
	z = q.z*s1 + Target.z*s2;
	w = q.w*s1 + Target.w*s2;
}


//------------------------------------------------------
//		çsóÒÇ©ÇÁçÏê¨
//------------------------------------------------------
void Quaternion::fromMatrix( Matrix& mat )
{
	// ç≈ëÂê¨ï™Çåüçı
    float elem[ 4 ]; // 0:x, 1:y, 2:z, 3:w
    elem[ 0 ] = mat._11 - mat._22 - mat._33 + 1.0f;
    elem[ 1 ] = -mat._11 + mat._22 - mat._33 + 1.0f;
    elem[ 2 ] = -mat._11 - mat._22 + mat._33 + 1.0f;
    elem[ 3 ] = mat._11 + mat._22 + mat._33 + 1.0f;
	
    unsigned biggestIndex = 0;
    for ( int i = 1; i < 4; i++ ) {
        if ( elem[i] > elem[biggestIndex] )
            biggestIndex = i;
    }
	
    if ( elem[biggestIndex] < 0.0f ) return;
	
    // ç≈ëÂóvëfÇÃílÇéZèo
    float *q[4] = {&x, &y, &z, &w};
    float v = sqrtf( elem[biggestIndex] ) * 0.5f;
    *q[biggestIndex] = v;
    float mult = 0.25f / v;
	
    switch ( biggestIndex ) {
		case 0: // x
			*q[1] = (mat._12 + mat._21) * mult;
			*q[2] = (mat._31 + mat._13) * mult;
			*q[3] = (mat._23 - mat._32) * mult;
			break;
		case 1: // y
			*q[0] = (mat._12 + mat._21) * mult;
			*q[2] = (mat._23 + mat._32) * mult;
			*q[3] = (mat._31 - mat._13) * mult;
			break;
		case 2: // z
			*q[0] = (mat._31 + mat._13) * mult;
			*q[1] = (mat._23 + mat._32) * mult;
			*q[3] = (mat._12 - mat._21) * mult;
			break;
		case 3: // w
			*q[0] = (mat._23 - mat._32) * mult;
			*q[1] = (mat._31 - mat._13) * mult;
			*q[2] = (mat._12 - mat._21) * mult;
			break;
    }
}

