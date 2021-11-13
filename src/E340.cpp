#include "plugin.hpp"


template <typename T>
T sin2pi_pade_05_5_4(T x) {
	x -= 0.5f;
	return (T(-6.283185307) * x + T(33.19863968) * simd::pow(x, 3) - T(32.44191367) * simd::pow(x, 5))
		/ (1 + T(1.296008659) * simd::pow(x, 2) + T(0.7028072946) * simd::pow(x, 4));
}


/** Most of this class copied from Fundamental VCO */
template <int MAX_CHANNELS>
struct E340Oscillator {
	using T = simd::float_4;
	static constexpr int N = MAX_CHANNELS / T::size;

	// Settings
	T spreadTunings[N] = {};
	bool sinEnabled = false;
	bool sawEnabled = false;
	bool syncEnabled = false;
	int channels = 1;

	// Parameters
	float pitch;
	float spread;
	float chaos;
	float chaosBandwidth;

	// State
	T phases[N] = {};
	dsp::MinBlepGenerator<16, 32, T> sinMinBleps[N];
	dsp::MinBlepGenerator<16, 32, T> sawMinBleps[N];
	float lastSyncValue = 0.f;
	dsp::RCFilter sinFilter;
	dsp::RCFilter sawFilter;
	dsp::RCFilter noiseFilters[MAX_CHANNELS];

	// Output
	float sinOutput;
	float sawOutput;

	E340Oscillator() {
		// Randomize initial phases
		for (int i = 0; i < MAX_CHANNELS; i++) {
			phases[i / T::size][i % T::size] = random::uniform();
		}
		setSpreadTuning(0);
	}

	void setSpreadTuning(int mode) {
		if (mode == 0) {
			// Fourths
			spreadTunings[0] = T(-21, 21, -9, 9) / 12;
			spreadTunings[1] = T(-3, 3, -15, 15) / 12;
		}
		else if (mode == 1) {
			// Fifths
			spreadTunings[0] = T(-24, 24, -12, 12) / 12;
			spreadTunings[1] = T(-5, 7, -17, 19) / 12;
		}
	}

	void process(float deltaTime, float syncValue) {
		if (!sinEnabled && !sawEnabled)
			return;

		T deltaPhases[N];
		float noiseCutoff = chaosBandwidth * deltaTime;

		for (int c = 0; c < channels; c += T::size) {
			int i = c / T::size;

			// Spread
			T spreadPitch = pitch;
			spreadPitch += spreadTunings[i] * spread;

			// Chaos
			if (chaos > 0.f) {
				T noise = 0.f;
				for (int j = 0; c + j < channels; j++) {
					noise[j] = 2.f * random::uniform() - 1.f;
					noiseFilters[c + j].setCutoffFreq(noiseCutoff);
					noiseFilters[c + j].process(noise[j]);
					noise[j] = noiseFilters[c + j].lowpass();
				}
				spreadPitch += noise * chaos;
			}

			// Frequency
			T freq = dsp::FREQ_C4 * simd::pow(2.f, spreadPitch);

			// Advance phase
			deltaPhases[i] = simd::clamp(freq * deltaTime, 1e-6f, 0.35f);
			phases[i] += deltaPhases[i];
			phases[i] -= simd::floor(phases[i]);

			if (sawEnabled) {
				// Jump saw when crossing 0.5
				T halfCrossing = (0.5f - (phases[i] - deltaPhases[i])) / deltaPhases[i];
				int halfMask = simd::movemask((0 < halfCrossing) & (halfCrossing <= 1.f));
				if (halfMask) {
					for (int j = 0; c + j < channels; j++) {
						if (halfMask & (1 << j)) {
							T mask = simd::movemaskInverse<T>(1 << j);
							float p = halfCrossing[j] - 1.f;
							T x = mask & (-2.f);
							sawMinBleps[i].insertDiscontinuity(p, x);
						}
					}
				}
			}
		}

		// Detect sync
		// Might be NAN or outside of [0, 1) range
		if (syncEnabled) {
			float deltaSync = syncValue - lastSyncValue;
			float syncCrossing = -lastSyncValue / deltaSync;
			lastSyncValue = syncValue;
			if ((0.f < syncCrossing) && (syncCrossing <= 1.f) && (syncValue >= 0.f)) {
				for (int c = 0; c < channels; c += T::size) {
					int i = c / T::size;
					T newPhase = syncCrossing * deltaPhases[i];
					// Insert minBLEP for sync
					float p = syncCrossing - 1.f;
					if (sawEnabled) {
						T x = saw(newPhase) - saw(phases[i]);
						sawMinBleps[i].insertDiscontinuity(p, x);
					}
					if (sinEnabled) {
						T x = sin(newPhase) - sin(phases[i]);
						sinMinBleps[i].insertDiscontinuity(p, x);
					}
					phases[i] = newPhase;
				}
			}
		}

		// Output
		float f = 20.f * deltaTime;

		if (sinEnabled) {
			float sinTotal = 0.f;
			for (int c = 0; c < channels; c += T::size) {
				int i = c / T::size;

				T sinValue = sin(phases[i]);
				sinValue += sinMinBleps[i].process();

				for (int j = 0; c + j < channels; j++)
					sinTotal += sinValue[j];
			}
			sinTotal /= channels;

			// DC block
			sinFilter.setCutoffFreq(f);
			sinFilter.process(sinTotal);
			sinOutput = sinFilter.highpass();
		}

		if (sawEnabled) {
			float sawTotal = 0.f;
			for (int c = 0; c < channels; c += T::size) {
				int i = c / T::size;

				T sawValue = saw(phases[i]);
				sawValue += sawMinBleps[i].process();

				for (int j = 0; c + j < channels; j++)
					sawTotal += sawValue[j];
			}
			sawTotal /= channels;

			// DC block
			sawFilter.setCutoffFreq(f);
			sawFilter.process(sawTotal);
			sawOutput = sawFilter.highpass();
		}
	}

