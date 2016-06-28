#include "context.h"

#ifdef PLATFORM_RPI
#include <time.h>
#include <sys/time.h>
#include <assert.h>
#include <fcntl.h>
#include <iostream>
#include <termios.h>
#include <string>
#include <fstream>

#define check() assert(glGetError() == 0)
EGLDisplay display;
EGLSurface surface;
EGLContext context;
struct timeval tv;
unsigned long long timeStart;
#else
//  ---------------------------------------- using GLFW
//
static GLFWwindow* window;
#endif

// Initialize the OpenGL library
void initGL(int width, int height) {

    #ifdef PLATFORM_RPI
    // Start clock
    gettimeofday(&tv, NULL);
    timeStart = (unsigned long long)(tv.tv_sec) * 1000 +
                (unsigned long long)(tv.tv_usec) / 1000; 

    // Start OpenGL ES
    bcm_host_init();

    // Clear application state
    int32_t success = 0;
    EGLBoolean result;
    EGLint num_config;

    static EGL_DISPMANX_WINDOW_T nativeviewport;

    DISPMANX_ELEMENT_HANDLE_T dispman_element;
    DISPMANX_DISPLAY_HANDLE_T dispman_display;
    DISPMANX_UPDATE_HANDLE_T dispman_update;
    VC_RECT_T dst_rect;
    VC_RECT_T src_rect;

    uint32_t screen_width = width;
    uint32_t screen_height = height;

    static const EGLint attribute_list[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_DEPTH_SIZE, 16,
        EGL_NONE
    };

    static const EGLint context_attributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    EGLConfig config;

    // get an EGL display connection
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    assert(display!=EGL_NO_DISPLAY);
    check();

    // initialize the EGL display connection
    result = eglInitialize(display, NULL, NULL);
    assert(EGL_FALSE != result);
    check();

    // get an appropriate EGL frame buffer configuration
    result = eglChooseConfig(display, attribute_list, &config, 1, &num_config);
    assert(EGL_FALSE != result);
    check();

    // get an appropriate EGL frame buffer configuration
    result = eglBindAPI(EGL_OPENGL_ES_API);
    assert(EGL_FALSE != result);
    check();

    // create an EGL rendering context
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attributes);
    assert(context!=EGL_NO_CONTEXT);
    check();

    // create an EGL viewport surface
    success = graphics_get_display_size(0 /* LCD */, &screen_width, &screen_height);
    assert( success >= 0 );

    //  Initially the viewport is for all the screen
    dst_rect.x = 0;
    dst_rect.y = 0;
    dst_rect.width = screen_width;
    dst_rect.height = screen_height;

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = screen_width << 16;
    src_rect.height = screen_height << 16;

    dispman_display = vc_dispmanx_display_open( 0 /* LCD */);
    dispman_update = vc_dispmanx_update_start( 0 );

    dispman_element = vc_dispmanx_element_add( dispman_update, dispman_display,
                                       0/*layer*/, &dst_rect, 0/*src*/,
                                       &src_rect, DISPMANX_PROTECTION_NONE, 0 /*alpha*/, 0/*clamp*/, (DISPMANX_TRANSFORM_T)0/*transform*/);

    nativeviewport.element = dispman_element;
    nativeviewport.width = screen_width;
    nativeviewport.height = screen_height;
    vc_dispmanx_update_submit_sync( dispman_update );
    check();

    surface = eglCreateWindowSurface( display, config, &nativeviewport, NULL );
    assert(surface != EGL_NO_SURFACE);
    check();

    // connect the context to the surface
    result = eglMakeCurrent(display, surface, surface, context);
    assert(EGL_FALSE != result);
    check();

    // Set background color and clear buffers
    // glClearColor(0.15f, 0.25f, 0.35f, 1.0f);
    // glClear( GL_COLOR_BUFFER_BIT );

    glViewport(0.0f, 0.0f, (float)screen_width, (float)screen_height);
    check();

    #else
    //  ---------------------------------------- using GLFW
    //
    if (!glfwInit()) {
        return -1;
    }

    glfwWindowHint(GLFW_SAMPLES, 2);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    window = glfwCreateWindow(width, height, "", NULL, NULL);
    if (!window) {
        glfwTerminate();
    }

    // Make the main_window's context current
    glfwMakeContextCurrent(window);
    #endif
}

void renderGL() {
    #ifdef PLATFORM_RPI
    eglSwapBuffers(display, surface);
    #else
    //  ---------------------------------------- using GLFW
    //
    glfwSwapBuffers(window);
    #endif
}

// Release OpenGL resources
void closeGL() {
    #ifdef PLATFORM_RPI
    eglMakeCurrent( display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
    eglDestroySurface( display, surface );
    eglDestroyContext( display, context );
    eglTerminate( display );
    #else
    //  ---------------------------------------- using GLFW
    //
    glfwTerminate();
    #endif
}

double getTime() {
    #ifdef PLATFORM_RPI
    gettimeofday(&tv, NULL);
    unsigned long long timeNow = (unsigned long long)(tv.tv_sec) * 1000 + (unsigned long long)(tv.tv_usec) / 1000;
    return (timeNow - timeStart)*0.001;
    #else
    //  ---------------------------------------- using GLFW
    //
    return glfwGetTime();
    #endif
}

