#pragma once
#include "../Common/d3dApp.h"
#include "../Common/MathHelper.h"
#include "../Common/UploadBuffer.h"
#include "../Common/GeometryGenerator.h"
#include "../Common/Camera.h"
#include "Waves.h"
#include "FrameResource.h"
#include "SkinnedData.h"
#include "ModelLoader.h"
#include "ShadowMap.h"

#include "DirectXTex.h"

//DirectXTK 12
#include "DDSTextureLoader.h"
#include "WICTextureLoader.h"

//Assimp
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>


using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

struct ModelMaterial
{
	std::string Name;

	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 0.8f;
	bool AlphaClip = false;

	std::string MaterialTypeName;
	std::string DiffuseMapName;
	std::string NormalMapName;
};

struct SkinnedVertex
{
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexC;
	DirectX::XMFLOAT3 TangentU;
	float BoneWeights;
	std::vector<UINT> BoneIndices;
};

struct SkinnedModelInstance
{
	SkinnedData* SkinnedInfo = nullptr;
	std::vector<DirectX::XMFLOAT4X4> FinalTransforms;
	std::string ClipName;
	float TimePos = 0.0f;

	// Called every frame and increments the time position, interpolates the 
	// animations for each bone based on the current animation clip, and 
	// generates the final transforms which are ultimately set to the effect
	// for processing in the vertex shader.
	void UpdateSkinnedAnimation(float dt)
	{
		TimePos += dt;

		// Loop animation
		if (TimePos > SkinnedInfo->GetClipEndTime(ClipName))
			TimePos = 0.0f;

		// Compute the final transforms for this time position.
		SkinnedInfo->GetFinalTransforms(ClipName, TimePos, FinalTransforms);
	}
};

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	BoundingBox Bounds;
	std::vector<InstanceData> Instances;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT InstanceCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;


	// Only applicable to skinned render-items.
	UINT SkinnedCBIndex = -1;

	// nullptr if this render-item is not animated by skinned mesh.
	SkinnedModelInstance* SkinnedModelInst = nullptr;
};

enum class RenderLayer : int
{
	Opaque = 0,
	InstanceOpaque,
	SkinnedOpaque,
	OpaqueDynamicReflectors,
	Sky,
	Mirrors,
	Reflected,
	Transparent,
	AlphaTested,
	AlphaTestedTreeSprites,
	Debug,
	Count
};

class Graphics : public D3DApp
{
public:
	Graphics(HINSTANCE hInstance);
	Graphics(const Graphics& rhs) = delete;
	Graphics& operator=(const Graphics& rhs) = delete;
	~Graphics();

	virtual bool Initialize()override;

private:
	virtual void CreateRtvAndDsvDescriptorHeaps()override;
	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

	void OnKeyboardInput(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialBuffer(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateReflectedPassCB(const GameTimer& gt);
	void UpdateWaves(const GameTimer& gt);
	void UpdateInstanceData(const GameTimer& gt);

	void UpdateShadowTransform(const GameTimer& gt);
	void UpdateShadowPassCB(const GameTimer& gt);

	void UpdateSkinnedCBs(const GameTimer& gt);

	void LoadModelData(const std::wstring filename, const std::string modelName);
	bool LoadModel(const std::wstring filename);

	void processNode(aiNode* node, const aiScene* scene);
	void processMesh(aiMesh* mesh, const aiScene* scene, std::vector<Vertex>& vertices, std::vector<std::uint16_t>& indices);
	void LoadMaterialTextures(const aiScene* scene);


	void AiMatrixToXMFLOAT4X4(const aiMatrix4x4& aiMatrix, XMFLOAT4X4* matrix);

	void LoadContents();
	void LoadTextures(const std::wstring filename, const std::string texName);

	void BuildRootSignature();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	void BuildTreeSpritesGeometry();
	void BuildLandGeometry();
	void BuildWavesGeometry();
	void BuildBoxGeometry();
	void BuildShapeGeometry();
	void BuildSkullGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();
	void BuildInstanceRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	void DrawInstanceRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	void DrawSceneToShadowMap();

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();

	float GetHillsHeight(float x, float z)const;
	XMFLOAT3 GetHillsNormal(float x, float z)const;

private:

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;


	std::vector<ModelMaterial> mModelMaterials;
	std::vector<std::string> mModelTextureNames;
	
	UINT mModelSrvHeapStart = 0;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mStdInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mTreeSpriteInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mSkinnedInputLayout;
	
	RenderItem* mBoxRitem = nullptr;
	RenderItem* mReflectedBoxRitem = nullptr;
	RenderItem* mShadowBoxRitem = nullptr;

	RenderItem* mWavesRitem = nullptr;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<std::unique_ptr<RenderItem>> mAllInstanceRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	UINT mSkyTexHeapIndex = 0;
	UINT mShadowMapHeapIndex = 0;

	UINT mNullCubeSrvIndex = 0;
	UINT mNullTexSrvIndex = 0;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mNullSrv;

	std::unique_ptr<Waves> mWaves;

	PassConstants mMainPassCB;  // index 0 of pass cbuffer.
	PassConstants mShadowPassCB;// index 1 of pass cbuffer.

	PassConstants mReflectedPassCB;

	XMFLOAT3 mBoxTranslation = { 3.0f, 2.0f, -9.0f };

	POINT mLastMousePos;

	Camera mCamera;

	UINT mInstanceCount = 0;

	BoundingFrustum mCamFrustum;
	bool mFrustumCullingEnabled = true;

	std::unique_ptr<ShadowMap> mShadowMap;

	DirectX::BoundingSphere mSceneBounds;

	float mLightNearZ = 0.0f;
	float mLightFarZ = 0.0f;
	XMFLOAT3 mLightPosW;
	XMFLOAT4X4 mLightView = MathHelper::Identity4x4();
	XMFLOAT4X4 mLightProj = MathHelper::Identity4x4();
	XMFLOAT4X4 mShadowTransform = MathHelper::Identity4x4();

	float mLightRotationAngle = 0.0f;
	XMFLOAT3 mBaseLightDirections[3] = {
		XMFLOAT3(0.57735f, -0.57735f, 0.57735f),
		XMFLOAT3(-0.57735f, -0.57735f, 0.57735f),
		XMFLOAT3(0.0f, -0.707f, -0.707f)
	};
	XMFLOAT3 mRotatedLightDirections[3];

	std::unique_ptr<SkinnedModelInstance> mSkinnedModelInst;
	SkinnedData mSkinnedInfo;


	std::vector<UINT> mVertexId;
	std::vector<float> mWeight;

	DirectX::XMFLOAT4X4 mGlobalTransform = {
										1, 0, 0, 0,
										0, 1, 0, 0,
										0, 0, 1, 0,
										0, 0, 0, 1 };
};