	T sin(T phase) {
		T v;
		v = sin2pi_pade_05_5_4(phase);
		return v;
	}

	T saw(T phase) {
		T v;
		T x = phase + 0.5f;
		x -= simd::trunc(x);
		v = 2 * x - 1;
		return v;
	}
};


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

	E340Oscillator<8> oscillators[16];
	int spreadTuning = 0;

	E340() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);

		configParam(COARSE_PARAM, -48.0, 48.0, 0.0, "Coarse frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
		configParam(FINE_PARAM, -1.0, 1.0, 0.0, "Fine frequency", "", 0, 5, 5);
		configParam(FM_PARAM, 0.0, 1.0, 0.0, "Frequency modulation", "", 0, 10);
		configParam(SPREAD_PARAM, 0.0, 1.0, 0.0, "Spread", "", 0, 10);
		configParam(CHAOS_PARAM, 0.0, 1.0, 0.0, "Chaos", "", 0, 10);
		configParam(CHAOS_BW_PARAM, 0.0, 1.0, 0.5, "Chaos bandwidth", "", 0, 10);
		configSwitch(DENSITY_PARAM, 0.0, 2.0, 2.0, "Density", {"2", "4", "8"});

		configInput(PITCH_INPUT, "1V/oct");
		configInput(FM_INPUT, "FM");
		configInput(SYNC_INPUT, "Sync");
		configInput(SPREAD_INPUT, "Spread");
		configInput(CHAOS_INPUT, "Chaos");
		configInput(CHAOS_BW_INPUT, "Chaos bandwidth");

		configOutput(SAW_OUTPUT, "Sawtooth");
		configOutput(SINE_OUTPUT, "Sine");
	}

	void onReset() override {
		setSpreadTuning(0);
	}

	void setSpreadTuning(int spreadTuning) {
		this->spreadTuning = spreadTuning;
		for (int c = 0; c < 16; c++)
			oscillators[c].setSpreadTuning(spreadTuning);
	}

	void process(const ProcessArgs &args) override {
		int channels = std::max(inputs[PITCH_INPUT].getChannels(), 1);

		for (int c = 0; c < channels; c++) {
			auto& oscillator = oscillators[c];

			// Settings
			oscillator.sinEnabled = outputs[SINE_OUTPUT].isConnected();
			oscillator.sawEnabled = outputs[SAW_OUTPUT].isConnected();
			oscillator.syncEnabled = inputs[SYNC_INPUT].isConnected();

			// Density
			switch ((int) params[DENSITY_PARAM].getValue()) {
				case 0: oscillator.channels = 2; break;
				case 1: oscillator.channels = 4; break;
				default: oscillator.channels = 8; break;
			}

			// Pitch
			float pitch = params[COARSE_PARAM].getValue() / 12.f + inputs[PITCH_INPUT].getVoltage(c);
			if (inputs[FM_INPUT].isConnected()) {
				pitch += params[FM_PARAM].getValue() / 4.f * inputs[FM_INPUT].getPolyVoltage(c);
			}
			pitch += params[FINE_PARAM].getValue() / 12.f;
			oscillator.pitch = pitch;

			// Spread
			float spread = params[SPREAD_PARAM].getValue() + inputs[SPREAD_INPUT].getPolyVoltage(c) / 10.f;
			spread = clamp(spread, 0.f, 1.f);
			spread = std::pow(spread, 3);
			oscillator.spread = spread;

			// Chaos
			float chaos = params[CHAOS_PARAM].getValue() + inputs[CHAOS_INPUT].getPolyVoltage(c) / 10.f;
			chaos = clamp(chaos, 0.f, 1.f);
			chaos = 8.f * std::pow(chaos, 3);
			oscillator.chaos = chaos;

			// Chaos bandwidth
			float chaosBandwidth = params[CHAOS_BW_PARAM].getValue() + inputs[CHAOS_BW_INPUT].getPolyVoltage(c) / 10.f;
			chaosBandwidth = clamp(chaosBandwidth, 0.f, 1.f);
			chaosBandwidth = 0.1f * std::pow(chaosBandwidth + 1.f, 6);
			oscillator.chaosBandwidth = chaosBandwidth;

			// Process
			float syncValue = inputs[SYNC_INPUT].getPolyVoltage(c);
			oscillator.process(args.sampleTime, syncValue);

			// Outputs
			outputs[SINE_OUTPUT].setVoltage(5.f * oscillator.sinOutput, c);
			outputs[SAW_OUTPUT].setVoltage(5.f * oscillator.sawOutput, c);
		}

		outputs[SINE_OUTPUT].setChannels(channels);
		outputs[SAW_OUTPUT].setChannels(channels);
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "spreadTuning", json_integer(spreadTuning));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* spreadTuningJ = json_object_get(rootJ, "spreadTuning");
		if (spreadTuningJ)
			setSpreadTuning(json_integer_value(spreadTuningJ));
	}
};


