/** \file       Exporter.cpp
 *  \brief      Contains the definition for the base class of exporters for the
 *              various file types.
 *  \author     Khralkatorrix
 */

/**
 * Copyright (C) 2016-2019 Khralkatorrix <https://github.com/kytulendu>
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

#include <sstream>
#include <string>
#include <wx/sstream.h>
#include <wx/wfstream.h>

#include "DatFile.h"
#include "DatIndex.h"
#include "FileReader.h"
#include "Readers/ImageReader.h"
#include "Readers/StringReader.h"
#include "Readers/ModelReader.h"
#include "Readers/PackedSoundReader.h"
#include "Readers/asndMP3Reader.h"
#include "Readers/SoundBankReader.h"
#include "Readers/EulaReader.h"
#include "Readers/ContentReader.h"
#include "Readers/AFNTReader.h"

#include "Exporter.h"

namespace gw2b {

    Exporter::Exporter( const Array<const DatIndexEntry*>& p_entries, DatFile& p_datFile, ExtractionMode p_mode )
        : m_datFile( p_datFile )
        , m_entries( p_entries )
        , m_progress( nullptr )
        , m_currentProgress( 0 )
        , m_mode( p_mode )
        , m_fileType( ANFT_Unknown ) {

        // If it's just one file, we could handle it here
        if ( m_entries.GetSize( ) == 1 ) {
            auto& entry = m_entries[0];
            auto entryData = m_datFile.readFile( entry->mftEntry( ) );
            // Valid data?
            if ( !entryData.GetSize( ) ) {
                wxMessageBox( wxT( "Failed to get file data, most likely due to a decompression error." ), wxT( "Error" ), wxOK | wxICON_ERROR );
                return;
            }

            // Identify file type
            m_datFile.identifyFileType( entryData.GetPointer( ), entryData.GetSize( ), m_fileType );

            // Ask for location
            wxFileDialog dialog( this,
                wxString::Format( wxT( "Extract %s..." ), entry->name( ) ),
                wxEmptyString,
                wxString::Format( wxT( "%s" ), entry->name( ) ),
                this->GetWildcard( ),
                wxFD_SAVE | wxFD_OVERWRITE_PROMPT );

            // Open file for writing
            if ( dialog.ShowModal( ) == wxID_OK ) {
                // Set file path
                m_filename.SetPath( dialog.GetDirectory( ) );
                // Set file name
                m_filename.SetName( dialog.GetFilename( ) );

                // Convert and export file
                this->extractFile( *entry );
            }

        // More than one files
        } else {
            // Ask for location
            wxDirDialog dialog( this, wxT( "Select output folder" ) );
            if ( dialog.ShowModal( ) == wxID_OK ) {

                m_path = dialog.GetPath( );

                uint numFile = static_cast<uint>( p_entries.GetSize( ) );

                auto title = wxString::Format( wxT( "Extracting %d %s..." ), numFile, ( p_entries.GetSize( ) == 1 ? wxT( "file" ) : wxT( "files" ) ) );
                m_progress = new wxProgressDialog( title, wxT( "Preparing to extract..." ), p_entries.GetSize( ), this, wxPD_SMOOTH | wxPD_CAN_ABORT | wxPD_ELAPSED_TIME );
                m_progress->Show( );

                // Loop through each files and update progress bar
                for ( uint i = 0; i < m_entries.GetSize( ); i++ ) {
                    // DONE
                    if ( m_currentProgress >= m_entries.GetSize( ) ) {
                        break;
                    }

                    auto entry = m_entries[m_currentProgress];

                    // Set file path
                    m_filename.SetPath( m_path );
                    // Set file name
                    m_filename.SetName( entry->name( ) );
                    // Set file extension
                    m_filename.SetExt( wxString( this->GetExtension( ) ) );

                    // Appen category name as path
                    this->appendPaths( m_filename, *entry->category( ) );

                    // Create directory if not exist
                    if ( !m_filename.DirExists( ) ) {
                        m_filename.Mkdir( 511, wxPATH_MKDIR_FULL );
                    }

                    // Extract current file
                    this->extractFile( *m_entries[m_currentProgress] );

                    bool shouldContinue = m_progress->Update( m_currentProgress, wxString::Format( wxT( "Extracting file %d/%d..." ), m_currentProgress, numFile ) );
                    if ( shouldContinue ) {
                        m_currentProgress++;
                    } else {
                        m_currentProgress = m_entries.GetSize( );
                    }
                }
                deletePointer( m_progress );
            }
        }
    }

    const wxChar* Exporter::GetExtension( ) const {
        if ( m_mode == EM_Converted ) {
            switch ( this->m_fileType ) {
            case ANFT_ATEX:
            case ANFT_ATTX:
            case ANFT_ATEC:
            case ANFT_ATEP:
            case ANFT_ATEU:
            case ANFT_ATET:
            case ANFT_DDS:
            case ANFT_JPEG:
            case ANFT_WEBP:
            case ANFT_PNG:
            case ANFT_BitmapFontFile:
                return wxT( "png" );
                break;
            case ANFT_Model:
                return wxT( "obj" );
                break;
            case ANFT_StringFile:
                return wxT( "csv" );
                break;
            case ANFT_GameContent:
                return wxT( "xml" );
                break;
            case ANFT_PackedOgg:
            case ANFT_Ogg:
                return wxT( "ogg" );
                break;
            case ANFT_PackedMP3:
            case ANFT_asndMP3:
            case ANFT_MP3:
            case ANFT_Bank:
                return wxT( "mp3" );
                break;
            case ANFT_EULA:
            case ANFT_TEXT:
            case ANFT_UTF8:
                return wxT( "txt" );
                break;
            case ANFT_Bink2Video:
                return wxT( "bk2" );
                break;
            case ANFT_DLL:
                return wxT( "dll" );
                break;
            case ANFT_EXE:
                return wxT( "exe" );
                break;
            default:
                return wxT( "raw" ); // todo
                break;
            }
        } else {
            switch ( this->m_fileType ) {
                // Texture
            case ANFT_ATEX:
                return wxT( "atex" );
                break;
            case ANFT_ATTX:
                return wxT( "attx" );
                break;
            case ANFT_ATEC:
                return wxT( "atec" );
                break;
            case ANFT_ATEP:
                return wxT( "atep" );
                break;
            case ANFT_ATEU:
                return wxT( "ateu" );
                break;
            case ANFT_ATET:
                return wxT( "atet" );
                break;
            case ANFT_DDS:
                return wxT( "dds" );
                break;
            case ANFT_JPEG:
                return wxT( "jpg" );
                break;
            case ANFT_WEBP:
                return wxT( "webp" );
                break;
            case ANFT_PNG:
                return wxT( "png" );
                break;

                // String files
            case ANFT_StringFile:
                return wxT( "strs" );
                break;
            case ANFT_EULA:
                return wxT( "eula" );
                break;


            case ANFT_GameContent:
                return wxT( "cntc" );
                break;
            case ANFT_GameContentPortalManifest:
                return wxT( "prlt" );
                break;

                // Maps stuff
            case ANFT_MapParam:
                return wxT( "parm" );
                break;
            case ANFT_MapShadow:
                return wxT( "mpsd" );
                break;
            case ANFT_MapCollision:
                return wxT( "havk" );
                break;
            case ANFT_PagedImageTable:
                return wxT( "pimg" );
                break;

            case ANFT_Material:
                return wxT( "amat" );
                break;
            case ANFT_Composite:
                return wxT( "cmpc" );
                break;
            case ANFT_AnimSequences:
                return wxT( "seqn" );
                break;
            case ANFT_EmoteAnimation:
                return wxT( "emoc" );
                break;

            case ANFT_Cinematic:
                return wxT( "cinp" );
                break;

            case ANFT_TextPackManifest:
                return wxT( "txtm" );
                break;
            case ANFT_TextPackVariant:
                return wxT( "txtvar" );
                break;
            case ANFT_TextPackVoices:
                return wxT( "txtv" );
                break;

                // Sound
            case ANFT_Sound:
            case ANFT_asndMP3:
            case ANFT_asndOgg:
                return wxT( "sound" );
                break;
            case ANFT_PackedMP3:
            case ANFT_PackedOgg:
                return wxT( "asnd" );
                break;
            case ANFT_MP3:
                return wxT( "mp3" );
                break;
            case ANFT_Ogg:
                return wxT( "ogg" );
                break;
            case ANFT_Bank:
                return wxT( "abnk" );
                break;

                // Audio script
            case ANFT_AudioScript:
                return wxT( "amsp" );
                break;

                // Font file
            case ANFT_FontFile:
                return wxT( "eot" );    // Embedded OpenType
                break;
            case ANFT_BitmapFontFile:
                return wxT( "afnt" );
                break;

                // Bink2 Video file
            case ANFT_Bink2Video:
                return wxT( "bk2" );
                break;

            case ANFT_ShaderCache:
                return wxT( "cdhs" );
                break;

            case ANFT_Model:
                return wxT( "modl" );
                break;

                // Binary
            case ANFT_DLL:
                return wxT( "dll" );
                break;
            case ANFT_EXE:
                return wxT( "exe" );
                break;

            case ANFT_Config:
                return wxT( "locl" );
                break;

            case ANFT_ARAP:
                return wxT( "arap" );
                break;

                // Text file
            case ANFT_TEXT:
            case ANFT_UTF8:
                return wxT( "txt" );
                break;

            default:
                return wxT( "raw" );
                break;
            }
        }
    }

    const wxString Exporter::GetWildcard( ) const {
        if ( m_mode == EM_Raw ) {
            switch ( m_fileType ) {
                // Texture
            case ANFT_ATEX:
                return wxT( "Guild Wars 2 Generic Texture file (*.atex)|*.atex" );
                break;
            case ANFT_ATTX:
                return wxT( "Guild Wars 2 Texture file (*.attx)|*.attx" );
                break;
            case ANFT_ATEC:
                return wxT( "Guild Wars 2 Texture file (*.atec)|*.atec" );
                break;
            case ANFT_ATEP:
                return wxT( "Guild Wars 2 Maps Texture file (*.atep)|*.atep" );
                break;
            case ANFT_ATEU:
                return wxT( "Guild Wars 2 UI Texture file (*.ateu)|*.ateu" );
                break;
            case ANFT_ATET:
                return wxT( "Guild Wars 2 Texture file (*.atet)|*.atet" );
                break;
            case ANFT_DDS:
                return wxT( "DirectDraw Surface file (*.dds)|*.dds" );
                break;
            case ANFT_JPEG:
                return wxT( "JPEG file (*.jpg)|*.jpg" );
                break;
            case ANFT_WEBP:
                return wxT( "WebP file (*.webp)|*.webp" );
                break;
            case ANFT_PNG:
                return wxT( "Portable Network Graphic file (*.png)|*.png" );
                break;

                // String files
            case ANFT_StringFile:
                return wxT( "Guild Wars 2 String file (*.strs)|*.strs" );
                break;
            case ANFT_EULA:
                return wxT( "Guild Wars 2 EULA file (*.eula)|*.eula" );
                break;

            case ANFT_GameContent:
                return wxT( "Guild Wars 2 Game Content file (*.cntc)|*.cntc" );
                break;
            case ANFT_GameContentPortalManifest:
                return wxT( "Guild Wars 2 Game Content Portal Manifest file (*.prlt)|*.prlt" );
                break;

                // Maps stuff
            case ANFT_MapParam:
                return wxT( "Guild Wars 2 Map Parameter file (*.parm)|*.parm" );
                break;
            case ANFT_MapShadow:
                return wxT( "Guild Wars 2 Map Shadow file (*.mpsd)|*.mpsd" );
                break;
            case ANFT_MapCollision:
                return wxT( "Guild Wars 2 Map Collision file (*.havk)|*.havk" );
                break;
            case ANFT_PagedImageTable:
                return wxT( "Guild Wars 2 Paged Image Table file (*.pimg)|*.pimg" );
                break;

            case ANFT_Material:
                return wxT( "Guild Wars 2 Material file (*.amat)|*.amat" );
                break;
            case ANFT_Composite:
                return wxT( "Guild Wars 2 Composite file (*.cmpc)|*.cmpc" );
                break;
            case ANFT_AnimSequences:
                return wxT( "Guild Wars 2 Animation Sequences file (*.seqn)|*.seqn" );
                break;
            case ANFT_EmoteAnimation:
                return wxT( "Guild Wars 2 Emote Animation file (*.emoc)|*.emoc" );
                break;

            case ANFT_Cinematic:
                return wxT( "Guild Wars 2 Cinematic file (*.cinp)|*.cinp" );
                break;

            case ANFT_TextPackManifest:
                return wxT( "Guild Wars 2 Text Pack Manifest file (*.txtm)|*.txtm" );
                break;
            case ANFT_TextPackVariant:
                return wxT( "Guild Wars 2 Text Pack Variant file (*.txtvar)|*.txtvar" );
                break;
            case ANFT_TextPackVoices:
                return wxT( "Guild Wars 2 Text Pack Voices file (*.txtv)|*.txtv" );
                break;

                // Sound
            case ANFT_Sound:
            case ANFT_asndMP3:
            case ANFT_asndOgg:
                return wxT( "Guild Wars 2 Sound file (*.sound)|*.sound" );
                break;
            case ANFT_PackedMP3:
            case ANFT_PackedOgg:
                return wxT( "Guild Wars 2 Packed Sound file (*.asnd)|*.asnd" );
                break;
            case ANFT_MP3:
                return wxT( "MPEG Layer 3 file (*.mp3)|*.mp3" );
                break;
            case ANFT_Ogg:
                return wxT( "Ogg file (*.ogg)|*.ogg" );
                break;
            case ANFT_Bank:
                return wxT( "Guild Wars 2 Bank file (*.abnk)|*.abnk" );
                break;

                // Audio script
            case ANFT_AudioScript:
                return wxT( "Guild Wars 2 Audio Script file (*.amsp)|*.amsp" );
                break;

                // Font file
            case ANFT_FontFile:
                return wxT( "Embedded OpenType Font file (*.eot)|*.eot" );
                break;
            case ANFT_BitmapFontFile:
                return wxT( "Guild Wars 2 Bitmap Font file (*.afnt)|*.afnt" );
                break;

                // Bink2 Video file
            case ANFT_Bink2Video:
                return wxT( "Bink 2 Video file (*.bk2)|*.bk2" );
                break;

            case ANFT_ShaderCache:
                return wxT( "Guild Wars 2 Shader Cache file (*.cdhs)|*.cdhs" );
                break;

            case ANFT_Model:
                return wxT( "Guild Wars 2 Model file (*.modl)|*.modl" );
                break;

                // Binary
            case ANFT_DLL:
                return wxT( "Dynamic-link library file (*.dll)|*.dll" );
                break;
            case ANFT_EXE:
                return wxT( "Executable file (*.exe)|*.exe" );
                break;

            case ANFT_Config:
                return wxT( "Guild Wars 2 Config file (*.locl)|*.locl" );
                break;

            case ANFT_ARAP:
                return wxT( "Guild Wars 2 ARAP file (*.arap)|*.arap" );
                break;

                // Text file
            case ANFT_TEXT:
            case ANFT_UTF8:
                return wxT( "Text file (*.txt)|*.txt" );
                break;

            default:
                return wxT( "Guild Wars 2 Raw file (*.raw)|*.raw" );
                break;
            }
        } else {
            return wxFileSelectorDefaultWildcardStr;
        }
    }

    void Exporter::extractFile( const DatIndexEntry& p_entry ) {
        auto entryData = m_datFile.readFile( p_entry.mftEntry( ) );
        // Valid data?
        if ( !entryData.GetSize( ) ) {
            wxMessageBox( wxT( "Failed to extract the file, most likely due to a decompression error." ), wxT( "Error" ), wxOK | wxICON_ERROR );
            return;
        }

        // Identify file type
        m_datFile.identifyFileType( entryData.GetPointer( ), entryData.GetSize( ), m_fileType );

        auto reader = FileReader::readerForData( entryData, m_datFile, m_fileType );

        if ( reader ) {
            // Should we convert the file?
            if ( m_mode == EM_Converted ) {
                // Set file extension
                m_filename.SetExt( wxString( this->GetExtension( ) ) );

                switch ( m_fileType ) {
                case ANFT_ATEX:
                case ANFT_ATTX:
                case ANFT_ATEC:
                case ANFT_ATEP:
                case ANFT_ATEU:
                case ANFT_ATET:
                case ANFT_DDS:
                case ANFT_JPEG:
                case ANFT_WEBP:
                    this->exportImage( reader, p_entry.name( ) );
                    break;
                case ANFT_StringFile:
                    this->exportString( reader, p_entry.name( ) );
                    break;
                case ANFT_EULA:
                    this->exportEula( reader, p_entry.name( ) );
                    break;
                case ANFT_PackedMP3:
                case ANFT_PackedOgg:
                case ANFT_asndMP3:
                    this->exportSound( reader, p_entry.name( ) );
                    break;
                case ANFT_Bank:
                    this->exportSoundBank( reader, p_entry.name( ) );
                    break;
                case ANFT_Model:
                    this->exportModel( reader, p_entry.name( ) );
                    break;
                case ANFT_GameContent:
                    this->exportGameContent( reader, p_entry.name( ) );
                    break;
                case ANFT_BitmapFontFile:
                    this->exportBitmapFont( reader, p_entry.name( ) );
                    break;
                default:
                    //entryData = reader->rawData( );
                    this->writeFile( entryData );
                    break;
                }
            } else {
                //entryData = reader->rawData( );
                this->writeFile( entryData );
            }

            deletePointer( reader );

        } else {
            this->writeFile( entryData );
        }
    }

    void Exporter::exportImage( FileReader* p_reader, const wxString& p_entryname ) {
        // Bail if not an image
        auto imgReader = dynamic_cast<ImageReader*>( p_reader );
        if ( !imgReader ) {
            wxLogMessage( wxString::Format( wxT( "Entry %s is not an image." ), p_entryname ) );
            return;
        }

        // Get image in wxImage
        auto imageData = imgReader->getImage( );

        if ( !imageData.IsOk( ) ) {
            wxLogMessage( wxString::Format( wxT( "imgReader->getImage( ) error in entry %s." ), p_entryname ) );
            return;
        }

        this->writeImage( imageData );
    }

    void Exporter::exportString( FileReader* p_reader, const wxString& p_entryname ) {
        // Bail if not a string
        auto strReader = dynamic_cast<StringReader*>( p_reader );
        if ( !strReader ) {
            wxLogMessage( wxString::Format( wxT( "Entry %s is not a string file." ), p_entryname ) );
            return;
        }

        // Get string
        auto string = strReader->getString( );
        if ( string.empty( ) ) {
            wxLogMessage( wxString::Format( wxT( "Entry %s is an empty string file." ), p_entryname ) );
            return;
        }

        wxString StringOut;
        for ( auto const& it : string ) {
            StringOut << wxT( "\"" ) << it.id << wxT( "\";\"" ) << it.string << L"\"\r\n";
        }

        // Convert string to byte array
        Array<byte> data( strlen( StringOut.utf8_str( ) ) );
        ::memcpy( data.GetPointer( ), StringOut.utf8_str( ), strlen( StringOut.utf8_str( ) ) );

        // Write to file
        this->writeFile( data );
    }

    void Exporter::exportEula( FileReader* p_reader, const wxString& p_entryname ) {
        auto eulaReader = dynamic_cast<EulaReader*>( p_reader );
        if ( !eulaReader ) {
            wxLogMessage( wxString::Format( wxT( "Entry %s is not a eula file." ), p_entryname ) );
            return;
        }

        // Get eula
        auto eula = eulaReader->getString( );
        // Get file name
        auto filename = m_filename.GetName( );
        // Write each record
        for ( uint i = 0; i < eula.size( ); i++ ) {
            wxString StringOut;
            StringOut << eula[i];

            // Convert string to byte array
            Array<byte> data( strlen( StringOut.utf8_str( ) ) );
            ::memcpy( data.GetPointer( ), StringOut.utf8_str( ), strlen( StringOut.utf8_str( ) ) );

            m_filename.SetName( wxString::Format( wxT( "%s_%d" ), filename, i ) );

            // Write to file
            this->writeFile( data );
        }
    }

    void Exporter::exportSound( FileReader* p_reader, const wxString& p_entryname ) {
        Array<byte> data;
        if ( m_fileType == ANFT_asndMP3 ) {
            auto asndMP3 = dynamic_cast<asndMP3Reader*>( p_reader );
            if ( !asndMP3 ) {
                wxLogMessage( wxString::Format( wxT( "Entry %s is not a sound file." ), p_entryname ) );
                return;
            }
            // Get sound data
            data = asndMP3->getMP3Data( );

        } else if (
            ( m_fileType == ANFT_PackedMP3 ) ||
            ( m_fileType == ANFT_PackedOgg )
            ) {
            auto packedSound = dynamic_cast<PackedSoundReader*>( p_reader );

            if ( !packedSound ) {
                wxLogMessage( wxString::Format( wxT( "Entry %s is not a sound file." ), p_entryname ) );
                return;
            }
            // Get sound data
            data = packedSound->getSoundData( );
        }
        // Write to file
        this->writeFile( data );
    }

    void Exporter::exportSoundBank( FileReader* p_reader, const wxString& p_entryname ) {
        auto sound = dynamic_cast<SoundBankReader*>( p_reader );
        if ( !sound ) {
            wxLogMessage( wxString::Format( wxT( "Entry %s is not a sound bank file." ), p_entryname ) );
            return;
        }

        auto data = sound->getSoundData( );
        auto filename = m_filename.GetName( );

        for ( auto const& it : data ) {
            m_filename.SetName( wxString::Format( wxT( "%s_%d" ), filename, it.voiceId ) );

            //uint16 format = *reinterpret_cast<const uint16*>( it.data.GetPointer( ) );
            // the data is either compressed or encrypted or not mp3 format
            //if ( format != FCC_MP3 ) {
            //  //m_filename.SetExt( wxT( "raw" ) );
            //  continue;
            //}

            // Write to file
            this->writeFile( it.data );
        }
    }

    void Exporter::exportModel( FileReader* p_reader, const wxString& p_entryname ) {
        // Bail if not a model
        auto modlReader = dynamic_cast<ModelReader*>( p_reader );
        if ( !modlReader ) {
            wxMessageBox( wxString::Format( wxT( "Entry %s is in wrong format." ), p_entryname ), wxT( "" ), wxOK | wxICON_WARNING );
            return;
        }

        // Get model data
        auto model = modlReader->getModel( );

        std::ostringstream stream;

        // Note: wxWidgets only does locale-specific number formatting. This does
        // not work well with obj-files.
        stream.imbue( std::locale( "C" ) );

        // get material name list first.
        std::vector<std::string> materialName;
        materialName.resize( model.numMaterial( ) );

        for ( uint i = 0; i < model.numMeshes( ); i++ ) {
            const GW2Mesh& mesh = model.mesh( i );

			if (mesh.materialIndex == -1) continue;

            materialName[mesh.materialIndex] = mesh.materialName.c_str( );
        }

        auto version = wxString::Format( "%d.%d.%d.%d", APP_MAJOR_VERSION, APP_MINOR_VERSION, APP_RELEASE_NUMBER, APP_SUBRELEASE_NUMBER );

        // ----------------
        // Export Materials
        // ----------------

        stream << "# Gw2Browser " << version << std::endl;
        stream << "# MTL File: \'" << m_filename.GetName( ) << "\'" << std::endl;
        stream << "# Material Count : " << model.numMaterial( ) << std::endl;
        stream << std::endl;

        for ( uint i = 0; i < model.numMaterial( ); i++ ) {
            const GW2Material& material = model.material( i );

            stream << "# Material " << i + 1 << std::endl;
            stream << "# Material ID : " << material.materialId << std::endl;
            stream << "# Material flags : " << material.materialFlags << std::endl;
            stream << "# Material file : " << material.materialFile << std::endl;
            if ( material.diffuseMap ) {
                stream << "# Diffuse map texture : " << material.diffuseMap << std::endl;
            }
            if ( material.normalMap ) {
                stream << "# Normal map texture : " << material.normalMap << std::endl;
            }
            if ( material.specularMap ) {
                stream << "# Specular map texture : " << material.specularMap << std::endl;
            }
            if ( material.dye ) {
                stream << "# Dye texture : " << material.dye << std::endl;
            }
            if ( material.lightMap ) {
                stream << "# Light map texture : " << material.lightMap << std::endl;
            }

            // Define new material
            if ( !materialName[i].empty( ) ) {
                stream << "newmtl " << materialName[i] << std::endl;
            } else {
                stream << "newmtl " << "mat" << i + 1 << std::endl;
            }

            // Specular exponent (ranges between 0 and 1000)
            stream << "Ns 0.000000" << std::endl;
            // Ambient color
            stream << "Ka 1.000000 1.000000 1.000000" << std::endl;
            // Diffuse color
            stream << "Kd 1.000000 1.000000 1.000000" << std::endl;
            // Specular color
            stream << "Ks 0.000000 0.000000 0.000000" << std::endl;
            // Light emit color
            stream << "Ke 0.000000 0.000000 0.000000" << std::endl;
            // Transparent
            stream << "d 1.000000" << std::endl;
            // Illumination model
            stream << "illum 2" << std::endl;

            // Load diffuse texture
            if ( material.diffuseMap ) {
                // Ambient texture map
                //stream << "map_Ka " << material.diffuseMap << ".png" << std::endl;
                // Diffuse texture map
                stream << "map_Kd " << material.diffuseMap << ".png" << std::endl;
                // Specular color texture map
                if ( material.specularMap ) {
                    stream << "map_Ks " << material.specularMap << ".png" << std::endl;
                }
            }
            // Load normal map texture
            if ( material.normalMap ) {
                // Normal map texture
                stream << "map_Bump " << material.normalMap << ".png" << std::endl;
            }
            // Load light map
            if ( material.lightMap ) {
                // Light map texture
                stream << "map_Ke " << material.lightMap << ".png" << std::endl;
            }

            // newline before next material!
            stream << std::endl;
        }

        // Material None
        stream << "newmtl " << "None" << std::endl;
        stream << "Ns 0.000000" << std::endl;
        stream << "Ka 1.000000 1.000000 1.000000" << std::endl;
        stream << "Kd 1.000000 1.000000 1.000000" << std::endl;
        stream << "Ks 0.000000 0.000000 0.000000" << std::endl;
        stream << "Ke 0.000000 0.000000 0.000000" << std::endl;
        stream << "d 1.000000" << std::endl;
        stream << "illum 2" << std::endl;

        // Close stream
        stream.flush( );
        wxString matString( stream.str( ) );
        // Clear the stream
        stream.str( std::string( ) );
        stream.clear( );

        // -------------
        // Export meshes
        // -------------

        // Export to Wavefront .obj file
        // https://en.wikipedia.org/wiki/Wavefront_.obj_file

        stream << "# Gw2Browser " << version << std::endl;
        stream << "# OBJ File: \'" << m_filename.GetName( ) << "\'" << std::endl;
        stream << "# Mesh Count : " << model.numMeshes( ) << std::endl;
        stream << "mtllib " << m_filename.GetName( ) << ".mtl" << std::endl;
        stream << std::endl;

        uint indexBase = 1;
        for ( uint i = 0; i < model.numMeshes( ); i++ ) {
            const GW2Mesh& mesh = model.mesh( i );

            // Write header
            stream << "# Mesh " << i + 1 << ": " << mesh.vertices.size( ) << " vertices, " << mesh.triangles.size( ) << " triangles" << std::endl;
            stream << "g mesh" << i + 1 << std::endl;

            // Write positions
            for ( uint j = 0; j < mesh.vertices.size( ); j++ ) {
                stream << "v " << mesh.vertices[j].position.x << ' ' << mesh.vertices[j].position.y << ' ' << mesh.vertices[j].position.z << std::endl;
            }

            // Write UVs
            if ( mesh.hasUV ) {
                for ( uint j = 0; j < mesh.vertices.size( ); j++ ) {
                    // Flip UV coordinate
                    auto u = mesh.vertices[j].uv.x;
                    auto v = 1.0f - mesh.vertices[j].uv.y;
                    stream << "vt " << u << ' ' << v << std::endl;
                }
            }

            // Write normals
            if ( mesh.hasNormal ) {
                for ( uint j = 0; j < mesh.vertices.size( ); j++ ) {
                    stream << "vn " << mesh.vertices[j].normal.x << ' ' << mesh.vertices[j].normal.y << ' ' << mesh.vertices[j].normal.z << std::endl;
                }
            }

            // If no material, use None material
            if ( mesh.materialName.empty( ) ) {
                stream << "usemtl None" << std::endl;
            } else {
                stream << "usemtl " << mesh.materialName.c_str( ) << std::endl;
            }

            // Write faces
            for ( uint j = 0; j < mesh.triangles.size( ); j++ ) {
                const Triangle& triangle = mesh.triangles[j];

                stream << 'f';
                for ( uint k = 0; k < 3; k++ ) {
                    uint index = triangle.indices[k] + indexBase;
                    stream << ' ' << index;

                    // UV reference
                    if ( mesh.hasUV ) {
                        stream << '/' << index;
                    }

                    // Normal reference
                    if ( mesh.hasNormal ) {
                        if ( !mesh.hasUV ) {
                            stream << '/';
                        }
                        stream << '/' << index;
                    }
                }
                stream << std::endl;
            }

            // newline before next mesh!
            stream << std::endl;
            indexBase += mesh.vertices.size( );
        }

        // Close stream
        stream.flush( );
        wxString meshString( stream.str( ) );
        stream.clear( );

        // --------------------
        // Write string to file
        // --------------------

        // Convert string to byte array
        Array<byte> meshData( meshString.length( ) );
        ::memcpy( meshData.GetPointer( ), meshString.c_str( ), meshString.length( ) );

        wxLogMessage( wxString::Format( wxT( "Writing %s OBJ file..." ), m_filename.GetName( ) ) );

        // Write to file
        this->writeFile( meshData );

        // --------------------------------------

        // Convert string to byte array
        Array<byte> matData( matString.length( ) );
        ::memcpy( matData.GetPointer( ), matString.c_str( ), matString.length( ) );

        // Set file extension to .mtl
        m_filename.SetExt( wxT( "mtl" ) );

        wxLogMessage( wxString::Format( wxT( "Writing %s MTL file..." ), m_filename.GetName( ) ) );

        // Write to file
        this->writeFile( matData );

        // ---------------
        // Export Textures
        // ---------------

        // Exported texture(s) is in the same directory as exported model.

        wxLogMessage( wxString::Format( wxT( "Exporting %s's textures..." ), m_filename.GetName( ) ) );

        std::vector<uint32> textureFileList;

        for ( uint i = 0; i < model.numMaterial( ); i++ ) {
            auto material = model.material( i );
            if ( material.diffuseMap ) {
                textureFileList.push_back( material.diffuseMap );
            }
            if ( material.normalMap ) {
                textureFileList.push_back( material.normalMap );
            }
            if ( material.specularMap ) {
                textureFileList.push_back( material.specularMap );
            }
            if ( material.lightMap ) {
                textureFileList.push_back( material.lightMap );
            }
            if ( material.dye ) {
                textureFileList.push_back( material.dye );
            }
        }

        std::vector<uint32>::iterator temp;

        std::sort( textureFileList.begin( ), textureFileList.end( ) );
        temp = std::unique( textureFileList.begin( ), textureFileList.end( ) );
        textureFileList.resize( std::distance( textureFileList.begin( ), temp ) );

        // Create directory if not exist
        if ( !m_filename.DirExists( ) ) {
            m_filename.Mkdir( 511, wxPATH_MKDIR_FULL );
        }

        // Extract textures
        for ( auto const& it : textureFileList ) {
            this->exportModelTexture( it );
        }

        wxLogMessage( wxString::Format( wxT( "Finish export model %s." ), m_filename.GetName( ) ) );
    }

    void Exporter::exportModelTexture( uint32 p_fileid ) {
        auto entryNumber = m_datFile.entryNumFromFileOrBaseId( p_fileid );
        auto fileData = m_datFile.readEntry( entryNumber );

        // Bail if read failed
        if ( !fileData.GetSize( ) ) {
            wxLogMessage( wxString::Format( wxT( "File id %d is empty or not exist." ), p_fileid ) );
            return;
        }

        m_filename.SetName( wxString::Format( wxT( "%d" ), p_fileid ) );
        m_filename.SetExt( wxT( "png" ) );

        if ( m_filename.FileExists( ) ) {
            wxLogMessage( wxString::Format( wxT( "File %s is already exists, skiping." ), m_filename.GetFullPath( ) ) );
            return;
        }

        // Convert to image
        ANetFileType fileType;
        m_datFile.identifyFileType( fileData.GetPointer( ), fileData.GetSize( ), fileType );
        auto reader = FileReader::readerForData( fileData, m_datFile, fileType );

        wxLogMessage( wxString::Format( wxT( "Writing texture file %s." ), m_filename.GetFullPath( ) ) );

        this->exportImage( reader, wxT( "Dummy" ) );

        deletePointer( reader );
    }

    void Exporter::exportGameContent( FileReader* p_reader, const wxString& p_entryname ) {
        auto content = dynamic_cast<ContentReader*>( p_reader );
        if ( !content ) {
            wxLogMessage( wxString::Format( wxT( "Entry %s is not GameContent file." ), p_entryname ) );
            return;
        }

        this->writeXML( content->getContentData( ) );
    }

    void Exporter::exportBitmapFont( FileReader* p_reader, const wxString& p_entryname ) {
        auto fontreader = dynamic_cast<AFNTReader*>(p_reader);
        if ( !fontreader ) {
            wxLogMessage( wxString::Format( wxT( "Entry %s is not bitmap font file." ), p_entryname ) );
            return;
        }

        m_filename.SetExt( wxT( "png" ) );

        auto fonts = fontreader->getFont( );
        for ( auto const& it : fonts ) {
            // export each glyphs
            for ( auto const& it2 : it.glyphs ) {
                auto glyph = it2.data;

                m_filename.SetName( wxString::Format( wxT( "%s_%d_%d" ), p_entryname, it.id, it2.character ) );

                if ( m_filename.FileExists( ) ) {
                    wxLogMessage( wxString::Format( wxT( "File %s is already exists, skiping." ), m_filename.GetFullPath( ) ) );
                    return;
                }

                this->writeImage( glyph );
            }
        }
    }

    void Exporter::writeImage( wxImage p_image ) {
        // Write the png to memory
        wxMemoryOutputStream stream;
        if ( !p_image.SaveFile( stream, wxBITMAP_TYPE_PNG ) ) {
            wxMessageBox( wxT( "Failed to write png to memory." ), wxT( "Error" ), wxOK | wxICON_ERROR );
            return;
        }

        // Reset the position of the stream
        auto buffer = stream.GetOutputStreamBuffer( );
        buffer->Seek( 0, wxFromStart );

        // Read the data from the stream and into a buffer
        Array<byte> data( buffer->GetBytesLeft( ) );
        buffer->Read( data.GetPointer( ), data.GetSize( ) );

        // Write to file
        this->writeFile( data );
    }

    void Exporter::writeXML( std::unique_ptr<tinyxml2::XMLDocument> p_xml ) {
        tinyxml2::XMLError eResult = p_xml->SaveFile( m_filename.GetFullPath( ).c_str( ) );
        if ( eResult != tinyxml2::XML_SUCCESS ) {
            wxLogMessage( wxT( "XML error: %i" ), eResult );
        }
    }

    bool Exporter::writeFile( const Array<byte>& p_data ) {
        // Open file for writing
        wxFile file( m_filename.GetFullPath( ), wxFile::write );
        if ( file.IsOpened( ) ) {
            file.Write( p_data.GetPointer( ), p_data.GetSize( ) );
        } else {
            wxMessageBox( wxString::Format( wxT( "Failed to open the file %s for writing." ), m_filename.GetFullPath( ) ),
                wxT( "Error" ),
                wxOK | wxICON_ERROR );
            wxLogMessage( wxString::Format( wxT( "Failed to open the file %s for writing." ), m_filename.GetFullPath( ) ) );
            return false;
        }
        file.Close( );
        return true;
    }

    void Exporter::appendPaths( wxFileName& p_path, const DatIndexCategory& p_category ) {
        auto parent = p_category.parent( );
        if ( parent ) {
            this->appendPaths( p_path, *parent );
        }
        p_path.AppendDir( p_category.name( ) );
    }

}; // namespace gw2b
