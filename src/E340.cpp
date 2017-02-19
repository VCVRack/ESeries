#include "ESeries.hpp"
#include "dsp.hpp"


struct E340 : Module {
	enum ParamIds {
		COARSE_PARAM,
		FINE_PARAM,
		FM_PARAM,
		SPREAD_PARAM,
		CHAOS_PARAM,
		CHAOS_BW_PARAM,
		DENSITY_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		PITCH_INPUT,
		FM_INPUT,
		SYNC_INPUT,
		SPREAD_INPUT,
		CHAOS_INPUT,
		CHAOS_BW_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		SAW_OUTPUT,
		SINE_OUTPUT,
		NUM_OUTPUTS
	};

	float phases[8] = {};
	RCFilter noiseFilters[8];
	float sync = 0.0;

	MinBLEP<16> sineMinBLEP;
	MinBLEP<16> sawMinBLEP;

	// For removing DC
	RCFilter sineFilter;
	RCFilter sawFilter;

	E340();
	void step();
};


E340::E340() {
	params.resize(NUM_PARAMS);
	inputs.resize(NUM_INPUTS);
	outputs.resize(NUM_OUTPUTS);

	sineMinBLEP.minblep = minblep_16_32;
	sineMinBLEP.oversample = 32;
	sawMinBLEP.minblep = minblep_16_32;
	sawMinBLEP.oversample = 32;

	// Randomize initial phases
	for (int i = 0; i < 8; i++) {
		phases[i] = randomf();
	}
}

void E340::step() {
	// Base pitch
	float basePitch = params[COARSE_PARAM] + 12.0 * getf(inputs[PITCH_INPUT]);
	if (inputs[FM_INPUT]) {
		basePitch += quadraticBipolar(params[FM_PARAM]) * 12.0 * *inputs[FM_INPUT];
	}
	basePitch += 3.0 * quadraticBipolar(params[FINE_PARAM]);

	// Spread
	float spread = params[SPREAD_PARAM] + getf(inputs[SPREAD_INPUT]) / 10.0;
	spread = clampf(spread, 0.0, 1.0);
	const float spreadPower = 50.0;
	spread = (powf(spreadPower + 1.0, spread) - 1.0) / spreadPower;

	// Chaos
	float chaos = params[CHAOS_PARAM] + getf(inputs[CHAOS_INPUT]) / 10.0;
	chaos = clampf(chaos, 0.0, 1.0);
	const float chaosPower = 50.0;
	chaos = 8.0 * (powf(chaosPower + 1.0, chaos) - 1.0) / chaosPower;

	// Chaos BW
	float chaosBW = params[CHAOS_BW_PARAM] + getf(inputs[CHAOS_BW_INPUT]) / 10.0;
	chaosBW = clampf(chaosBW, 0.0, 1.0);
	chaosBW = 6.0 * powf(100.0, chaosBW);
	// This shouldn't scale with the global sample rate, because of reasons.
	float filterCutoff = chaosBW / 44100.0;

	// Check sync input
	float newSync = getf(inputs[SYNC_INPUT]) - 0.25;
	float syncCrossing = INFINITY;
	if (sync < 0.0 && newSync >= 0.0) {
		float deltaSync = newSync - sync;
		syncCrossing = -newSync / deltaSync;
	}
	sync = newSync;

	// Density
	int density;
	switch ((int)roundf(params[DENSITY_PARAM])) {
		case 0: density = 2; break;
		case 1: density = 4; break;
		default: density = 8; break;
	}

	// Detuning amounts, in note value
	const static float detunings[8] = {-21, 21, -9, 9, -3, 3, -15, 15}; // Perfect fourths
	// const static float detunings[8] = {-24, 24, -12, 12, -5, 7, -17, 19}; // Fifths

	// Oscillator block
	float sines = 0.0;
	float saws = 0.0;
	for (int i = 0; i < density; i++) {
		// Noise
		float noise = 0.0;
		if (chaos > 0.0) {
			noise = randomNormal();
			noiseFilters[i].setCutoff(filterCutoff);
			noiseFilters[i].process(noise);
			noise = noiseFilters[i].lowpass();
			noise *= chaos;
		}

		// Frequency
		float pitch = basePitch + spread * detunings[i] + 12.0 * noise;
		pitch = clampf(pitch, -72.0, 72.0);
		float freq = 261.626 * powf(2.0, pitch / 12.0);

		// Advance phase
		float deltaPhase = freq / gSampleRate;
		float phase = phases[i] + deltaPhase;

		// Reset phase
		if (phase >= 1.0) {
			phase -= 1.0;
			float crossing = -phase / deltaPhase;
			sawMinBLEP.jump(crossing, -2.0);
		}

		// Compute output
		float sine = -cosf(2*M_PI * phase);
		float saw = 2.0*phase - 1.0;

		// Sync
		if (syncCrossing <= 0.0) {
			phase = deltaPhase * -syncCrossing;
			float newSine = -cosf(2*M_PI * phase);
			float newSaw = 2.0*phase - 1.0;
			sineMinBLEP.jump(syncCrossing, newSine - sine);
			sawMinBLEP.jump(syncCrossing, newSaw - saw);
			sine = newSine;
			saw = newSaw;
		}

		phases[i] = phase;
		sines += sine;
		saws += saw;
	}

	sines += sineMinBLEP.shift();
	saws += sawMinBLEP.shift();

	sines /= density;
	saws /= density;

	// Apply HP filter at 20Hz
	float r = 20.0 / gSampleRate;
	sineFilter.setCutoff(r);
	sawFilter.setCutoff(r);

	sineFilter.process(sines);
	sawFilter.process(saws);

	setf(outputs[SINE_OUTPUT], 5.0 * sineFilter.highpass());
	setf(outputs[SAW_OUTPUT], 5.0 * sawFilter.highpass());
}


