#include <thread>
#include <pthread.h>
#include <mutex>
#include <atomic>

#include <iostream>

#include <curl/curl.h>      // Curl
#include "glm/trigonometric.hpp" // GLM for the radians/degree calc

#include "tangram.h"        // Tangram
#include "platform.h"       // Tangram platform specifics

#include "context.h"        // This set the headless context
#include "platform_headless.h" // headless platforms (Linux and RPi)

// Some of my own tools to scale down the rendering image and download it to a file
#include "image_out.h"
#include "gl/fbo.h"         // simple FBO implementation
#include "gl/shader.h"      // simple ShaderProgram implementation
#include "gl/vbo.h"         // simple VBO implementation
#include "types/shapes.h"   // Small library to compose basic shapes (use for rect)

#include "utils.h"

// Default parametters
Tangram::Map* map = nullptr;
int width = 800;     // Default Width of the image (will be multipl by 2 for the antialiasing)
int height = 480;    // Default height of the image (will be multipl by 2 for the antialiasing)
std::string style = "scene.yaml";
#define ZEROMQ_PORT 5555

std::atomic<bool> bRun(true);   //
std::vector<std::string> queue; // Commands Queue
std::mutex queueMutex;

bool bUpdateStatus = false;

// Antialiase tools
Fbo renderFbo;      // FrameBufferObject where the tangram scene will be renderd 
Fbo smallFbo;       // FrameBufferObject of the half of the size to fake an antialiased image
Vbo* smallVbo;      // VertexBufferObject to down sample the renderFbo to (like a billboard)
Shader smallShader; // Shader program to use to down sample the renderFbo with

//============================================================================== CONTROL THREADS
void consoleThread() {
    std::string line;
    while (bRun.load() && std::getline(std::cin, line)) {
        std::lock_guard<std::mutex> lock(queueMutex);
        queue.push_back(line);
    }
}

void zmqThread() {
    std::string line = "";
    while (bRun.load() && zmqRecv(line)) {
        if (line.size() > 3) {
            std::cout << "ZMQ> " << line << std::endl; 
            std::lock_guard<std::mutex> lock(queueMutex);
            queue.push_back(line);
        }
    }
}

//============================================================================== MAIN FUNCTION
bool updateTangram();
void processCommand (std::string &_command);
void resize (int _width, int _height);
void screenshot (std::string &_outputFile);

int main (int argc, char **argv) {

    // CONTROL LOOP
    std::thread console(&consoleThread);

    zmqConnect(ZEROMQ_PORT);
    std::thread zmq(&zmqThread);

    LOG("PAPARAZZI CLIENT at port %i ", ZEROMQ_PORT);

    // Initialize cURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // Start OpenGL ES context
    LOG("Creating OpenGL ES context");
    initGL(width, height);
    // Create a rectangular Billboard to draw the main FBO
    smallVbo = rect(0.0,0.0,1.,1.).getVbo();
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
    smallShader.load(smallFrag, smallVert);

    // TANGRAM LOOP
    LOG("Creating a new TANGRAM instances");
    map = new Tangram::Map();
    map->loadSceneAsync(style.c_str());
    map->setupGL();
    resize(width, height);
    while (bRun.load()) {
        std::string lastStr;
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (queue.size() > 0) {
                lastStr = queue.back();
                queue.pop_back();
            }
        }

        if (lastStr.size() > 0) {
            if (lastStr == "quit") {
                bRun.store(false);
            } 
            else if (lastStr == "status") {
                LOG("Status: %s", bUpdateStatus ? "finish" : "updating");
            }
            else {
                std::vector<std::string> commands = split(lastStr, ';');
                for (auto&& command : commands) {
                    processCommand(command);
                    updateTangram();
                }
            }
        } 
        updateTangram();
    }

    // Clear running threaths and close OpenGL ES
    logMsg("Closing...");
    finishUrlRequests();
    curl_global_cleanup();
    closeGL();
    
    if (map) {
        delete map;
        map = nullptr;
    }

    // Force cinWatcher to finish (because is waiting for input)
    pthread_t consoleHandler = console.native_handle();
    pthread_cancel(consoleHandler);
    console.join();

    // Force cinWatcher to finish (because is waiting for input)
    zmqClose();
    pthread_t zmqHandler = zmq.native_handle();
    pthread_cancel(zmqHandler);
    zmq.join();

    logMsg("END\n");

    // Go home
    return 0;
}

