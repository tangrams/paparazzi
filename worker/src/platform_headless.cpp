#include "platform.h"
#include "log.h"
#include "context.h"

#include <stdio.h>
#include <stdarg.h>
#include <iostream>
#include <fstream>
#include <functional>
#include <string>
#include <list>

#include "urlWorker.h"
#include "platform_headless.h"
#include "gl/hardware.h"

#include <libgen.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/syscall.h>

#ifdef PLATFORM_OSX
#define DEFAULT "fonts/NotoSans-Regular.ttf"
#define FONT_AR "fonts/NotoNaskh-Regular.ttf"
#define FONT_HE "fonts/NotoSansHebrew-Regular.ttf"
#define FONT_JA "fonts/DroidSansJapanese.ttf"
#define FALLBACK "fonts/DroidSansFallback.ttf"
#else
#include <fontconfig.h>
static std::vector<std::string> s_fallbackFonts;
static FcConfig* s_fcConfig = nullptr;
#endif

#define NUM_WORKERS 10

static bool s_isContinuousRendering = false;

static UrlWorker s_Workers[NUM_WORKERS];
static std::list<std::unique_ptr<UrlTask>> s_urlTaskQueue;

static double startTime = 0.0;

void logMsg(const char* fmt, ...) {
    std::string str = std::to_string(getTime()-startTime) + " - " + fmt;
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, str.c_str(), args);
    va_end(args);
}

