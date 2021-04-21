//#include "FbxMesh.h"
//
//FBXMesh::FBXMesh(std::wstring fbxFilename, ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
//{
//    m_device = device;
//
//    std::string _filename = WstringToString(fbxFilename);
//
//    FbxManager* manager = FbxManager::Create();
//
//    manager->SetIOSettings(FbxIOSettings::Create(manager, IOSROOT));
//
//    FbxImporter* importer = FbxImporter::Create(manager, "");
//
//    bool import_status = false;
//
//    import_status = importer->Initialize(_filename.c_str(), -1, manager->GetIOSettings());
//    _ASSERT_EXPR(import_status, importer->GetStatus().GetErrorString());
//
//    FbxScene* scene = FbxScene::Create(manager, "");
//
//    import_status = importer->Import(scene);
//    _ASSERT_EXPR(import_status, importer->GetStatus().GetErrorString());
//
//    fbxsdk::FbxGeometryConverter gemoetry_converter(manager);
//    gemoetry_converter.Triangulate(scene, /*replace*/true);
//
//    std::vector <FbxNode*> fetched_meshes;
//    std::vector <FbxNode*> bone_nodes;
//    std::function<void(FbxNode*)> traverse = [&](FbxNode* node)
//    {
//        if (node)
//        {
//            FbxNodeAttribute* fbx_node_attribute = node->GetNodeAttribute();
//
//            if (fbx_node_attribute)
//            {
//                switch (fbx_node_attribute->GetAttributeType())
//                {
//                case FbxNodeAttribute::eMesh:
//                    fetched_meshes.push_back(node);
//                    break;
//                case FbxNodeAttribute::eSkeleton:
//                    bone_nodes.push_back(node);
//
//                    break;
//                }
//            }
//
//            for (int i = 0; i < node->GetChildCount(); i++)
//            {
//                traverse(node->GetChild(i));
//            }
//        }
//
//    };
//    traverse(scene->GetRootNode());
//
//    if (fetched_meshes.size() > 0)
//    {
//        meshes.resize(fetched_meshes.size());
//        for (int i = 0; i < fetched_meshes.size(); i++)
//        {
//            FbxMesh* fbx_mesh = fetched_meshes.at(i)->GetMesh();
//            Mesh& mesh = meshes.at(i);
//            const int number_of_materials = fbx_mesh->GetNode()->GetMaterialCount();
//            mesh.subsets.resize(number_of_materials);//UNIT18
//
//            mesh.name = fbx_mesh->GetName();
//
//            //load bone_node influence per meshA
//            std::vector<bone_influences_per_control_point> bone_influences;
//            Fetch_bone_influences(fbx_mesh, bone_influences);
//
//            FbxTime::EMode time_mode = fbx_mesh->GetScene()->GetGlobalSettings().GetTimeMode();
//            FbxTime frame_time;
//            frame_time.SetTime(0, 0, 0, 1, 0, time_mode);
//
//            Fetch_bone_matrices(fbx_mesh);
//
//            FbxAMatrix global_transform = fbx_mesh->GetNode()->EvaluateGlobalTransform(0);
//
//            for (int row = 0; row < 4; row++)
//            {
//                for (int column = 0; column < 4; column++)
//                {
//                    mesh.global_transform.m[row][column] = static_cast<float>(global_transform[row][column]);
//                }
//            }
//
//            for (int index_of_material = 0; index_of_material < number_of_materials; index_of_material++)
//            {
//                const FbxSurfaceMaterial* surface_material = fbx_mesh->GetNode()->GetMaterial(index_of_material);
//                //load diffuseMap
//                const FbxProperty diffuseMap = surface_material->FindProperty(FbxSurfaceMaterial::sDiffuse);
//                const FbxProperty factor = surface_material->FindProperty(FbxSurfaceMaterial::sDiffuseFactor);
//                Subset& subset = mesh.subsets.at(index_of_material);
//
//                subset.material.Name = surface_material->GetName();
//
//                //if (diffuseMap.IsValid() && factor.IsValid())
//                //{
//                //    FbxDouble3 color = diffuseMap.Get<FbxDouble3>();
//                //    double f = factor.Get<FbxDouble>();
//                //    subset.material.DiffuseAlbedo.x = static_cast<float>(color[0] * f);
//                //    subset.material.DiffuseAlbedo.y = static_cast<float>(color[1] * f);
//                //    subset.material.DiffuseAlbedo.z = static_cast<float>(color[2] * f);
//                //    subset.material.DiffuseAlbedo.w = 1.0f;
//                //}
//
//                //load normalMap
//                if (diffuseMap.IsValid())
//                {
//                    const int number_of_textures = diffuseMap.GetSrcObjectCount<FbxFileTexture>();
//                    if (number_of_textures)
//                    {
//                        const FbxFileTexture* file_texture = diffuseMap.GetSrcObject<FbxFileTexture>();
//                        if (file_texture)
//                        {
//                            subset.material.diffuse_map_name = file_texture->GetRelativeFileName();
//                        }
//                    }
//                }
//                const FbxProperty normalMap = surface_material->FindProperty(FbxSurfaceMaterial::sNormalMap);
//                if (normalMap.IsValid())
//                {
//                    const int number_of_textures = normalMap.GetSrcObjectCount<FbxFileTexture>();
//                    if (number_of_textures)
//                    {
//                        const FbxFileTexture* file_texture = normalMap.GetSrcObject<FbxFileTexture>();
//                        if (file_texture)
//                        {
//                            subset.material.normal_map_name = file_texture->GetRelativeFileName();
//                        }
//                    }
//                }
//                const FbxProperty specularMap = surface_material->FindProperty(FbxSurfaceMaterial::sSpecular);
//                if (specularMap.IsValid())
//                {
//                    const int number_of_textures = specularMap.GetSrcObjectCount<FbxFileTexture>();
//                    if (number_of_textures)
//                    {
//                        const FbxFileTexture* file_texture = specularMap.GetSrcObject<FbxFileTexture>();
//                        if (file_texture)
//                        {
//                            subset.material.specular_map_name = file_texture->GetRelativeFileName();
//                        }
//                    }
//                }
//            }
//            // Count the polygon count of each material
//            if (number_of_materials > 0)
//            {
//                // Count the faces of each material
//                const int number_of_polygons = fbx_mesh->GetPolygonCount();
//                for (int index_of_polygon = 0; index_of_polygon < number_of_polygons; ++index_of_polygon)
//                {
//                    const u_int material_index = fbx_mesh->GetElementMaterial()->GetIndexArray().GetAt(index_of_polygon);
//                    mesh.subsets.at(material_index).index_count += 3;
//                }
//                // Record the offset (how many vertex)
//                int offset = 0;
//                for (Subset& subset : mesh.subsets)
//                {
//                    subset.index_start = offset;
//                    offset += subset.index_count;
//                    // This will be used as counter in the following procedures, reset to zero
//                    subset.index_count = 0;
//                }
//            }
//
//            std::vector<SkinnedVertex> vertices;
//            std::vector<uint16_t> indices;
//            u_int vertex_count = 0;
//
//            //Tangent
//            FbxGeometryElementTangent* element = fbx_mesh->CreateElementTangent();
//            //  保存形式の取得
//            FbxGeometryElement::EMappingMode mapmode = element->GetMappingMode();
//            FbxGeometryElement::EReferenceMode refmode = element->GetReferenceMode();
//
//            const FbxVector4* array_of_control_points = fbx_mesh->GetControlPoints();
//            const int number_of_polygons = fbx_mesh->GetPolygonCount();
//            indices.resize(number_of_polygons * 3);
//            for (int index_of_polygon = 0; index_of_polygon < number_of_polygons; index_of_polygon++)
//            {
//
//                // The material for current face.
//                int index_of_material = 0;
//                if (number_of_materials > 0)
//                {
//                    index_of_material = fbx_mesh->GetElementMaterial()->GetIndexArray().GetAt(index_of_polygon);
//                }
//                // Where should I save the vertex attribute index, according to the material
//                Subset& subset = mesh.subsets.at(index_of_material);
//                const int index_offset = subset.index_start + subset.index_count;
//
//                for (int index_of_vertex = 0; index_of_vertex < 3; index_of_vertex++)
//                {
//                    SkinnedVertex vertex;
//                    const int index_of_control_point = fbx_mesh->GetPolygonVertex(index_of_polygon, index_of_vertex);
//                    vertex.Pos.x = static_cast<float>(array_of_control_points[index_of_control_point][0]);
//                    vertex.Pos.y = static_cast<float>(array_of_control_points[index_of_control_point][1]);
//                    vertex.Pos.z = static_cast<float>(array_of_control_points[index_of_control_point][2]);
//
//                    FbxVector4 normal;
//                    fbx_mesh->GetPolygonVertexNormal(index_of_polygon, index_of_vertex, normal);
//                    vertex.Normal.x = static_cast<float>(normal[0]);
//                    vertex.Normal.y = static_cast<float>(normal[1]);
//                    vertex.Normal.z = static_cast<float>(normal[2]);
//
//                    fbxsdk::FbxStringList uv_names;
//                    fbx_mesh->GetUVSetNames(uv_names);
//
//                    if (uv_names.GetCount() > 0)
//                    {
//                        FbxVector2 uv;
//                        bool unmapped_uv;
//                        fbx_mesh->GetPolygonVertexUV(index_of_polygon, index_of_vertex, uv_names[0], uv, unmapped_uv);
//                        vertex.TexC.x = static_cast<float>(uv[0]);
//                        vertex.TexC.y = 1.0f - static_cast<float>(uv[1]);
//                    }
//                    //    ポリゴン頂点に対するインデックス参照形式のみ対応
//                    if (mapmode == FbxGeometryElement::eByPolygonVertex)
//                    {
//                        if (refmode == FbxGeometryElement::eIndexToDirect)
//                        {
//                            FbxLayerElementArrayTemplate<int>* index = &element->GetIndexArray();
//                            // FbxColor取得
//                            FbxVector4 v = element->GetDirectArray().GetAt(index->GetAt(index_of_control_point));
//                            // DWORD型のカラー作成        
//                            vertex.TangentU.x = (float)v[0];
//                            vertex.TangentU.y = (float)v[1];
//                            vertex.TangentU.z = (float)v[2];
//                        }
//                    }
//                    else
//                    {
//                        vertex.TangentU.x = 0;
//                        vertex.TangentU.y = 0;
//                        vertex.TangentU.z = 0;
//                    }
//
//                    bone_influences_per_control_point influences_per_control_point = bone_influences.at(index_of_control_point);
//
//                    for (size_t bone_index = 0; bone_index < influences_per_control_point.size(); ++bone_index)
//                    {
//
//                        if (bone_index < MAX_BONE_INFLUENCES)
//                        {
//                            vertex.BoneIndices[bone_index] = influences_per_control_point.at(bone_index).index;
//                            vertex.BoneWeights[bone_index] = influences_per_control_point.at(bone_index).weight;
//                        }
//                    }
//
//                    vertices.push_back(vertex);
//
//                    indices.at(index_offset + index_of_vertex) = static_cast<uint16_t>(vertex_count);
//
//
//                    vertex_count += 1;
//                }
//                subset.index_count += 3;
//            }
//
//            auto geo = std::make_unique<MeshGeometry>();
//            geo->Name = mesh.name;
//
//            int submeshIndex = 0;
//            for (auto& subset : mesh.subsets)
//            {
//                subset.name = mesh.name + std::to_string(submeshIndex);
//
//                SubmeshGeometry submesh;
//                submesh.IndexCount = subset.index_count;
//                submesh.StartIndexLocation = subset.index_start;
//                submesh.BaseVertexLocation = 0;
//
//                geo->DrawArgs[subset.name] = submesh;
//                submeshIndex++;
//
//            }
//
//            const UINT vbByteSize = (UINT)vertices.size() * sizeof(SkinnedVertex);
//            const UINT ibByteSize = (UINT)indices.size() * sizeof(uint16_t);
//
//            ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
//            CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
//
//            ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
//            CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);
//
//            geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(device,
//                commandList, vertices.data(), vbByteSize, geo->VertexBufferUploader);
//
//            geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(device,
//                commandList, indices.data(), ibByteSize, geo->IndexBufferUploader);
//
//            geo->VertexByteStride = sizeof(SkinnedVertex);
//            geo->VertexBufferByteSize = vbByteSize;
//            geo->IndexFormat = DXGI_FORMAT_R16_UINT;
//            geo->IndexBufferByteSize = ibByteSize;
//
//            mesh.geo = std::move(geo);
//        }
//    }
//    if (bone_nodes.size() > 0)
//    {
//        scene->SetName(_filename.c_str());
//        Fetch_bone_animations(bone_nodes, animations);
//    }
//    manager->Destroy();
//
//
//    BuildRootSignature();
//    BuildShadersAndInputLayout();
//    BuildPSOs();
//    BuildSkinnedRenderItems();
//
//}
//
//void FBXMesh::Fetch_bone_animations(std::vector<FbxNode*> bone_nodes, std::vector<Skeletal_animation>& skeletal_animations, u_int sampling_rate)
//{
//    // Get the list of all the animation stack.
//    FbxArray<FbxString*> array_of_animation_stack_names;
//    bone_nodes[0]->GetScene()->FillAnimStackNameArray(array_of_animation_stack_names);
//
//    // Get the number of animations.
//    int number_of_animations = array_of_animation_stack_names.Size();
//    if (number_of_animations > 0)
//    {
//        for (size_t index_of_animation = 0; index_of_animation < number_of_animations; ++index_of_animation)
//        {
//            Skeletal_animation skeletal_animation;
//            // Get the FbxTime per animation's frame.
//            FbxTime::EMode time_mode = bone_nodes[0]->GetScene()->GetGlobalSettings().GetTimeMode();
//            FbxTime frame_time;
//            frame_time.SetTime(0, 0, 0, 1, 0, time_mode);
//            sampling_rate = sampling_rate > 0 ? sampling_rate : frame_time.GetFrameRate(time_mode);
//            float sampling_time = 1.0f / sampling_rate;
//            skeletal_animation.sampling_time = sampling_time;
//            skeletal_animation.animation_tick = 0.0f;
//            FbxString* animation_stack_name = array_of_animation_stack_names.GetAt(index_of_animation);
//
//            skeletal_animation.name = bone_nodes[0]->GetScene()->GetName();
//
//            FbxAnimStack* current_animation_stack
//                = bone_nodes[0]->GetScene()->FindMember<FbxAnimStack>(animation_stack_name->Buffer());
//            bone_nodes[0]->GetScene()->SetCurrentAnimationStack(current_animation_stack);
//            FbxTakeInfo* take_info = bone_nodes[0]->GetScene()->GetTakeInfo(animation_stack_name->Buffer());
//            FbxTime start_time = take_info->mLocalTimeSpan.GetStart();
//            FbxTime end_time = take_info->mLocalTimeSpan.GetStop();
//            FbxTime sampling_step;
//            sampling_step.SetTime(0, 0, 1, 0, 0, time_mode);
//            sampling_step = static_cast<FbxLongLong>(sampling_step.Get() * sampling_time);
//            for (FbxTime current_time = start_time; current_time < end_time; current_time += sampling_step)
//            {
//                FBXMesh::Skeletal skeletal;
//                skeletal.resize(bone_nodes.size());
//                for (size_t bone_index = 0; bone_index < bone_nodes.size(); bone_index++)
//                {
//                    FbxSkeleton* fbx_skeletal = bone_nodes[bone_index]->GetSkeleton();
//                    const FbxAMatrix& transform = bone_nodes[bone_index]->EvaluateGlobalTransform(current_time) * fixed_matrix[bone_index];
//                    FBXMesh::Bone& bone = skeletal.at(bone_index);
//                    // convert FbxAMatrix(transform) to XMDLOAT4X4(bone_node.transform)
//                    for (int i = 0; i < 4; ++i)
//                    {
//                        for (int j = 0; j < 4; ++j)
//                        {
//                            bone.transform.m[i][j] = transform[i][j];
//                        }
//                    }
//                }
//                skeletal_animation.push_back(skeletal);
//            }
//            skeletal_animations.push_back(skeletal_animation);
//
//        }
//        for (int i = 0; i < number_of_animations; i++)
//        {
//            delete array_of_animation_stack_names[i];
//        }
//    }
//}
//
//void FBXMesh::Fetch_bone_influences(const FbxMesh* fbx_mesh, std::vector<bone_influences_per_control_point>& influences)
//{
//    const int number_of_control_points = fbx_mesh->GetControlPointsCount();
//    influences.resize(number_of_control_points);
//    const int number_of_deformers = fbx_mesh->GetDeformerCount(FbxDeformer::eSkin);
//    for (int index_of_deformer = 0; index_of_deformer < number_of_deformers; ++index_of_deformer)
//    {
//        FbxSkin* skin = static_cast<FbxSkin*>(fbx_mesh->GetDeformer(index_of_deformer, FbxDeformer::eSkin));
//        const int number_of_clusters = skin->GetClusterCount();
//
//        for (int index_of_cluster = 0; index_of_cluster < number_of_clusters; ++index_of_cluster)
//        {
//            FbxCluster* cluster = skin->GetCluster(index_of_cluster);
//            const int number_of_control_point_indices = cluster->GetControlPointIndicesCount();
//            const int* array_of_control_point_indices = cluster->GetControlPointIndices();
//            const double* array_of_control_point_weights = cluster->GetControlPointWeights();
//
//            for (int i = 0; i < number_of_control_point_indices; ++i)
//            {
//                bone_influences_per_control_point& influences_per_control_point
//                    = influences.at(array_of_control_point_indices[i]);
//                bone_influence influence;
//
//                influence.index = index_of_cluster;
//                influence.weight = static_cast<float>(array_of_control_point_weights[i]);
//                influences_per_control_point.push_back(influence);
//            }
//        }
//    }
//}
//
//void FBXMesh::Fetch_bone_matrices(const FbxMesh* fbx_mesh)
//{
//    const int number_of_deformers = fbx_mesh->GetDeformerCount(FbxDeformer::eSkin);
//    for (int index_of_deformer = 0; index_of_deformer < number_of_deformers; ++index_of_deformer)
//    {
//        FbxSkin* skin = static_cast<FbxSkin*>(fbx_mesh->GetDeformer(index_of_deformer, FbxDeformer::eSkin));
//
//        const int number_of_cluster = skin->GetClusterCount();
//        fixed_matrix.resize(number_of_cluster);
//        for (int index_of_cluster = 0; index_of_cluster < number_of_cluster; ++index_of_cluster)
//        {
//
//            FbxCluster* cluster = skin->GetCluster(index_of_cluster);
//
//            // this matrix trnasforms coordinates of the initial pose from mesh space to global space
//            FbxAMatrix reference_global_init_position;
//            cluster->GetTransformMatrix(reference_global_init_position);
//
//            // this matrix trnasforms coordinates of the initial pose from bone_node space to global space
//            FbxAMatrix cluster_global_init_position;
//            cluster->GetTransformLinkMatrix(cluster_global_init_position);
//
//            fixed_matrix.push_back(cluster_global_init_position.Inverse() * reference_global_init_position);
//        }
//
//    }
//}
//
