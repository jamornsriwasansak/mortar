#pragma once

//#define DEBUG_CheckInvalidSplittedGeometry

#include "pch/pch.h"

#include "core/logger.h"
#include "core/vmath.h"
#include "shaders/shared/compact_vertex.h"
#include "shaders/shared/types.h"

struct AiGeometryInfo
{
    using aiFaceSizeT     = decltype(aiMesh::mNumFaces);
    using aiFaceRange     = rangeT<aiFaceSizeT>;
    using aiMeshSizeT     = decltype(aiScene::mNumMeshes);
    using aiVertexSizeT   = std::remove_reference_t<decltype(*aiFace::mIndices)>;
    using aiMaterialSizeT = decltype(aiMesh::mMaterialIndex);

    // dst means mortar's data format
    size_t m_dst_num_vertices;
    size_t m_dst_num_indices;

    // src means index / range / id are referred to assimp format
    aiMeshSizeT         m_src_mesh_index;
    rangeT<aiFaceSizeT> m_src_faces_range;
    aiMaterialSizeT     m_src_material_index;

    bool m_is_indices_reorder_needed;

    std::string
    to_string() const
    {
        std::string str;
        str += "m_src_mesh_index    : " + std::to_string(m_src_mesh_index) + "\n";
        str += "m_dst_num_vertices  : " + std::to_string(m_dst_num_vertices) + "\n";
        str += "m_dst_num_indices   : " + std::to_string(m_dst_num_indices) + "\n";
        str += "m_src_indices_range : " + m_src_faces_range.to_string() + "\n";
        return str;
    }
};

struct AiScene
{
    using aiFaceSizeT     = decltype(aiMesh::mNumFaces);
    using aiFaceRange     = rangeT<aiFaceSizeT>;
    using aiMeshSizeT     = decltype(aiScene::mNumMeshes);
    using aiVertexSizeT   = std::remove_reference_t<decltype(*aiFace::mIndices)>;
    using aiMaterialSizeT = decltype(aiMesh::mMaterialIndex);

    // the scene is automatically be freed if the importer is destructed
    std::unique_ptr<Assimp::Importer> m_ai_importer;
    const aiScene *                   m_ai_scene = nullptr;

    // read AiScene from path
    static std::optional<AiScene>
    ReadScene(const std::filesystem::path & path)
    {
        AiScene result;
        result.m_ai_importer = std::make_unique<Assimp::Importer>();
        result.m_ai_scene    = result.m_ai_importer->ReadFile(path.string(), aiProcess_GenNormals);
        if (result.m_ai_scene == nullptr)
        {
            return std::nullopt;
        }
        return result;
    }

    // split assimp scene into multiple geometries where each geometry is guaranteed to have #vertices less than user-specified max num vertices
    std::vector<AiGeometryInfo>
    get_geometry_infos(const size_t max_dst_num_vertices_per_geometry) const
    {
        std::vector<AiGeometryInfo> result;

        // iterate over all meshes and split them
        for (aiMeshSizeT i_mesh = 0; i_mesh < m_ai_scene->mNumMeshes; i_mesh++)
        {
            get_geometry_infos(&result, i_mesh, max_dst_num_vertices_per_geometry);
        }
        return result;
    }

