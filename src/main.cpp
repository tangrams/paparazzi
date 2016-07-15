#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>
#include <sys/shm.h>

#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include <sstream>
#include <iostream>

#include <curl/curl.h>  // Curl
#include "glm/trigonometric.hpp" // GLM for the radians/degree calc

#include "tangram.h"    // Tangram
#include "platform.h"   // Tangram platform specifics

#include "context.h"    // This set the headless context
#include "platform_headless.h" // headless platforms (Linux and RPi)

// Some of my own tools to scale down the rendering image and download it to a file
#include "image_out.h"
#include "gl/fbo.h"     // simple FBO implementation
#include "gl/shader.h"  // simple ShaderProgram implementation
#include "gl/vbo.h"     // simple VBO implementation
#include "types/shapes.h" // Small library to compose basic shapes (use for rect)

// Default parametters
static std::string outputFile = "out.png";  // default filename of the generated image
static std::string scene = "scene.yaml";    // default scene file
static double lat = 0.0f;   // Default lat position
static double lon = 0.0f;   // Default lng position
static float zoom = 0.0f;   // Default zoom of the scene
static float rot = 0.0f;    // Default rotation of the scene (deg)
static float tilt = 0.0f;   // Default tilt angle (deg)
static int width = 800;     // Default Width of the image (will be multipl by 2 for the antialiasing)
static int height = 480;    // Default height of the image (will be multipl by 2 for the antialiasing)

// Global variables
static float maxTime = 20.0; // Max amount of seconds it can take to load a scene
static bool bUpdate = true; // This keeps the render loop running, once the scene is loaded will turn FALSE

// Antialiase tools
Fbo renderFbo;      // FrameBufferObject where the tangram scene will be renderd 
Fbo smallFbo;       // FrameBufferObject of the half of the size to fake an antialiased image
Vbo* smallVbo;      // VertexBufferObject to down sample the renderFbo to (like a billboard)
Shader smallShader; // Shader program to use to down sample the renderFbo with

// Setup and Update sub rutines to make it more readable
void setup(int argc, char **argv);  // Setup the scene
void update(double delta);          // Update the scene: where the scene is construct over time

//============================================================================== MAIN FUNCTION
int main(int argc, char **argv) {

    // Initialize cURL
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Set background color and clear buffers
    setup(argc, argv);

    // Keep track of time
    double startTime = getTime();
    double lastTime = startTime;
    double currentTime = startTime;

    // MAIN LOOP (will be force to end after maxTime)
    while ((currentTime - startTime) < maxTime && bUpdate) {
        // Update the time
        currentTime = getTime();

        // Update Network Queue
        processNetworkQueue();
        // Update Scene
        update(currentTime - lastTime);

        // Update time
        lastTime = currentTime;
    }

    if ((currentTime - startTime) >= maxTime) {
        logMsg("Time out!\n");
    }

    // Clear running threaths and close OpenGL ES
    logMsg("Closing...\n");
    finishUrlRequests();
    curl_global_cleanup();
    closeGL();
    logMsg("END\n");


    // Go home
    return 0;
}

//============================================================================== SETUP
void setup(int argc, char **argv) {

    // Parse arguments into default variables
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

    // What ever the user said let's multiply the scene size x2 
    // to then scale it down x.5 for a cheap antialiase
    width *= 2;
    height *= 2;

    // Start Tangram
    Tangram::initialize(scene.c_str());     // Initialite Tangram scene engine
    Tangram::setPixelScale(2.0f);           // Because we are scaling everything x2, tangram needs to belive this is a retina display
    Tangram::loadSceneAsync(scene.c_str()); // Start loading the scene file

    // Start OpenGL ES context
    initGL(width, height);
    
    // Start OpenGL resource of Tangram
    Tangram::setupGL();
    Tangram::resize(width, height);

    // Allocate the main FrameBufferObject were tangram will be draw
    renderFbo.resize(width, height);

    // Allocate the smaller FrameBufferObject were the main FBO will be draw
    smallFbo.resize(width/2, height/2);
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

    // If one of the default parameters is different than 0.0 change it
    if (lon != 0.0f && lat != 0.0f) {
        logMsg('set position %f lng, %f lat\n', lon, lat);
        Tangram::setPosition(lon,lat);
    }
    if (zoom != 0.0f) {
        logMsg('set zoom %f\n', zoom);
        Tangram::setZoom(zoom);
    }
    if (tilt != 0.0f) {
        logMsg('set tilt %f\n', tilt);
        Tangram::setTilt(glm::radians(tilt));
    }
    if (rot != 0.0f) {
        logMsg('set rotation %f\n', tilt);
        Tangram::setRotation(glm::radians(rot));
    }
}

//============================================================================== UPDATE
void update(double delta) {
    // Tangram:update return TRUE when the scene finish loading
    bool bFinish = Tangram::update(delta); 

    // Render the Tangram scene inside an FrameBufferObject
    renderFbo.bind();   // Bind main FBO
    Tangram::render();  // Render Tangram Scene
    renderFbo.unbind(); // Unbind main FBO

    if (bFinish) {
        // If it finish 

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
        LOG("SAVING PNG %s\n", outputFile.c_str());
        unsigned char* pixels = new unsigned char[w*h*4];   // allocate memory for the pixels
        glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels); // Read throug the current buffer pixels
        savePixels(outputFile.c_str(), pixels, w, h);   // save them to a file
       
        // Close the smaller FBO because we are civilize ppl
        smallFbo.unbind();

        // Kill the main update loop
        bUpdate = false;
    }
}
