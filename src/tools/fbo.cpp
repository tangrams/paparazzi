#include "fbo.h"

#include "platform_gl.h"
#include "gl.h"

#include "platform.h"
#include "tangram.h"

Fbo::Fbo():m_id(0), m_old_fbo_id(0), m_texture(0), m_depth_buffer(0), m_width(0), m_height(0), m_allocated(false), m_binded(false) {
}

Fbo::Fbo(const unsigned int &_width, const unsigned int &_height, bool _depth):Fbo() {
    resize(_width, _height, _depth);
}

Fbo::~Fbo() {
    unbind();

    if (m_allocated) {
        Tangram::GL::deleteTextures(1, &m_texture);
        glDeleteRenderbuffers(1, &m_depth_buffer);
        glDeleteFramebuffers(1, &m_id);
        m_allocated = false;
    }
}

void Fbo::resize(const unsigned int &_width, const unsigned int &_height, bool _depth) {
    if (!m_allocated) {
        // Create a frame buffer
        glGenFramebuffers(1, &m_id);

        // Generate a texture to hold the colour buffer
        Tangram::GL::genTextures(1, &m_texture);

        // Depth Buffer
        if (_depth) {
            // Create a texture to hold the depth buffer
            glGenRenderbuffers(1, &m_depth_buffer);
        }
    }

    if (m_width != _width || m_height != _height) {
        m_width = _width;
        m_height = _height;

        bind();

        //  Color Texture
        Tangram::GL::bindTexture(GL_TEXTURE_2D, m_texture);
        Tangram::GL::texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

        Tangram::GL::texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        Tangram::GL::texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        Tangram::GL::texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        Tangram::GL::texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);
        
        // Depth Buffer
        if (_depth) {
            glBindRenderbuffer(GL_RENDERBUFFER, m_depth_buffer);
#ifndef PLATFORM_RPI
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, m_width, m_height);
#else  
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, m_width, m_height);
#endif
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depth_buffer);
        }

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
            m_allocated = true;
        }

        unbind();

        Tangram::GL::bindTexture(GL_TEXTURE_2D, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }
}

void Fbo::bind() {
    if (!m_binded) {
        Tangram::GL::getIntegerv(GL_FRAMEBUFFER_BINDING, (GLint *)&m_old_fbo_id);

        Tangram::GL::bindTexture(GL_TEXTURE_2D, 0);
        Tangram::GL::enable(GL_TEXTURE_2D);
        glBindFramebuffer(GL_FRAMEBUFFER, m_id);
        Tangram::GL::viewport(0.0f, 0.0f, m_width, m_height);
        Tangram::GL::clearColor(0.0f, 0.0f, 0.0f, 1.0f);

        if (m_depth_buffer) {
            Tangram::GL::clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        } else {
            Tangram::GL::clear(GL_COLOR_BUFFER_BIT);
        }
        
        m_binded = true;
    }
}

void Fbo::unbind() {
    if (m_binded) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_old_fbo_id);
        m_binded = false;
    }
}
