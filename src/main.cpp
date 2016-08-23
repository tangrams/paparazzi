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

#include <zmq.h>            // ZeroMQ

// Default parametters
Tangram::Map* map = nullptr;
static int width = 800;     // Default Width of the image (will be multipl by 2 for the antialiasing)
static int height = 480;    // Default height of the image (will be multipl by 2 for the antialiasing)

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

    void *context = zmq_ctx_new ();
    void *requester = zmq_socket (context, ZMQ_REQ);
    zmq_connect (requester, "tcp://localhost:5555");

    std::string line;
    char buffer [140];
    while (bRun.load() && zmq_recv(requester, buffer, 10, 0)) {
        zmq_send (requester, "<", 5, 0);
    }

    zmq_close (requester);
    zmq_ctx_destroy (context);
}

//============================================================================== MAIN FUNCTION
bool updateTangram();
void processCommand (std::string &_command);
void resize(int _width, int _height);
void screenshot(std::string _outputFile);

int main (int argc, char **argv) {

    // CONTROL LOOP
    std::thread console(&consoleThread);
    std::thread zmq(&zmqThread);

    LOG("Tangram ES - Paparazzi");

    // Initialize cURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // Start OpenGL ES context
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

    // MAIN LOOP
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
            } else {
                processCommand(lastStr);
            }
        } 
        else {
            updateTangram();
        }
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
    
    if (map) {
        bFinish = map->update(10.);
    }
     
    if ( bFinish && !bUpdateStatus) {
        LOG("< FINISH");
    }
    bUpdateStatus = bFinish;
    
    return bFinish;
}

void processCommand (std::string &_command) {
    std::vector<std::string> elements = split(_command, ' ');
    if (elements[0] == "set" && elements.size() > 2) {
        if (elements[1] == "scene") {
            resetTimer();
            if (!map) {
                map = new Tangram::Map();
                map->loadSceneAsync(elements[2].c_str());
                resize(width, height);
                // Start OpenGL resource of Tangram
                map->setupGL();
            } else {
                map->loadSceneAsync(elements[2].c_str());
            }
        }
        else if (map && elements[1] == "zoom") {
            resetTimer();
            LOG("Set zoom: %f", toFloat(elements[2]));
            map->setZoom(toFloat(elements[2]));
        }
        else if (map &&  elements[1] == "tilt") {
            resetTimer();
            LOG("Set tilt: %f", toFloat(elements[2]));
            map->setTilt(toFloat(elements[2]));
        }
        else if (map &&  elements[1] == "rotation") {
            resetTimer();
            LOG("Set rotation: %f", toFloat(elements[2]));
            map->setRotation(toFloat(elements[2]));
        }
        else if (map &&  elements.size() > 3 && elements[1] == "position") {
            resetTimer();
            LOG("Set position: %f (lon), %f (lat)", toFloat(elements[2]), toFloat(elements[3]));
            map->setPosition(toDouble(elements[2]), toDouble(elements[3]));
            if (elements.size() == 5) {
                LOG("Set zoom: %f", toFloat(elements[4]));
                map->setZoom(toFloat(elements[4]));
            }

        }
        else if (map && elements.size() > 2 && elements[1] == "size") {
            resetTimer();
            resize(toInt(elements[2]), toInt(elements[3]));
        }
    }
    else if (map && elements[0] == "get") {
        if (elements[1] == "status") {
            LOG("Status: %s", bUpdateStatus ? "finish" : "updating");
        }
    }
    else if (map && elements[0] == "print") {
        resetTimer();
        screenshot(elements[1]);
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

void screenshot (std::string _outputFile) {
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
            std::cout << delta << std::endl;
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
 