#pragma once

#include "platform_rpi.h"

#include <string>

bool savePixels(const std::string& _path, unsigned char* _pixels, int _width, int _height);

