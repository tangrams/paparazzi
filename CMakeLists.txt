cmake_minimum_required(VERSION 2.8)
project(tangram)

# helper functions
include(${PROJECT_SOURCE_DIR}/tangram-es/toolchains/utils.cmake)

# cmake output configuration
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(NOT EXISTS "${PROJECT_SOURCE_DIR}/tangram-es/external/alfons/deps/harfbuzz-icu-freetype/CMakeLists.txt")
    message(SEND_ERROR "Missing submodules - Please run:\n 'git submodule update --init --recursive'")
    return()
endif()

# common compiler options
set(EXECUTABLE_NAME "paparazzi")

# add sources and include headers
find_sources_and_include_directories(
    ${PROJECT_SOURCE_DIR}/src/*.h 
    ${PROJECT_SOURCE_DIR}/src/*.cpp)

if(${PLATFORM_TARGET} MATCHES "rpi")

    #
    #   RASPBERRY PI
    #
    #############################################################################
    message(STATUS "Building config for RaspberryPI")

    # compiler options
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fpermissive -g")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_CXX_FLAGS} -L/opt/vc/lib/ -lGLESv2 -lEGL -lbcm_host -lvchiq_arm -lvcos -lrt -lpthread")
    set(CXX_FLAGS_DEBUG "-g -O0")
    
    add_definitions(-DPLATFORM_RPI)

    # check for c++11 compiler
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)

    if("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
        if(NOT (GCC_VERSION VERSION_GREATER 4.8 OR GCC_VERSION VERSION_EQUAL 4.8))
            message(FATAL_ERROR "Please install g++ version 4.8 or greater")
        else()
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
        endif()
    else()
        message(FATAL_ERROR "Please install a C++14 compatible compiler")
    endif()

    # add sources and include headers
    find_sources_and_include_directories(
        ${PROJECT_SOURCE_DIR}/tangram-es/linux/src/urlWorker.h
        ${PROJECT_SOURCE_DIR}/tangram-es/linux/src/urlWorker.cpp)

    # include headers for rpi-installed libraries
    include_directories(/opt/vc/include/)
    include_directories(/opt/vc/include/interface/vcos/pthreads)
    include_directories(/opt/vc/include/interface/vmcs_host/linux)

    # load core library
    add_subdirectory(${PROJECT_SOURCE_DIR}/tangram-es/external)
    add_subdirectory(${PROJECT_SOURCE_DIR}/tangram-es/core)
    include_directories(${CORE_INCLUDE_DIRS})
    include_directories(${CORE_LIBRARIES_INCLUDE_DIRS})

    # add executable
    add_executable(${EXECUTABLE_NAME} ${SOURCES})

    # copy resources
    add_resources(${EXECUTABLE_NAME} "${PROJECT_SOURCE_DIR}/tangram-es/scenes")

    # link libraries
    target_link_libraries(${EXECUTABLE_NAME} -lcurl)
    target_link_libraries(${EXECUTABLE_NAME} ${CORE_LIBRARY})

elseif(${PLATFORM_TARGET} MATCHES "linux")

    #
    #   LINUX
    #
    #############################################################################
    message(STATUS "Building config for Linux")

    find_package(OpenGL REQUIRED)

    # set for test in other cmake files
    set(PLATFORM_LINUX ON)
    cmake_minimum_required(VERSION 2.8)
    project(tangram)

    # helper functions
    include(${PROJECT_SOURCE_DIR}/tangram-es/toolchains/utils.cmake)

    # cmake output configuration
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

    # compiler options
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fpermissive -g -std=c++1y")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer")
    set(CXX_FLAGS_DEBUG "-g -O0")

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-gnu-zero-variadic-macro-arguments")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}  -lc++ -lc++abi")
    endif()

elseif(${PLATFORM_TARGET} MATCHES "osx")

    #
    #   DARWIN
    #
    #############################################################################
    message(STATUS "Building config for Darwin OSX")

    find_package(OpenGL REQUIRED)

    # set for test in other cmake files
    set(PLATFORM_OSX ON)
    cmake_minimum_required(VERSION 2.8)
    project(tangram)

    # helper functions
    include(${PROJECT_SOURCE_DIR}/tangram-es/toolchains/utils.cmake)

    # cmake output configuration
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

    # compiler options
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -std=c++1y")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wreturn-type -Wsign-compare")
    set(CXX_FLAGS_DEBUG "-g -O0")

    add_executable(${EXECUTABLE_NAME} MACOSX_BUNDLE ${SOURCES} ${RESOURCES} ${OSX_RESOURCES})

    target_link_libraries(${EXECUTABLE_NAME} ${CORE_LIBRARY} glfw ${OPENGL_LIBRARIES})

endif()

add_resources(${EXECUTABLE_NAME} "${PROJECT_SOURCE_DIR}/scenes")
