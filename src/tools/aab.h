#pragma once

#include <string>
#include <vector>

#include "gl.h"

#include "fbo.h"
#include "shader.h"

class AntiAliasedBuffer {
public:
    AntiAliasedBuffer();
    AntiAliasedBuffer(const unsigned int &_width, const unsigned int &_height);
    virtual ~AntiAliasedBuffer();

    void    bind();
    void    unbind();

    void    setSize(const unsigned int &_width, const unsigned int &_height);
    void    setScale(const float &_scale);
    void    getPixelsAsString(std::string &_image);

protected:
    std::unique_ptr<Fbo>    m_fbo_in;
    std::unique_ptr<Fbo>    m_fbo_out;
    std::unique_ptr<Shader> m_shader;

    GLuint                  m_vbo;
    GLuint                  m_index_vbo;
    GLint                   m_pos_attrib;
    float                   m_scale;
};