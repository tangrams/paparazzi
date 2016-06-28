#pragma once

#if defined(PLATFORM_RPI)
#include "platform_rpi.h"
#elif defined(PLATFORM_LINUX)
#include "platform_linux.h"
#endif


bool savePixels(char const* _path, unsigned char* _pixels, int _width, int _height);

