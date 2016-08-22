#include <thread>
#include <pthread.h>
#include <chrono>
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
static int width = 800;     // Default Width of the image (will be multipl by 2 for the antialiasing)
static int height = 480;    // Default height of the image (will be multipl by 2 for the antialiasing)

std::atomic<bool> bRun(true);   //
std::vector<std::string> queue; // Commands Queue
std::mutex queueMutex;

// Antialiase tools
Fbo renderFbo;      // FrameBufferObject where the tangram scene will be renderd 
Fbo smallFbo;       // FrameBufferObject of the half of the size to fake an antialiased image
Vbo* smallVbo;      // VertexBufferObject to down sample the renderFbo to (like a billboard)
Shader smallShader; // Shader program to use to down sample the renderFbo with

//============================================================================== MAIN FUNCTION
void controlThread() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::lock_guard<std::mutex> lock(queueMutex);
        queue.push_back(line);
    }
}

//============================================================================== MAIN FUNCTION
bool updateTangram();
void processCommand (std::string &_command);
void resize(int _width, int _height);
void screenshot(std::string _outputFile);
bool waitForScene = false;

int main (int argc, char **argv) {

    // Initialize cURL
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Start Tangram
    Tangram::initialize("scene.yaml");     // Initialite Tangram scene engine
    Tangram::setPixelScale(2.0f);           // Because we are scaling everything x2, tangram needs to belive this is a retina display
    
    // Start OpenGL ES context
    initGL(width, height);
    resize(width, height);
    
    // Start OpenGL resource of Tangram
    Tangram::setupGL();

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

    // CONTROL LOOP
    std::thread control(&controlThread);

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
            processCommand(lastStr);
        } 
        else {
            updateTangram();
        }
    }

    // Clear running threaths and close OpenGL ES
    logMsg("Closing...\n");
    finishUrlRequests();
    curl_global_cleanup();
    closeGL();
    
    // Force cinWatcher to finish (because is waiting for input)
    pthread_t handler = control.native_handle();
    pthread_cancel(handler);

    logMsg("END\n");

    // Go home
    return 0;
}

//============================================================================== MAIN FUNCTION
bool updateTangram() {
    // Keep track of time
    static double lastTime = getTime();
    double currentTime = getTime();

    float delta = currentTime - lastTime;
    lastTime = currentTime;

    // Update Network Queue
    processNetworkQueue();

    if (Tangram::update(delta)) {
        waitForScene = false;
        return true;
    }
    else {
        return false;
    }
}

void processCommand (std::string &_command) {
    std::vector<std::string> elements = split(_command, ',');
    if (elements[0] == "quit") {
        bRun.store(false);
    } else if (elements[0] == "load") {
        resetTimer();
        waitForScene = true;

        Tangram::loadSceneAsync(elements[1].c_str());
    } else if (elements[0] == "zoom") {
        resetTimer();
        waitForScene = true;

        LOG("Set zoom: %f", toFloat(elements[1]));
        Tangram::setZoom(toFloat(elements[1]));
    } else if (elements[0] == "tilt") {
        resetTimer();
        waitForScene = true;

        LOG("Set tilt: %f", toFloat(elements[1]));
        Tangram::setTilt(toFloat(elements[1]));
    } else if (elements[0] == "rotation") {
        resetTimer();
        waitForScene = true;

        LOG("Set rotation: %f", toFloat(elements[1]));
        Tangram::setRotation(toFloat(elements[1]));
    } else if (elements[0] == "position") {
        resetTimer();
        waitForScene = true;

        LOG("Set position: %f (lon), %f (lat)", toFloat(elements[1]), toFloat(elements[2]));
        Tangram::setPosition(toDouble(elements[1]), toDouble(elements[2]));
    } else if (elements[0] == "resize") {
        resetTimer();
        waitForScene = true;

        LOG("Set resize: %ix%i", toInt(elements[1]), toInt(elements[2]));
        resize(toInt(elements[1]), toInt(elements[2]));
    } else if (elements[0] == "print") {
        resetTimer();
        screenshot(elements[1]);
    }
}

void resize(int _width, int _height) {
    width = _width*2;
    height = _height*2;

    // Setup the size of the image
    Tangram::resize(width, height);
    renderFbo.resize(width, height);    // Allocate the main FrameBufferObject were tangram will be draw
    smallFbo.resize(width/2, height/2); // Allocate the smaller FrameBufferObject were the main FBO will be draw
}

void screenshot(std::string _outputFile) {

    while (waitForScene) {
        if (updateTangram()) {
            waitForScene = false;
        }
        std::cout << 0;
    }

    // Render the Tangram scene inside an FrameBufferObject
    renderFbo.bind();   // Bind main FBO
    Tangram::render();  // Render Tangram Scene
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
    LOG("SAVING PNG %s\n", _outputFile.c_str());
    unsigned char* pixels = new unsigned char[w*h*4];   // allocate memory for the pixels
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels); // Read throug the current buffer pixels
    savePixels(_outputFile.c_str(), pixels, w, h);   // save them to a file
   
    // Close the smaller FBO because we are civilize ppl
    smallFbo.unbind();
}
