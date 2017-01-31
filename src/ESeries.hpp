#include "rack.hpp"


using namespace rack;

////////////////////
// helpers
////////////////////

struct ESeriesPanel : ModulePanel {
	ESeriesPanel() {
		backgroundColor = nvgRGB(0xa0, 0xa0, 0xa0);
		highlightColor = nvgRGB(0xd0, 0xd0, 0xd0);
	}
};

struct ESeriesKnob : Knob {
	ESeriesKnob() {
		minIndex = 44+5;
		maxIndex = -46-5;
		spriteCount = 120;
		box.size = Vec(48, 48);
		spriteOffset = Vec(-4, -3);
		spriteSize = Vec(64, 64);
		spriteFilename = "plugins/ESeries/res/knob_medium.png";
	}
};

struct ESeriesSwitch : Knob {
	ESeriesSwitch() {
		minIndex = 1;
		maxIndex = 0;
		spriteCount = 2;
		box.size = Vec(27, 27);
		spriteOffset = Vec(-15, -15);
		spriteSize = Vec(56, 56);
		spriteFilename = "plugins/ESeries/res/switch.png";
	}
};

////////////////////
// module widgets
////////////////////

struct E340Widget : ModuleWidget {
	E340Widget();
};