void resetTimer(std::string _msg) {
    LOG(" ");
    startTime = getTime();
    LOG("START %s",_msg.c_str());
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
    #ifndef PLATFORM_RPI
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
    if (!bytes) { return {}; }

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

#ifdef PLATFORM_OSX
FontSourceHandle getFontHandle(const char* _path) {
    FontSourceHandle fontSourceHandle = [_path](size_t* _size) -> unsigned char* {
        LOG("Loading font %s", _path);

        auto cdata = bytesFromFile(_path, *_size);

        return cdata;
    };

    return fontSourceHandle;
}
#else

void initPlatformFontSetup() {
    static bool s_platformFontsInit = false;
    if (s_platformFontsInit) { return; }

    s_fcConfig = FcInitLoadConfigAndFonts();

    std::string style = "Regular";

    FcStrSet* fcLangs = FcGetLangs();
    FcStrList* fcLangList = FcStrListCreate(fcLangs);
    FcChar8* fcLang;
    while ((fcLang = FcStrListNext(fcLangList))) {
        FcValue fcStyleValue, fcLangValue;

        fcStyleValue.type = fcLangValue.type = FcType::FcTypeString;
        fcStyleValue.u.s = reinterpret_cast<const FcChar8*>(style.c_str());
        fcLangValue.u.s = fcLang;

        // create a pattern with style and family font properties
        FcPattern* pat = FcPatternCreate();

        FcPatternAdd(pat, FC_STYLE, fcStyleValue, true);
        FcPatternAdd(pat, FC_LANG, fcLangValue, true);
        //FcPatternPrint(pat);

        FcConfigSubstitute(s_fcConfig, pat, FcMatchPattern);
        FcDefaultSubstitute(pat);

        FcResult res;
        FcPattern* font = FcFontMatch(s_fcConfig, pat, &res);
        if (font) {
            FcChar8* file = nullptr;
            if (FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch) {
                // Make sure this font file is not previously added.
                if (std::find(s_fallbackFonts.begin(), s_fallbackFonts.end(),
                              reinterpret_cast<char*>(file)) == s_fallbackFonts.end()) {
                    s_fallbackFonts.emplace_back(reinterpret_cast<char*>(file));
                }
            }
            FcPatternDestroy(font);
        }
        FcPatternDestroy(pat);
    }
    FcStrListDone(fcLangList);
    s_platformFontsInit = true;
}

#endif

std::vector<FontSourceHandle> systemFontFallbacksHandle() {
    std::vector<FontSourceHandle> handles;

    #ifdef PLATFORM_OSX
        handles.push_back(getFontHandle(DEFAULT));
        handles.push_back(getFontHandle(FONT_AR));
        handles.push_back(getFontHandle(FONT_HE));
        handles.push_back(getFontHandle(FONT_JA));
        handles.push_back(getFontHandle(FALLBACK));
    #else
    initPlatformFontSetup();
    for (auto& path : s_fallbackFonts) {
        FontSourceHandle fontSourceHandle = [&](size_t* _size) -> unsigned char* {
            LOG("Loading font %s", path.c_str());

            auto cdata = bytesFromFile(path.c_str(), *_size);

            return cdata;
        };

        handles.push_back(fontSourceHandle);
    }
    #endif

    return handles;
}

// std::string fontFallbackPath(int _importance, int _weightHint) {
//     #ifndef PLATFORM_OSX
//     if ((size_t)_importance >= s_fallbackFonts.size()) {
//         return "";
//     }
//     return s_fallbackFonts[_importance];
//     #else
//     return "";
//     #endif
// }

std::string fontPath(const std::string& _name, const std::string& _weight, const std::string& _face) {
    #ifndef PLATFORM_OSX
    initPlatformFontSetup();

    if (!s_fcConfig) {
        return "";
    }

    std::string fontFile = "";
    FcValue fcFamily, fcFace, fcWeight;

    fcFamily.type = fcFace.type = fcWeight.type = FcType::FcTypeString;
    fcFamily.u.s = reinterpret_cast<const FcChar8*>(_name.c_str());
    fcWeight.u.s = reinterpret_cast<const FcChar8*>(_weight.c_str());
    fcFace.u.s = reinterpret_cast<const FcChar8*>(_face.c_str());

    // Create a pattern with family, style and weight font properties
    FcPattern* pattern = FcPatternCreate();

    FcPatternAdd(pattern, FC_FAMILY, fcFamily, true);
    FcPatternAdd(pattern, FC_STYLE, fcFace, true);
    FcPatternAdd(pattern, FC_WEIGHT, fcWeight, true);
    //FcPatternPrint(pattern);

    FcConfigSubstitute(s_fcConfig, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult res;
    FcPattern* font = FcFontMatch(s_fcConfig, pattern, &res);
    if (font) {
        FcChar8* file = nullptr;
        FcChar8* fontFamily = nullptr;
        if (FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch &&
            FcPatternGetString(font, FC_FAMILY, 0, &fontFamily) == FcResultMatch) {
            // We do not want the "best" match, but an "exact" or at least the same "family" match
            // We have fallbacks to cover rest here.
            if (strcmp(reinterpret_cast<const char*>(fontFamily), _name.c_str()) == 0) {
                fontFile = reinterpret_cast<const char*>(file);
            }
        }
        FcPatternDestroy(font);
    }

    FcPatternDestroy(pattern);

    return fontFile;
    #else
    return "";
    #endif
}

unsigned char* systemFont(const std::string& _name, const std::string& _weight, const std::string& _face, size_t* _size) {
    std::string path = fontPath(_name, _weight, _face);

    if (path.empty()) { return nullptr; }

    return bytesFromFile(path.c_str(), *_size);
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
    Tangram::Hardware::supportsMapBuffer = true;
    // #ifdef PLATFORM_LINUX
    // glBindVertexArrayOESEXT = (PFNGLBINDVERTEXARRAYPROC)glfwGetProcAddress("glBindVertexArray");
    // glDeleteVertexArraysOESEXT = (PFNGLDELETEVERTEXARRAYSPROC)glfwGetProcAddress("glDeleteVertexArrays");
    // glGenVertexArraysOESEXT = (PFNGLGENVERTEXARRAYSPROC)glfwGetProcAddress("glGenVertexArrays");
    // #endif
}

// #ifdef PLATFORM_RPI
// // Dummy VertexArray functions
// GL_APICALL void GL_APIENTRY glBindVertexArray(GLuint array) {}
// GL_APICALL void GL_APIENTRY glDeleteVertexArrays(GLsizei n, const GLuint *arrays) {}
// GL_APICALL void GL_APIENTRY glGenVertexArrays(GLsizei n, GLuint *arrays) {}
// #endif

