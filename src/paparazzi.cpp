#include "paparazzi.h"

#define AA_SCALE 2.0
#define MAX_WAITING_TIME 10.0
#define IMAGE_DEPTH 4

//nuts and bolts required
#include <functional>
#include <csignal>

#include <curl/curl.h>      // Curl
#include "glm/trigonometric.hpp" // GLM for the radians/degree calc

#include "platform.h"       // Tangram platform specifics

#include "context.h"        // This set the headless context
#include "platform_headless.h" // headless platforms (Linux and RPi)

#include "utils.h"

Paparazzi::Paparazzi() : m_style("scene.yaml"), m_lat(0.0), m_lon(0.0), m_zoom(0.0f), m_rotation(0.0f), m_tilt(0.0), m_width(800), m_height(480) {

    // Initialize cURL
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Start OpenGL ES context
    LOG("Creating OpenGL ES context");
    initGL(width, height);

    // Create a simple vert/frag glsl shader to draw the main FBO with
    std::string smallVert = "#ifdef GL_ES\n\
precision mediump float;\n\
#endif\n\
attribute vec4 a_position;\n\
void main(void) {\n\
    gl_Position = a_position;\n\
}";
    std::string smallFrag = "#ifdef GL_ES\n\
precision mediump float;\n\
#endif\n\
uniform sampler2D u_buffer;\n\
uniform vec2 u_resolution;\n\
void main() {\n\
    gl_FragColor = texture2D(u_buffer, gl_FragCoord.xy/u_resolution.xy);\n\
}";
    m_smallShader = new Shader();
    m_smallShader->load(smallFrag, smallVert);

    // Create a rectangular Billboard to draw the main FBO
    m_smallVbo = rect(0.0,0.0,1.,1.).getVbo();

    LOG("Creating a new TANGRAM instances");
    m_map = new Tangram::Map();
    m_map->loadSceneAsync(style.c_str());
    m_map->setupGL();

    setSize(m_width, m_height);

    update();
}

Paparazzi::~Paparazzi() {
    LOG("Closing...");

    finishUrlRequests();
    curl_global_cleanup();

    if (m_smallShader) {
        delete m_smallShader;
    }
    
    if (m_smallFbo) {
        delete m_smallFbo;
    }
    
    if (m_smallVbo) {
        delete m_smallVbo;
    }
    
    if (m_renderFbo) {
        delete m_renderFbo;
    }

    if (m_map) {
        delete m_map;
    }

    closeGL();
    LOG("END\n");
}

void Paparazzi::setSize (int _width, int _height) {
    // Setup the size of the image
    if (m_map) {
        m_map->setPixelScale(AA_SCALE);
        m_width = _width*AA_SCALE;
        m_height = _height*AA_SCALE;
        m_map->resize(width, height);
    }

    if (!m_renderFbo) {
        m_renderFbo = new Fbo();
    }
    m_renderFbo.resize(width, height);    // Allocate the main FrameBufferObject were tangram will be draw

    if (!m_smallFbo) {
        m_smallFbo = new Fbo();
    }
    m_smallFbo.resize(width/AA_SCALE, height/AA_SCALE); // Allocate the smaller FrameBufferObject were the main FBO will be draw    
}

// prime_server stuff
worker_t::result_t Paparazzi::work (const std::list<zmq::message_t>& job, void* request_info){
    //false means this is going back to the client, there is no next stage of the pipeline
    worker_t::result_t result{false};

    //this type differs per protocol hence the void* fun
    auto& info = *static_cast<http_request_t::info_t*>(request_info);
    http_response_t response;
    try {
        //TODO: actually use/validate the request parameters
        auto request = http_request_t::from_string(
            static_cast<const char*>(job.front().data()), job.front().size());
            //TODO:get your image bytes here
        std::string image(":p"); // = tangram.render(args...);
        response = http_response_t(200, "OK", image);
    }
    catch(const std::exception& e) {
        //complain
        response = http_response_t(400, "Bad Request", e.what());
    }

    //does some tricky stuff with headers and different versions of http
    response.from_info(info);
    
    //formats the response to protocal that the client will understand
    result.messages.emplace_back(response.to_string());
    return result;
}

void Paparazzi::cleanup () {

}

void Paparazzi::update () {
    double lastTime = getTime();
    double currentTime = lastTime;
    float delta = 0.0;

    bool bFinish = false;
    while (delta < MAX_WAITING_TIME && !bFinish) {
        // Update Network Queue
        processNetworkQueue();
        bFinish = m_map->update(10.);
        delta = currentTime - lastTime;
    }
}

