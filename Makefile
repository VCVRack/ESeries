SLUG = ESeries
VERSION = 0.5.0

SOURCES += $(wildcard src/*.cpp)

DISTRIBUTABLES += $(wildcard LICENSE*) res


include ../../plugin.mk
