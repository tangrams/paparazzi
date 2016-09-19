#include "paparazzi.h"

#define AA_SCALE 2.0
#define MAX_WAITING_TIME 5.0

#include "platform.h"       // Tangram platform specifics
#include "log.h"
#include "gl.h"

#include "context.h"        // This set the headless context
#include "platform_headless.h" // headless platforms (Linux and RPi)

// MD5
#include "tools/md5.h"

//nuts and bolts required
#include <functional>
#include <csignal>
#include <fstream>
#include <curl/curl.h>      // Curl
#include "glm/trigonometric.hpp" // GLM for the radians/degree calc

// HTTP RESPONSE HEADERS
const headers_t::value_type CORS{"Access-Control-Allow-Origin", "*"};
const headers_t::value_type PNG_MIME{"Content-type", "image/png"};
const headers_t::value_type TXT_MIME{"Content-type", "text/plain;charset=utf-8"};

Paparazzi::Paparazzi() : m_scene("scene.yaml"), m_lat(0.0), m_lon(0.0), m_zoom(0.0f), m_rotation(0.0f), m_tilt(0.0), m_width(100), m_height(100) {

    // Initialize cURL
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Start OpenGL ES context
    LOG("Creating OpenGL ES context");

    initGL(m_width, m_height);

    LOG("Creating a new TANGRAM instances");
    m_map = std::unique_ptr<Tangram::Map>(new Tangram::Map());
    m_map->loadSceneAsync(m_scene.c_str());
    m_map->setupGL();
    m_map->setPixelScale(AA_SCALE);
    m_map->resize(m_width, m_height);
    update();

    m_aab = std::unique_ptr<AntiAliasedBuffer>(new AntiAliasedBuffer(m_width, m_height));
    m_aab->setScale(AA_SCALE);

    setSize(800, 600);
}

Paparazzi::~Paparazzi() {
    LOG("Closing...");

    finishUrlRequests();
    curl_global_cleanup();

    closeGL();
    LOG("END\n");
}

void Paparazzi::setSize (const int &_width, const int &_height) {
    if (_width != m_width || _height != m_height) {
        resetTimer("set size");

        m_width = _width;
        m_height = _height;

        // Setup the size of the image
        m_map->resize(m_width*AA_SCALE, m_height*AA_SCALE);
        // m_map->setPixelScale(AA_SCALE);
        update();

        m_aab->setSize(m_width, m_height);
    }
}

void Paparazzi::setZoom (const float &_zoom) {
    if (_zoom != m_zoom) {
        resetTimer("set zoom");

        m_zoom = _zoom;

        m_map->setZoom(_zoom);
        update();
    }
}

void Paparazzi::setTilt (const float &_deg) {
    if (_deg != m_tilt) {
        resetTimer("set tilt");

        m_tilt = _deg;

        m_map->setTilt(glm::radians(m_tilt));
        update();
    }
}
void Paparazzi::setRotation (const float &_deg) {
    if (_deg != m_rotation) {
        resetTimer("set rotation");

        m_rotation = _deg;

        m_map->setRotation(glm::radians(m_rotation));
        update();
    }
}

void Paparazzi::setPosition (const double &_lon, const double &_lat) {
    if (_lon != m_lon || _lat != m_lat) {
        resetTimer("set position");

        m_lon = _lon;
        m_lat = _lat;

        m_map->setPosition(m_lon, m_lat);
        update();
    }
}

void Paparazzi::setScene (const std::string &_url) {
    if (_url != m_scene) {
        resetTimer("set scene");

        m_scene = _url;

        m_map->loadSceneAsync(m_scene.c_str());
        update();
    }
}

void Paparazzi::setSceneContent(const std::string &_yaml_content) {
    std::string md5_scene =  md5(_yaml_content);

    if (md5_scene != m_scene) {
        resetTimer("set scene");
        m_scene = md5_scene;

        // TODO:
        //    - This is waiting for LoadSceneConfig to be implemented in Tangram::Map
        //      Once that's done there is no need to save the file.
        std::string name = "cache/"+md5_scene+".yaml";
        std::ofstream out(name.c_str());
        out << _yaml_content.c_str();
        out.close();

        m_map->loadSceneAsync(name.c_str());
        update();
    }
}

void Paparazzi::update () {
    double startTime = getTime();
    float delta = 0.0;

    bool bFinish = false;
    while (delta < MAX_WAITING_TIME && !bFinish) {
        // Update Network Queue
        processNetworkQueue();
        bFinish = m_map->update(10.);
        delta = float(getTime() - startTime);
    }
    LOG("FINISH");
}

