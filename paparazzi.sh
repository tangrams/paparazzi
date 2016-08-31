#!/bin/bash

# If something goes wrong terminate this script
set -e

OS=$(uname)
DIST="UNKNOWN"

# Dependencies
DEPS_COMMON="cmake " 
DEPS_LINUX_COMMON="libcurl4-openssl-dev uuid-dev libtool pkg-config build-essential autoconf automake lcov libzmq3-dev"
DEPS_LINUX_RASPBIAN="curl "
DEPS_LINUX_UBUNTU="xorg-dev libgl1-mesa-dev "
DEPS_LINUX_REDHAT="libX*-devel mesa-libGL-devel curl-devel glx-utils git libmpc-devel mpfr-devel gmp-devel"
DEPS_DARWIN="glfw3 pkg-config zeromq"

# Compiling
CMAKE_ARG=""
N_CORES=1

# Installing
INSTALL_FOLDER="/usr/local/bin/"

# Running
PORT=8080
N_THREAD=1
# what linux distribution is?
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DIST=$NAME
fi

case "$1" in
    install)
        # INSTALL DEPENDECIES
        if [ $OS == "Linux" ]; then
            echo "Install dependeces for Linux - $DIST"

            if [ "$DIST" == "Amazon Linux AMI" ]; then

                # Amazon Linux
                sudo yum update
                sudo yum upgrade
                sudo yum groupinstall "Development Tools"
                sudo yum install $DEPS_LINUX_REDHAT -y
                sudo yum remove zmq
                export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig 

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
                    make -j $N_CORES
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

                    # Run X in the back with out screen
                    echo '/usr/bin/X :0 &' | sudo tee -a /etc/rc.d/rc.local
                    echo 'export DISPLAY=:0' >> ~/.bashrc
                    if [ ! -e ~/.zshrc ]; then
                        echo 'export DISPLAY=:0' >> ~/.zshrc
                    fi
                fi

            elif [ "$DIST" == "Ubuntu" ]; then

                 # UBUNTU
                sudo apt-get update
                sudo apt-get upgrade
                sudo apt-get install $DEPS_COMMON $DEPS_LINUX_COMMON $DEPS_LINUX_UBUNTU

            elif [ "$DIST" == "Raspbian GNU/Linux" ]; then

                # Raspian
                sudo apt-get update
                sudo apt-get upgrade
                sudo apt-get install $DEPS_COMMON $DEPS_LINUX_COMMON $DEPS_LINUX_RASPBIAN

            fi

            # Install ZeroMQ 
            if [ ! -e /usr/local/lib/libzmq.so ]; then
                git clone https://github.com/zeromq/libzmq
                cd libzmq
                ./autogen.sh && ./configure && make -j $N_CORES
                make check && sudo make install && sudo ldconfig
                cd ..
                rm -rf libzmq
            fi

        elif [ $OS == "Darwin" ]; then
            echo "Install dependeces for Darwin"

            # DARWIN
            if [ ! -e /usr/local/bin/brew ]; then
                ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
            fi

            brew update
            brew upgrade
            brew install $DEPS_COMMON $DEPS_DARWIN
        fi

        # Install  the super awesome PRIME_SERVER by Kevin Kreiser ( https://mapzen.com/blog/zmq-http-server )
        if [ ! -e /usr/local/bin/prime_httpd ]; then
            echo "installing prime_server"
            git clone https://github.com/kevinkreiser/prime_server.git
            cd prime_server
            git submodule update --init --recursive
            ./autogen.sh
            ./configure
            make test -j $N_CORES
            sudo make install
            cd ..
            rm -rf prime_server
        fi

        # GET SUBMODULES
        echo "Installing submodules"
        git submodule update --init --recursive

        # Other needed files/folders
        if [ ! -e fonts ]; then
            echo "Installing fonts"
            ln -s tangram-es/scenes/fonts .
        fi

        $0 make
        ;;

    make)
        if [ $OS == "Linux" ]; then
            echo "Preparing CMAKE for Linux - $DIST"
            N_CORES=$(grep -c ^processor /proc/cpuinfo)

            if [ "$DIST" == "Amazon Linux AMI" ]; then
                export CXX=/usr/local/bin/g++
                export CC=/usr/local/bin/gcc
                CMAKE_ARG="-DPLATFORM_TARGET=linux"

            elif [ "$DIST" == "Ubuntu" ]; then
                CMAKE_ARG="-DPLATFORM_TARGET=linux"

            elif [ "$DIST" == "Raspbian GNU/Linux" ]; then
                export CXX=/usr/bin/g++-4.9
                CMAKE_ARG="-DPLATFORM_TARGET=rpi"
            fi

        elif [ $OS == "Darwin" ]; then
            echo "Preparing CMAKE for Darwin"
            N_CORES="4"
            CMAKE_ARG="-DPLATFORM_TARGET=osx"
        fi

        if [ "$2" == "xcode" ]; then
            echo "Making XCode project"
            mkdir build
            cd build
            cmake .. -GXcode -DPLATFORM_TARGET=osx
        else
            echo "Compiling"
            cmake . -Bbuild $CMAKE_ARG
            cd build
            make -j $N_CORES
        fi

        echo "Installing"
        sudo cp bin/paparazzi_thread /usr/local/bin/paparazzi_thread
        cd ..
        ;;

    clean)
        rm -rf build
        ;;

    start)
        # start http server
        prime_httpd tcp://*:$PORT ipc:///tmp/proxy_in ipc:///tmp/loopback &

        # start proxy
        prime_proxyd ipc:///tmp/proxy_in ipc:///tmp/proxy_out &
        
        # Is important to attach the threads to the display on the Amazon servers 
        if [ "$DIST" == "Amazon Linux AMI" ]; then
            export DISPLAY=:0
        fi

        # run paparazzi threads
        # paparazzi_thread ipc:///tmp/proxy_out ipc:///tmp/loopback

        $0 add $2
        ;;
    add)
        if [ $# -eq 2 ]; then
            N_THREAD=$2
        fi

        echo "Adding $N_THREAD paparazzi threads" 
        for i in $(eval echo "{$0..$N_THREAD}"); do 
            paparazzi_thread ipc:///tmp/proxy_out ipc:///tmp/loopback &
        done 
        ;;

    stop)
        killall prime_httpd
        killall prime_proxyd 
        killall paparazzi_thread
        ;;

    restart)
        $0 stop
        $0 start $2
        ;;

    status)
        ps -ef | grep -v grep | grep prime_httpd
        ps -ef | grep -v grep | grep prime_proxyd
        ps -ef | grep -v grep | grep paparazzi_thread
        ;;

    *)
        if [ ! -e /usr/local/bin/paparazzi_thread ]; then
            echo "Usage: $0 install"
        else
            echo "Usage: $0 start"
        fi
        exit 1
        ;;
esac





