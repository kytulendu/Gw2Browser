/** \file       DatFile.cpp
 *  \brief      Contains declaration of the class representing the .dat file.
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

#include <gw2dattools/exception/Exception.h>

#include "FileReader.h"

#include "DatFile.h"

namespace gw2b {

    struct DatFile::IdEntry {
        uint32  baseId;
        uint32  fileId;
    };

    DatFile::DatFile( )
        : m_lastReadEntry( -1 ) {
        ::memset( &m_datHead, 0, sizeof( m_datHead ) );
        ::memset( &m_mftHead, 0, sizeof( m_mftHead ) );
    }

    DatFile::DatFile( const wxString& p_filename )
        : m_lastReadEntry( -1 ) {
        ::memset( &m_datHead, 0, sizeof( m_datHead ) );
        ::memset( &m_mftHead, 0, sizeof( m_mftHead ) );
        this->open( p_filename );
    }

    DatFile::~DatFile( ) {
        this->close( );
    }

    bool DatFile::open( const wxString& p_filename ) {
        this->close( );

        while ( true ) {
            // Open file
            m_file.Open( p_filename );
            if ( !m_file.IsOpened( ) ) {
                break;
            }

            // Read header
            if ( static_cast<size_t>( m_file.Length( ) ) < sizeof( m_datHead ) ) {
                break;
            }
            m_file.Read( &m_datHead, sizeof( m_datHead ) );

            // Read MFT Header
            if ( static_cast<uint64>( m_file.Length( ) ) < m_datHead.mftOffset + m_datHead.mftSize ) {
                break;
            }
            m_file.Seek( m_datHead.mftOffset, wxFromStart );
            m_file.Read( &m_mftHead, sizeof( m_mftHead ) );

            // Read all of the MFT
            if ( m_datHead.mftSize != m_mftHead.numEntries * sizeof( ANetMftEntry ) ) {
                break;
            }
            if ( m_datHead.mftSize % sizeof( ANetMftEntry ) ) {
                break;
            }

            m_mftEntries.SetSize( m_datHead.mftSize / sizeof( ANetMftEntry ) );
            m_file.Seek( m_datHead.mftOffset, wxFromStart );
            m_file.Read( m_mftEntries.GetPointer( ), m_datHead.mftSize );

            // Read the file id entry table
            if ( static_cast<uint64>( m_file.Length( ) ) < m_mftEntries[2].offset + m_mftEntries[2].size ) {
                break;
            }
            if ( m_mftEntries[2].size % sizeof( ANetFileIdEntry ) ) {
                break;
            }

            uint numFileIdEntries = m_mftEntries[2].size / sizeof( ANetFileIdEntry );
            Array<ANetFileIdEntry> fileIdTable( numFileIdEntries );
            m_file.Seek( m_mftEntries[2].offset, wxFromStart );
            m_file.Read( fileIdTable.GetPointer( ), m_mftEntries[2].size );

            // Extract the entry -> base/file ID tables
            m_entryToId.SetSize( m_mftEntries.GetSize( ) );
            ::memset( m_entryToId.GetPointer( ), 0, m_entryToId.GetByteSize( ) );

            for ( uint i = 0; i < numFileIdEntries; i++ ) {
                if ( fileIdTable[i].fileId == 0 && fileIdTable[i].mftEntryIndex == 0 ) {
                    continue;
                }

                uint entryIndex = fileIdTable[i].mftEntryIndex;
                auto& entry = m_entryToId[entryIndex];

                if ( entry.baseId == 0 ) {
                    entry.baseId = fileIdTable[i].fileId;
                } else if ( entry.fileId == 0 ) {
                    entry.fileId = fileIdTable[i].fileId;
                }

                if ( entry.baseId > 0 && entry.fileId > 0 ) {
                    if ( entry.baseId > entry.fileId ) {
                        std::swap( entry.baseId, entry.fileId );
                    }
                }
            }

            // Success!
            return true;
        }

        this->close( );
        return false;
    }

    bool DatFile::isOpen( ) const {
        return m_file.IsOpened( );
    }

    void DatFile::close( ) {
        // Clear input buffer and lookup tables
        m_inputBuffer.Clear( );
        m_entryToId.Clear( );

        // Clear PODs
        ::memset( &m_datHead, 0, sizeof( m_datHead ) );
        ::memset( &m_mftHead, 0, sizeof( m_mftHead ) );

        // Remove MFT entries and close the file
        m_mftEntries.Clear( );
        m_file.Close( );
    }

    uint DatFile::entrySize( uint p_entryNum ) {
        if ( !isOpen( ) ) {
            return std::numeric_limits<uint>::max( );
        }
        if ( p_entryNum >= m_mftEntries.GetSize( ) ) {
            return std::numeric_limits<uint>::max( );
        }

        auto& entry = m_mftEntries[p_entryNum];

        // If the entry is compressed we need to read the uncompressed size from the .dat
        if ( entry.compressionFlag & ANCF_Compressed ) {
            uint32 uncompressedSize = 0;
            m_file.Seek( entry.offset + 4, wxFromStart );
            m_file.Read( &uncompressedSize, sizeof( uncompressedSize ) );
            return uncompressedSize;
        }

        return entry.size;
    }

    uint DatFile::fileSize( uint p_fileNum ) {
        return this->entrySize( p_fileNum + MFT_FILE_OFFSET );
    }

    uint DatFile::entryNumFromFileOrBaseId( uint p_Id ) const {
		if (!isOpen()) {
			return std::numeric_limits<uint>::max();
		}

		for (uint i = 0; i < m_entryToId.GetSize(); i++) {
			uint fileId = (m_entryToId[i].fileId == 0 ? m_entryToId[i].baseId : m_entryToId[i].fileId);
			if (fileId == p_Id) {
				return i;
			}
		}
		
		for (uint i = 0; i < m_entryToId.GetSize(); i++) {
			if (m_entryToId[i].baseId == p_Id) {
				return i;
			}
		}
		return std::numeric_limits<uint>::max();
    }

    uint DatFile::fileIdFromEntryNum( uint p_entryNum ) const {
        if ( !isOpen( ) ) {
            return std::numeric_limits<uint>::max( );
        }
        if ( p_entryNum >= m_entryToId.GetSize( ) ) {
            return std::numeric_limits<uint>::max( );
        }
        return ( m_entryToId[p_entryNum].fileId == 0 ? m_entryToId[p_entryNum].baseId : m_entryToId[p_entryNum].fileId );
    }

    uint DatFile::fileIdFromFileNum( uint p_fileNum ) const {
        return this->fileIdFromEntryNum( p_fileNum + MFT_FILE_OFFSET );
    }

    uint DatFile::baseIdFromEntryNum( uint p_entryNum ) const {
        if ( !isOpen( ) ) {
            return std::numeric_limits<uint>::max( );
        }
        if ( p_entryNum >= m_entryToId.GetSize( ) ) {
            return std::numeric_limits<uint>::max( );
        }
        return m_entryToId[p_entryNum].baseId;
    }

    uint DatFile::baseIdFromFileNum( uint p_fileNum ) const {
        return this->baseIdFromEntryNum( p_fileNum + MFT_FILE_OFFSET );
    }

    uint DatFile::peekFile( uint p_fileNum, uint p_peekSize, byte* po_Buffer ) {
        return this->peekEntry( p_fileNum + MFT_FILE_OFFSET, p_peekSize, po_Buffer );
    }

    uint DatFile::peekEntry( uint p_entryNum, uint p_peekSize, byte* po_Buffer ) {
        Ensure::notNull( po_Buffer );
        uint inputSize;

        // Return instantly if size is 0, or if the file isn't open
        if ( p_peekSize == 0 || !this->isOpen( ) ) {
            return 0;
        }

        // If this was the last entry we read, there's no need to re-read it. The
        // input buffer should already contain the full file.
        if ( m_lastReadEntry != p_entryNum ) {
            // Perform some checks
            auto entryIsInRange = m_mftHead.numEntries > ( uint ) p_entryNum;
            if ( !entryIsInRange ) {
                return 0;
            }

            auto entryIsInUse = ( m_mftEntries[p_entryNum].entryFlags & ANMEF_InUse );
            auto fileIsLargeEnough = ( uint64 ) m_file.Length( ) >= m_mftEntries[p_entryNum].offset + m_mftEntries[p_entryNum].size;
            if ( !entryIsInUse || !fileIsLargeEnough ) {
                return 0;
            }

            // Make sure we can re-use the input buffer
            inputSize = m_mftEntries[p_entryNum].size;
            if ( m_inputBuffer.GetSize( ) < inputSize ) {
                m_inputBuffer.SetSize( inputSize );
            }

            // Read the file data
            m_file.Seek( m_mftEntries[p_entryNum].offset, wxFromStart );
            m_file.Read( m_inputBuffer.GetPointer( ), inputSize );
            m_lastReadEntry = p_entryNum;
        } else {
            inputSize = m_mftEntries[p_entryNum].size;
        }

        // If the file is compressed we need to uncompress it
        if ( m_mftEntries[p_entryNum].compressionFlag ) {
            uint32 outputSize = p_peekSize;
            try {
                gw2dt::compression::inflateDatFileBuffer( inputSize, m_inputBuffer.GetPointer( ), outputSize, po_Buffer );
            } catch ( const gw2dt::exception::Exception& err ) {
                wxLogMessage( wxT( "Failed to decompress file %u: %s" ), p_entryNum, std::string( err.what( ) ) );
                outputSize = 0;
            }
            return outputSize;
        } else {
            const uint dataSize = wxMin( p_peekSize, inputSize );

            const uint blockSize = 65536;
            const uint blockDataSize = 65532;
            // skip 4 byte every 65532 byte, this is hash?
            const uint bytetoskip = 4;
            // how many block in data?
            // (round down so if the data less than 65532, it will skip the copy block loop)
            const uint numBlock = floor( dataSize / blockSize );

#pragma omp parallel for
            for ( int i = 0; i < static_cast<int>( numBlock ); i++ ) {
                ::memcpy( &po_Buffer[i * blockDataSize], &m_inputBuffer[( i * blockDataSize ) + ( i * bytetoskip )], blockDataSize );
            }

            const auto dataReaded = numBlock * blockSize;
            const auto dataRemain = dataSize - dataReaded;

            // copy the last remaining data
            if ( dataRemain ) {
                auto outBufferIndex = blockDataSize * numBlock;
                ::memcpy( &po_Buffer[outBufferIndex], &m_inputBuffer[dataReaded], dataRemain );
            }

            return sizeof( po_Buffer );
        }
    }

    Array<byte> DatFile::peekFile( uint p_fileNum, uint p_peekSize ) {
        return this->peekEntry( p_fileNum + MFT_FILE_OFFSET, p_peekSize );
    }

    Array<byte> DatFile::peekEntry( uint p_entryNum, uint p_peekSize ) {
        Array<byte> output = Array<byte>( p_peekSize );
        uint readBytes = this->peekEntry( p_entryNum, p_peekSize, output.GetPointer( ) );

        if ( readBytes == 0 ) {
            return Array<byte>( );
        }

        return output;
    }

    uint DatFile::readFile( uint p_fileNum, byte* po_Buffer ) {
        return this->readEntry( p_fileNum + MFT_FILE_OFFSET, po_Buffer );
    }

    uint DatFile::readEntry( uint p_entryNum, byte* po_Buffer ) {
        uint size = this->entrySize( p_entryNum );

        if ( size != std::numeric_limits<uint>::max( ) ) {
            return this->peekEntry( p_entryNum, size, po_Buffer );
        }

        return 0;
    }

    Array<byte> DatFile::readFile( uint p_fileNum ) {
        return this->readEntry( p_fileNum + MFT_FILE_OFFSET );
    }

    Array<byte> DatFile::readEntry( uint p_entryNum ) {
        uint size = this->entrySize( p_entryNum );
        Array<byte> output;

        if ( size != std::numeric_limits<uint>::max( ) ) {
            output.SetSize( size );
            uint readBytes = this->peekEntry( p_entryNum, size, output.GetPointer( ) );

            if ( readBytes > 0 ) {
                return output;
            }
        }

        return Array<byte>( );
    }

    DatFile::IdentificationResult DatFile::identifyFileType( const byte* p_data, size_t p_size, ANetFileType& po_fileType ) {
        po_fileType = ANFT_Unknown;

        if ( p_size < 4 ) {
            return IR_Failure;
        }

        // start with fourcc
        auto fourcc = *reinterpret_cast<const uint32*>( p_data );
        if ( fourcc == FCC_ATEX ) {
            po_fileType = ANFT_ATEX;

        } else if ( fourcc == FCC_ATTX ) {
            po_fileType = ANFT_ATTX;

        } else if ( fourcc == FCC_ATEC ) {
            po_fileType = ANFT_ATEC;

        } else if ( fourcc == FCC_ATEP ) {
            po_fileType = ANFT_ATEP;

        } else if ( fourcc == FCC_ATEU ) {
            po_fileType = ANFT_ATEU;

        } else if ( fourcc == FCC_ATET ) {
            po_fileType = ANFT_ATET;

        } else if ( fourcc == FCC_DDS ) {
            po_fileType = ANFT_DDS;

        } else if ( fourcc == FCC_strs ) {
            po_fileType = ANFT_StringFile;

        } else if ( fourcc == FCC_asnd ) {
            po_fileType = ANFT_Sound;
            if ( p_size >= 12 ) {
                auto format = *reinterpret_cast<const byte*>( p_data + 8 );

                // all of files of this type is MP3 format, but who know.
                switch ( format ) {
                case 0x01:
                    po_fileType = ANFT_asndMP3;
                    break;
                case 0x02:
                    po_fileType = ANFT_asndOgg;
                    break;
                }
            } else {
                return IR_NotEnoughData;
            }

        } else if ( fourcc == FCC_OggS ) {
            po_fileType = ANFT_Ogg;

        } else if ( fourcc == FCC_TTF ) {
            po_fileType = ANFT_FontFile;

        } else if ( fourcc == FCC_RIFF ) {
            po_fileType = ANFT_RIFF;
            auto format = *reinterpret_cast<const uint32*>( p_data + 8 );
            switch ( format ) {
            case FCC_WEBP:
                po_fileType = ANFT_WEBP;
                break;
            }

        } else if ( fourcc == FCC_ARAP ) {
            po_fileType = ANFT_ARAP;

        } else if ( fourcc == FCC_PNG ) {
            po_fileType = ANFT_PNG;

        } else if ( ( fourcc & 0xffff ) == FCC_PF ) {   // Identify PF files
            po_fileType = ANFT_PF;

            fourcc = *reinterpret_cast<const uint32*>( p_data + 8 );

            switch ( fourcc ) {
            case FCC_ARMF:
                po_fileType = ANFT_Manifest;
                break;
            case FCC_txtm:
                po_fileType = ANFT_TextPackManifest;
                break;
            case FCC_txtV:
                po_fileType = ANFT_TextPackVariant;
                break;
            case FCC_txtv:
                po_fileType = ANFT_TextPackVoices;
                break;
            case FCC_ASND:
                po_fileType = ANFT_Sound;
                // 92 is offset to sound data
                if ( p_size >= 92 ) {
                    auto format = *reinterpret_cast<const byte*>( p_data + 68 );

                    switch ( format ) {
                    case 0x01:
                        po_fileType = ANFT_PackedMP3;
                        break;
                    case 0x02:
                        po_fileType = ANFT_PackedOgg;
                        break;
                    }
                } else {
                    return IR_NotEnoughData;
                }
                break;
            case FCC_ABNK:
                po_fileType = ANFT_Bank;
                break;
            case FCC_ABIX:
                po_fileType = ANFT_BankIndex;
                break;
            case FCC_AMSP:
                po_fileType = ANFT_AudioScript;
                break;
            case FCC_MODL:
                po_fileType = ANFT_Model;
                break;
            case FCC_cmaC:
                po_fileType = ANFT_ModelCollisionManifest;
                break;
            case FCC_DEPS:
                po_fileType = ANFT_DependencyTable;
                break;
            case FCC_EULA:
                po_fileType = ANFT_EULA;
                break;
            case FCC_cntc:
                po_fileType = ANFT_GameContent;
                break;
            case FCC_prlt:
                po_fileType = ANFT_GameContentPortalManifest;
                break;
            case FCC_hvkC:
                po_fileType = ANFT_MapCollision;
                break;
            case FCC_mapc:
                po_fileType = ANFT_MapParam;
                break;
            case FCC_mpsd:
                po_fileType = ANFT_MapShadow;
                break;
            case FCC_mMet:
                po_fileType = ANFT_MapMetadata;
                break;
            case FCC_PIMG:
                po_fileType = ANFT_PagedImageTable;
                break;
            case FCC_AMAT:
                po_fileType = ANFT_Material;
                break;
            case FCC_cmpc:
                po_fileType = ANFT_Composite;
                break;
            case FCC_anic:
                po_fileType = ANFT_AnimSequences;
                break;
            case FCC_emoc:
                po_fileType = ANFT_EmoteAnimation;
                break;
            case FCC_CINP:
                po_fileType = ANFT_Cinematic;
                break;
            case FCC_CDHS:
                po_fileType = ANFT_ShaderCache;
                break;
            case FCC_locl:
                po_fileType = ANFT_Config;
                break;
            case FCC_AFNT:
                po_fileType = ANFT_BitmapFontFile;
                break;
            }

        } else if ( ( fourcc & 0xffff ) == FCC_MZ ) {   // Identify binary files
            po_fileType = ANFT_Binary;

            if ( p_size >= 0x40 ) {
                auto peOffset = *reinterpret_cast<const uint32*>( p_data + 0x3c );

                if ( p_size >= ( peOffset + 0x18 ) ) {
                    auto flags = *reinterpret_cast<const uint16*>( p_data + peOffset + 0x16 );
                    po_fileType = ( flags & 0x2000 ) ? ANFT_DLL : ANFT_EXE;
                } else {
                    return IR_NotEnoughData;
                }
            } else {
                return IR_NotEnoughData;
            }

        } else if ( ( fourcc & 0xffffff ) == FCC_BINK2 ) {
            po_fileType = ANFT_Bink2Video;

        } else if ( ( fourcc & 0xffffff ) == FCC_ID3 ) {
            po_fileType = ANFT_MP3;

        } else if ( ( fourcc & 0xffffff ) == FCC_JPEG ) {   // Identify JPEG files
            po_fileType = ANFT_JPEG;

        } else if ( ( fourcc & 0xffffff ) == FCC_UTF8 ) {
            po_fileType = ANFT_UTF8;

        } else {
            int result = 0;

            // Printable character detection in files for detect text files.
            for ( uint i = 0; i < p_size; i++ ) {
                auto c = p_data[i];
                if ( isprint( c ) || isspace( c ) ) {
                    result = 1;
                } else {
                    result = 0;
                    break;
                }
            }
            // All byte in the file is printable and space characters
            if ( result ) {
                po_fileType = ANFT_TEXT;
            }

        }

        return IR_Success;
    }

    uint DatFile::fileIdFromFileReference( const ANetFileReference& p_fileRef ) {
        Assert( p_fileRef.parts[2] == 0 );
        return 0xFF00 * ( p_fileRef.parts[1] - 0x100 ) + ( p_fileRef.parts[0] - 0x100 ) + 1;
    }

}; // namespace gw2b