// prime_server stuff
worker_t::result_t Paparazzi::work (const std::list<zmq::message_t>& job, void* request_info){
    //false means this is going back to the client, there is no next stage of the pipeline
    worker_t::result_t result{false};

    //this type differs per protocol hence the void* fun
    auto& info = *static_cast<http_request_t::info_t*>(request_info);

    // Try to generate a response 
    http_response_t response;
    try {
        double start_call = getTime();

        //TODO: 
        //   - actually use/validate the request parameters
        auto request = http_request_t::from_string(static_cast<const char*>(job.front().data()), job.front().size());

        if (request.path == "/check") {
            // ELB check
            response = http_response_t(200, "OK", "OK", headers_t{CORS, TXT_MIME});
        } else {

            //  SCENE
            //  ---------------------
            auto scene_itr = request.query.find("scene");
            if (scene_itr == request.query.cend() || scene_itr->second.size() == 0) {
                // If there is NO SCENE QUERY value 
                if (request.body.empty()) 
                    // if there is not POST body content return error...
                    throw std::runtime_error("scene is required punk");

                // ... other whise load content
                setSceneContent(request.body);
            }
            else {
                // If there IS a SCENE QUERRY value load it
                setScene(scene_itr->second.front());
            }

            //  SIZE
            //  ---------------------
            auto width_itr = request.query.find("width");
            if (width_itr == request.query.cend() || width_itr->second.size() == 0)
                // If no WIDTH QUERRY return error
                throw std::runtime_error("width is required punk");
            auto height_itr = request.query.find("height");
            if (height_itr == request.query.cend() || height_itr->second.size() == 0)
                // If no HEIGHT QUERRY return error
                throw std::runtime_error("height is required punk");
            // Set Map and OpenGL context size
            setSize(std::stoi(width_itr->second.front()), std::stoi(height_itr->second.front()));

            //  POSITION
            //  ---------------------
            auto lat_itr = request.query.find("lat");
            if (lat_itr == request.query.cend() || lat_itr->second.size() == 0)
                // If not LAT QUERRY return error
                throw std::runtime_error("lat is required punk");
            auto lon_itr = request.query.find("lon");
            if (lon_itr == request.query.cend() || lon_itr->second.size() == 0)
                // If not LON QUERRY return error
                throw std::runtime_error("lon is required punk");
            setPosition(std::stod(lon_itr->second.front()), std::stod(lat_itr->second.front()));
            auto zoom_itr = request.query.find("zoom");
            if (zoom_itr == request.query.cend() || zoom_itr->second.size() == 0)
                // If not ZOOM QUERRY return error
                throw std::runtime_error("zoom is required punk");
            setZoom(std::stof(zoom_itr->second.front()));

            // //  TILT (optional)
            // //  ---------------------
            // std::cout << "Tilt" << std::endl;
            // auto tilt_itr = request.query.find("tilt");
            // if (tilt_itr == request.query.cend() && tilt_itr->second.size() == 0) {
            //     // If TILT QUERRY is provided assigned ...
            //     setTilt(std::stof(tilt_itr->second.front()));
            // }
            // else {
            //     // othewise use default (0.)
            //     setTilt(0.0f);
            // }
        
            // //  ROTATION (OPTIONAL)
            // //  ---------------------
            // std::cout << "Rotation" << std::endl;
            // auto rotation_itr = request.query.find("rotation");
            // if (rotation_itr != request.query.cend() && rotation_itr->second.size() != 0) {
            //     // If ROTATION QUERRY is provided assigned ...
            //     setRotation(std::stof(rotation_itr->second.front()));
            // }
            // else {
            //     // othewise use default (0.)
            //     setRotation(0.0f);
            // }

            std::cout << "Rendering" << std::endl;
            resetTimer("Rendering");
            std::string image;
            if (m_map) {
                update();

                m_aab->bind();
                Tangram::GL::viewport(0.0f, 0.0f, m_width*AA_SCALE, m_height*AA_SCALE);
                Tangram::GL::clearColor(0.0f, 0.0f, 0.0f, 1.0f);
                Tangram::GL::clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                m_map->render();  // Render Tangram Scene
                m_aab->unbind();
   
                // Once the main FBO is draw take a picture
                resetTimer("Extracting pixels...");
                m_aab->getPixelsAsString(image);

                double total_time = getTime()-start_call;
                LOG("TOTAL CALL: %f", total_time);
                LOG("TOTAL speed: %f millisec per pixel", (total_time/((m_width * m_height)/1000.0)));
            }

            response = http_response_t(200, "OK", image, headers_t{CORS, PNG_MIME});
        }
    }
    catch(const std::exception& e) {
        //complain
        response = http_response_t(400, "Bad Request", e.what(), headers_t{CORS});
    }

    //does some tricky stuff with headers and different versions of http
    response.from_info(info);

    //formats the response to protocal that the client will understand
    result.messages.emplace_back(response.to_string());
    return result;
}

void Paparazzi::cleanup () {

}
