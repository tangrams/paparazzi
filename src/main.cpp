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

bool bUpdateStatus = true;

// Antialiase tools
Fbo renderFbo;      // FrameBufferObject where the tangram scene will be renderd 
Fbo smallFbo;       // FrameBufferObject of the half of the size to fake an antialiased image
Vbo* smallVbo;      // VertexBufferObject to down sample the renderFbo to (like a billboard)
Shader smallShader; // Shader program to use to down sample the renderFbo with

//============================================================================== MAIN FUNCTION
void controlThread() {
    std::string line;
    while (bRun.load() && std::getline(std::cin, line)) {
        std::lock_guard<std::mutex> lock(queueMutex);
        queue.push_back(line);
    }
}

//============================================================================== MAIN FUNCTION
bool updateTangram();
void processCommand (std::string &_command);
void resize(int _width, int _height);
void screenshot(std::string _outputFile);

int main (int argc, char **argv) {

    // CONTROL LOOP
    std::thread control(&controlThread);

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
    logMsg("Closing...\n");
    finishUrlRequests();
    curl_global_cleanup();
    closeGL();
    
    // Force cinWatcher to finish (because is waiting for input)
    pthread_t handler = control.native_handle();
    pthread_cancel(handler);
    control.join();

    logMsg("END\n");

    // Go home
    return 0;
}

//============================================================================== MAIN FUNCTION
bool updateTangram() {
    // Update Network Queue
    processNetworkQueue();

    bool bFinish = Tangram::update(10.);
     
    if ( bFinish && !bUpdateStatus) {
        LOG("Command excecute");
    }
    bUpdateStatus = bFinish;
    
    return bFinish;
}

void processCommand (std::string &_command) {
    std::vector<std::string> elements = split(_command, ' ');
    if (elements[0] == "set") {
        if (elements[1] == "scene") {
            resetTimer();
            Tangram::loadSceneAsync(elements[2].c_str());
        }
        else if (elements[1] == "zoom") {
            resetTimer();

            LOG("Set zoom: %f", toFloat(elements[2]));
            Tangram::setZoom(toFloat(elements[2]));

        }
        else if (elements[1] == "tilt") {
            resetTimer();

            LOG("Set tilt: %f", toFloat(elements[2]));
            Tangram::setTilt(toFloat(elements[2]));

        }
        else if (elements[1] == "rotation") {
            resetTimer();

            LOG("Set rotation: %f", toFloat(elements[2]));
            Tangram::setRotation(toFloat(elements[2]));

        }
        else if (elements[1] == "position") {
            resetTimer();

            LOG("Set position: %f (lon), %f (lat)", toFloat(elements[2]), toFloat(elements[3]));
            Tangram::setPosition(toDouble(elements[2]), toDouble(elements[3]));

        }
        else if (elements[1] == "size") {
            resetTimer();

            LOG("Resize: %ix%i", toInt(elements[2]), toInt(elements[3]));
            resize(toInt(elements[2]), toInt(elements[3]));

        }
    }
    else if (elements[0] == "get") {
        if (elements[1] == "status") {
            LOG("Status: %s", bUpdateStatus ? "finish" : "updating");

        }
    }
    else if (elements[0] == "print") {
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
    LOG("Extracting pixels...");
    unsigned char* pixels = new unsigned char[w*h*4];   // allocate memory for the pixels
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels); // Read throug the current buffer pixels
    LOG("Saving pixels at %s", _outputFile.c_str());
    savePixels(_outputFile.c_str(), pixels, w, h);   // save them to a file
   
    // Close the smaller FBO because we are civilize ppl
    smallFbo.unbind();

    LOG("Image saved at %s", _outputFile.c_str());
}
// 