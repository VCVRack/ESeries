#include "ESeries.hpp"


Plugin *plugin;

void init(rack::Plugin *p) {
	plugin = p;
	p->slug = "ESeries";
#ifdef VERSION
	p->version = TOSTRING(VERSION);
#endif

	p->addModel(createModel<E340Widget>("E-Series", "E340", "E340 Cloud Generator", OSCILLATOR_TAG));
}
