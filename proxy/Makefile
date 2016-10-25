EXE = ./paparazzi_proxy

CXX = g++
SOURCES := $(wildcard src/*.cpp)
HEADERS := $(wildcard src/*.h)
OBJECTS := $(SOURCES:.cpp=.o)

PLATFORM = $(shell uname)
ifneq ("$(wildcard /etc/os-release)","")
PLATFORM = $(shell . /etc/os-release && echo $$NAME)
endif

$(info Platform ${PLATFORM}) 

INCLUDES +=	-Isrc/ -I/usr/local/include/prime_server/
CFLAGS += -Wall -O3 -std=c++11 -fpermissive $(shell pkg-config --cflags libprime_server)
LDFLAGS += $(shell pkg-config --libs libprime_server)

ifeq ($(PLATFORM),Raspbian GNU/Linux)
CFLAGS += -DGLM_FORCE_CXX98 -DPLATFORM_RPI -fno-strict-aliasing 
else ifeq ($(shell uname),Linux)
CFLAGS += -DPLATFORM_LINUX
else ifeq ($(PLATFORM),Darwin)
CXX = /usr/bin/clang++
ARCH = -arch x86_64
CFLAGS += $(ARCH) -DPLATFORM_OSX -stdlib=libc++
endif

all: $(EXE)

%.o: %.cpp
	@echo $@
	$(CXX) $(CFLAGS) $(INCLUDES) -g -c $< -o $@ -Wno-deprecated-declarations

$(EXE): $(OBJECTS) $(HEADERS)
	$(CXX) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -rdynamic -o $@

clean:
	@rm -rvf $(EXE) src/*.o

install:
	@cp $(EXE) /usr/local/bin

uninstall:
	@rm /usr/local/bin/$(EXE)
