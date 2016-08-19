#pragma once

#include "platform.h"

#ifdef PLATFORM_RPI
#include "bcm_host.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GLFW/glfw3.h>
// #define GLFW_INCLUDE_GLEXT
#endif

#ifdef PLATFORM_OSX
#define glClearDepthf glClearDepth
#define glDepthRangef glDepthRange
#define glDeleteVertexArrays glDeleteVertexArraysAPPLE
#define glGenVertexArrays glGenVertexArraysAPPLE
#define glBindVertexArray glBindVertexArrayAPPLE
#endif

void processNetworkQueue();
void finishUrlRequests();
