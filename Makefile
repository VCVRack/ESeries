
SOURCES = $(wildcard src/*.cpp)

include ../../plugin.mk


dist: all
	mkdir -p dist/ESeries
	cp LICENSE* dist/ESeries/
	cp $(TARGET) dist/ESeries/
	cp -R res dist/ESeries/
	cd dist && zip -5 -r ESeries-$(VERSION)-$(ARCH).zip ESeries
