#pragma once

#include "platform_headless.h"
#undef countof

// GL Context
void    initGL(int width, int height);
void    renderGL();
void    closeGL();

double  getTime();
