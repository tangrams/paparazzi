//#ifdef PLATFORM_RPI
#include "platform.h"
#include "gl.h"
#include "context.h"
//#endif

#include <stdio.h>
#include <stdarg.h>
#include <iostream>
#include <fstream>
#include <string>
#include <list>

#include "urlWorker.h"
#include "platform_headless.h"

#include <libgen.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/syscall.h>

// ZeroMQ
#include <zmq.h>
#include "utils.h"

#define NUM_WORKERS 3

#ifdef PLATFORM_LINUX
PFNGLBINDVERTEXARRAYPROC glBindVertexArrayOESEXT = 0;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArraysOESEXT = 0;
PFNGLGENVERTEXARRAYSPROC glGenVertexArraysOESEXT = 0;
#endif

static bool s_isContinuousRendering = false;
static std::string s_resourceRoot;

static UrlWorker s_Workers[NUM_WORKERS];
static std::list<std::unique_ptr<UrlTask>> s_urlTaskQueue;

static double startTime = 0.0;

void *context;
void *requester;

void logMsg(const char* fmt, ...) {
    std::string str = std::to_string(getTime()-startTime) + " - " + fmt;
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, str.c_str(), args);
    va_end(args);
}

void resetTimer() {
    LOG(">");
    startTime = getTime();
}

void zmqConnect (int _port) {
    context = zmq_ctx_new();
    requester = zmq_socket(context, ZMQ_REQ);
    zmq_connect (requester, std::string("tcp://localhost:"+toString(_port)).c_str());
}

bool zmqRecv (std::string &_buffer) {
    char buffer[140];
    bool rta = zmq_recv(requester, buffer, 140, 0);
    _buffer = std::string(buffer);
    return rta;
}

bool zmqSend (std::string &_buffer) {
    return zmq_send(requester, _buffer.c_str(), _buffer.size(), 0);
}

void zmqClose() {
    zmq_close(requester);
    zmq_ctx_destroy(context);
}

void processNetworkQueue() {
    // attach workers to NetWorkerData
    auto taskItr = s_urlTaskQueue.begin();
    for (auto& worker:s_Workers) {
        if (taskItr == s_urlTaskQueue.end()) {
            break;
        }
        if (worker.isAvailable()) {
            worker.perform(std::move(*taskItr));
            taskItr = s_urlTaskQueue.erase(taskItr);
        }
    }
}

void requestRender() {
    #ifdef PLATFORM_LINUX
    glfwPostEmptyEvent();
    #endif
}

void setContinuousRendering(bool _isContinuous) {
    s_isContinuousRendering = _isContinuous;
}

bool isContinuousRendering() {
    return s_isContinuousRendering;
}

std::string stringFromFile(const char* _path) {
    size_t length = 0;
    unsigned char* bytes = bytesFromFile(_path, length);

    std::string out(reinterpret_cast<char*>(bytes), length);
    free(bytes);

    return out;
}

unsigned char* bytesFromFile(const char* _path, size_t& _size) {

    std::ifstream resource(_path, std::ifstream::ate | std::ifstream::binary);

    if(!resource.is_open()) {
        logMsg("Failed to read file at path: %s\n", _path);
        _size = 0;
        return nullptr;
    }

    _size = resource.tellg();

    resource.seekg(std::ifstream::beg);

    char* cdata = (char*) malloc(sizeof(char) * (_size));

    resource.read(cdata, _size);
    resource.close();

    return reinterpret_cast<unsigned char *>(cdata);
}


// No system fonts implementation (yet!)
std::string systemFontPath(const std::string& _name, const std::string& _weight,
                           const std::string& _face) {
    return "";
}

// No system fonts fallback implementation (yet!)
std::string systemFontFallbackPath(int _importance, int _weightHint) {
    return "";
}

bool startUrlRequest(const std::string& _url, UrlCallback _callback) {
    std::unique_ptr<UrlTask> task(new UrlTask(_url, _callback));
    for (auto& worker:s_Workers) {
        if(worker.isAvailable()) {
            worker.perform(std::move(task));
            return true;
        }
    }
    s_urlTaskQueue.push_back(std::move(task));
    return true;

}

void cancelUrlRequest(const std::string& _url) {

    // Only clear this request if a worker has not started operating on it!
    // otherwise it gets too convoluted with curl!
    auto itr = s_urlTaskQueue.begin();
    while(itr != s_urlTaskQueue.end()) {
        if((*itr)->url == _url) {
            itr = s_urlTaskQueue.erase(itr);
        } else {
            itr++;
        }
    }
}

void finishUrlRequests() {
    for (auto& worker:s_Workers) {
        worker.join();
    }
}

void setCurrentThreadPriority(int priority){
    int tid = syscall(SYS_gettid);
    //int  p1 = getpriority(PRIO_PROCESS, tid);

    setpriority(PRIO_PROCESS, tid, priority);

    //int  p2 = getpriority(PRIO_PROCESS, tid);
    //logMsg("set niceness: %d -> %d\n", p1, p2);
}

void initGLExtensions() {
    #ifdef PLATFORM_LINUX
    glBindVertexArrayOESEXT = (PFNGLBINDVERTEXARRAYPROC)glfwGetProcAddress("glBindVertexArray");
    glDeleteVertexArraysOESEXT = (PFNGLDELETEVERTEXARRAYSPROC)glfwGetProcAddress("glDeleteVertexArrays");
    glGenVertexArraysOESEXT = (PFNGLGENVERTEXARRAYSPROC)glfwGetProcAddress("glGenVertexArrays");
    #endif
}

#ifdef PLATFORM_RPI
// Dummy VertexArray functions
GL_APICALL void GL_APIENTRY glBindVertexArray(GLuint array) {}
GL_APICALL void GL_APIENTRY glDeleteVertexArrays(GLsizei n, const GLuint *arrays) {}
GL_APICALL void GL_APIENTRY glGenVertexArrays(GLsizei n, GLuint *arrays) {}
#endif

