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
#include <regex>
#include <sstream>
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
    m_map->resize(m_width*AA_SCALE, m_height*AA_SCALE);
    update();

    m_aab = std::unique_ptr<AntiAliasedBuffer>(new AntiAliasedBuffer(m_width, m_height));
    m_aab->setScale(AA_SCALE);

    setSize(800, 600, 1.0);
}

Paparazzi::~Paparazzi() {
    LOG("Closing...");

    finishUrlRequests();
    curl_global_cleanup();

    closeGL();
    LOG("END\n");
}

void Paparazzi::setSize (const int &_width, const int &_height, const float &_density) {
    if (_density*_width != m_width || _density*_height != m_height || _density*AA_SCALE != m_map->getPixelScale()) {
        resetTimer("set size");

        m_width = _width*_density;
        m_height = _height*_density;

        // Setup the size of the image
        if (_density*AA_SCALE != m_map->getPixelScale()) {
            m_map->setPixelScale(_density*AA_SCALE);
        }
        m_map->resize(m_width*AA_SCALE, m_height*AA_SCALE);
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

/**
 * @brief Bounds representation: minx, miny, maxx, maxy
 */
typedef struct futile_bounds_s {
    /** @brief minimum x value */
    double minx;
    /** @brief minimum y value */
    double miny;
    /** @brief maximum x value */
    double maxx;
    /** @brief maximum y value */
    double maxy;
} futile_bounds_s;

typedef struct futile_coord_s {
    /** @brief coordinate x or column value */
    uint32_t x;
    /** @brief coordinate y or row value */
    uint32_t y;
    /** @brief coordinate z or zoom value */
    uint32_t z;
} futile_coord_s;

static double min(double a, double b) {
    return a < b ? a : b;
}

static double radians_to_degrees(double radians) {
    return radians * 180 / M_PI;
}

static double degrees_to_radians(double degrees) {
    return degrees * M_PI / 180;
}

// http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
// TODO make output into point
void futile_coord_to_lnglat(futile_coord_s *coord, double *out_lng_deg, double *out_lat_deg) {
    double n = pow(2, coord->z);
    double lng_deg = coord->x / n * 360.0 - 180.0;
    double lat_rad = atan(sinh(M_PI * (1 - 2 * coord->y / n)));
    double lat_deg = radians_to_degrees(lat_rad);
    *out_lng_deg = lng_deg;
    *out_lat_deg = lat_deg;
}

// http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
// make input point
void futile_lnglat_to_coord(double lng_deg, double lat_deg, int zoom, futile_coord_s *out) {
    double lat_rad = degrees_to_radians(lat_deg);
    double n = pow(2.0, zoom);
    out->x = (lng_deg + 180.0) / 360.0 * n;
    out->y = (1.0 - log(tan(lat_rad) + (1 / cos(lat_rad))) / M_PI) / 2.0 * n;
    out->z = zoom;
}

void futile_coord_to_bounds(futile_coord_s *coord, futile_bounds_s *out) {
    double topleft_lng, topleft_lat, bottomright_lng, bottomright_lat;
    futile_coord_s coord_bottomright = {
        .x=coord->x + 1,
        .y=coord->y + 1,
        .z=coord->z
    };
    futile_coord_to_lnglat(coord, &topleft_lng, &topleft_lat);
    futile_coord_to_lnglat(&coord_bottomright, &bottomright_lng, &bottomright_lat);
    double minx = topleft_lng;
    double miny = bottomright_lat;
    double maxx = bottomright_lng;
    double maxy = topleft_lat;

    // coord_to_bounds is used to calculate boxes that could be off the grid
    // clamp the max values in that scenario
    maxx = min(180, maxx);
    maxy = min(90, maxy);

    *out = (futile_bounds_s){minx, miny, maxx, maxy};
}

void futile_bounds_center(futile_bounds_s *bounds, double *x, double*y) {
    std::cout << "min:" << bounds->minx << " / " << bounds->miny << std::endl;
    std::cout << "max:" << bounds->maxx << " / " << bounds->maxy << std::endl;

    *x = bounds->minx + (bounds->maxx-bounds->minx)*0.5;
    *y = bounds->miny + (bounds->maxy-bounds->miny)*0.5;
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
                // The size of the custom scene is unique enough
                result.heart_beat = std::to_string(request.body.size());
            }
            else {
                // If there IS a SCENE QUERRY value load it
                setScene(scene_itr->second.front());
                result.heart_beat = scene_itr->second.front();
            }

            bool size_and_pos = true;
            float pixel_density = 1.0f;

            //  SIZE
            //  ---------------------
            auto width_itr = request.query.find("width");
            if (width_itr == request.query.cend() || width_itr->second.size() == 0)
                size_and_pos = false;
            auto height_itr = request.query.find("height");
            if (height_itr == request.query.cend() || height_itr->second.size() == 0)
                size_and_pos = false;
            auto density_itr = request.query.find("density");
            if (density_itr != request.query.cend() && density_itr->second.size() > 0)
                pixel_density = fmax(1.,std::stof(density_itr->second.front()));
            //  POSITION
            //  ---------------------
            auto lat_itr = request.query.find("lat");
            if (lat_itr == request.query.cend() || lat_itr->second.size() == 0)
                size_and_pos = false;
            auto lon_itr = request.query.find("lon");
            if (lon_itr == request.query.cend() || lon_itr->second.size() == 0)
                size_and_pos = false;
            auto zoom_itr = request.query.find("zoom");
            if (zoom_itr == request.query.cend() || zoom_itr->second.size() == 0)
                size_and_pos = false;
            

            if (size_and_pos) {
                // Set Map and OpenGL context size
                setSize(std::stoi(width_itr->second.front()), std::stoi(height_itr->second.front()), pixel_density);
                setPosition(std::stod(lon_itr->second.front()), std::stod(lat_itr->second.front()));
                setZoom(std::stof(zoom_itr->second.front()));
            } else {
                const std::regex re("\\/(\\d*)\\/(\\d*)\\/(\\d*)\\.png");
                std::smatch match;

                if (std::regex_search(request.path, match, re) && match.size() == 4) {
                    setSize(256,256, pixel_density);

                    int tile_coord[3] = {0,0,0};
                    for (int i = 0; i < 3; i++) {
                        std::istringstream cur(match.str(i+1));
                        cur >> tile_coord[i];
                    }
                    futile_coord_s tile;
                    tile.z = tile_coord[0];
                    setZoom(tile.z);

                    tile.x = tile_coord[1];
                    tile.y = tile_coord[2];
                    futile_bounds_s bounds;
                    futile_coord_to_bounds(&tile, &bounds);

                    setPosition(bounds.minx + (bounds.maxx-bounds.minx)*0.5,bounds.miny + (bounds.maxy-bounds.miny)*0.5);
                }
                else {
                    LOG("not enought data to construct image");
                    throw std::runtime_error("not enought data to construct image");
                }
            }

            //  OPTIONAL tilt and rotation
            //  ---------------------
            auto tilt_itr = request.query.find("tilt");
            if (tilt_itr != request.query.cend() && tilt_itr->second.size() != 0) {
                // If TILT QUERRY is provided assigned ...
                setTilt(std::stof(tilt_itr->second.front()));
            }
            else {
                // othewise use default (0.)
                setTilt(0.0f);
            }

            auto rotation_itr = request.query.find("rotation");
            if (rotation_itr != request.query.cend() && rotation_itr->second.size() != 0) {
                // If ROTATION QUERRY is provided assigned ...
                setRotation(std::stof(rotation_itr->second.front()));
            }
            else {
                // othewise use default (0.)
                setRotation(0.0f);
            }

            // Time to render
            //  ---------------------
            resetTimer("Rendering");
            std::string image;
            if (m_map) {
                update();

                // Render Tangram Scene
                m_aab->bind();
                m_map->render();
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