    // split imported assimp mesh into multiple geometries where each geometry is guaranteed to have #vertices less than user-specified max num vertices
    void
    get_geometry_infos(std::vector<AiGeometryInfo> * geometries,
                       const unsigned int            src_mesh_index,
                       const size_t                  max_dst_num_vertices_per_geometry) const
    {
        const aiMesh & src_mesh = *m_ai_scene->mMeshes[src_mesh_index];

        {
            // src mesh
            aiVertexSizeT max_vindex      = std::numeric_limits<aiVertexSizeT>::min();
            aiVertexSizeT min_vindex      = std::numeric_limits<aiVertexSizeT>::max();
            aiFaceSizeT   num_dst_indices = 0;
            for (aiFaceSizeT i_src_face = 0; i_src_face < src_mesh.mNumFaces; i_src_face++)
            {
                const aiFace & src_face = src_mesh.mFaces[i_src_face];
                num_dst_indices += (src_face.mNumIndices - 2) * 3;
                for (aiVertexSizeT i_index = 0; i_index < src_face.mNumIndices; i_index++)
                {
                    const aiVertexSizeT vindex = src_face.mIndices[i_index];
                    max_vindex                 = std::max(vindex, max_vindex);
                    min_vindex                 = std::min(vindex, min_vindex);
                }
            }

            aiVertexSizeT num_vindices = max_vindex - min_vindex + 1;
            if (num_vindices < max_dst_num_vertices_per_geometry)
            {
                AiGeometryInfo geometry;
                geometry.m_src_mesh_index            = static_cast<uint32_t>(src_mesh_index);
                geometry.m_src_faces_range.m_begin   = 0;
                geometry.m_src_faces_range.m_end     = src_mesh.mNumFaces;
                geometry.m_dst_num_indices           = num_dst_indices;
                geometry.m_dst_num_vertices          = num_vindices;
                geometry.m_src_material_index        = src_mesh.mMaterialIndex;
                geometry.m_is_indices_reorder_needed = false;
                geometries->push_back(geometry);
                return;
            }
        }

        // since max vertices that we support per geometry could be as low as MAX_UINT16 or
        // MAX_UINT8 but number of vertices and indices in mesh could be higher to avoid this we
        // have to split mesh indices and vertices into multiple groups create list of submesh info
        size_t                  used_num_indices = 0;
        std::set<aiVertexSizeT> used_ai_vertex_indices;
        aiFaceSizeT             i_range_begin = 0;

        for (aiFaceSizeT i_src_face = 0; i_src_face < src_mesh.mNumFaces;)
        {
            // check if we can push a new face into the current dst mesh
            // if we cannot, num_indices and num_vertices along with range will be recorded as the info for submesh
            const size_t num_indices_before_increment  = used_num_indices;
            const size_t num_vertices_before_increment = used_ai_vertex_indices.size();

            // in src mesh, a single face could be a triangle fan
            // where in dst mesh, a single face is only a triangle
            // for instance, 4 indices face -> 2 faces, 5 indices face -> 3 faces, ...
            const aiFace & src_face = src_mesh.mFaces[i_src_face];
            used_num_indices += static_cast<size_t>((src_face.mNumIndices - 2) * 3);
            for (unsigned int i_index = 0; i_index < src_face.mNumIndices; i_index++)
            {
                used_ai_vertex_indices.insert(src_face.mIndices[i_index]);
            }

            // if the increment fail, record num_indices and num_vertices and range as the info of a new submesh
            const bool is_vertices_exceed = used_ai_vertex_indices.size() >= max_dst_num_vertices_per_geometry;
            if (is_vertices_exceed)
            {
                // range
                const aiFaceSizeT i_range_end = i_src_face;

                // push back
                AiGeometryInfo geometry;
                geometry.m_src_mesh_index            = static_cast<uint32_t>(src_mesh_index);
                geometry.m_src_faces_range.m_begin   = i_range_begin;
                geometry.m_src_faces_range.m_end     = i_range_end;
                geometry.m_dst_num_indices           = num_indices_before_increment;
                geometry.m_dst_num_vertices          = num_vertices_before_increment;
                geometry.m_src_material_index        = src_mesh.mMaterialIndex;
                geometry.m_is_indices_reorder_needed = true;
                geometries->push_back(geometry);

#ifdef DEBUG_CheckInvalidSplittedGeometry
                if (geometry.m_src_faces_range.length() == 0 || geometry.m_dst_num_indices == 0 ||
                    geometry.m_dst_num_vertices == 0)
                {
                    Logger::Info(__FUNCTION__, "src_ai_mesh.num_faces : ", src_mesh.mNumFaces);
                    Logger::Error<true>(__FUNCTION__,
                                        "dst_geometry.m_src_indices_range.length() : ",
                                        geometry.m_src_faces_range.length());
                }
#endif

                // update
                i_range_begin    = i_range_end;
                used_num_indices = 0;
                used_ai_vertex_indices.clear();
            }
            else
            {
                // proceed to the next face
                i_src_face++;
            }
        }

        // push back the last geometry
        AiGeometryInfo geometry;
        geometry.m_src_mesh_index            = static_cast<uint32_t>(src_mesh_index);
        geometry.m_src_faces_range           = urange32_t(i_range_begin, src_mesh.mNumFaces);
        geometry.m_dst_num_indices           = used_num_indices;
        geometry.m_dst_num_vertices          = used_ai_vertex_indices.size();
        geometry.m_src_material_index        = src_mesh.mMaterialIndex;
        geometry.m_is_indices_reorder_needed = true;
        geometries->push_back(geometry);

#ifdef DEBUG_CheckInvalidSplittedGeometry
        if (geometry.m_src_faces_range.length() == 0 || geometry.m_dst_num_indices == 0 ||
            geometry.m_dst_num_vertices == 0)
        {
            Logger::Info(__FUNCTION__, "src_ai_mesh.num_faces : ", src_mesh.mNumFaces);
            Logger::Error<true>(__FUNCTION__,
                                "dst_geometry.m_src_indices_range.length() : ",
                                geometry.m_src_faces_range.length());
        }
#endif
    }

