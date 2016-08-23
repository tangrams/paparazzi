#pragma once

#include "platform.h"

#ifdef PLATFORM_RPI
#include "bcm_host.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

#ifdef PLATFORM_LINUX
#define GL_GLEXT_PROTOTYPES
#include <GLFW/glfw3.h>
#endif

#ifdef PLATFORM_OSX
#define GLFW_INCLUDE_GLEXT
#include <GLFW/glfw3.h>
#endif

void zmqConnect(int _port);
bool zmqRecv(std::string &_buffer);
bool zmqSend(std::string &_buffer);
void zmqClose();

void processNetworkQueue();
void finishUrlRequests();

void resetTimer();
