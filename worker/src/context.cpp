#include "context.h"

static bool bRender;

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
DISPMANX_DISPLAY_HANDLE_T dispman_display;

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
    EGLBoolean result;
    EGLint num_config;

    static const EGLint attribute_list[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        // EGL_SAMPLE_BUFFERS, 1, EGL_SAMPLES, 4,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
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
    assert(context != EGL_NO_CONTEXT);
    check();
    
    static EGL_DISPMANX_WINDOW_T nativeviewport;

    VC_RECT_T dst_rect;
    VC_RECT_T src_rect;

    uint32_t dest_image_handle;
    uint32_t screen_width = width;
    uint32_t screen_height = height;

    //  Initially the viewport is for all the screen
    dst_rect.x = 0;
    dst_rect.y = 0;
    dst_rect.width = screen_width;
    dst_rect.height = screen_height;

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = screen_width << 16;
    src_rect.height = screen_height << 16;

    DISPMANX_RESOURCE_HANDLE_T dispman_resource;
    dispman_resource = vc_dispmanx_resource_create(VC_IMAGE_RGBA32, screen_width, screen_height, &dest_image_handle);
    dispman_display = vc_dispmanx_display_open_offscreen(dispman_resource, DISPMANX_NO_ROTATE);
    // dispman_display = vc_dispmanx_display_open( 0 /* LCD */);

    DISPMANX_UPDATE_HANDLE_T dispman_update;
    dispman_update = vc_dispmanx_update_start(0);

    DISPMANX_ELEMENT_HANDLE_T dispman_element;
    dispman_element = vc_dispmanx_element_add( dispman_update, dispman_display,
                                                0/*layer*/, &dst_rect, 0/*src*/,
                                                &src_rect, DISPMANX_PROTECTION_NONE, 
                                                0 /*alpha*/, 0/*clamp*/, (DISPMANX_TRANSFORM_T)0/*transform*/);

    nativeviewport.element = dispman_element;
    nativeviewport.width = screen_width;
    nativeviewport.height = screen_height;
    vc_dispmanx_update_submit_sync(dispman_update);
    check();

    surface = eglCreateWindowSurface(display, config, &nativeviewport, NULL);
    assert(surface != EGL_NO_SURFACE);
    check();

    // connect the context to the surface
    result = eglMakeCurrent(display, surface, surface, context);
    assert(EGL_FALSE != result);
    check();

    glViewport(0.0f, 0.0f, (float)screen_width, (float)screen_height);
    check();

    #else
    //  ---------------------------------------- using GLFW
    //
    if (!glfwInit()) {
        return;
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
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    if (surface != EGL_NO_SURFACE && eglDestroySurface(display, surface)) {
        printf("Surface destroyed ok\n");
    }
    if (context !=  EGL_NO_CONTEXT && eglDestroyContext(display, context)) {
        printf("Main context destroyed ok\n");
    }
    if (display != EGL_NO_DISPLAY && eglTerminate(display)) {
        printf("Display terminated ok\n");
    }
    if (eglReleaseThread()) {
        printf("EGL thread resources released ok\n");
    }
    if (vc_dispmanx_display_close(dispman_display) == 0) {
        printf("Dispmanx display rleased ok\n");
    }
    bcm_host_deinit();

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

void setRenderRequest(bool _render) {
    bRender = _render;
}

bool getRenderRequest() {
    return bRender;
}