void Paparazzi::do (std::string &_command) {
    if (m_map) {
        std::vector<std::string> elements = split(_command, ' ');
        if (elements[0] == "scene") {
            if (elements.size() == 1) {
                std::cout << style << std::endl;
            }
            else {
                style = elements[1];
                resetTimer(_command);
                m_map->loadSceneAsync(style.c_str());
            }
        }
        else if (elements[0] == "zoom") {
            if (elements.size() == 1) {
                std::cout << zoom << std::endl;
            }
            else if (zoom != toFloat(elements[1])) {
                zoom = toFloat(elements[1]);
                resetTimer(_command);
                LOG("Set zoom: %f", zoom);
                m_map->setZoom(zoom);
            }
        }
        else if (elements[0] == "tilt") {
            if (elements.size() == 1) {
                std::cout << tilt << std::endl;
            }
            else if (tilt != toFloat(elements[1])) {
                tilt = toFloat(elements[1]);
                resetTimer(_command);
                LOG("Set tilt: %f", tilt);
                m_map->setTilt(tilt);
            }
        }
        else if (elements[0] == "rotation") {
            if (elements.size()) {
                std::cout << rotation << std::endl;
            }
            else if (rotation != toFloat(elements[1])) {
                rotation = toFloat(elements[1]);
                resetTimer(_command);
                LOG("Set rotation: %f", rotation);
                m_map->setRotation(rotation);
            }
        }
        else if (elements.size() > 2 && elements[0] == "position") {
            if (elements.size() == 1) {
                std::cout << lon << 'x' << lat << std::endl;
            }
            else if (lon != toDouble(elements[1]) || lat != toDouble(elements[2])) {
                lon = toDouble(elements[1]);
                lat = toDouble(elements[2]);
                resetTimer(_command);
                LOG("Set position: %f (lon), %f (lat)", lon, lat);
                m_map->setPosition(lon, lat); 
            }

            if (elements.size() == 4) {
                zoom = toFloat(elements[3]);
                resetTimer(_command);
                LOG("Set zoom: %f", zoom);
                m_map->setZoom(zoom);
            }  
        }
        else if (elements[0] == "size") {
            if (elements.size() == 1) {
                std::cout << width << "x" << height << std::endl;
            }
            else if (width != toInt(elements[1]) || height != toInt(elements[2])) {
                width = toInt(elements[1]);
                height = toInt(elements[2]);
                resetTimer(_command);
                resize(width, height);
            }
        }
        else if (elements[0] == "print") {
            resetTimer(_command);
            screenshot(elements[1]);
        }
    }
    else {
        LOG("No TANGRAM instance");
    }
}

Pixels Paparazzi::getPixels () {
    Pixels pixels;
    if (m_map) {
        LOG("Rendering...");
        // Render the Tangram scene inside an FrameBufferObject
        renderFbo.bind();   // Bind main FBO
        map->render();  // Render Tangram Scene
        renderFbo.unbind(); // Unbind main FBO
        
        // at the half of the size of the rendered scene
        pixels.width = width/AA_SCALE;
        pixels.height = height/AA_SCALE;
        pixels.depth = IMAGE_DEPTH;

        // Draw the main FBO inside the small one
        smallFbo.bind();
        smallShader.use();
        smallShader.setUniform("u_resolution", pixels.width, pixels.height);
        smallShader.setUniform("u_buffer", &renderFbo, 0);
        smallVbo->draw(&smallShader);
        
        // Once the main FBO is draw take a picture
        LOG("Extracting pixels...");
        pixels.bytes = new unsigned char[pixels.width * pixels.height * pixels.depth];   // allocate memory for the pixels
        glReadPixels(0, 0, pixels.width, pixels.height, GL_RGBA, GL_UNSIGNED_BYTE, raw_pixels); // Read throug the current buffer pixels

        // Flip the image on Y
        int row, col, z;
        stbi_uc temp;

        for (row = 0; row < (_height>>1); row++) {
            for (col = 0; col < pixels.width; col++) {
                for (z = 0; z < pixels.depth; z++) {
                    temp = pixels.bytes[(row * pixels.width + col) * pixels.depth + z];
                    pixels.bytes[(row * pixels.width + col) * pixels.depth + z] = result[((pixels.height - row - 1) * pixels.width + col) * pixels.depth + z];
                    pixels.bytes[((pixels.height - row - 1) * pixels.width + col) * pixels.depth + z] = temp;
                }
            }
        }

        // Close the smaller FBO because we are civilize ppl
        smallFbo.unbind();
    }
    return pixels;
}