E340Widget::E340Widget() {
	E340 *module = new E340();
	setModule(module);
	box.size = Vec(15*14, 380);

	{
		Panel *panel = new LightPanel();
		panel->box.size = box.size;
		panel->backgroundImage = Image::load("plugins/ESeries/res/E340.png");
		addChild(panel);
	}

	addParam(createParam<SynthTechAlco>(Vec(27, 43), module, E340::COARSE_PARAM, -48.0, 48.0, 0.0));
	addParam(createParam<SynthTechAlco>(Vec(138, 43), module, E340::FINE_PARAM, -1.0, 1.0, 0.0));

	addParam(createParam<SynthTechAlco>(Vec(27, 109), module, E340::FM_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<SynthTechAlco>(Vec(138, 109), module, E340::SPREAD_PARAM, 0.0, 1.0, 0.0));

	addParam(createParam<SynthTechAlco>(Vec(27, 175), module, E340::CHAOS_PARAM, 0.0, 1.0, 0.0));
	addParam(createParam<SynthTechAlco>(Vec(138, 175), module, E340::CHAOS_BW_PARAM, 0.0, 1.0, 0.5));

	addParam(createParam<ESeriesSwitch>(Vec(91, 147), module, E340::DENSITY_PARAM, 0.0, 2.0, 2.0));

	addInput(createInput<CL1362Port>(Vec(13, 243), module, E340::PITCH_INPUT));
	addInput(createInput<CL1362Port>(Vec(63, 243), module, E340::FM_INPUT));
	addInput(createInput<CL1362Port>(Vec(113, 243), module, E340::SYNC_INPUT));
	addInput(createInput<CL1362Port>(Vec(163, 243), module, E340::SPREAD_INPUT));

	addInput(createInput<CL1362Port>(Vec(13, 301), module, E340::CHAOS_INPUT));
	addInput(createInput<CL1362Port>(Vec(63, 301), module, E340::CHAOS_BW_INPUT));
	addOutput(createOutput<CL1362Port>(Vec(113, 301), module, E340::SAW_OUTPUT));
	addOutput(createOutput<CL1362Port>(Vec(163, 301), module, E340::SINE_OUTPUT));

	addChild(createScrew<SilverScrew>(Vec(15, 0)));
	addChild(createScrew<SilverScrew>(Vec(box.size.x-30, 0)));
	addChild(createScrew<SilverScrew>(Vec(15, 365)));
	addChild(createScrew<SilverScrew>(Vec(box.size.x-30, 365)));
}
