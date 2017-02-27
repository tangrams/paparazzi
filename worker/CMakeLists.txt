cmake_minimum_required(VERSION 2.8)
project(tangram)

# set(CMAKE_BUILD_TYPE Debug)

# helper functions
include(${PROJECT_SOURCE_DIR}/tangram-es/toolchains/utils.cmake)

# cmake output configuration
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(NOT EXISTS "${PROJECT_SOURCE_DIR}/tangram-es/core/deps/harfbuzz-icu-freetype/harfbuzz/README")
    message(SEND_ERROR "Missing submodules - Please run:\n 'git submodule update --init --recursive'")
    return()
endif()

# common compiler options
set(EXECUTABLE_NAME "paparazzi_worker")

# add sources and include headers
find_sources_and_include_directories(
    ${PROJECT_SOURCE_DIR}/src/*.h 
    ${PROJECT_SOURCE_DIR}/src/*.cpp)

if(${PLATFORM_TARGET} MATCHES "rpi")
    message(STATUS "Building config for RaspberryPI")

    add_definitions(-DPLATFORM_RPI)

    # compiler options
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fpermissive -g")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_CXX_FLAGS} -L/opt/vc/lib/ -lGLESv2 -lEGL -lbcm_host -lvchiq_arm -lvcos -lrt -lpthread")
    set(CXX_FLAGS_DEBUG "-g -O0")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
    
    include_directories(/opt/vc/include/)
    include_directories(/opt/vc/include/interface/vcos/pthreads)
    include_directories(/opt/vc/include/interface/vmcs_host/linux)
    include_directories(SYSTEM "/usr/include/fontconfig")

elseif(${PLATFORM_TARGET} MATCHES "linux")
    message(STATUS "Building config for Linux")

    # set for test in other cmake files
    set(PLATFORM_LINUX ON)
    add_definitions(-DPLATFORM_LINUX)
    set(CORE_COMPILE_DEFS PLATFORM_LINUX)

    include_directories(SYSTEM "/usr/include/fontconfig")
    
    # compiler options
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fpermissive -g -std=c++1y")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer")
    set(CXX_FLAGS_DEBUG "-g -O0")

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-gnu-zero-variadic-macro-arguments")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lc++ -lc++abi")
    endif()

    if (CMAKE_COMPILER_IS_GNUCC)
        execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
        string(REGEX MATCHALL "[0-9]+" GCC_VERSION_COMPONENTS ${GCC_VERSION})
        list(GET GCC_VERSION_COMPONENTS 0 GCC_MAJOR)
        list(GET GCC_VERSION_COMPONENTS 1 GCC_MINOR)
        message(STATUS "Using gcc ${GCC_VERSION}")
        if (GCC_VERSION VERSION_GREATER 5.1)
            message(STATUS "USE CXX11_ABI")
            add_definitions("-D_GLIBCXX_USE_CXX11_ABI=1")
        endif()
    endif()

elseif(${PLATFORM_TARGET} MATCHES "osx")
    message(STATUS "Building config for Darwin OSX")

    # set for test in other cmake files
    set(PLATFORM_OSX ON)
    add_definitions(-DPLATFORM_OSX)

    # compiler options
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -std=c++1y")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wreturn-type -Wsign-compare")
    set(CXX_FLAGS_DEBUG "-g -O0")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I/usr/local/include")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L/usr/local/lib")

endif()


set(COMMON_SOURCES
    ${PROJECT_SOURCE_DIR}/tangram-es/platforms/common/platform_gl.cpp
    ${PROJECT_SOURCE_DIR}/tangram-es/platforms/common/urlClient.cpp)
include_directories(${PROJECT_SOURCE_DIR}/tangram-es/platforms/common)

# add sources and include headers
set(LINUX_SOURCES
    ${PROJECT_SOURCE_DIR}/tangram-es/platforms/linux/src/platform_linux.cpp)
include_directories(${PROJECT_SOURCE_DIR}/tangram-es/platforms/linux/src)

# add sources and include headers
add_subdirectory(${PROJECT_SOURCE_DIR}/tangram-es/core)
include_directories(${CORE_INCLUDE_DIRS})
include_directories(${CORE_LIBRARIES_INCLUDE_DIRS})

# add executable
add_executable(${EXECUTABLE_NAME} ${SOURCES} ${COMMON_SOURCES} ${LINUX_SOURCES})

# link libraries
target_link_libraries(${EXECUTABLE_NAME} ${CORE_LIBRARY})
target_link_libraries(${EXECUTABLE_NAME} -lcurl)
target_link_libraries(${EXECUTABLE_NAME} prime_server)


if(NOT ${PLATFORM_TARGET} MATCHES "osx")
    target_link_libraries(${EXECUTABLE_NAME} -lfontconfig)
    target_link_libraries(${EXECUTABLE_NAME} -lfreetype)
endif()

if(NOT ${PLATFORM_TARGET} MATCHES "rpi")
    find_package(OpenGL REQUIRED)

    # Build GLFW.
    if (USE_SYSTEM_GLFW_LIBS)
        include(FindPkgConfig)
        pkg_check_modules(GLFW REQUIRED glfw3)
    else()
        # configure GLFW to build only the library
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs")
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs")
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation")
        set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target")
        add_subdirectory(${PROJECT_SOURCE_DIR}/tangram-es/platforms/common/glfw)
    endif()

    target_include_directories(${EXECUTABLE_NAME}
        PUBLIC
        ${GLFW_SOURCE_DIR}/tangram-es/tangr/include
        ${PROJECT_SOURCE_DIR}/tangram-es/platforms/common)

    target_link_libraries(${EXECUTABLE_NAME} glfw)
    target_link_libraries(${EXECUTABLE_NAME} ${GLFW_LIBRARIES})
    target_link_libraries(${EXECUTABLE_NAME} ${OPENGL_LIBRARIES})
endif()

add_resources(${EXECUTABLE_NAME} "${PROJECT_SOURCE_DIR}/scenes")

install(TARGETS ${EXECUTABLE_NAME} RUNTIME DESTINATION bin)
