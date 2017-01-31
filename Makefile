
ARCH ?= linux
CXXFLAGS = -MMD -fPIC -g -Wall -std=c++11 -O3 -msse -mfpmath=sse -ffast-math \
	-I../../include
LDFLAGS =

SOURCES = $(wildcard src/*.cpp)


# Linux
ifeq ($(ARCH), linux)
CC = gcc
CXX = g++
LDFLAGS += -shared
TARGET = plugin.so
endif

# Apple
ifeq ($(ARCH), apple)
CC = clang
CXX = clang++
CXXFLAGS += -stdlib=libc++
LDFLAGS += -stdlib=libc++ -shared -undefined dynamic_lookup
TARGET = plugin.dylib
endif

# Windows
ifeq ($(ARCH), windows)
CC = x86_64-w64-mingw32-gcc
CXX = x86_64-w64-mingw32-g++
CXXFLAGS += -D_USE_MATH_DEFINES
SOURCES +=
LDFLAGS += -shared -L../../ -lRack
TARGET = plugin.dll
endif

all: $(TARGET)

dist: $(TARGET)
	mkdir -p dist/ESeries
	cp LICENSE* dist/ESeries/
	cp plugin.* dist/ESeries/
	cp -R res dist/ESeries/

clean:
	rm -rfv build $(TARGET)

include ../../Makefile.inc
