#!/bin/bash

# If something goes wrong terminate this script
# set -e

OS=$(uname)
DIST="UNKNOWN"

# Dependencies
DEPS_COMMON="cmake " 
DEPS_LINUX_COMMON="libcurl4-openssl-dev uuid-dev libtool pkg-config build-essential autoconf automake lcov libzmq3-dev"
DEPS_LINUX_RASPBIAN="curl libfontconfig1-dev"
DEPS_LINUX_UBUNTU="xorg-dev libgl1-mesa-dev "
DEPS_LINUX_REDHAT="libX*-devel mesa-libGL-devel curl-devel glx-utils git libmpc-devel mpfr-devel gmp-devel"
DEPS_DARWIN="glfw3 pkg-config zeromq autoconfig automake libtool"

# Compiling
CMAKE_ARG=""
N_CORES=1

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

                # Install FreeType 2.6.5
                wget http://public.p-knowledge.co.jp/Savannah-nongnu-mirror//freetype/freetype-2.6.5.tar.gz
                tar xzvf freetype-2.6.5.tar.gz
                cd freetype-2.6.5
                ./configure --prefix=/usr
                make -j $N_CORES
                sudo make install
                # Do some linking
                sudo rm /usr/lib64/libfreetype.so
                sudo rm /usr/lib64/libfreetype.so.6
                sudo ln -s /usr/lib/libfreetype.so /usr/lib64/
                sudo ln -s /usr/lib/libfreetype.so.6 /usr/lib64/
                sudo ln -s /usr/lib/libfreetype.so.6.12.5 /usr/lib64/
                cd ..

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
            git clone --quiet --branch 0.4.0 --depth 1 --recursive  https://github.com/kevinkreiser/prime_server.git
            export PKG_CONFIG_PATH=PKG_CONFIG_PATH:/usr/local/lib/pkgconfig/
            cd prime_server
            #git submodule update --init --recursive
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
        if [ ! -d fonts ]; then
            echo "Installing fonts"
            cp tangram-es/scenes/fonts fonts
        fi

        # Other needed files/folders
        if [ ! -d cache ]; then
            echo "Creating cache folder"
            mkdir cache
        fi

        $0 make all
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

        if [ "$2" == "all" ]; then
            $0 make proxy
            $0 make worker
        elif [ "$2" == "proxy" ]; then
            echo "Compiling proxy"
            cd proxy
            make
            sudo make install
            cd ..
        else
            cd worker
            if [ "$2" == "xcode" ]; then
                echo "Making XCode project"
                mkdir build
                cd build
                cmake .. -GXcode -DPLATFORM_TARGET=osx
            else
                echo "Compiling worker"
                cmake . -Bbuild $CMAKE_ARG
                cd build
                make -j $N_CORES
            fi

            echo "Installing"
            ../../$0 stop
            sudo make install
            cd ../..
        fi        
        ;;

    clean)
        if [ -d worker/build ]; then
            rm -rf worker/build
        fi  
        if [ ! -e proxy/paparazzi_proxy ]; then
            cd proxy
            make clean
            cd ..
        fi
        rm *.log     
        ;;

    remake)
        $0 clean
        $0 make $2
        ;;

    start)
        rm *.log

        # start http server
        prime_httpd tcp://*:$PORT ipc:///tmp/proxy_in ipc:///tmp/loopback &> httpd.log &

        # start proxy
        paparazzi_proxy ipc:///tmp/proxy_in ipc:///tmp/proxy_out &> proxy.log &
        
        # Is important to attach the threads to the display on the Amazon servers 
        if [ "$DIST" == "Amazon Linux AMI" ]; then
            export DISPLAY=:0
        fi

        # run paparazzi threads
        paparazzi_worker ipc:///tmp/proxy_out ipc:///tmp/loopback

        #$0 add $2
        ;;
    add)
        if [ $# -eq 2 ]; then
            N_THREAD=$2
        fi

        echo "Adding $N_THREAD paparazzi threads" 
        for i in $(eval echo "{1..$N_THREAD}"); do 
            # paparazzi_worker ipc:///tmp/proxy_out ipc:///tmp/loopback &> worker_$$.log &
            ./worker.sh &
        done 
        ;;

    stop)
        killall prime_httpd
        killall paparazzi_proxy 
        killall paparazzi_worker
        ;;

    restart)
        $0 stop
        $0 start $2
        ;;

    status)

        if [ $# -eq 2 ]; then
            tail -f worker_$2.log
        else
            ps -ef | grep -v grep | grep prime_httpd
            ps -ef | grep -v grep | grep paparazzi_proxy
            ps -ef | grep -v grep | grep paparazzi_worker
        fi
        ;;

    *)
        if [ ! -e /usr/local/bin/paparazzi_worker ]; then
            echo "Usage: $0 install"
        else
            echo "Usage: $0 start"
        fi
        exit 1
        ;;
esac





