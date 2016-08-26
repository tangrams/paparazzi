#!/bin/bash

OS=$(uname)
DIST="UNKNOWN"
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DIST=$NAME
fi

cmake_arg=""
n_cores="1"

if [ $OS == "Linux" ]; then
    echo "Install dependeces for Linux - $DIST"
    n_cores=$(grep -c ^processor /proc/cpuinfo)

    if [ "$DIST" == "Amazon Linux AMI" ]; then
        export CXX=/usr/local/bin/g++
        export CC=/usr/local/bin/gcc
        export DISPLAY=:0
        cmake_arg="-DPLATFORM_TARGET=linux"

    elif [ "$DIST" == "Ubuntu" ]; then
        cmake_arg="-DPLATFORM_TARGET=linux"

    elif [ "$DIST" == "Raspbian GNU/Linux" ]; then
        export CXX=/usr/bin/g++-4.9
        cmake_arg="-DPLATFORM_TARGET=rpi"
    fi

elif [ $OS == "Darwin" ]; then
    n_cores="4"
    cmake_arg="-DPLATFORM_TARGET=osx"
fi

git submodule update --init --recursive

if [ $1 == "xcode" ]; then
    mkdir build
    cd build
    cmake .. -GXcode -DPLATFORM_TARGET=osx
else
    cmake . -Bbuild $cmake_arg
    cd build
    make -j $n_cores
fi

cd ..