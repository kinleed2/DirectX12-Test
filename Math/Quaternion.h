#pragma once

#include	<math.h>
#include	"Matrix.h"

class Quaternion {
public:
	float x, y, z, w;
	Quaternion(){ x=0; y=0; z=0; w=1; }
	Quaternion( float qx, float qy, float qz, float qw ){ x=qx; y=qy; z=qz; w=qw; }
	Quaternion( Vector3& v, float t ){ v.Normalize(); x = v.x*sinf(t*0.5f); y = v.y*sinf(t*0.5f); z = v.z*sinf(t*0.5f); w = cosf(t*0.5f); }

	void toMatrix( Matrix& m);
	void slerp( Quaternion& q, Quaternion& r, float t );	


	//------------------------------------------------------
	//	生成
	//------------------------------------------------------
	//	単位クォータニオン生成
	void Identity(){ x = y = z = 0; w = 1; }
	
	//	正規化
	inline void normalize()
	{
		float legnth = getLength();
		if(legnth == .0f ) return;
		float invL = 1.0f / legnth;
		(*this) *= invL;
	}
	//------------------------------------------------------
	//	情報取得
	//------------------------------------------------------
	inline float getLength() const{ return sqrtf( x*x + y*y + z*z + w*w); }
	void fromMatrix( Matrix& mat );
	
	//------------------------------------------------------
	//	オペレーター
	//------------------------------------------------------
	inline Quaternion& operator = ( const Quaternion& v ){ x=v.x; y=v.y; z=v.z; w=v.w; return *this; }
	inline Quaternion operator + () const { Quaternion ret( x, y, z, w ); return ret; }
	inline Quaternion operator - () const { Quaternion ret( -x, -y, -z, -w ); return ret; }
	
	//	VS クォータニオン
	inline Quaternion& operator +=(const Quaternion& v){ x+=v.x; y+=v.y; z+=v.z; w+=v.w; return *this; }
	inline Quaternion& operator -=(const Quaternion& v){ x-=v.x; y-=v.y; z-=v.z; w-=v.w; return *this; }
	inline Quaternion& operator *=(const Quaternion& v)
	{
		x = y * v.z - z * v.y + x * v.w + w * v.x;
		y = z * v.x - x * v.z + y * v.w + w * v.y;
		z = x * v.y - y * v.x + z * v.w + w * v.z;
		w = w * v.w - x * v.x - y * v.y - z * v.z;
		return *this;
    }
	
	inline Quaternion operator +(const Quaternion& v) const{ return Quaternion( x+v.x, y+v.y, z+v.z, w+v.w); }
	inline Quaternion operator -(const Quaternion& v) const{ return Quaternion( x-v.x, y-v.y, z-v.z, w-v.w); }
	inline Quaternion operator *(const Quaternion& v) const
	{
		return Quaternion(
						  y * v.z - z * v.y + x * v.w + w * v.x,
						  z * v.x - x * v.z + y * v.w + w * v.y,
						  x * v.y - y * v.x + z * v.w + w * v.z,
						  w * v.w - x * v.x - y * v.y - z * v.z
						  );
    }
	
	//	VS 値
	inline Quaternion& operator *=(float v){ x*=v; y*=v; z*=v; w*=v; return *this; }
	inline Quaternion& operator /=(float v){ x/=v; y/=v; z/=v; w/=v; return *this; }
	
	inline Quaternion operator *( float v ) const{ return Quaternion( x*v, y*v, z*v, w*v); }
	inline Quaternion operator /( float v ) const{ return Quaternion( x/v, y/v, z/v, w/v); }

};

