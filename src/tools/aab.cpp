#include "aab.h"

#define IMAGE_DEPTH 4

// Tangram
#include "log.h"

// STB
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

AntiAliasedBuffer::AntiAliasedBuffer() : m_fbo_in(nullptr), m_fbo_out(nullptr), m_shader(nullptr), m_vbo(0), m_scale(2.) {

    // Create a simple vert/frag glsl shader to draw the main FBO with
    std::string vertexShader = "#ifdef GL_ES\n\
precision mediump float;\n\
#endif\n\
attribute vec4 a_position;\n\
void main(void) {\n\
    gl_Position = a_position;\n\
}";

    std::string fragmentShader = "#ifdef GL_ES\n\
precision mediump float;\n\
#endif\n\
uniform sampler2D u_buffer;\n\
uniform vec2 u_resolution;\n\
void main() {\n\
    vec2 st = gl_FragCoord.xy/u_resolution.xy;\n\
    st.y = 1.-st.y;\n\
    gl_FragColor = vec4(0.,0.,0.,1.);\n\
    gl_FragColor += texture2D(u_buffer, st);\n\
    gl_FragColor.a = 1.;\n\
    // gl_FragColor = vec4(st.x,st.y,0.,1.);\n\
}";

    m_fbo_in = std::unique_ptr<Fbo>(new Fbo());
    m_fbo_out = std::unique_ptr<Fbo>(new Fbo());

    m_shader = std::unique_ptr<Shader>(new Shader());
    m_shader->load(fragmentShader, vertexShader);

    GLfloat vertices[] = {  -1.0f, -1.0f, 0.0f,
                             1.0f, -1.0f, 0.0f,
                             1.0f,  1.0f, 0.0f,
                             1.0f,  1.0f, 0.0f,
                            -1.0f,  1.0f, 0.0f,
                            -1.0f, -1.0f, 0.0f  };

    Tangram::GL::genBuffers(1, &m_vbo);
    Tangram::GL::bindBuffer(GL_ARRAY_BUFFER, m_vbo);
    Tangram::GL::bufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
}

AntiAliasedBuffer::AntiAliasedBuffer(const unsigned int &_width, const unsigned int &_height) : AntiAliasedBuffer() {
    setSize(_width, _height);
}

AntiAliasedBuffer::~AntiAliasedBuffer() {

}

void AntiAliasedBuffer::bind() {
    m_fbo_in->bind();
    Tangram::GL::viewport(0.0f, 0.0f, m_fbo_in->getWidth(), m_fbo_in->getHeight());
    Tangram::GL::clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    Tangram::GL::clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void AntiAliasedBuffer::unbind() {
    m_fbo_in->unbind();
}

void AntiAliasedBuffer::setSize(const unsigned int &_width, const unsigned int &_height) {
    m_fbo_in->resize(_width*m_scale, _height*m_scale);
    m_fbo_out->resize(_width, _height, false);
}

void AntiAliasedBuffer::setScale(const float &_scale){
    m_scale = _scale;
}

void write_func(void *context, void *data, int size) {
    static_cast<std::string*>(context)->append(static_cast<const char*>(data), size);
}

void AntiAliasedBuffer::getPixelsAsString(std::string &_image) {
    int width = m_fbo_out->getWidth();
    int height = m_fbo_out->getHeight();

    m_fbo_out->bind();
    Tangram::GL::viewport(0.0f, 0.0f, width, height);
    Tangram::GL::clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    Tangram::GL::clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Load the vertex data
    Tangram::GL::bindBuffer(GL_ARRAY_BUFFER, m_vbo);

    m_shader->use();
    m_shader->setUniform("u_resolution", width, height);
    m_shader->setUniform("u_buffer", m_fbo_in.get(), 0);
    Tangram::GL::enableVertexAttribArray(0);
    Tangram::GL::vertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    Tangram::GL::drawArrays(GL_TRIANGLES, 0, 6);

    unsigned char *pixels = new unsigned char[width * height * IMAGE_DEPTH];   // allocate memory for the pixels
    Tangram::GL::readPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels); // Read throug the current buffer pixels
    stbi_write_png_to_func(&write_func, &_image, width, height, IMAGE_DEPTH, pixels, width * IMAGE_DEPTH);
    delete [] pixels;

    m_fbo_out->unbind();
}
