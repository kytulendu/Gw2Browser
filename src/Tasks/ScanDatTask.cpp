/** \file       ScanDatTask.cpp
 *  \brief      Contains declaration of the ScanDatTask class.
 *  \author     Rhoot
 */

/**
 * Copyright (C) 2014-2019 Khralkatorrix <https://github.com/kytulendu>
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

#include "ScanDatTask.h"

#include "DatFile.h"
#include "DatIndex.h"
#include "FileReader.h"

namespace gw2b {

    ScanDatTask::ScanDatTask( const std::shared_ptr<DatIndex>& p_index, DatFile& p_datFile )
        : m_index( p_index )
        , m_datFile( p_datFile ) {
        Ensure::notNull( p_index.get( ) );
        Ensure::notNull( &p_datFile );
    }

    ScanDatTask::~ScanDatTask( ) {
    }

    bool ScanDatTask::init( ) {
        this->setMaxProgress( m_datFile.numFiles( ) );
        this->setCurrentProgress( m_index->highestMftEntry( ) + 1 );

        uint filesLeft = m_datFile.numFiles( ) - ( m_index->highestMftEntry( ) + 1 );
        m_index->reserveEntries( filesLeft );

        return true;
    }

    void ScanDatTask::perform( ) {
        uint bytetoread = 32;
        // Make sure the output buffer is big enough
        this->ensureBufferSize( bytetoread );

        // Read file
        uint32 entryNumber = this->currentProgress( );
        uint size = m_datFile.peekFile( entryNumber, bytetoread, m_outputBuffer.GetPointer( ) );

        // Skip if empty
        if ( !size ) {
            this->setCurrentProgress( entryNumber + 1 );
            return;
        }

        // Get the file type
        ANetFileType fileType;
        auto results = m_datFile.identifyFileType( m_outputBuffer.GetPointer( ), size, fileType );

        // Enough data to identify the file type?
        uint lastRequestedSize = bytetoread;
        while ( results == DatFile::IR_NotEnoughData ) {
            uint sizeRequired = this->requiredIdentificationSize( m_outputBuffer.GetPointer( ), size, fileType );

            // Prevent infinite loops
            if ( sizeRequired == lastRequestedSize ) {
                break;
            }
            lastRequestedSize = sizeRequired;

            // Re-read with the newly asked-for size
            this->ensureBufferSize( sizeRequired );
            size = m_datFile.peekFile( entryNumber, sizeRequired, m_outputBuffer.GetPointer( ) );
            results = m_datFile.identifyFileType( m_outputBuffer.GetPointer( ), size, fileType );
        }

        // Need another check, since the file might have been reloaded a couple of times
        if ( !size ) {
            this->setCurrentProgress( entryNumber + 1 );
            return;
        }

        // Categorize the entry
        auto category = this->categorize( fileType, m_outputBuffer.GetPointer( ), size );

        // Add to index
        uint baseId = m_datFile.baseIdFromFileNum( entryNumber );
        auto& newEntry = m_index->addIndexEntry( )
            ->setBaseId( baseId )
            .setFileId( m_datFile.fileIdFromFileNum( entryNumber ) )
            .setFileType( fileType )
            .setMftEntry( entryNumber )
            .setName( wxString::Format( wxT( "%d" ), baseId ) );
        // Found a file with no baseId...
        if ( baseId == 0 ) {
            newEntry.setName( wxString::Format( wxT( "ID-less_%d" ), entryNumber ) );
        }
        // Finalize the add
        category->addEntry( &newEntry );
        newEntry.finalizeAdd( );

        // Delete the reader and proceed to the next file
        this->setText( wxString::Format( wxT( "Scanning .dat: %d/%d" ), entryNumber, this->maxProgress( ) ) );
        this->setCurrentProgress( entryNumber + 1 );
    }

    uint ScanDatTask::requiredIdentificationSize( const byte* p_data, size_t p_size, ANetFileType p_fileType ) {
        switch ( p_fileType ) {
        case ANFT_Binary:
            if ( p_size >= 0x40 ) {
                return *reinterpret_cast<const uint32*>( p_data + 0x3c ) + 0x18;
            } else {
                return 0x140;   // Seems true for most of them
            }
        case ANFT_Sound:
            return 0x80;        // 128 byte
        default:
            return 0x20;
        }
    }

    bool ScanDatTask::isBitmapFontChunk(uint p_baseId)
    {
        // Hard coded file list from Bitmap Font (AFNT), file number 154945
        uint chunklist[] =
        {
            154824, 154825, 154826, 154827, 154828, 154829, 154842, 154843,
            154844, 154845, 154846, 154876, 154877, 154878, 154879, 154880,
            154881, 154882, 154883, 154884, 154885, 154886, 154887, 154889,
            154890, 154891, 154892, 154893, 154894, 154895, 154896, 154898,
            154899, 154900, 154901, 154902, 154903, 154904, 154905, 154906,
            154907, 154908, 154909, 154910, 154911, 154912, 154913, 154914,
            154915, 154916, 154917, 154918, 154919, 154920, 154921, 154922,
            154923, 154924, 154925, 154926, 154927, 154928, 154929, 154930,
            154931, 154932, 154933, 154934, 154935, 154936, 154937, 154938,
            154939, 154940, 154941, 154942, 154944, 439864, 439865, 439866,
            439867, 439868, 439869, 439870, 439871, 439872, 439873, 439874,
            439875, 439876, 439877, 439878, 439879, 439880, 439881, 439882,
            439883, 439884, 439885, 439886, 439887, 439896, 439897, 439898,
            439899, 439900, 439901, 439902, 439903, 439904, 439905, 439906,
            439907, 439908, 439909, 439910, 439911, 439912, 439913, 439914,
            439915, 439916, 439917, 439918, 439919, 439920, 439921, 439922,
            439923, 439924, 439925, 439926, 439927, 439928, 439929, 439930,
            439931, 439932, 439933, 439934, 439935, 439936, 439937, 439938,
            439939, 439940, 439941, 439942, 439943, 439944, 439945, 439946,
            439947, 439948, 439949, 439950, 439951, 439952, 439953, 439954,
            439955, 439956, 439957, 439958, 439959, 439960, 439961, 439962,
            439963, 439964, 439965, 439966, 439967, 439968, 439969, 439970,
            439971, 439972, 439973, 439974, 439975, 439976, 439977, 439978,
            439979, 439980, 439981, 439982, 439983, 439984, 439985, 439986,
            439987, 439988, 439989, 439990, 439991, 439992, 439993, 439994,
            439995, 439996, 439997, 439998, 439999, 459802, 459803, 459804,
            459805, 459806, 459807, 459808, 459809, 459926, 459935, 459981,
            858064, 858065, 858066, 858067, 858068, 858069, 858070, 858071,
            858072, 858073, 858074, 858075, 858076, 858077, 858078, 858079,
            858080, 858081, 858082, 858083, 858084, 858085, 858086, 858087,
            858088, 858089, 858090, 858091, 858092, 858093, 858094, 858095,
            858096, 858097, 858098, 858099, 858100, 858101, 858102, 858103,
            858104, 858105, 858106, 858107, 858108, 858109, 858110, 858111,
            858112, 858113, 858114, 858115, 858116, 858117, 858118, 858119,
            858120, 858121, 858122, 858123, 858124, 858125, 858126, 858127,
            858128, 858129, 858130, 858131, 858132, 858133, 858134, 858135
        };

        auto numFiles = sizeof(chunklist)/sizeof(*chunklist);

        for (uint i = 0; i < numFiles; i++)
            if (p_baseId == chunklist[i])
                return true;

        return false;
    }

#define MakeCategory(x)     { category = m_index->findOrAddCategory(x); }
#define MakeSubCategory(x)  { category = category->findOrAddSubCategory(x); }
    DatIndexCategory* ScanDatTask::categorize( ANetFileType p_fileType, const byte* p_data, size_t p_size ) {
        DatIndexCategory* category = nullptr;

        switch ( p_fileType ) {
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
            MakeCategory( wxT( "Textures" ) );

            switch ( p_fileType ) {
            case ANFT_ATEX:
                MakeSubCategory( wxT( "Generic Textures" ) );
                break;
            case ANFT_ATTX:
                MakeSubCategory( wxT( "Terrain Textures" ) );
                break;
            case ANFT_ATEC:
                MakeSubCategory( wxT( "ATEC" ) );
                break;
            case ANFT_ATEP:
                MakeSubCategory( wxT( "Map Textures" ) );
                break;
            case ANFT_ATEU:
                MakeSubCategory( wxT( "UI Textures" ) );
                break;
            case ANFT_ATET:
                MakeSubCategory( wxT( "ATET" ) );
                break;
            case ANFT_DDS:
                MakeSubCategory( wxT( "DDS" ) );
                break;
            case ANFT_JPEG:
                MakeSubCategory( wxT( "JPEG" ) );
                break;
            case ANFT_WEBP:
                MakeSubCategory( wxT( "WebP" ) );
                break;
            case ANFT_PNG:
                MakeSubCategory( wxT( "PNG" ) );
                break;
            }

            if ( ( p_fileType == ANFT_ATEX || p_fileType == ANFT_ATTX || p_fileType == ANFT_ATEC ||
                p_fileType == ANFT_ATEP || p_fileType == ANFT_ATEU || p_fileType == ANFT_ATET ) ) {
                uint16 width = *reinterpret_cast<const uint16*>( p_data + 8 );
                uint16 height = *reinterpret_cast<const uint16*>( p_data + 10 );
                MakeSubCategory( wxString::Format( wxT( "%ux%u" ), width, height ) );
            } else if ( p_fileType == ANFT_DDS && p_size >= 20 ) {
                uint32 width = *reinterpret_cast<const uint32*>( p_data + 16 );
                uint32 height = *reinterpret_cast<const uint32*>( p_data + 12 );
                MakeSubCategory( wxString::Format( wxT( "%ux%u" ), width, height ) );
            }

            break;
        case ANFT_Sound:
        case ANFT_MP3:
        case ANFT_Ogg:
        case ANFT_PackedMP3:
        case ANFT_PackedOgg:
        case ANFT_asndMP3:
        case ANFT_asndOgg:
            MakeCategory( wxT( "Sounds" ) );

            switch ( p_fileType ) {
            case ANFT_MP3:
                MakeSubCategory( wxT( "MP3" ) );
                break;
            case ANFT_Ogg:
                MakeSubCategory( wxT( "Ogg" ) );
                break;
            case ANFT_asndMP3:
                MakeSubCategory( wxT( "asndMP3" ) );
                break;
            case ANFT_asndOgg:
                MakeSubCategory( wxT( "asndOgg" ) );
                break;
            case ANFT_PackedMP3:
                MakeSubCategory( wxT( "PackedMP3" ) );
                break;
            case ANFT_PackedOgg:
                MakeSubCategory( wxT( "PackedOgg" ) );
                break;
            }
            break;

        case ANFT_Binary:
        case ANFT_EXE:
        case ANFT_DLL:
            MakeCategory( wxT( "Binaries" ) );
            break;

        case ANFT_StringFile:
        {
            MakeCategory( wxT( "Strings" ) );

            uint32 entryNumber = this->currentProgress( );

            auto buffer = allocate<byte>( m_datFile.fileSize( entryNumber ) );
            auto size = m_datFile.readFile( entryNumber, buffer );

            // strs file format that have to read near end of file to know what language is
            auto end = buffer + size - 2;
            auto language = static_cast<uint32>( *end );

            switch ( language ) {
            case language::English:
                MakeSubCategory( wxT( "English" ) );
                break;
            case language::Korean:
                MakeSubCategory( wxT( "Korean" ) );
                break;
            case language::French:
                MakeSubCategory( wxT( "French" ) );
                break;
            case language::German:
                MakeSubCategory( wxT( "German" ) );
                break;
            case language::Spanish:
                MakeSubCategory( wxT( "Spanish" ) );
                break;
            case language::Chinese:
                MakeSubCategory( wxT( "Chinese" ) );
                break;
            default:
                MakeSubCategory( wxT( "Unknown" ) );
                MakeSubCategory( wxString::Format( wxT( "%u" ), language ) );
            }
            freePointer( buffer );
            break;
        }
        case ANFT_Manifest:
            MakeCategory( wxT( "Manifests" ) );
            break;
        case ANFT_TEXT:
        case ANFT_UTF8:
            MakeCategory( wxT( "Text" ) );
            break;
        case ANFT_TextPackManifest:
            MakeCategory( wxT( "TextPack Manifests" ) );
            break;
        case ANFT_TextPackVariant:
            MakeCategory( wxT( "TextPack Variant" ) );
            break;
        case ANFT_TextPackVoices:
            MakeCategory( wxT( "TextPack Voices" ) );
            break;
        case ANFT_Bank:
            MakeCategory( wxT( "Soundbank" ) );
            break;
        case ANFT_BankIndex:
            MakeCategory( wxT( "Soundbank Index" ) );
            break;
        case ANFT_AudioScript:
            MakeCategory( wxT( "Audio Scripts" ) );
            break;
		case ANFT_Model: 
		{
			MakeCategory(wxT("Models"));
			uint baseId = m_datFile.baseIdFromFileNum(this->currentProgress());
			MakeSubCategory(wxString::Format(wxT("%i"), ((uint32)baseId / 10000)) + wxT("xxxx"));
			break;
		}
        case ANFT_ModelCollisionManifest:
            MakeCategory( wxT( "Model Collision Manifest" ) );
            break;
        case ANFT_DependencyTable:
            MakeCategory( wxT( "Dependency Tables" ) );
            break;
        case ANFT_EULA:
            MakeCategory( wxT( "EULA" ) );
            break;
        case ANFT_Cinematic:
            MakeCategory( wxT( "Cinematics" ) );
            break;
        case ANFT_MapCollision:
            MakeCategory( wxT( "Map Collision" ) );
            break;
        case ANFT_GameContent:
            MakeCategory( wxT( "Game Content" ) );
            break;
        case ANFT_GameContentPortalManifest:
            MakeCategory( wxT( "Game Content Portal Manifest" ) );
            break;
        case ANFT_MapParam:
            MakeCategory( wxT( "Map" ) );
            break;
        case ANFT_MapShadow:
            MakeCategory( wxT( "Map Shadow" ) );
            break;
        case ANFT_MapMetadata:
            MakeCategory( wxT( "Map Metadata" ) );
            break;
        case ANFT_PagedImageTable:
            MakeCategory( wxT( "Paged Image Table" ) );
            break;
        case ANFT_Material:
            MakeCategory( wxT( "Materials" ) );
            break;
        case ANFT_Composite:
            MakeCategory( wxT( "Composite Data" ) );
            break;
        case ANFT_AnimSequences:
            MakeCategory( wxT( "Animation Sequences" ) );
            break;
        case ANFT_EmoteAnimation:
            MakeCategory( wxT( "Emote Animations" ) );
            break;
        case ANFT_FontFile:
            MakeCategory( wxT( "Font" ) );
            break;
        case ANFT_BitmapFontFile:
            MakeCategory( wxT( "Bitmap Font" ) );
            break;
        case ANFT_Bink2Video:
            MakeCategory( wxT( "Bink Videos" ) );
            break;
        case ANFT_ShaderCache:
            MakeCategory( wxT( "Shader Cache" ) );
            break;
        case ANFT_Config:
            MakeCategory( wxT( "Configuration" ) );
            break;
        case ANFT_PF:
        case ANFT_ARAP:
            MakeCategory( wxT( "Misc" ) );

            if ( p_fileType == ANFT_PF && p_size >= 12 ) {
                MakeSubCategory( wxString( reinterpret_cast<const char*>( p_data + 8 ), 4 ) );
            }
            break;
        default:
        {
            uint32 entryNumber = this->currentProgress();
            uint baseId = m_datFile.baseIdFromFileNum(entryNumber);
            //auto fileId = m_datFile.fileIdFromFileNum(entryNumber); // uint
            if (isBitmapFontChunk(baseId))
            {
                MakeCategory(wxT("Bitmap Font"));
                MakeSubCategory(wxT("Chunk"));
            }
            else
            {
                MakeCategory( wxT( "Unknown" ) );
            }
        }
        } // switch (p_fileType)

        return category;
    }

    void ScanDatTask::ensureBufferSize( size_t p_size ) {
        if ( m_outputBuffer.GetSize( ) < p_size ) {
            m_outputBuffer.SetSize( p_size );
        }
    }

}; // namespace gw2b
