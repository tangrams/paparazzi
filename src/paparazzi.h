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

#include <string>

class Paparazzi {
public:
    Paparazzi();
    ~Paparazzi();

    void    setSize(const int &_width, const int &_height);
    void    setZoom(const float &_zoom);
    void    setTilt(const float &_deg);
    void    setRotation(const float &_deg);
    void    setScene(const std::string &_url);
    void    setSceneContent(const std::string &_yaml_content);
    void    setPosition(const double &_lon, const double &_lat);

    // prime_server stuff
    worker_t::result_t work (const std::list<zmq::message_t>& job, void* request_info);

    void    cleanup();

protected:
    void    update();

    std::string m_scene;
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
