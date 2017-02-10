
ARCH ?= lin
FLAGS = -fPIC -g -Wall -O3 -msse -mfpmath=sse -ffast-math \
	-I../../include
LDFLAGS =

SOURCES = $(wildcard src/*.cpp)


ifeq ($(ARCH), lin)
LDFLAGS += -shared
TARGET = plugin.so
endif

ifeq ($(ARCH), mac)
LDFLAGS += -shared -undefined dynamic_lookup
TARGET = plugin.dylib
endif

ifeq ($(ARCH), win)
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
