/** \file       DatFile.h
 *  \brief      Contains declaration of the class representing the .dat file.
 *  \author     Rhoot
 */

/**
 * Copyright (C) 2014-2015 Khralkatorrix <https://github.com/kytulendu>
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

#pragma once

#ifndef DATFILE_H_INCLUDED
#define DATFILE_H_INCLUDED

#include <wx/file.h>

#include "ANetStructs.h"

namespace gw2b {
    class FileReader;

    /** Represents a GW2 .dat file. */
    class DatFile {
        struct IdEntry;
    private:
        typedef Array<ANetMftEntry> EntryArray;
        typedef Array<IdEntry>      EntryToIdArray;
        typedef Array<byte>         InputBufferArray;
    private:
        wxFile              m_file;
        ANetDatHeader       m_datHead;
        ANetMftHeader       m_mftHead;
        EntryArray          m_mftEntries;
        EntryToIdArray      m_entryToId;
        InputBufferArray    m_inputBuffer;
        uint                m_lastReadEntry;
    private:
        enum MFTFileOffset {
            MFT_FILE_OFFSET = 16
        };
    public:
        enum IdentificationResult {
            IR_Success,
            IR_NotEnoughData,
            IR_Failure,
        };
    public:
        /** Default constructor. Initializes internals. */
        DatFile( );
        /** Constructor. Initializes internals and opens the given .dat file.
        *  \param[in]  p_filename   Name of the .dat file to open. */
        DatFile( const wxString& p_filename );
        /** Destructor. Makes sure to clear out any unfreed data. */
        ~DatFile( );

        /** Opens the given .dat file for reading.
        *  \param[in]  p_filename   Name of the .dat file to open.
        *  \return bool    true if opening succeeded, false if not. */
        bool open( const wxString& p_filename );
        /** Checks whether or not this object currently has a .dat file open.
        *  \return bool    true if a .dat file is open, false if not. */
        bool isOpen( ) const;
        /** Closes the open .dat file, if any. */
        void close( );

        /** Gets the MFT entry number for the file with the given file id.
        *  \param[in]  p_fileId     ID of the file to get the entry number for.
        *  \return uint    The MFT entry num if it was found, UINT_MAX if not. */
        uint entryNumFromFileOrBaseId( uint p_fileId ) const;
        /** Gets the file ID of the file with the given MFT entry number.
        *  \param[in]  p_entryNum   Entry number to get the file ID for.
        *  \return uint    The file ID if it was found, UINT_MAX if not. */
        uint fileIdFromEntryNum( uint p_entryNum ) const;
        /** Gets the file ID of the file with the given MFT file entry number.
        *  \param[in]  p_entryNum   File entry number to get the file ID for.
        *  \return uint    The file ID if it was found, UINT_MAX if not. */
        uint fileIdFromFileNum( uint p_entryNum ) const;

        /** Gets the base id of the file with the given MFT entry number.
        *  \param[in]  p_entryNum   Entry number to get the base id for.
        *  \return uint    The base id if it was found, UINT_MAX if not. */
        uint baseIdFromEntryNum( uint p_entryNum ) const;
        /** Gets the base id of the file with the given MFT file entry number.
        *  \param[in]  p_entryNum   File entry number to get the base id for.
        *  \return uint    The base id if it was found, UINT_MAX if not. */
        uint baseIdFromFileNum( uint p_entryNum ) const;

        /** Gets the total uncompressed size of the given entry.
        *  \param[in]  p_entryNum   Entry number to check the size for.
        *  \return uint    Uncompressed size of the entry. */
        uint entrySize( uint p_entryNum );
        /** Gets the total uncompressed size of the given file.
        *  \param[in]  p_fileNum   File entry number to check the size for.
        *  \return uint    Uncompressed size of the file. */
        uint fileSize( uint p_fileNum );
        /** Gets the amount of total MFT entries in the .dat file.
        *  \return uint    Amount of entries in the .dat file, UINT_MAX if file not open. */
        uint numEntries( ) const {
            if ( !this->isOpen( ) ) {
                return UINT_MAX;
            } return m_mftHead.numEntries;
        }
        /** Gets the amount of file MFT entries in the .dat file.
        *  \return uint    Amount of file entries in the .dat file, UINT_MAX if file not open. */
        uint numFiles( ) const {
            if ( !this->isOpen( ) ) {
                return UINT_MAX;
            } return m_mftHead.numEntries - MFT_FILE_OFFSET;
        }
        /** Gets the amount of entries that appear before the file entries in the
        *   .dat file.
        *  \return uint    Index of the first file entry in the MFT. */
        uint mftFileOffset( ) const {
            return MFT_FILE_OFFSET;
        }

        /** Peeks at the contents of the given MFT entry and returns the results.
        *  \param[in]  p_entryNum   MFT entry number to get contents for.
        *  \param[in]  p_peekSize   Amount of bytes to peek at. Specifying 0 will read the whole entry.
        *  \param[in,out]  po_buffer    Buffer to store results in. Must be *at least*
        *                  pPeekSize in length.
        *  \return uint    Size of poBuffer. */
        uint peekEntry( uint p_entryNum, uint p_peekSize, byte* po_buffer );
        /** Peeks at the contents of the given MFT file entry and returns the results.
        *  \param[in]  p_fileNum    MFT entry number to get contents for.
        *  \param[in]  p_peekSize   Amount of bytes to peek at. Specifying 0 will read the whole file.
        *  \param[in,out]  po_buffer    Buffer to store results in. Must be *at least*
        *                  pPeekSize in length.
        *  \return uint    Size of poBuffer. */
        uint peekFile( uint p_fileNum, uint p_peekSize, byte* po_buffer );
        /** Peeks at the contents of the given MFT entry and returns the results.
        *  \param[in]  p_entryNum   MFT entry number to get contents for.
        *  \param[in]  p_peekSize   Amount of bytes to peek at.
        *  \return Array<byte>  Object used to handle the peeked data. */
        Array<byte> peekEntry( uint p_entryNum, uint p_peekSize );
        /** Peeks at the contents of the given MFT file entry and returns the results.
        *  \param[in]  p_fileNum    MFT file entry number to get contents for.
        *  \param[in]  p_peekSize   Amount of bytes to peek at.
        *  \return Array<byte>  Object used to handle the peeked data. */
        Array<byte> peekFile( uint p_fileNum, uint p_peekSize );

        /** Reads the contents of the given MFT entry and returns the results.
        *  \param[in]  p_entryNum   MFT entry number to get contents for.
        *  \param[in,out]  po_buffer    Buffer to store results in. It is up to the
        caller to make sure the buffer is big enough.
        *  \return uint    Size of poBuffer. */
        uint readEntry( uint p_entryNum, byte* po_buffer );
        /** Reads the contents of the given MFT file entry and returns the results.
        *  \param[in]  p_fileNum   MFT entry number to get contents for.
        *  \param[in,out]  po_buffer    Buffer to store results in. It is up to the
        caller to make sure the buffer is big enough.
        *  \return uint    Size of poBuffer. */
        uint readFile( uint p_fileNum, byte* po_buffer );
        /** Reads the data contained at the given MFT entry.
        *  \param[in]  p_entryNum   MFT entry number to read.
        *  \return Array<byte>  Object used to handle the read data. */
        Array<byte> readEntry( uint p_entryNum );
        /** Reads the file contained at the given MFT entry.
        *  \param[in]  p_fileNum    MFT file entry number to read.
        *  \return Array<byte>  Object used to handle the read file. */
        Array<byte> readFile( uint p_fileNum );

        IdentificationResult identifyFileType( const byte* p_data, size_t p_size, ANetFileType& p_fileType );
        static uint fileIdFromFileReference( const ANetFileReference& p_fileRef );

    }; // class DatFile

}; // namespace gw2b

#endif // DATFILE_H_INCLUDED
