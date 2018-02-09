#include "ESeries.hpp"


Plugin *plugin;

void init(rack::Plugin *p) {
	plugin = p;
	p->slug = TOSTRING(SLUG);
	p->version = TOSTRING(VERSION);

	p->addModel(createModel<E340Widget>("E-Series", "E340", "E340 Cloud Generator", OSCILLATOR_TAG));
}
