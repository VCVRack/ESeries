SOURCES += $(wildcard src/*.cpp)
FLAGS += -w

DISTRIBUTABLES += $(wildcard LICENSE*) res

RACK_DIR ?= ../..
include $(RACK_DIR)/plugin.mk
