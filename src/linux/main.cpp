#include <curl/curl.h>
#include <memory>

#include "tangram.h"
#include "data/clientGeoJsonSource.h"
#include "platform_linux.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <cstdlib>
#include <signal.h>

#include <sstream>
#include <iostream>
#include "glm/trigonometric.hpp"

#include "image_out.h"
#include "fbo.h"

int width = 800;
int height = 600;
float rot = 0.0f;
float zoom = 0.0f;
float tilt = 0.0f;
double lat = 0.0f;
double lon = 0.0f;
std::string sceneFile = "scene.yaml";
std::string outputFile = "out.png";

Fbo fbo;

// Forward declaration
void init_main_window(bool recreate);

GLFWwindow* main_window = nullptr;
bool recreate_context;
float pixel_scale = 1.0;

// Input handling
// ==============

const double double_tap_time = 0.5; // seconds
const double scroll_span_multiplier = 0.05; // scaling for zoom and rotation
const double scroll_distance_multiplier = 5.0; // scaling for shove
const double single_tap_time = 0.25; //seconds (to avoid a long press being considered as a tap)

bool was_panning = false;
double last_time_released = -double_tap_time; // First click should never trigger a double tap
double last_time_pressed = 0.0;
double last_time_moved = 0.0;
double last_x_down = 0.0;
double last_y_down = 0.0;
double last_x_velocity = 0.0;
double last_y_velocity = 0.0;
bool scene_editing_mode = false;

std::shared_ptr<Tangram::ClientGeoJsonSource> data_source;

void init_main_window(bool recreate) {

    // Setup tangram
    Tangram::initialize(sceneFile.c_str());

    if (!recreate) {
        // Destroy old window
        if (main_window != nullptr) {
            glfwDestroyWindow(main_window);
        }

        // Create a windowed mode window and its OpenGL context
        glfwWindowHint(GLFW_SAMPLES, 2);
        main_window = glfwCreateWindow(width, height, "Tangram ES", NULL, NULL);
        if (!main_window) {
            glfwTerminate();
        }

        // Make the main_window's context current
        glfwMakeContextCurrent(main_window);
    }

    // Setup graphics
    Tangram::setupGL();
    Tangram::resize(width, height);

    data_source = std::make_shared<Tangram::ClientGeoJsonSource>("touch", "");
    Tangram::addDataSource(data_source);
}


// Main program
// ============

int main(int argc, char* argv[]) {

    for (int i = 1; i < argc ; i++) {
        if (std::string(argv[i]) == "-s" ||
            std::string(argv[i]) == "--scene") {
            sceneFile = std::string(argv[i+1]);
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

    static bool keepRunning = true;

    // Give it a chance to shutdown cleanly on CTRL-C
    signal(SIGINT, [](int) {
            if (keepRunning) {
                logMsg("shutdown\n");
                keepRunning = false;
                glfwPostEmptyEvent();
            } else {
                logMsg("killed!\n");
                exit(1);
            }});

    int argi = 0;
    while (++argi < argc) {
        if (strcmp(argv[argi - 1], "-f") == 0) {
            sceneFile = std::string(argv[argi]);
            logMsg("File from command line: %s\n", argv[argi]);
            break;
        }
    }

    // Initialize the windowing library
    if (!glfwInit()) {
        return -1;
    }

    struct stat sb;
    if (stat(sceneFile.c_str(), &sb) == -1) {
        logMsg("scene file not found!");
        exit(EXIT_FAILURE);
    }
    auto last_mod = sb.st_mtime;

    init_main_window(false);

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

    // Initialize cURL
    curl_global_init(CURL_GLOBAL_DEFAULT);

    double lastTime = glfwGetTime();

    setContinuousRendering(false);
    glfwSwapInterval(0);

    fbo.resize(width,height);
    fbo.bind();
    // glBindFramebuffer(GL_DRAW_FRAMEBUFFER​,fbo);

    recreate_context = false;

    // Loop until the user closes the window
    while (keepRunning && !glfwWindowShouldClose(main_window)) {

        double currentTime = glfwGetTime();
        double delta = currentTime - lastTime;
        lastTime = currentTime;

        processNetworkQueue();

        // Render
        bool bFinish = Tangram::update(delta);

        Tangram::render();

        if (bFinish) {
            LOG("SAVING PNG %s", outputFile.c_str());
            unsigned char* pixels = new unsigned char[width*height*4];
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            savePixels(outputFile.c_str(), pixels, width, height);
            glfwSetWindowShouldClose(main_window, true);
        }

        // Swap front and back buffers
        //glfwSwapBuffers(main_window);

        // Poll for and process events
        // if (isContinuousRendering()) {
        //     glfwPollEvents();
        // } else {
        //     glfwWaitEvents();
        // }

        if (recreate_context) {
            logMsg("recreate context\n");
             // Simulate GL context loss
            init_main_window(true);
            recreate_context = false;
        }

        if (scene_editing_mode) {
            if (stat(sceneFile.c_str(), &sb) == 0) {
                if (last_mod != sb.st_mtime) {
                    Tangram::loadScene(sceneFile.c_str());
                    last_mod = sb.st_mtime;
                }
            }
        }
    }

    finishUrlRequests();

    curl_global_cleanup();

    glfwTerminate();
    return 0;
}
