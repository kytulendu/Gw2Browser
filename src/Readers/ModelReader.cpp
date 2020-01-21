/** \file       Readers/ModelReader.cpp
 *  \brief      Contains the definition of the model reader class.
 *  \author     Rhoot
 */

/**
 * Copyright (C) 2014-2017 Khralkatorrix <https://github.com/kytulendu>
 * Copyright (C) 2012 Rhoot <https://github.com/rhoot>
 *
 * This file is part of Gw2Browser.
 *
 * Gw2Browser is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "stdafx.h"

#include <new>
#include <vector>

#include <gw2formats/pf/ModelPackFile.h>

#include "ModelReader.h"

#include "DatFile.h"
#include "PackFile.h"

namespace gw2b {

    //----------------------------------------------------------------------------
    //      GW2ModelData
    //----------------------------------------------------------------------------

    GW2ModelData::GW2ModelData( ) {
    }

    GW2ModelData::GW2ModelData( const GW2ModelData& p_other ) {
        meshes.assign( p_other.meshes.begin( ), p_other.meshes.end( ) );
        material.assign( p_other.material.begin( ), p_other.material.end( ) );
    }

    GW2ModelData::~GW2ModelData( ) {
    }

    //----------------------------------------------------------------------------
    //      GW2Model
    //----------------------------------------------------------------------------

    GW2Model::GW2Model( )
        : m_data( new GW2ModelData( ) ) {
    }

    GW2Model::GW2Model( const GW2Model& p_other )
        : m_data( p_other.m_data ) {
    }

    GW2Model::~GW2Model( ) {
    }

    GW2Model& GW2Model::operator=( const GW2Model& p_other ) {
        m_data = p_other.m_data;
        return *this;
    }

    uint GW2Model::numMeshes( ) const {
        return m_data->meshes.size( );
    }

    const GW2Mesh& GW2Model::mesh( uint p_index ) const {
        Assert( p_index < this->numMeshes( ) );
        return m_data->meshes[p_index];
    }

    const std::vector<GW2Mesh>& GW2Model::mesh( ) const {
        return m_data->meshes;
    }

    GW2Mesh* GW2Model::addMeshes( uint p_amount ) {
        this->unShare( );

        uint oldSize = m_data->meshes.size( );
        m_data->meshes.resize( oldSize + p_amount );

        return &( m_data->meshes[oldSize] );
    }

    uint GW2Model::numMaterial( ) const {
        return m_data->material.size( );
    }

    const GW2Material& GW2Model::material( uint p_index ) const {
        Assert( p_index < this->numMaterial( ) );
        return m_data->material[p_index];
    }

    const std::vector<GW2Material>& GW2Model::material( ) const {
        return m_data->material;
    }

    GW2Material* GW2Model::addMaterial( uint p_amount ) {
        this->unShare( );

        GW2Material emptyData;
        ::memset( &emptyData, 0, sizeof( emptyData ) );

        uint oldSize = m_data->material.size( );
        m_data->material.resize( oldSize + p_amount, emptyData );

        return &( m_data->material[oldSize] );
    }

    void GW2Model::unShare( ) {
        if ( m_data->GetRefCount( ) == 1 ) {
            return;
        }
        m_data = new GW2ModelData( *m_data );
    }

    Bounds GW2Model::bounds( ) const {
        // If this model contains meshes, return proper bounds
        if ( m_data->meshes.size( ) ) {
            Bounds retval;
            for ( uint i = 0; i < m_data->meshes.size( ); i++ ) {
                if ( i == 0 ) {
                    retval = m_data->meshes[i].bounds;
                } else {
                    retval += m_data->meshes[i].bounds;
                }
            }
            return retval;
        }

        // If not, return empty bounds
        Bounds retval;
        ::memset( &retval, 0, sizeof( retval ) );
        return retval;
    }


    //----------------------------------------------------------------------------
    //      ModelReader
    //----------------------------------------------------------------------------

    ModelReader::ModelReader( const Array<byte>& p_data, DatFile& p_datFile, ANetFileType p_fileType )
        : FileReader( p_data, p_datFile, p_fileType ) {
    }

    ModelReader::~ModelReader( ) {
    }

    GW2Model ModelReader::getModel( ) const {
        GW2Model newModel;

        // Bail if there is no data to read
        if ( m_data.GetSize( ) == 0 ) {
            wxLogMessage( wxT( "No data." ) );
            return newModel;
        }

        wxLogMessage( wxT( "Reading model file..." ) );

        gw2f::pf::ModelPackFile modelPackFile( m_data.GetPointer( ), m_data.GetSize( ) );

        this->readGeometry( newModel, modelPackFile );
        this->readMaterial( newModel, modelPackFile );

        wxLogMessage( wxT( "Finished reading model file." ) );

        return newModel;
    }

    void ModelReader::readGeometry( GW2Model& p_model, gw2f::pf::ModelPackFile& p_modelPackFile ) const {
        wxLogMessage( wxT( "Reading GOEM chunk..." ) );

        std::shared_ptr<gw2f::pf::chunks::ModelFileGeometryV1> geometryChunk;
        try {
            geometryChunk = p_modelPackFile.chunk<gw2f::pf::ModelChunks::Geometry>( );
        } catch ( const gw2f::exception::Exception& exception ) {
            wxLogMessage( wxT( "Failed to read GEOM chunk using gw2formats: %s" ), wxString( exception.what( ) ) );
            return;
        } catch ( ... ) {
            wxLogMessage( wxT( "An unknown error has occurred." ) );
            return;
        }

        // Bail if no geometry data
        if ( !geometryChunk ) {
            wxLogMessage( wxT( "No data." ) );
            return;
        }

        uint32 meshCount = geometryChunk->meshes.size( );

        // Bail if no meshes to read
        if ( !meshCount ) {
            wxLogMessage( wxT( "No mesh." ) );
            return;
        }

        wxLogMessage( wxT( "%d mesh(es)." ), meshCount );

        // Create storage for submeshes now, so we can parallelize the loop
        GW2Mesh* meshes = p_model.addMeshes( meshCount );

        auto& meshInfoArray = geometryChunk->meshes;

        uint verticesCount = 0;
        uint trianglesCount = 0;

#pragma omp parallel for shared( meshes )
        for ( int i = 0; i < static_cast<int>( meshCount ); i++ ) {
            // Fetch mesh info
            auto& meshInfo = meshInfoArray[i];

            // Fetch buffer info
            auto& vertexInfo = meshInfo.geometry->verts;
            auto& indicesInfo = meshInfo.geometry->indices;
            auto vertexCount = vertexInfo.vertexCount;
            auto indiceCount = indicesInfo.indices.size( );

            // Add new mesh
            GW2Mesh& mesh = meshes[i];

            // Material data
            mesh.materialIndex = meshInfo.materialIndex;
            mesh.materialName = wxString::FromUTF8( meshInfo.materialName.data( ) );

            // Vertex data
            if ( vertexCount ) {
                this->readVertexBuffer( mesh, vertexInfo.mesh.vertices.data( ), vertexCount, static_cast<ANetFlexibleVertexFormat>( vertexInfo.mesh.fvf ) );
            }

            // DirectX coordinate to OpenGL coordinate by rotate ZY and invert Z
            this->rotZYinvZ( mesh );

            // Index data
            if ( indiceCount ) {
                this->readIndexBuffer( mesh, reinterpret_cast<const byte*>( indicesInfo.indices.data( ) ), indiceCount );
                this->computeBond( mesh, reinterpret_cast<const byte*>( indicesInfo.indices.data( ) ), indiceCount );
            }

            if ( mesh.hasNormal ) {
                // Normalize normal
                this->normalizeNormals( mesh );
            } else {
                // Calculate the vertex normal if it not exist
                this->computeVertexNormals( mesh );
                mesh.hasNormal = true;
            }

            // Count the vertices and triangles
            verticesCount += vertexCount;
            trianglesCount += indiceCount;
        }

        wxLogMessage( wxT( "%d vertices." ), verticesCount );
        wxLogMessage( wxT( "%d triangles." ), trianglesCount );
    }

    void ModelReader::readVertexBuffer( GW2Mesh& p_mesh, const byte* p_data, uint p_vertexCount, ANetFlexibleVertexFormat p_vertexFormat ) const {
        p_mesh.vertices.resize( p_vertexCount );
        uint vertexSize = this->vertexSize( p_vertexFormat );

        p_mesh.hasNormal = ( ( p_vertexFormat & ANFVF_Normal ) ? 1 : 0 );
        p_mesh.hasUV = ( ( p_vertexFormat & ( ANFVF_UV32Mask | ANFVF_UV16Mask ) ) ? 1 : 0 );

#pragma omp parallel for
        for ( int i = 0; i < static_cast<int>( p_vertexCount ); i++ ) {
            auto pos = &p_data[i * vertexSize];
            Vertex& vertex = p_mesh.vertices[i];
            uint uvIndex = 0;

            // Bit 0: Position
            if ( p_vertexFormat & ANFVF_Position ) {
                ::memcpy( &vertex.position, pos, sizeof( vertex.position ) );
                pos += sizeof( vertex.position );
            }
            // Bit 1: Weights
            if ( p_vertexFormat & ANFVF_Weights ) {
                pos += 4;
            }
            // Bit 2: Group
            if ( p_vertexFormat & ANFVF_Group ) {
                pos += 4;
            }
            // Bit 3: Normal
            if ( p_vertexFormat & ANFVF_Normal ) {
                ::memcpy( &vertex.normal, pos, sizeof( vertex.normal ) );
                pos += sizeof( vertex.normal );
            }
            // Bit 4: Color
            if ( p_vertexFormat & ANFVF_Color ) {
                pos += sizeof( uint32 );
            }
            // Bit 5: Tangent
            if ( p_vertexFormat & ANFVF_Tangent ) {
                pos += sizeof( glm::vec3 );
            }
            // Bit 6: Bitangent
            if ( p_vertexFormat & ANFVF_Bitangent ) {
                pos += sizeof( glm::vec3 );
            }
            // Bit 7: Tangent frame
            if ( p_vertexFormat & ANFVF_TangentFrame ) {
                pos += sizeof( glm::vec3 );
            }
            // Bit 8-15: 32-bit UV
            uint uvFlag = ( p_vertexFormat & ANFVF_UV32Mask ) >> 8;
            if ( uvFlag ) {
                for ( uint j = 0; j < 7; j++ ) {
                    if ( ( ( uvFlag >> j ) & 1 ) == 0 ) {
                        continue;
                    }
                    if ( uvIndex < 1 ) {
                        ::memcpy( &vertex.uv, pos, sizeof( vertex.uv ) );
                    }
                    pos += sizeof( vertex.uv );
                }
            }
            // Bit 16-23: 16-bit UV
            uvFlag = ( p_vertexFormat & ANFVF_UV16Mask ) >> 16;
            if ( uvFlag ) {
                for ( uint j = 0; j < 7; j++ ) {
                    if ( ( ( uvFlag >> j ) & 1 ) == 0 ) {
                        continue;
                    }
                    if ( uvIndex < 1 ) {
                        auto uv = reinterpret_cast<const half*>( pos );
                        vertex.uv.x = uv[0];
                        vertex.uv.y = uv[1];
                        uvIndex++;
                    }
                    pos += sizeof( half ) * 2;
                }
            }
            // Bit 24: Unknown 48-byte value
            if ( p_vertexFormat & ANFVF_Unknown1 ) {
                pos += 48;
            }
            // Bit 25: Unknown 4-byte value
            if ( p_vertexFormat & ANFVF_Unknown2 ) {
                pos += 4;
            }
            // Bit 26: Unknown 4-byte value
            if ( p_vertexFormat & ANFVF_Unknown3 ) {
                pos += 4;
            }
            // Bit 27: Unknown 16-byte value
            if ( p_vertexFormat & ANFVF_Unknown4 ) {
                pos += 16;
            }
            // Bit 28: Compressed position
            if ( p_vertexFormat & ANFVF_PositionCompressed ) {
                vertex.position.x = *reinterpret_cast<const half*>( pos + 0 * sizeof( half ) );
                vertex.position.y = *reinterpret_cast<const half*>( pos + 1 * sizeof( half ) );
                vertex.position.z = *reinterpret_cast<const half*>( pos + 2 * sizeof( half ) );
                pos += 3 * sizeof( half );
            }
            // Bit 29: Unknown 12-byte value
            if ( p_vertexFormat & ANFVF_Unknown5 ) {
                pos += 12;
            }
        }
    }

    uint ModelReader::vertexSize( ANetFlexibleVertexFormat p_vertexFormat ) const {
        uint uvCount = 0;
        uint uvField = ( p_vertexFormat & ANFVF_UV32Mask ) >> 0x08;
        for ( uint i = 0; i < 7; i++ ) {
            if ( ( ( uvField >> i ) & 1 ) != 0 )
                uvCount++;
        }

        uvField = ( p_vertexFormat & ANFVF_UV16Mask ) >> 0x10;
        uint uv16Count = 0;
        for ( uint i = 0; i < 7; i++ ) {
            if ( ( ( uvField >> i ) & 1 ) != 0 )
                uv16Count++;
        }

        return ( ( p_vertexFormat & ANFVF_Position ) * 12 )
            + ( ( p_vertexFormat & ANFVF_Weights ) * 2 )
            + ( ( p_vertexFormat & ANFVF_Group ) )
            + ( ( ( p_vertexFormat & ANFVF_Normal ) >> 3 ) * 12 )
            + ( ( p_vertexFormat & ANFVF_Color ) >> 2 )
            + ( ( ( p_vertexFormat & ANFVF_Tangent ) >> 5 ) * 12 )
            + ( ( ( p_vertexFormat & ANFVF_Bitangent ) >> 6 ) * 12 )
            + ( ( ( p_vertexFormat & ANFVF_TangentFrame ) >> 7 ) * 12 )
            + ( uvCount * 8 )
            + ( uv16Count * 4 )
            + ( ( ( p_vertexFormat & ANFVF_Unknown1 ) >> 24 ) * 48 )
            + ( ( ( p_vertexFormat & ANFVF_Unknown2 ) >> 25 ) * 4 )
            + ( ( ( p_vertexFormat & ANFVF_Unknown3 ) >> 26 ) * 4 )
            + ( ( ( p_vertexFormat & ANFVF_Unknown4 ) >> 27 ) * 16 )
            + ( ( ( p_vertexFormat & ANFVF_PositionCompressed ) >> 28 ) * 6 )
            + ( ( ( p_vertexFormat & ANFVF_Unknown5 ) >> 29 ) * 12 );

    }

    void ModelReader::computeBond( GW2Mesh& p_mesh, const byte* p_data, uint p_indiceCount ) const {
        // Calculate bounds
        float floatMin = std::numeric_limits<float>::min( );
        float floatMax = std::numeric_limits<float>::max( );
        p_mesh.bounds.min = glm::vec3( floatMax, floatMax, floatMax );
        p_mesh.bounds.max = glm::vec3( floatMin, floatMin, floatMin );

        glm::vec3 min = p_mesh.bounds.min;
        glm::vec3 max = p_mesh.bounds.max;

        auto indices = reinterpret_cast<const uint16*>( p_data );
#pragma omp parallel for
        for ( int i = 0; i < static_cast<int>( p_indiceCount ); i++ ) {
            auto& vertex = p_mesh.vertices[indices[i]];
            glm::vec3 position = vertex.position;
            min = glm::min( min, position );
            max = glm::max( max, position );
        }
        p_mesh.bounds.min = min;
        p_mesh.bounds.max = max;
    }

    void ModelReader::readIndexBuffer( GW2Mesh& p_mesh, const byte* p_data, uint p_indiceCount ) const {
        p_mesh.triangles.resize( p_indiceCount / 3 );
#pragma omp parallel for
        for ( int i = 0; i < static_cast<int>( p_mesh.triangles.size( ) ); i++ ) {
            auto pos = &p_data[i * sizeof( Triangle )];
            Triangle& triangle = p_mesh.triangles[i];
            // Flip the order of the faces of the triangle
            triangle.index1 = *reinterpret_cast<const uint16*>( pos + 0 * sizeof( uint16 ) );
            triangle.index2 = *reinterpret_cast<const uint16*>( pos + 2 * sizeof( uint16 ) );
            triangle.index3 = *reinterpret_cast<const uint16*>( pos + 1 * sizeof( uint16 ) );
        }
    }

    void ModelReader::normalizeNormals( GW2Mesh& p_mesh ) const {
        auto& vert = p_mesh.vertices;
#pragma omp parallel for
        for ( int i = 0; i < static_cast<int>( p_mesh.vertices.size( ) ); i++ ) {
            vert[i].normal = glm::normalize( vert[i].normal );
        }
    }

    void ModelReader::computeVertexNormals( GW2Mesh& p_mesh ) const {
        // Calculate vertex normals
        // http://www.iquilezles.org/www/articles/normals/normals.htm

        auto& verts = p_mesh.vertices;
        auto& faces = p_mesh.triangles;

        // verts[i].normal is already initialized with zero

#pragma omp parallel for
        for ( int i = 0; i < static_cast<int>( faces.size( ) ); i++ ) {
            // Re-flip the order of the faces of the triangle to original order
            const int ia = faces[i].index1;
            const int ib = faces[i].index3;
            const int ic = faces[i].index2;

            const glm::vec3 e1 = verts[ia].position - verts[ib].position;
            const glm::vec3 e2 = verts[ic].position - verts[ib].position;
            const glm::vec3 no = glm::normalize(glm::cross( e1, e2 ));

            verts[ia].normal += no;
            verts[ib].normal += no;
            verts[ic].normal += no;
        }
    }

    void ModelReader::rotZYinvZ( GW2Mesh& p_mesh ) const {
        // Rotate ZY and invert Z
        const glm::mat3 transform = glm::mat3(
            glm::vec3( 1.0f, 0.0f, 0.0f ),
            glm::vec3( 0.0f, 0.0f, -1.0f ),
            glm::vec3( 0.0f, -1.0f, 0.0f )
            );

#pragma omp parallel for
        for ( int i = 0; i < static_cast<int>( p_mesh.vertices.size( ) ); i++ ) {
            auto& vertex = p_mesh.vertices[i];
            vertex.position = transform * vertex.position;
            // Todo: Untested!
            if ( p_mesh.hasNormal ) {
                vertex.normal = transform * vertex.normal;
            }
        }
    }

    void ModelReader::readMaterial( GW2Model& p_model, gw2f::pf::ModelPackFile& p_modelPackFile ) const {
        wxLogMessage( wxT( "Reading MODL chunk..." ) );

        std::shared_ptr<gw2f::pf::chunks::ModelFileDataV65> modelChunk;
        try {
            modelChunk = p_modelPackFile.chunk<gw2f::pf::ModelChunks::Model>( );
        } catch ( const gw2f::exception::Exception& exception ) {
            wxLogMessage( wxT( "Failed to read MODL chunk using gw2formats: %s" ), wxString( exception.what( ) ) );

            wxLogMessage( wxT( "Try another method..." ) );
            this->readMaterialPF( p_model, p_modelPackFile );
        } catch (const std::out_of_range & outofrange) {
			wxLogMessage(wxT("Failed to read MODL chunk using gw2formats: %s"), wxString(outofrange.what()));

			wxLogMessage(wxT("Try another method..."));
			this->readMaterialPF(p_model, p_modelPackFile);
		} catch (const std::invalid_argument & invalidargument) {		// Tried to load more that was maximum size.
			wxLogMessage(wxT("Failed to read MODL chunk using gw2formats: %s"), wxString(invalidargument.what()));

			wxLogMessage(wxT("Try another method..."));
			this->readMaterialPF(p_model, p_modelPackFile);
		} catch ( ... ) {
            wxLogMessage( wxT( "An unknown error has occurred." ) );
            return;
        }

        // Successfully read MODL chunk using gw2formats
        if ( modelChunk ) {
            // Bail if no permutations data
            if ( !modelChunk->permutations.data( ) ) {
                wxLogMessage( wxT( "No permutations data." ) );
                return;
            }

            // Read some necessary data
            auto& permutationsInfoArray = modelChunk->permutations;
            uint32 numMaterialInfo = modelChunk->permutations.size( );

            // Count materials
            uint materialCount = 0;
            for ( uint i = 0; i < numMaterialInfo; i++ ) {
                materialCount = wxMax( materialCount, permutationsInfoArray[i].materials.size( ) );
            }

            // Bail if no materials data
            if ( !materialCount ) {
                wxLogMessage( wxT( "No material data." ) );
                return;
            }
            wxLogMessage( wxT( "Have %d material(s)." ), materialCount );

            GW2Material* materials = p_model.addMaterial( materialCount );
            // Loop through each material
            for ( int i = 0; i < static_cast<int>( numMaterialInfo ); i++ ) {
                    // Bail if no material data
                    if ( !permutationsInfoArray[i].materials.data( ) ) {
                        wxLogMessage( wxT( "No material data in material array %d." ), i );
                        continue;
                    }

                #pragma omp parallel
                {
                    auto& materialsArray = permutationsInfoArray[i].materials;

                    // Loop through each material in these permutations infos
                    #pragma omp for
                    for ( int j = 0; j < static_cast<int>( materialsArray.size( ) ); j++ ) {
                        auto& mat = materialsArray[j];
                        auto& material = materials[j];

                        // Copy the data
                        material.materialId = mat.materialId;
                        material.materialFlags = mat.materialFlags;

                        // Get material filename from each materials
                        material.materialFile = mat.filename.fileId( );

                        if ( !mat.textures.size( ) ) {
                            continue;
                        }

                        for ( uint t = 0; t < mat.textures.size( ); t++ ) {
                            auto& texture = mat.textures[t];
                            auto fileId = texture.filename.fileId( ) + 1;

                            // diffuse  token 0x0060b401 67531924
                            // normal   token 0x000305a0 1816c9ee
                            // specular token 0x0c168090 59519613
                            // dye      token 0x0060b402 e6169724

                            // 0x67531924   01100111010100110001100100100100    Diffuse
                            // 0x1816c9ee   00011000000101101100100111101110    Normal
                            // 0x59519613   01011001010100011001011000010011    Specular
                            // 0x680bbd87   01101000000010111011110110000111    Light Map
                            // 0xe6169724   11100110000101101001011100100100    Dye

                            switch ( texture.token & 0xffffffff ) {
                            case 0x67531924:                    // Diffuse?
                                material.diffuseMap = fileId;
                                continue;
                            case 0x1816c9ee:                    // Normal?
                                material.normalMap = fileId;
                                continue;
                            case 0x59519613:                    // Specular?
                                material.specularMap = fileId;
                                continue;
                            case 0x680bbd87:                    // Light Map?
                                material.lightMap = fileId;
                                continue;
                            case 0xe6169724:                    // Dye?
                                material.dye = fileId;
                                continue;
                            default:
                                continue;
                            }
                        }
                    }
                }   //#pragma omp parallel
            }
        }

        wxLogMessage( wxT( "Finished reading MODL chunk." ) );
    }

    void ModelReader::readMaterialPF( GW2Model& p_model, gw2f::pf::ModelPackFile& p_modelPackFile ) const {
        size_t size;
        auto data = p_modelPackFile.chunk( gw2f::pf::ModelChunks::Model, size );

        // Read some necessary data
        uint32 numPermutations = *reinterpret_cast<const uint32*>( data );
        uint32 permutationsOffset = *reinterpret_cast<const uint32*>( &data[4] );
        auto permutationInfoArray = reinterpret_cast<const ANetModelMaterialPermutations*>( &data[4 + permutationsOffset] );

        // Count permutations
        uint materialCount = 0;
        for ( uint i = 0; i < numPermutations; i++ ) {
            materialCount = wxMax( materialCount, permutationInfoArray[i].materialCount );
        }

        // Bail if no permutations
        if ( !materialCount ) {
            wxLogMessage( wxT( "No permutations data." ) );
            return;
        }

        // Prepare parallel loop
        std::vector<omp_lock_t> locks( materialCount );
        for ( auto& it : locks ) {
            omp_init_lock( &( it ) );
        }

        GW2Material* materials = p_model.addMaterial( materialCount );

        // Loop through each permutations
#pragma omp parallel for shared(locks, permutationInfoArray)
        for ( int i = 0; i < static_cast<int>( numPermutations ); i++ ) {
            // Bail if no offset or count
            if ( !permutationInfoArray[i].materialCount || !permutationInfoArray[i].materialsOffset ) {
                continue;
            }

            // Read the offset table for this set of materials
            auto pos = permutationInfoArray[i].materialsOffset + reinterpret_cast<const byte*>( &permutationInfoArray[i].materialsOffset );
            auto offsetTable = reinterpret_cast<const int32*>( pos );

            // Loop through each material in these material infos
            for ( uint j = 0; j < permutationInfoArray[i].materialCount; j++ ) {
                auto& material = materials[j];

                // Bail if offset is NULL
                if ( !offsetTable[j] ) {
                    continue;
                }

                // Only one thread must access this material at a time
                omp_set_lock( &locks[j] );

                // Bail if this material index already has data
                if ( material.diffuseMap && material.normalMap && material.lightMap && material.materialFlags ) {
                    omp_unset_lock( &locks[j] );
                    continue;
                }

                // Read material info
                pos = offsetTable[j] + reinterpret_cast<const byte*>( &offsetTable[j] );
                auto materialInfo = reinterpret_cast<const ANetModelMaterialData*>( pos );

                // Copy the data
                material.materialId = materialInfo->materialId;
                material.materialFlags = materialInfo->materialFlags;

                // Get material filename from each materials
                auto ref = reinterpret_cast<const ANetFileReference*>( materialInfo->materialFileOffset + reinterpret_cast<const byte*>( &materialInfo->materialFileOffset ) );
                material.materialFile = DatFile::fileIdFromFileReference( *ref );

                if ( !materialInfo->textureCount ) {
                    omp_unset_lock( &locks[j] );
                    continue;
                }

                pos = materialInfo->texturesOffset + reinterpret_cast<const byte*>( &materialInfo->texturesOffset );
                auto textures = reinterpret_cast<const ANetModelTextureReference*>( pos );

                for ( uint t = 0; t < materialInfo->textureCount; t++ ) {
                    // Get file reference
                    pos = textures[t].offsetToFileReference + reinterpret_cast<const byte*>( &textures[t].offsetToFileReference );
                    auto fileReference = reinterpret_cast<const ANetFileReference*>( pos );

                    // Diffuse?
                    if ( ( textures[t].token & 0xffffffff ) == 0x67531924 ) {
                        material.diffuseMap = ( DatFile::fileIdFromFileReference( *fileReference ) + 1 );
                    }

                    // Normal?
                    else if ( ( textures[t].token & 0xffffffff ) == 0x1816c9ee ) {
                        material.normalMap = ( DatFile::fileIdFromFileReference( *fileReference ) + 1 );
                    }

                    // Light Map?
                    else if ( ( textures[t].token & 0xffffffff ) == 0x680bbd87 ) {
                        material.lightMap = ( DatFile::fileIdFromFileReference( *fileReference ) + 1 );
                        break;
                    }
                }

                // Let other threads access this material now that we're done
                omp_unset_lock( &locks[j] );
            }

        }

        // Destroy locks
        for ( auto& it : locks ) {
            omp_destroy_lock( &( it ) );
        }

    }

}; // namespace gw2b
