#!/bin/bash

os=$(uname)
arq=$(uname -m)

deps_common="cmake "
deps_linux_rpi="curl "
deps_linux_common="libcurl4-openssl-dev nodejs "
deps_linux_ubuntu="xorg-dev libgl1-mesa-dev npm "
deps_linux_amazon="libX*-devel mesa-libGL-devel curl-devel glx-utils git "
cmake_arg=""

if [ $os == "Linux" ]; then

    if [ $arq == "x86_64" ]; then
        # Amazon Linux
        sudo yum groupinstall "Development Tools"
        sudo yum install $deps_linux_amazon -y

        # Run X in the back with out screen
        echo '/usr/bin/X :0 &' | sudo tee -a /etc/rc.d/rc.local
        echo 'export DISPLAY=:0' >> .bashrc

        # Install Cmake 3.6
        wget https://cmake.org/files/v3.6/cmake-3.6.0-rc3-Linux-x86_64.sh
        chmod +x cmake-3.6.0-rc3-Linux-x86_64.sh
        mv cmake-3.6.0-rc3-Linux-x86_64.sh /usr/local/
        /usr/local/cmake-3.6.0-rc3-Linux-x86_64.sh

        # Install GCC 4.9
        sudo yum install libmpc-devel mpfr-devel gmp-devel
        cd ..
        curl ftp://ftp.mirrorservice.org/sites/sourceware.org/pub/gcc/releases/gcc-4.9.2/gcc-4.9.2.tar.bz2 -O
        tar xvfj gcc-4.9.2.tar.bz2
        cd gcc-4.9.2
        ./contrib/download_prerequisites
        ./configure --disable-multilib --enable-languages=c,c++
        make -j 8
        make install
        cd ../paparazzi

        # Install Node
        curl -o- https://raw.githubusercontent.com/creationix/nvm/v0.31.0/install.sh | bash
        #nvm install node

    else 
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
            sudo ln -s /usr/bin/nodejs /usr/local/bin/node
        fi
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
