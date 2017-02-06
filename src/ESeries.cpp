#include "ESeries.hpp"
#include "dsp.hpp"


struct ESeriesPlugin : Plugin {
	ESeriesPlugin() {
		slug = "ESeries";
		name = "E-Series";
		createModel<E340Widget>(this, "E340", "E340 Cloud Generator");
	}
};


Plugin *init() {
	return new ESeriesPlugin();
}