//============================================================================== MAIN FUNCTION
bool updateTangram () {
    // Update Network Queue
    processNetworkQueue();

    bool bFinish = false;
    bFinish = map->update(10.);
    if (bFinish && !bUpdateStatus) {
        LOG("< FINISH");
    }
    bUpdateStatus = bFinish;

    return bFinish;
}

void processCommand (std::string &_command) {
    if (map) {
        resetTimer();
        std::vector<std::string> elements = split(_command, ' ');
        if (elements[0] == "scene") {
            if (elements.size() == 1) {
                std::cout << style << std::endl;
            } else {
                style = elements[1];
                map->loadSceneAsync(style.c_str());
            }
        }
        else if (elements[0] == "zoom") {
            if (elements.size() == 1) {
                std::cout << map->getZoom() << std::endl;
            } else {
                LOG("Set zoom: %f", toFloat(elements[1]));
                map->setZoom(toFloat(elements[1]));
            }
        }
        else if (elements[0] == "tilt") {
            if (elements.size() == 1) {
                std::cout << map->getZoom() << std::endl;
            } else {
                LOG("Set tilt: %f", toFloat(elements[1]));
                map->setTilt(toFloat(elements[1]));
            }
        }
        else if (elements[0] == "rotate") {
            if (elements.size()) {
                std::cout << map->getZoom() << std::endl;
            } else {
                LOG("Set rotation: %f", toFloat(elements[1]));
                map->setRotation(toFloat(elements[1]));
            }
        }
        else if (elements.size() > 2 && elements[0] == "position") {
            if (elements.size() == 1) {
                double lat = 0;
                double lon = 0;
                map->getPosition(lon, lat);
                std::cout << lon << 'x' << lat << std::endl;
            } else {
                LOG("Set position: %f (lon), %f (lat)", toFloat(elements[1]), toFloat(elements[2]));
                map->setPosition(toDouble(elements[1]), toDouble(elements[2]));
                if (elements.size() == 4) {
                    LOG("Set zoom: %f", toFloat(elements[3]));
                    map->setZoom(toFloat(elements[3]));
                }   
            }
        }
        else if (elements[0] == "size") {
            if (elements.size() == 1) {
                std::cout << width << "x" << height << std::endl;
            } else {
                resize(toInt(elements[1]), toInt(elements[2]));
            }
        }
        else if (elements[0] == "print") {
            screenshot(elements[1]);
        }
    }
    else {
        LOG("No TANGRAM instance");
    }
}

void resize (int _width, int _height) {
    // Setup the size of the image
    if (map) {
        map->setPixelScale(2.0f);
        width = _width*2;
        height = _height*2;
        map->resize(width, height);
        renderFbo.resize(width, height);    // Allocate the main FrameBufferObject were tangram will be draw
        smallFbo.resize(width/2, height/2); // Allocate the smaller FrameBufferObject were the main FBO will be draw
    }
}

void screenshot (std::string &_outputFile) {
    if (map) {
        double lastTime = getTime();
        double currentTime = lastTime;
        float delta = 0.0;
        if (!bUpdateStatus) {
            LOG("We need to wait until scene finish loading");
        }
        while (delta < 5.0 && !bUpdateStatus && !updateTangram()) {
            currentTime = getTime();
            delta = currentTime - lastTime;
            // std::cout << delta << std::endl;
        }
        
        LOG("Rendering...");
        // Render the Tangram scene inside an FrameBufferObject
        renderFbo.bind();   // Bind main FBO
        map->render();  // Render Tangram Scene
        renderFbo.unbind(); // Unbind main FBO
        
        // at the half of the size of the rendered scene
        int w = width/2;
        int h = height/2;
        
        // Draw the main FBO inside the small one
        smallFbo.bind();
        smallShader.use();
        smallShader.setUniform("u_resolution",w ,h);
        smallShader.setUniform("u_buffer", &renderFbo, 0);
        smallVbo->draw(&smallShader);
        
        // Once the main FBO is draw take a picture
        LOG("Extracting pixels...");
        unsigned char* pixels = new unsigned char[w*h*4];   // allocate memory for the pixels
        glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels); // Read throug the current buffer pixels
        LOG("Saving pixels at %s", _outputFile.c_str());
        savePixels(_outputFile.c_str(), pixels, w, h);   // save them to a file
        
        // Close the smaller FBO because we are civilize ppl
        smallFbo.unbind();
        
        LOG("< FINISH");
    }
}
 