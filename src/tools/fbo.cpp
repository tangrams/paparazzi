#include "fbo.h"

#include "gl.h"

#include "platform.h"
#include "tangram.h"

#include "platform_headless.h"

Fbo::Fbo():m_id(0), m_old_fbo_id(0), m_texture(0), m_depth_texture(0), m_width(0), m_height(0), m_allocated(false), m_binded(false) {
}

Fbo::Fbo(const unsigned int &_width, const unsigned int &_height):Fbo() {
    // Allocate Textures
    resize(_width,_height);
}

Fbo::~Fbo() {
    unbind();
    if (m_allocated) {
        Tangram::GL::deleteTextures(1, &m_texture);
        Tangram::GL::deleteTextures(1, &m_depth_texture);
        glDeleteFramebuffers(1, &m_id);
        m_allocated = false;
    }
}

void Fbo::resize(const unsigned int &_width, const unsigned int &_height) {
    if (!m_allocated) {
        // Create a frame buffer
        glGenFramebuffers(1, &m_id);

        // Generate a texture to hold the colour buffer
        glGenTextures(1, &m_texture);

        // Create a texture to hold the depth buffer
        glGenTextures(1, &m_depth_texture);
    }

    m_width = _width;
    m_height = _height;

    //  Render (color) texture
    Tangram::GL::bindTexture(GL_TEXTURE_2D, m_texture);
    Tangram::GL::texImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                            m_width, m_height,
                            0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    Tangram::GL::texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    Tangram::GL::texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    Tangram::GL::texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    Tangram::GL::texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    Tangram::GL::bindTexture(GL_TEXTURE_2D, 0);

    // Depth Texture
    Tangram::GL::bindTexture(GL_TEXTURE_2D, m_depth_texture);
    Tangram::GL::texImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
                            m_width, m_height,
                            0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NULL);

    Tangram::GL::texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    Tangram::GL::texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    Tangram::GL::texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    Tangram::GL::texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    Tangram::GL::bindTexture(GL_TEXTURE_2D, 0);

    // Associate the textures with the FBO
    bind();
    glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_TEXTURE_2D, m_texture, 0);
    glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_TEXTURE_2D, m_depth_texture, 0);
    unbind();

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
        m_allocated = true;
    }
}

void Fbo::bind() {
    if (!m_binded) {
        Tangram::GL::getIntegerv(GL_FRAMEBUFFER_BINDING, (GLint *)&m_old_fbo_id);
        glBindFramebuffer(GL_FRAMEBUFFER, m_id);
        Tangram::GL::viewport(0.0f, 0.0f, m_width, m_height);
        Tangram::GL::clearColor(0.0f, 0.0f, 0.0f, 1.0f);
        Tangram::GL::clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        m_binded = true;
    }
}

void Fbo::unbind() {
    if (m_binded) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_old_fbo_id);
        m_binded = false;
    }
}