struct E340Widget : ModuleWidget {
	E340Widget(E340 *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/E340.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));

		addParam(createParam<SynthTechAlco>(Vec(26, 43), module, E340::COARSE_PARAM));
		addParam(createParam<SynthTechAlco>(Vec(137, 43), module, E340::FINE_PARAM));

		addParam(createParam<SynthTechAlco>(Vec(26, 109), module, E340::FM_PARAM));
		addParam(createParam<SynthTechAlco>(Vec(137, 109), module, E340::SPREAD_PARAM));

		addParam(createParam<SynthTechAlco>(Vec(26, 175), module, E340::CHAOS_PARAM));
		addParam(createParam<SynthTechAlco>(Vec(137, 175), module, E340::CHAOS_BW_PARAM));

		addParam(createParam<NKK>(Vec(89, 140), module, E340::DENSITY_PARAM));

		addInput(createInput<CL1362Port>(Vec(13, 243), module, E340::PITCH_INPUT));
		addInput(createInput<CL1362Port>(Vec(63, 243), module, E340::FM_INPUT));
		addInput(createInput<CL1362Port>(Vec(113, 243), module, E340::SYNC_INPUT));
		addInput(createInput<CL1362Port>(Vec(163, 243), module, E340::SPREAD_INPUT));

		addInput(createInput<CL1362Port>(Vec(13, 301), module, E340::CHAOS_INPUT));
		addInput(createInput<CL1362Port>(Vec(63, 301), module, E340::CHAOS_BW_INPUT));
		addOutput(createOutput<CL1362Port>(Vec(113, 301), module, E340::SAW_OUTPUT));
		addOutput(createOutput<CL1362Port>(Vec(163, 301), module, E340::SINE_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		E340* module = dynamic_cast<E340*>(this->module);

		menu->addChild(new MenuSeparator);

		static const std::vector<std::string> spreadTuningLabels = {
			"Fourths",
			"Fifths",
		};
		menu->addChild(createIndexSubmenuItem("Spread tuning", spreadTuningLabels,
			[=]() {return module->spreadTuning;},
			[=](int spreadTuning) {module->setSpreadTuning(spreadTuning);}
		));
	}
};


Model *modelE340 = createModel<E340, E340Widget>("E340");
