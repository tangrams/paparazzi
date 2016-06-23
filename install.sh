#!/bin/bash

os=$(uname)
arq=$(uname -m)

deps_common="cmake "
deps_linux_rpi="curl "
deps_linux_common="libcurl4-openssl-dev nodejs "
deps_linux_ubuntu="xorg-dev libgl1-mesa-dev npm "
cmake_arg=""

if [ $os == "Linux" ]; then
    # on Debian Linux distributions
    sudo apt-get update
    sudo apt-get upgrade
    sudo apt-get install $deps_common
    sudo apt-get install $deps_linux_common

    # on RaspberryPi
    if [ $arq == "armv7l" ]; then
        sudo apt-get install $deps_linux_rpi
        curl -sL https://deb.nodesource.com/setup_6.x | sudo -E bash -
        sudo apt-get install nodejs -y
        export CXX=/usr/bin/g++-4.9
        cmake_arg="-DPLATFORM_TARGET=rpi"
    else
        sudo apt-get install $deps_linux_ubuntu
        cmake_arg="-DPLATFORM_TARGET=linux"
    fi
elif [ $os == "Darwin" ]; then
    
    # ON MacOX 
    if [ ! -e /usr/local/bin/brew ]; then
        ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
    fi

    brew update
    brew upgrade
    brew install $deps_common
fi

git submodule update --init --recursive
cmake . -Bbuild $cmake_arg
cmake --build build 
