#pragma once

#include "platform_gl.h"
#undef countof

// GL Context
void    initGL(int width, int height);
void    renderGL();
void    closeGL();

// SET
void    setRenderRequest(bool _render);

// GET
bool    getRenderRequest();
double  getTime();
