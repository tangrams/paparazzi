#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>
#include <sys/shm.h>

#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include <curl/curl.h>

#include "context.h"
#include "tangram.h"
#include "platform.h"
#include "platform_headless.h"

#include <sstream>
#include <iostream>
#include "glm/trigonometric.hpp"

#include "image_out.h"
#include "fbo.h"

static int width = 0;
static int height = 0;
static bool bUpdate = true;
static std::string outputFile = "out.png";

Fbo fbo;

//==============================================================================
void setup(int argc, char **argv);
void update(double delta);

int main(int argc, char **argv) {

    // Initialize cURL
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Set background color and clear buffers
    setup(argc, argv);

    double lastTime = getTime();

    while (bUpdate) {
        double currentTime = getTime();
        double delta = currentTime - lastTime;
        lastTime = currentTime;

        processNetworkQueue();

        update(delta);
    }

    finishUrlRequests();
    curl_global_cleanup();
    closeGL();
    return 0;
}

void setup(int argc, char **argv) {
    width = 800;
    height = 480;
    float rot = 0.0f;
    float zoom = 0.0f;
    float tilt = 0.0f;
    double lat = 0.0f;
    double lon = 0.0f;
    std::string scene = "scene.yaml";

    for (int i = 1; i < argc ; i++) {
        if (std::string(argv[i]) == "-s" ||
            std::string(argv[i]) == "--scene") {
            scene = std::string(argv[i+1]);
        } else if (std::string(argv[i]) == "-lat" ) {
            std::string argument = std::string(argv[i+1]);
            std::istringstream cur(argument);
            cur >> lat;
        } else if (std::string(argv[i]) == "-lon" ) {
            std::string argument = std::string(argv[i+1]);
            std::istringstream cur(argument);
            cur >> lon;
        } else if (std::string(argv[i]) == "-z" ||
                   std::string(argv[i]) == "--zoom" ) {
            std::string argument = std::string(argv[i+1]);
            std::istringstream cur(argument);
            cur >> zoom;
        } else if (std::string(argv[i]) == "-w" ||
                   std::string(argv[i]) == "--width") {
            std::string argument = std::string(argv[i+1]);
            std::istringstream cur(argument);
            cur >> width;
        } else if (std::string(argv[i]) == "-h" ||
                   std::string(argv[i]) == "--height") {
            std::string argument = std::string(argv[i+1]);
            std::istringstream cur(argument);
            cur >> height;
        } else if (std::string(argv[i]) == "-t" ||
                   std::string(argv[i]) == "--tilt") {
            std::string argument = std::string(argv[i+1]);
            std::istringstream cur(argument);
            cur >> tilt;
        } else if (std::string(argv[i]) == "-r" ||
                   std::string(argv[i]) == "--rotation") {
            std::string argument = std::string(argv[i+1]);
            std::istringstream cur(argument);
            cur >> rot;
        } else if (std::string(argv[i]) == "-o" ) {
            outputFile = std::string(argv[i+1]);
        }
    }

    // Start Tangram
    Tangram::initialize("scene.yaml");
    //Tangram::loadScene(scene.c_str());
    Tangram::loadSceneAsync(scene.c_str());

    // Start OpenGL context
    initGL(width, height);
    fbo.resize(width, height);
    fbo.bind();

    Tangram::setupGL();
    Tangram::resize(width, height);

    if (lon != 0.0f && lat != 0.0f) {
        Tangram::setPosition(lon,lat);
    }
    if (zoom != 0.0f) {
        Tangram::setZoom(zoom);
    }
    if (tilt != 0.0f) {
        Tangram::setTilt(glm::radians(tilt));
    }
    if (rot != 0.0f) {
        Tangram::setRotation(glm::radians(rot));
    }
}

void update(double delta) {// Update    
    bool bFinish = Tangram::update(delta);

    // Render
    Tangram::render();

    if (bFinish) {
        LOG("SAVING PNG %s", outputFile.c_str());
        unsigned char* pixels = new unsigned char[width*height*4];
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        savePixels(outputFile.c_str(), pixels, width, height);
        fbo.unbind();
        bUpdate = false;
    }
    renderGL();
}
