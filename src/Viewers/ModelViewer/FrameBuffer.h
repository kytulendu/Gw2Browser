/** \file       Viewers/ModelViewer/FrameBuffer.h
 *  \brief      Contains the declaration of the FrameBuffer class.
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

#ifndef VIEWERS_MODELVIEWER_FRAMEBUFFER_H_INCLUDED
#define VIEWERS_MODELVIEWER_FRAMEBUFFER_H_INCLUDED

#include <vector>

namespace gw2b {

    class FrameBuffer {
        GLuint                      m_fbo;              // Framebuffer object
        GLuint                      m_rbo;              // Renderbuffer object
        std::vector<GLuint>         m_fbTexture;        // Framebuffer texture

        GLuint                      m_fboMultiSample;
        GLuint                      m_rboMultiSample;
        std::vector<GLuint>         m_textureColorBufferMultiSampled;

        GLuint                      m_quadVAO;          // Quad vertex array object
        GLuint                      m_quadVBO;          // Quad buffer array object
        wxSize                      m_clientSize;       // Size of OpenGL control

        bool                        m_isMultisample;

    public:
        /** Constructor. Create framebuffer object.
        *  \param[in]  p_size       Size of the OpenGL screen.
        *  \param[in]  p_attachCount Number of texture attachment. */
        FrameBuffer( const wxSize& p_size, GLuint p_attachCount );
        /** Constructor. Create multi-sample framebuffer object.
        *  \param[in]  p_size       Size of the OpenGL screen.
        *  \param[in]  p_samples    Number of sample for multi-sample texture.
        *  \param[in]  p_attachCount Number of texture attachment. */
        FrameBuffer( const wxSize& p_size, GLuint p_samples, GLuint p_attachCount );
        /** Destructor. Clears all data. */
        ~FrameBuffer( );

        /** Draw framebuffer. */
        void draw( );
        /** Bind to framebuffer. */
        void bind( );
        /** Unbind to framebuffer. */
        void unbind( );
        /** Set framebuffer texture size. */
        void setClientSize( const wxSize& p_size );

    private:
        /** Setup screen quad. */
        void setupQuad( );
        /** Create normal framebuffer.
        *  \param[in]  p_numTex     Number of texture attachment. */
        void setupFramebuffer( GLuint p_numTex );
        /** Create multi-sample framebuffer.
        *  \param[in]  p_samples    Number of samples.
        *  \param[in]  p_numTex     Number of texture attachment. */
        void setupMultiSampleFramebuffer( GLuint p_samples, GLuint p_numTex );
        /** Generates a texture that is suited for attachments to a framebuffer. */
        GLuint generateAttachmentTexture( );
        /** Generates a multi-sample texture.
        *  \param[in]  p_samples    Number of samples for multi-sample texture.*/
        GLuint generateMultiSampleTexture( GLuint p_samples );
        /** Check the frame buffer status. */
        bool checkFrameBufferStatus( );

    }; // class FrameBuffer

}; // namespace gw2b

#endif // VIEWERS_MODELVIEWER_FRAMEBUFFER_H_INCLUDED
