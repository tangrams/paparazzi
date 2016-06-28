#include "context.h"

static GLFWwindow* window;
static float dpiScale = 1.0;

void initGL(int width, int height) {
     // Initialize the windowing library
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
}

void renderGL() {
    glfwSwapBuffers(window);
}

void closeGL() {
    glfwTerminate();
}

bool doTerminate() {
    return glfwWindowShouldClose(window);
}

double getTime() {
    return glfwGetTime();
}

