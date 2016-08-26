#pragma once

//prime_server guts
#include <prime_server/prime_server.hpp>
#include <prime_server/http_protocol.hpp>
using namespace prime_server;

// Tangram-ES
#include "tangram.h"

// Some of my own tools to scale down the rendering image and download it to a file
#include "gl/fbo.h"         // simple FBO implementation
#include "gl/shader.h"      // simple ShaderProgram implementation
#include "gl/vbo.h"         // simple VBO implementation
#include "types/shapes.h"   // Small library to compose basic shapes (use for rect)

#include <string>

typedef struct {
    unsigned char * bytes;
    int width;
    int height;
    int depth;
} Pixels;

class Paparazzi {
public:
    Paparazzi();
    ~Paparazzi();

    void    setSize(int _width, int _height);

    void    update();

    bool    do(std::string &_command);

    int     getWidth() const { return m_width; };
    int     getHeight() const { return m_height; };
    Pixels  getPixels();

    // prime_server stuff
    worker_t::result_t work (const std::list<zmq::message_t>& job, void* request_info);
    void    cleanup();

protected:

    std::string m_style;
    double      m_lat;
    double      m_lon;
    float       m_zoom;
    float       m_rotation;
    float       m_tilt;
    int         m_width;
    int         m_height;

    Shader*         m_smallShader;  // Shader program to use to down sample the renderFbo with
    Fbo*            m_smallFbo;     // FrameBufferObject of the half of the size to fake an antialiased image
    Vbo*            m_smallVbo;     // VertexBufferObject to down sample the renderFbo to (like a billboard)
    Fbo*            m_renderFbo;    // FrameBufferObject where the tangram scene will be renderd 
    Tangram::Map*   m_map;          // Tangram
};
