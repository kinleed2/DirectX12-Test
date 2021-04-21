//#pragma once
//#include "../Common/d3dUtil.h"
//
//
//#include "FrameResource.h"
//#include "Graphics.h"
//
//#include <vector>
//#include <map>
//
//#include <fbxsdk.h>
//#include <SimpleMath.h>
//
//using Microsoft::WRL::ComPtr;
//using namespace DirectX::SimpleMath;
//using namespace fbxsdk;
//
//class FBXMesh
//{
//
//private:
//	struct Material
//	{
//		std::string Name;
//
//		float roughness = 0.8f;
//		bool alphaClip = false;
//
//		std::string material_type_name;
//		std::string diffuse_map_name;
//		std::string normal_map_name;
//		std::string specular_map_name;
//	};
//
//	struct Subset
//	{
//		u_int index_start = 0;
//		u_int index_count = 0;
//		Material material;
//		std::string name;
//	};
//
//	struct bone_influence
//	{
//		int index;
//		float weight;
//	};
//	typedef std::vector<bone_influence> bone_influences_per_control_point;
//
//	struct Bone
//	{
//		DirectX::XMFLOAT4X4 transform;
//	};
//	typedef std::vector<Bone> Skeletal;
//
//	struct Skeletal_animation : public std::vector<Skeletal>
//	{
//		float sampling_time = 1 / 24.0f;
//		float animation_tick = 0.0f;
//		std::string name;
//	};
//
//	struct Mesh
//	{
//		std::string name;
//		std::vector<Subset> subsets;
//		
//		std::unique_ptr<MeshGeometry> geo;
//
//		Matrix global_transform = { 1, 0, 0, 0,
//									 0, 1, 0, 0,
//									 0, 0, 1, 0,
//									 0, 0, 0, 1 };
//		
//		std::vector<Bone> skeletal;
//	
//		std::map<std::string, Skeletal_animation> skeletal_animations;
//	};
//
//	// matrix trnasforms coordinates of the initial pose from bone_node space to global space *
//	// matrix trnasforms coordinates of the initial pose from mesh space to global space
//	std::vector<FbxAMatrix> fixed_matrix;
//
//	std::vector<Skeletal_animation> animations;
//	UINT animation_index;
//	std::vector<Mesh> meshes;
//
//	ID3D12Device* m_device;
//	FrameResource* m_currFrameResource;
//
//	ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
//
//	ComPtr<ID3DBlob> mVSShaders;
//	ComPtr<ID3DBlob> mPSShaders;
//
//	std::vector<D3D12_INPUT_ELEMENT_DESC> mSkinnedInputLayout;
//
//	ComPtr<ID3D12PipelineState> mPSOs;
//
//	float animation_speed;
//
//	
//
//	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
//
//	
//
//public:
//	FBXMesh(std::wstring fbxFilename, ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
//
//	void Fetch_bone_animations(std::vector <FbxNode*> bone_nodes, std::vector<Skeletal_animation>& skeletal_animations, u_int sampling_rate = 0);
//
//	void Fetch_bone_influences(const FbxMesh* fbx_mesh, std::vector<bone_influences_per_control_point>& influences);
//
//	void Fetch_bone_matrices(const FbxMesh* fbx_mesh);
//		
//	void BuildRootSignature()
//	{
//		// Root parameter can be a table, root descriptor or root constants.
//		CD3DX12_ROOT_PARAMETER slotRootParameter[2];
//
//		// constant buffer
//		slotRootParameter[0].InitAsConstantBufferView(0);
//		// skinned buffer
//		slotRootParameter[1].InitAsConstantBufferView(1);
//
//		auto staticSamplers = Graphics::GetStaticSamplers();
//
//		auto size = sizeof(slotRootParameter);
//
//		// A root signature is an array of root parameters.
//		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter,
//			(UINT)staticSamplers.size(), staticSamplers.data(),
//			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
//
//		// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
//		ComPtr<ID3DBlob> serializedRootSig = nullptr;
//		ComPtr<ID3DBlob> errorBlob = nullptr;
//		HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
//			serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
//
//		if (errorBlob != nullptr)
//		{
//			::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
//		}
//		ThrowIfFailed(hr);
//
//		ThrowIfFailed(m_device->CreateRootSignature(
//			0,
//			serializedRootSig->GetBufferPointer(),
//			serializedRootSig->GetBufferSize(),
//			IID_PPV_ARGS(m_rootSignature.GetAddressOf())));
//	}
//
//	void BuildShadersAndInputLayout()
//	{
//		
//		const D3D_SHADER_MACRO skinnedDefines[] =
//		{
//			"SKINNED", "1",
//			NULL, NULL
//		};
//
//		mVSShaders = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", skinnedDefines, "VS", "vs_6_1");
//
//		mPSShaders = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", skinnedDefines, "PS", "ps_6_1");
//
//		mSkinnedInputLayout =
//		{
//			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
//			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
//			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
//			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
//			{ "WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
//			{ "BONEINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
//		};
//	}
//
//    void BuildPSOs()
//    {
//		DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
//		DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
//
//		D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedSmapPsoDesc;
//
//        ZeroMemory(&skinnedSmapPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
//		skinnedSmapPsoDesc.pRootSignature = m_rootSignature.Get();
//		skinnedSmapPsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
//		skinnedSmapPsoDesc.VS =
//		{
//			reinterpret_cast<BYTE*>(mVSShaders->GetBufferPointer()),
//			mVSShaders->GetBufferSize()
//		};
//		skinnedSmapPsoDesc.PS =
//		{
//			reinterpret_cast<BYTE*>(mPSShaders->GetBufferPointer()),
//			mPSShaders->GetBufferSize()
//		};
//		skinnedSmapPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
//		skinnedSmapPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
//		skinnedSmapPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
//		skinnedSmapPsoDesc.SampleMask = UINT_MAX;
//		skinnedSmapPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
//		skinnedSmapPsoDesc.NumRenderTargets = 1;
//		skinnedSmapPsoDesc.RTVFormats[0] = mBackBufferFormat;
//		skinnedSmapPsoDesc.SampleDesc.Count = 1;
//		skinnedSmapPsoDesc.SampleDesc.Quality = 0;
//		skinnedSmapPsoDesc.DSVFormat = mDepthStencilFormat;
//
//        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&skinnedSmapPsoDesc, IID_PPV_ARGS(&mPSOs)));
//
//    }
//
//
//	void BuildSkinnedRenderItems()
//	{
//		int objCBIndex = 0;
//		int skinnedIndex = 0;
//
//		for (auto& mesh : meshes)
//		{
//			int subsetIndex = 0;
//			for (auto& subset : mesh.subsets)
//			{
//				auto model = std::make_unique<RenderItem>();
//
//				Matrix modelScale = XMMatrixScaling(0.01f, 0.01f, 0.01f);
//				Matrix modelRot = XMMatrixRotationX(-XM_PIDIV2);
//				Matrix modelOffset = XMMatrixTranslation(0.0f, 0.0f, -10.0f);
//
//				model->World = modelScale * modelOffset;
//
//				model->ObjCBIndex = objCBIndex++;
//
//				model->Geo = mesh.geo.get();
//				model->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
//
//				model->IndexCount = model->Geo->DrawArgs[subset.name].IndexCount;
//				model->StartIndexLocation = model->Geo->DrawArgs[subset.name].StartIndexLocation;
//				model->BaseVertexLocation = 0;
//
//				// All render items for this solider.m3d instance share
//				// the same skinned model instance.
//				model->SkinnedCBIndex = 0;
//				model->SkinnedFlag = true;
//
//				//mRitemLayer[(int)RenderLayer::SkinnedOpaque].push_back(model.get());
//				mAllRitems.push_back(std::move(model));
//			}
//		}
//	}
//
//	void UpdateObjectCBs(const GameTimer& gt)
//	{
//
//		float animation_tick = animation_speed * gt.DeltaTime();
//		auto currObjectCB = m_currFrameResource->ObjectCB.get();
//		for (auto& e : mAllRitems)
//		{
//			// Only update the cbuffer data if the constants have changed.  
//			// This needs to be tracked per frame resource.
//			if (e->NumFramesDirty > 0)
//			{
//				XMMATRIX world = XMLoadFloat4x4(&e->World);
//				XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);
//
//				ObjectConstants objConstants;
//				XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
//				XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
//				objConstants.MaterialIndex = e->Mat->MatCBIndex;
//
//				currObjectCB->CopyData(e->ObjCBIndex, objConstants);
//				//Next FrameResource need to be updated too.
//				e->NumFramesDirty--;
//			}
//		}
//	}
//
//	void UpdateSkinnedCBs(const GameTimer& gt)
//	{
//		float animation_tick = animation_speed * gt.DeltaTime();
//		auto currSkinnedCB = m_currFrameResource->SkinnedCB.get();
//		SkinnedConstants skinnedConstants;
//
//		if (animations.size() > 0)
//		{
//			int frame = animations[animation_index].animation_tick /
//				animations[animation_index].sampling_time;
//			if (frame > animations[animation_index].size() - 1)
//			{
//				frame = 0;
//				animations[animation_index].animation_tick = 0;
//			}
//			std::vector<Bone>& skeletal = animations[animation_index].at(frame);
//			size_t number_of_bones = skeletal.size();
//			_ASSERT_EXPR(number_of_bones < MAX_BONES, L"'the number_of_bones' exceeds MAX_BONES.");
//			for (size_t i = 0; i < number_of_bones; i++)
//			{
//				XMStoreFloat4x4(&skinnedConstants.BoneTransforms[i], XMLoadFloat4x4(&skeletal.at(i).transform));
//			}
//			animations[animation_index].animation_tick += animation_tick;
//		}
//
//		currSkinnedCB->CopyData(0, skinnedConstants);
//	}
//
//	void Update(const GameTimer& gt, FrameResource* frameResource)
//	{
//		m_currFrameResource = frameResource;
//		UpdateObjectCBs(gt);
//		UpdateSkinnedCBs(gt);
//	}
//
//	void Draw(ID3D12GraphicsCommandList* cmdList)
//	{
//		cmdList->SetPipelineState(mPSOs.Get());
//
//		UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
//		UINT skinnedCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(SkinnedConstants));
//
//		auto objectCB = m_currFrameResource->ObjectCB->Resource();
//		auto skinnedCB = m_currFrameResource->SkinnedCB->Resource();
//
//		// For each render item...
//		for (size_t i = 0; i < mAllRitems.size(); ++i)
//		{
//			auto ri = mAllRitems[i].get();
//
//			cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
//			cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
//			cmdList->IASetPrimitiveTopology(ri->PrimitiveType);
//
//			D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
//
//			cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
//
//			if (ri->SkinnedFlag)
//			{
//				D3D12_GPU_VIRTUAL_ADDRESS skinnedCBAddress = skinnedCB->GetGPUVirtualAddress() + ri->SkinnedCBIndex * skinnedCBByteSize;
//				cmdList->SetGraphicsRootConstantBufferView(2, skinnedCBAddress);
//			}
//			else
//			{
//				cmdList->SetGraphicsRootConstantBufferView(2, 0);
//			}
//
//			cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
//		}
//
//	}
//
//};
//