    void
    write_geometry_info_reordered(std::span<float3> *        positions,
                                  std::span<CompactVertex> * compact_vertices,
                                  std::span<VertexIndexT> *  indices,
                                  const AiGeometryInfo &     geometry_info) const
    {
        std::span<float3> &        rpositions = *positions;
        std::span<CompactVertex> & rcvertices = *compact_vertices;
        std::span<VertexIndexT> &  rcindices  = *indices;
        const aiMesh &             ai_mesh = *m_ai_scene->mMeshes[geometry_info.m_src_mesh_index];

        // map from src vertex index in ai mesh to dst vertex index
        std::map<aiVertexSizeT, size_t> ai_vindex_to_dst_vindex;

        // for each face in ai mesh
        const urange32_t face_range      = geometry_info.m_src_faces_range;
        size_t           num_dst_indices = 0;
        for (uint32_t i_src_face = face_range.m_begin; i_src_face < face_range.m_end; i_src_face++)
        {
            auto get_dst_vindex = [&](const aiVertexSizeT ai_vindex)
            {
                // get dst_vindex
                size_t     dst_vindex;
                const auto map_result = ai_vindex_to_dst_vindex.find(ai_vindex);

                if (map_result == ai_vindex_to_dst_vindex.end())
                {
                    // this vertex does not exist in the position buffer before
                    dst_vindex = ai_vindex_to_dst_vindex.size();

                    // write new vertex position
                    const auto & ai_position = ai_mesh.mVertices[ai_vindex];
                    rpositions[dst_vindex].x = ai_position.x;
                    rpositions[dst_vindex].y = ai_position.y;
                    rpositions[dst_vindex].z = ai_position.z;

                    // write new shading normal
                    const auto & ai_normal = ai_mesh.mNormals[ai_vindex];
                    float3       snormal(ai_normal.x, ai_normal.y, ai_normal.z);
                    if (dot(snormal, snormal) == 0.0f)
                    {
                        snormal.z = 1.0f;
                    }
                    rcvertices[dst_vindex].set_snormal(snormal);

                    // write new texture coord
                    if (ai_mesh.HasTextureCoords(0))
                    {
                        rcvertices[dst_vindex].m_texcoord.x = ai_mesh.mTextureCoords[0][ai_vindex].x;
                        rcvertices[dst_vindex].m_texcoord.y = ai_mesh.mTextureCoords[0][ai_vindex].y;
                    }

                    // map new index
                    ai_vindex_to_dst_vindex[ai_vindex] = dst_vindex;
                }
                else
                {
                    dst_vindex = map_result->second;
                }

                return dst_vindex;
            };

            // we convert ai_indices into dst_indices
            const size_t dst_vindex0 = get_dst_vindex(ai_mesh.mFaces[i_src_face].mIndices[0]);
            size_t       dst_vindex1 = get_dst_vindex(ai_mesh.mFaces[i_src_face].mIndices[1]);
            for (unsigned int i_index = 2; i_index < ai_mesh.mFaces[i_src_face].mNumIndices; i_index++)
            {
                const size_t dst_vindex2 = get_dst_vindex(ai_mesh.mFaces[i_src_face].mIndices[i_index]);

                rcindices[num_dst_indices + 0] = static_cast<VertexIndexT>(dst_vindex0);
                rcindices[num_dst_indices + 1] = static_cast<VertexIndexT>(dst_vindex1);
                rcindices[num_dst_indices + 2] = static_cast<VertexIndexT>(dst_vindex2);
                num_dst_indices += 3;

                assert(dst_vindex0 < std::numeric_limits<VertexIndexT>::max());
                assert(dst_vindex1 < std::numeric_limits<VertexIndexT>::max());
                assert(dst_vindex2 < std::numeric_limits<VertexIndexT>::max());

                dst_vindex1 = dst_vindex2;
            }
        }

        assert(ai_vindex_to_dst_vindex.size() == geometry_info.m_dst_num_vertices);
        assert(num_dst_indices == geometry_info.m_dst_num_indices);
    }

