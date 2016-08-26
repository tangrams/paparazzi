#!/bin/bash

OS=$(uname)
DIST="UNKNOWN"
PORT=8000

deps_common="cmake " 
deps_linux_devian="libcurl4-openssl-dev uuid-dev libtool pkg-config build-essential autoconf automake lcov libzmq3-dev"
deps_linux_rpi="curl "
deps_linux_ubuntu="xorg-dev libgl1-mesa-dev "
deps_linux_amazon="libX*-devel mesa-libGL-devel curl-devel glx-utils git libmpc-devel mpfr-devel gmp-devel"
deps_darwin="glfw3 pkg-config zeromq"

cmake_arg=""
n_cores="1"

if [ -f /etc/os-release ]; then
    . /etc/os-release
    DIST=$NAME
fi

if [ "$DIST" == "Amazon Linux AMI" ]; then
    export DISPLAY=:0
fi

case "$1" in
    install)
        # INSTALL DEPENDECIES
        if [ $OS == "Linux" ]; then
            echo "Install dependeces for Linux - $DIST"
            n_cores=$(grep -c ^processor /proc/cpuinfo)

            if [ "$DIST" == "Amazon Linux AMI" ]; then

                # Amazon Linux
                sudo yum groupinstall "Development Tools"
                sudo yum install $deps_linux_amazon -y

                if [ ! -e /usr/local/bin/cmake ]; then
                    # Install Cmake 3.6
                    wget https://cmake.org/files/v3.6/cmake-3.6.0-rc3-Linux-x86_64.sh
                    chmod +x cmake-3.6.0-rc3-Linux-x86_64.sh
                    sudo mv cmake-3.6.0-rc3-Linux-x86_64.sh /usr/local/
                    cd /usr/local/
                    sudo ./cmake-3.6.0-rc3-Linux-x86_64.sh
                    rm cmake-3.6.0-rc3-Linux-x86_64.sh
                    cd ~/paparazzi
                fi

                # Install GCC 4.9
                if [ ! -e /usr/local/bin/gcc ]; then
                    cd ..
                    curl ftp://ftp.mirrorservice.org/sites/sourceware.org/pub/gcc/releases/gcc-4.9.2/gcc-4.9.2.tar.bz2 -O
                    tar xvfj gcc-4.9.2.tar.bz2
                    cd gcc-4.9.2
                    ./contrib/download_prerequisites
                    ./configure --disable-multilib --enable-languages=c,c++
                    make -j $n_cores
                    make install
                    cd ..
                    rm -rf gcc-4.9.2*
                    cd ~/paparazzi
                    echo 'export CXX=/usr/local/bin/g++' >> ~/.bashrc
                    echo 'export CC=/usr/local/bin/gcc' >> ~/.bashrc

                    if [ ! -e ~/.zshrc ]; then
                        echo 'export CXX=/usr/local/bin/g++' >> ~/.zshrc
                        echo 'export CC=/usr/local/bin/gcc' >> ~/.zshrc
                    fi
                fi
                export CXX=/usr/local/bin/g++
                export CC=/usr/local/bin/gcc
                export DISPLAY=:0
                cmake_arg="-DPLATFORM_TARGET=linux"

            elif [ "$DIST" == "Ubuntu" ]; then

                 # UBUNTU
                sudo apt-get update
                sudo apt-get upgrade
                sudo apt-get install $deps_common $deps_linux_devian $deps_linux_ubuntu
                cmake_arg="-DPLATFORM_TARGET=linux"

            elif [ "$DIST" == "Raspbian GNU/Linux" ]; then

                # Raspian
                sudo apt-get update
                sudo apt-get upgrade
                sudo apt-get install $deps_common $deps_linux_devian $deps_linux_rpi
                export CXX=/usr/bin/g++-4.9
                cmake_arg="-DPLATFORM_TARGET=rpi"
            fi

            # Install ZeroMQ 
            if [ ! -e /usr/local/lib/libzmq.so ]; then
                git clone https://github.com/zeromq/libzmq
                cd libzmq
                ./autogen.sh && ./configure && make -j $n_cores
                make check && sudo make install && sudo ldconfig
                cd ..
            fi

        elif [ $OS == "Darwin" ]; then
            n_cores="4"
            cmake_arg="-DPLATFORM_TARGET=osx"
            
            # DARWIN
            if [ ! -e /usr/local/bin/brew ]; then
                ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
            fi

            brew update
            brew upgrade
            brew install $deps_common $deps_darwin
        fi

        # Install  the super awesome PRIME_SERVER by Kevin Kreiser ( https://mapzen.com/blog/zmq-http-server )
        if [ ! -e /usr/local/bin/prime_httpd ]; then
            echo "installing prime_server"
            git clone https://github.com/kevinkreiser/prime_server.git
            cd prime_server
            git submodule update --init --recursive
            ./autogen.sh
            ./configure
            make test -j $n_cores
            sudo make install
            cd ..
            # rm -rf prime_server
        fi

        # GET SUBMODULES
        git submodule update --init --recursive

        # Other needed files/folders
        if [ ! -e fonts ]; then
            ln -s tangram-es/scenes/fonts .
        fi

        # Compile
        cmake . -Bbuild $cmake_arg
        cd build
        make -j $n_cores

        sudo cp bin/paparazzi_thread /usr/local/bin/paparazzi_thread
        cd ..
        ;;
    make)
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

        if [ $2 == "xcode" ]; then
            mkdir build
            cd build
            cmake .. -GXcode -DPLATFORM_TARGET=osx
        else
            cmake . -Bbuild $cmake_arg
            cd build
            make -j $n_cores
        fi

        cd ..
        ;;
    clean)
        rm -rf build
        ;;
    start)
        #start http server
        prime_httpd tcp://*:$PORT ipc:///tmp/proxy_in ipc:///tmp/loopback &

        #start proxy
        prime_proxyd ipc:///tmp/proxy_in ipc:///tmp/proxy_out &

        paparazzi_thread ipc:///tmp/proxy_out ipc:///tmp/loopback
        
        # #start 3 tangram workers
        # for i in {0..2}; do
        #     paparazzi_thread ipc:///tmp/proxy_out ipc:///tmp/loopback &
        # done
        ;;
    stop)
        killall prime_httpd
        killall prime_proxyd 
        killall paparazzi_thread
        ;;
    status)
        ps -ef | grep prime_httpd
        ps -ef | grep prime_proxyd
        ps -ef | grep paparazzi_thread
        ;;
    *)
        echo "Usage: $0 start"
        exit 1
        ;;
esac





