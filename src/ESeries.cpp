#include "ESeries.hpp"


Plugin *plugin;

void init(rack::Plugin *p) {
	plugin = p;
	plugin->slug = "ESeries";
	plugin->name = "E-Series";
	createModel<E340Widget>(plugin, "E340", "E340 Cloud Generator");
}