    void
    write_geometry_info_simple(std::span<float3> *        positions,
                               std::span<CompactVertex> * compact_vertices,
                               std::span<VertexIndexT> *  indices,
                               const AiGeometryInfo &     geometry_info) const
    {
        // make into reference, so we can use operator[] easily
        // (operator[] usually has lower cost than ->at())
        std::span<float3> &        rpositions = *positions;
        std::span<CompactVertex> & rcvertices = *compact_vertices;
        std::span<VertexIndexT> &  rcindices  = *indices;
        const aiMesh &             ai_mesh = *m_ai_scene->mMeshes[geometry_info.m_src_mesh_index];

        // for each vertices
        for (aiVertexSizeT i_vertex = 0; i_vertex < ai_mesh.mNumVertices; i_vertex++)
        {
            // setup positions
            rpositions[i_vertex].x = ai_mesh.mVertices[i_vertex].x;
            rpositions[i_vertex].y = ai_mesh.mVertices[i_vertex].y;
            rpositions[i_vertex].z = ai_mesh.mVertices[i_vertex].z;

            // setup compact information
            CompactVertex & vertex = rcvertices[i_vertex];

            // write new shading normal
            const aiVector3D & ai_normal = ai_mesh.mNormals[i_vertex];
            float3             snormal(ai_normal.x, ai_normal.y, ai_normal.z);
            if (dot(snormal, snormal) == 0.0f)
            {
                snormal.z = 1.0f;
            }
            vertex.set_snormal(snormal);

            // write texture coord
            if (ai_mesh.HasTextureCoords(0))
            {
                vertex.m_texcoord.x = ai_mesh.mTextureCoords[0][i_vertex].x;
                vertex.m_texcoord.y = ai_mesh.mTextureCoords[0][i_vertex].y;
            }
        }

        // for each face in ai mesh
        size_t index_buffer_offset = 0;
        for (aiFaceSizeT i_face = 0; i_face < ai_mesh.mNumFaces; i_face++)
        {
            const unsigned int ai_index0 = ai_mesh.mFaces[i_face].mIndices[0];

            // convert triangle fan -> triangles
            for (unsigned int i_fan = 0; i_fan < ai_mesh.mFaces[i_face].mNumIndices - 2; i_fan++)
            {
                const unsigned int ai_index1 = ai_mesh.mFaces[i_face].mIndices[i_fan + 1];
                const unsigned int ai_index2 = ai_mesh.mFaces[i_face].mIndices[i_fan + 2];

                rcindices[index_buffer_offset + 0] = static_cast<uint16_t>(ai_index0);
                rcindices[index_buffer_offset + 1] = static_cast<uint16_t>(ai_index1);
                rcindices[index_buffer_offset + 2] = static_cast<uint16_t>(ai_index2);

                index_buffer_offset += 3;

                assert(ai_index0 < std::numeric_limits<VertexIndexT>::max());
                assert(ai_index1 < std::numeric_limits<VertexIndexT>::max());
                assert(ai_index2 < std::numeric_limits<VertexIndexT>::max());
            }
        }
    }

    // write the aiScene's positions and indices into given positions and indices spans based on the given geometry_info
    void
    write_geometry_info(std::span<float3> *        positions,
                        std::span<CompactVertex> * compact_vertices,
                        std::span<VertexIndexT> *  indices,
                        const AiGeometryInfo &     geometry_info) const
    {
        if (geometry_info.m_is_indices_reorder_needed)
        {
            write_geometry_info_reordered(positions, compact_vertices, indices, geometry_info);
        }
        else
        {
            write_geometry_info_simple(positions, compact_vertices, indices, geometry_info);
        }
    }
};
