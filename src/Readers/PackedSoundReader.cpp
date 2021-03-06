/** \file       Readers/PackedSoundReader.cpp
 *  \brief      Contains the definition of the packed Ogg reader class.
 *  \author     Khralkatorrix
 */

/**
 * Copyright (C) 2014-2016 Khralkatorrix <https://github.com/kytulendu>
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

#include <gw2formats/pf/AudioPackFile.h>

#include "PackedSoundReader.h"

namespace gw2b {

    PackedSoundReader::PackedSoundReader( const Array<byte>& p_data, DatFile& p_datFile, ANetFileType p_fileType )
        : FileReader( p_data, p_datFile, p_fileType ) {
    }

    PackedSoundReader::~PackedSoundReader( ) {
    }

    Array<byte> PackedSoundReader::getSoundData( ) const {
        gw2f::pf::AudioPackFile asnd( m_data.GetPointer( ), m_data.GetSize( ) );

        auto audioChunk = asnd.chunk<gw2f::pf::AudioChunks::Waveform>( );

        int audioDataSize = audioChunk->audioData.size( );

        Array<byte> outputArray( audioDataSize );
        ::memcpy( outputArray.GetPointer( ), audioChunk->audioData.data( ), audioDataSize );

        return outputArray;
    }

}; // namespace gw2b
