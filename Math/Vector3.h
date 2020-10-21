#pragma once
#include <math.h>

class Matrix;

//------------------------------------------------------
//	３Ｄベクトル構造体
//------------------------------------------------------
class Vector3
{
public:
    float x, y, z;

	//	コンストラクタ
	Vector3(){ x=0; y=0; z=0; }
	inline Vector3( float x, float y, float z ){ this->x=x, this->y=y, this->z=z; }
	inline Vector3( const Vector3& v ){ this->x=v.x, this->y=v.y, this->z=v.z; }
    
	//	距離計算
	inline float Length(){ return sqrtf(x*x + y*y + z*z); }
	inline float LengthSq(){ return x*x + y*y + z*z; }
    
	//	正規化
	void Normalize()
	{
		float l = Length();
		if( l != .0f ){ x /= l; y /= l; z /= l; }
	}

    void add( Vector3 val ){ x+=val.x; y+=val.y; z+=val.z; }
	void sub( Vector3 val ){ x-=val.x; y-=val.y; z-=val.z; }
	void mul( float val ){ x*=val; y*=val; z*=val; }
	
	void Transform( Vector3& vec, Matrix& mat );
	void TransformCoord( Vector3& vec, Matrix& mat );
	void Transform3x3( Vector3& vec, Matrix& mat );
	
	static float dot( const Vector3& v1, const Vector3& v2 );
	static Vector3 cross( Vector3& out, const Vector3& v1, const Vector3& v2 );

    
	//	オペレーター
	inline Vector3& operator = ( const Vector3& v ){ x=v.x; y=v.y; z=v.z; return *this; }
	inline Vector3& operator += ( const Vector3& v ){ x+=v.x; y+=v.y; z+=v.z; return *this; }
	inline Vector3& operator -= ( const Vector3& v ){ x-=v.x; y-=v.y; z-=v.z; return *this; }
	inline Vector3& operator *= ( float v ){ x*=v; y*=v; z*=v; return *this; }
	inline Vector3& operator /= ( float v ){ x/=v; y/=v; z/=v; return *this; }
    
	inline Vector3 operator + () const { Vector3 ret( x, y, z ); return ret; }
	inline Vector3 operator - () const { Vector3 ret( -x, -y, -z ); return ret; }
    
	inline Vector3 operator + ( const Vector3& v ) const { return Vector3(x+v.x, y+v.y, z+v.z); }
	inline Vector3 operator - ( const Vector3& v ) const { return Vector3(x-v.x, y-v.y, z-v.z); }
	inline Vector3 operator * ( float v ) const { Vector3 ret(x*v, y*v, z*v); return ret; }
	inline Vector3 operator / ( float v ) const { Vector3 ret(x/v, y/v, z/v); return ret; }
    
	bool operator == ( const Vector3& v ) const { return (x==v.x) && (y==v.y) && (z==v.z); }
	bool operator != ( const Vector3& v ) const { return (x!=v.x) || (y!=v.y) || (z!=v.z); }
    
};

