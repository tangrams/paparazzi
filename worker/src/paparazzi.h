#pragma once

#include <memory>
#include <string>

//prime_server guts
#include <prime_server/prime_server.hpp>
#include <prime_server/http_protocol.hpp>
using namespace prime_server;

#include "tools/aab.h"  // AntiAliased Buffer
#include "tangram.h"    // Tangram-ES

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

    std::string         m_scene;
    double              m_lat;
    double              m_lon;
    float               m_zoom;
    float               m_rotation;
    float               m_tilt;
    int                 m_width;
    int                 m_height;

    std::unique_ptr<Tangram::Map>       m_map;  // Tangram Map instance
    std::unique_ptr<AntiAliasedBuffer>  m_aab;  // Antialiased Buffer
    
};
