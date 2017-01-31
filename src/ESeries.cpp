#include "ESeries.hpp"
#include "dsp.hpp"


Plugin *init() {
	Plugin *plugin = createPlugin("ESeries", "E-Series");
	createModel<E340Widget>(plugin, "E340", "E340 Cloud Generator");
	return plugin;
}
