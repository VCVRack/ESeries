#include "ESeries.hpp"


Plugin *pluginInstance;

void init(rack::Plugin *p) {
	pluginInstance = p;
	p->slug = TOSTRING(SLUG);
	p->version = TOSTRING(VERSION);

	p->addModel(modelE340);
}
