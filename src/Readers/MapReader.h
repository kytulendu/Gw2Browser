/** \file       Readers/MapReader.h
 *  \brief      Contains the declaration of the map reader class.
 *  \author     Khralkatorrix
 */

/**
 * Copyright (C) 2017 Khralkatorrix <https://github.com/kytulendu>
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

#ifndef READERS_MAPREADER_H_INCLUDED
#define READERS_MAPREADER_H_INCLUDED

#include "FileReader.h"

namespace gw2b {

    class MapReader : public FileReader {
    public:
        /** Constructor.
        *  \param[in]  p_data       Data to be handled by this reader.
        *  \param[in]  p_datFile    Reference to an instance of DatFile.
        *  \param[in]  p_fileType   File type of the given data. */
        MapReader( const Array<byte>& p_data, DatFile& p_datFile, ANetFileType p_fileType );
        /** Destructor. Clears all data. */
        virtual ~MapReader( );

        /** Gets the type of data contained in this file. Not to be confused with
        *  file type.
        *  \return DataType    type of data. */
        virtual DataType dataType( ) const override {
            return DT_Map;
        }
        /** Gets the strings contained in the data owned by this reader.
        *  \return Array<float> MP3 format. */
        Array<float> getMapData( ) const;

    }; // class MP3Reader

}; // namespace gw2b

#endif // READERS_MAPREADER_H_INCLUDED
