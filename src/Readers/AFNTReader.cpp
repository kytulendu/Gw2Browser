/** \file       Readers/AFNTReader.cpp
 *  \brief      Contains definition of the AFNT reader class.
 *  \author     BoyC, Khralkatorrix
 */

/**
 * Copyright (C) 2019 Khralkatorrix <https://github.com/kytulendu>
 * Copyright (C) 2019 BoyC <https://twitter.com/BoyCcns>
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

#include "PackFile.h"
#include "AFNTReader.h"

namespace gw2b {

    //----------------------------------------------------------------------------
    //      AFNT_RLEDecoder
    //----------------------------------------------------------------------------

    byte* AFNT_RLEDecoder::DecompressNextCharacter( byte* p_data ) {
        m_yOffset = p_data[0];
        m_width = p_data[1] + 1;
        m_height = p_data[2] + 1;
        size_t unpackedSize = m_width * m_height;
        byte rleType = p_data[3];
        byte repeatedByte = 0;

        if ( !rleType || rleType == 255 ) {
            repeatedByte = 255 - rleType;
        }

        m_stream = p_data + 4;

        ::memset( m_image, 0, 65536 );
        size_t imagePos = 0;

        while ( unpackedSize ) {
            size_t advance = 0;

            if ( rleType && rleType != 255 ) {
                Assert( rleType == 1 );
                repeatedByte = *m_stream++;
                if ( repeatedByte && repeatedByte != 255 ) {
                    advance = 1;
                }
            } else {
                repeatedByte = 255 - repeatedByte;
            }

            if ( !advance ) {
                advance = 1;
                while ( *m_stream++ == 255 ) {
                    advance += 255;
                }
                advance += m_stream[-1];
            }

            Assert( advance );
            Assert( advance <= unpackedSize );

            ::memset( m_image + imagePos, repeatedByte, advance );
            imagePos += advance;
            unpackedSize -= advance;
        }

        return m_stream;
    }

    //----------------------------------------------------------------------------
    //      AFNTReader
    //----------------------------------------------------------------------------

    struct AFNTReader::FontDescriptor {
        int8 unknown[14];
        uint8 extents_x;
        uint8 extents_y;
        uint32 fileNames[13];
    };

    AFNTReader::AFNTReader( const Array<byte>& p_data, DatFile& p_datFile, ANetFileType p_fileType )
        : FileReader( p_data, p_datFile, p_fileType ) {
    }

    AFNTReader::~AFNTReader( ) {
    }

    std::vector<Font> AFNTReader::getFont( ) const {
        uint32 charRanges[39] = {
            //start, end    , process
            0x00021, 0x0007F, 1,   // basic latin
            0x000A1, 0x000FF, 0,   // latin 1 supplement
            0x00100, 0x00180, 0,   // latin extended-a
            0x00391, 0x00460, 0,   // greek and coptic and cyrillic
            0x02010, 0x0266B, 0,   // lots of symbols
            0x03000, 0x03020, 0,   // CJK Symbols and Punctuation
            0x03041, 0x03100, 0,   // Hiragana
            0x03105, 0x0312A, 0,   // Bopomofo
            0x03131, 0x0318F, 0,   // Hangul Compatibility Jamo
            0x0AC00, 0x0D7A4, 0,   // Hangul Syllables
            0x04E00, 0x09FA6, 0,   // CJK Unified Ideographs
            0x0F900, 0x0FA6B, 0,   // CJK Compatibility Ideographs
            0x0FF01, 0x0FFE7, 0    // Halfwidth and Fullwidth Forms
        };

        wxImage image;
        size_t size = 0;

        std::vector<Font> fonts;

        auto pf = PackFile( m_data );
        auto afnt = pf.findChunk( FCC_AFNT, size );

        ANetPfChunkHeader* chunkHeader = (ANetPfChunkHeader*)( afnt );

        if ( chunkHeader->chunkTypeInteger != FCC_AFNT ) {
            wxLogMessage( wxT( "Not AFNT file. Seriously!?" ) );
            return fonts;
        }

        // get the font library
        FontDescriptor* fontArray = (FontDescriptor*)( afnt + sizeof( ANetPfChunkHeader ) + 8 );

        // get the font count
        uint32 fontCount = *(uint32*)(afnt + sizeof( ANetPfChunkHeader ));

        for ( int x = 0; x < static_cast<int>( fontCount ); x++ ) {
            wxLogMessage( wxT( "Processing font %d" ), x );

            std::vector<Glyph> glyphs;
            auto& fnt = fontArray[x];

            // the 13 glyph ranges are hardcoded into the gw2 client and not data driven in any way
            // each glyph range is stored in a separate file
            for ( int y = 0; y < 13; y++ ) {
                // process this character range? - this is only a toggle to allow easy removal of certain glyph ranges
                if ( !charRanges[y * 3 + 2] ) {
                    continue;
                }
                // not all fonts reference files for all glyph ranges
                if ( !fnt.fileNames[y] ) {
                    continue;
                }

                auto ref = reinterpret_cast<const ANetFileReference*>( ( (unsigned char*)&fnt.fileNames[y] ) + fnt.fileNames[y] );
                auto fileId = DatFile::fileIdFromFileReference( *ref );
                auto entryNumber = m_datFile.entryNumFromFileOrBaseId( fileId );
                auto fileData = m_datFile.readEntry( entryNumber );
                if ( !fileData.GetSize( ) ) {
                    wxLogMessage( wxT( "File id %d is empty or not exist." ), fileId );
                    continue;
                }

                // the referenced font file will contain exactly as many glyphs as are defined in the hardcoded range
                auto rangeStart = charRanges[y * 3];
                auto rangeEnd = charRanges[y * 3 + 1];

                auto charStream = fileData.GetPointer( );

                for ( auto ch = rangeStart; ch < rangeEnd; ch++ ) {
                    AFNT_RLEDecoder rleDecoder;
                    charStream = rleDecoder.DecompressNextCharacter( charStream ); // this decodes a single glyph

                    Glyph glyph;
                    glyph.character = ch;
                    glyph.width = rleDecoder.m_width;
                    glyph.height = rleDecoder.m_height;

                    size_t numPixels = glyph.width * glyph.height;
                    auto colors = allocate<BGR>( numPixels );

#pragma omp parallel for
                    for ( int y = 0; y < static_cast<int>( glyph.height ); y++ ) {
                        size_t curBlock = y * glyph.width;
                        for ( size_t x = 0; x < glyph.width; x++ ) {
                            auto curPixel = x + curBlock;
                            ::memcpy( &colors[curPixel].b, &rleDecoder.m_image[curPixel], sizeof( colors[curPixel].b ) );
                            ::memcpy( &colors[curPixel].g, &rleDecoder.m_image[curPixel], sizeof( colors[curPixel].g ) );
                            ::memcpy( &colors[curPixel].r, &rleDecoder.m_image[curPixel], sizeof( colors[curPixel].r ) );
                        }
                    }

                    // Create image and fill it with color data
                    glyph.data = wxImage( glyph.width, glyph.height );
                    glyph.data.SetData( reinterpret_cast<unsigned char*>( colors ) );

                    glyphs.push_back( glyph );
                }
            }

            if ( !glyphs.empty( ) ) {
                Font font;
                font.id = x;
                font.extents_x = fnt.extents_x;
                font.extents_y = fnt.extents_y;
                font.glyphs = glyphs;
                fonts.push_back( font );
            }
        }

        return fonts;
    }

}; // namespace gw2b
