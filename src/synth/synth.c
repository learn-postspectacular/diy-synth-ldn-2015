#include <string.h>
#include <stdlib.h>
#include "synth/synth.h"

static tinymt32_t synthRNG;

void synth_osc_init(SynthOsc *osc, OscFn fn, float gain, float phase,
		float freq, float dc) {
	osc->fn = fn;
	osc->phase = phase;
	osc->freq = FREQ_TO_RAD(freq);
	osc->amp = gain;
	osc->dcOffset = dc;
}

void synth_osc_set_wavetables(SynthOsc *osc, const float* tbl1,
		const float* tbl2) {
	osc->wtable1 = tbl1;
	osc->wtable2 = tbl2;
}

float synth_osc_sin(SynthOsc *osc, float lfo, float lfo2) {
	float phase = truncPhase(osc->phase + osc->freq + lfo);
	osc->phase = phase;
	return WTABLE_LOOKUP(wtable_sin, phase) * osc->amp;
}

float synth_osc_sin_dc(SynthOsc *osc, float lfo, float lfo2) {
	float phase = truncPhase(osc->phase + osc->freq + lfo);
	osc->phase = phase;
	return maddf(WTABLE_LOOKUP(wtable_sin, phase), osc->amp, osc->dcOffset);
}

float synth_osc_rect(SynthOsc *osc, float lfo, float lfo2) {
	float phase = truncPhase(osc->phase + osc->freq + lfo);
	osc->phase = phase;
	return stepf(phase, PI, osc->amp, -osc->amp);
}

float synth_osc_rect_phase(SynthOsc *osc, float lfo, float lfo2) {
	float phase = truncPhase(osc->phase + osc->freq + lfo);
	osc->phase = phase;
	return stepf(phase, PI + lfo2, osc->amp, -osc->amp);
}

float synth_osc_rect_dc(SynthOsc *osc, float lfo, float lfo2) {
	float phase = truncPhase(osc->phase + osc->freq + lfo);
	osc->phase = phase;
	return osc->dcOffset + stepf(phase, PI, osc->amp, -osc->amp);
}

float synth_osc_saw(SynthOsc *osc, float lfo, float lfo2) {
	float phase = truncPhase(osc->phase + osc->freq + lfo);
	osc->phase = phase;
	return (phase * INV_PI - 1.0f) * osc->amp;
}

float synth_osc_saw_dc(SynthOsc *osc, float lfo, float lfo2) {
	float phase = truncPhase(osc->phase + osc->freq + lfo);
	osc->phase = phase;
	return maddf(phase * INV_PI - 1.0f, osc->amp, osc->dcOffset);
}

float synth_osc_tri(SynthOsc *osc, float lfo, float lfo2) {
	float phase = truncPhase(osc->phase + osc->freq + lfo);
	osc->phase = phase;
	float x = 2.0f - (phase * INV_HALF_PI);
	x = 1.0f - stepf(x, 0.0f, -x, x);
	if (x > -1.0f) {
		return x * osc->amp;
	} else {
		return -osc->amp;
	}
}

float synth_osc_tri_dc(SynthOsc *osc, float lfo, float lfo2) {
	float phase = truncPhase(osc->phase + osc->freq + lfo);
	osc->phase = phase;
	float x = 2.0f - (phase * INV_HALF_PI);
	x = 1.0f - stepf(x, 0.0f, -x, x);
	if (x > -1.0f) {
		return maddf(x, osc->amp, osc->dcOffset);
	} else {
		return osc->dcOffset - osc->amp;
	}
}

float synth_osc_wtable_morph(SynthOsc *osc, float lfo, float morph) {
	float phase = truncPhase(osc->phase + osc->freq + lfo);
	truncPhase(phase);
	osc->phase = phase;
	return mixf(WTABLE_LOOKUP(osc->wtable1, phase), WTABLE_LOOKUP(osc->wtable2, phase), morph) * osc->amp;
}

float synth_osc_noise(SynthOsc *osc, float lfo, float lfo2) {
	return NORM_RANDF(&synthRNG) * osc->amp;
}

float synth_osc_noise_dc(SynthOsc *osc, float lfo, float lfo2) {
	return osc->dcOffset + NORM_RANDF(&synthRNG) * osc->amp;
}

float synth_osc_nop(SynthOsc *osc, float lfo, float lfo2) {
	return osc->dcOffset;
}

void synth_adsr_init(ADSR *env, float attRate, float decayRate,
		float releaseRate, float attGain, float sustainGain) {
	env->attackRate = attRate * ADSR_SCALE
	;
	env->decayRate = decayRate * ADSR_SCALE
	;
	env->releaseRate = releaseRate * ADSR_SCALE
	;
	env->attackGain = attGain * ADSR_SCALE
	;
	env->sustainGain = sustainGain * ADSR_SCALE
	;
	env->phase = ATTACK;
	env->currGain = 0.0f;
}

float synth_adsr_update(ADSR *env, float envMod) {
	switch (env->phase) {
	case ATTACK:
		if (env->currGain >= env->attackGain) {
			env->phase = DECAY;
		} else {
			env->currGain += env->attackRate * envMod;
		}
		break;
	case DECAY:
		if (env->currGain > env->sustainGain) {
			env->currGain -= env->decayRate * envMod;
		} else {
			env->phase = RELEASE; // skip SUSTAIN phase for now
		}
		break;
	case SUSTAIN:
		return env->sustainGain;
	case RELEASE:
		if (env->currGain > 0.0f) {
			env->currGain -= env->releaseRate;
			if (env->currGain < 0.0f) {
				env->currGain = 0.0f;
			}
		} else {
			env->phase = IDLE;
		}
		break;
	default:
		break;
	}
	return env->currGain;
}

void synth_voice_init(SynthVoice *voice, uint32_t flags) {
	synth_osc_init(&(voice->lfoPitch), synth_osc_nop, 0.0f, 0.0f, 0.0f, 0.0f);
	synth_osc_init(&(voice->lfoMorph), synth_osc_nop, 0.0f, 0.0f, 0.0f, 0.0f);
	voice->flags = flags;
}

void synth_init(Synth *synth) {
	synth->nextVoice = 0;
	for (uint8_t i = 0; i < SYNTH_POLYPHONY; i++) {
		SynthVoice *voice = synth_new_voice(synth);
		synth_adsr_init(&(voice->env), 0.0025f, 0.00025f, 0.00005f, 1.0f,
				0.25f);
		voice->env.phase = IDLE;
	}
	synth_osc_init(&(synth->lfoFilter), synth_osc_nop, 0.0f, 0.0f, 0.0f, 0.0f);
	synth_osc_init(&(synth->lfoEnvMod), synth_osc_nop, 0.0f, 0.0f, 0.0f, 0.0f);
	synth_bus_init(&(synth->bus[0]), malloc(sizeof(int16_t)), 1, 2);
	tinymt32_init(&synthRNG, 0xcafebad);
}

SynthVoice* synth_new_voice(Synth *synth) {
	SynthVoice* voice = &(synth->voices[synth->nextVoice]);
	synth_voice_init(voice, 0);
	synth->nextVoice++;
	if (synth->nextVoice == SYNTH_POLYPHONY) {
		synth->nextVoice = 0;
	}
	return voice;
}

void synth_bus_init(SynthFXBus *bus, int16_t *buf, size_t len, uint8_t decay) {
	if (bus->buf != NULL) {
		free(bus->buf);
	}
	bus->buf = buf;
	bus->len = len;
	bus->readPos = 1;
	bus->writePos = 0;
	bus->readPtr = &buf[bus->readPos];
	bus->writePtr = &buf[bus->writePos];
	bus->decay = decay;
	memset(buf, 0, len << 1);
}

void synth_render_slice(Synth *synth, int16_t *ptr, size_t len) {
	int16_t sumL, sumR;
	SynthOsc *lfoEnvMod = &(synth->lfoEnvMod);
	SynthFXBus *fx = &(synth->bus[0]);
	while (len--) {
		sumL = sumR = 0;
		float envMod = lfoEnvMod->fn(lfoEnvMod, 0.0f, 0.0f);
		SynthVoice *voice = &(synth->voices[SYNTH_POLYPHONY - 1]);
		while (voice >= synth->voices) {
			ADSR *env = &(voice->env);
			if (env->phase) {
				float gain = synth_adsr_update(env, envMod);
				SynthOsc *osc = &(voice->lfoPitch);
				float lfoVPitchVal = osc->fn(osc, 0.0f, 0.0f);
				osc = &(voice->lfoMorph);
				float lfoVMorphVal = osc->fn(osc, 0.0f, 0.0f);
				osc = &(voice->osc[0]);
				sumL += (int16_t) (gain
						* osc->fn(osc, lfoVPitchVal, lfoVMorphVal));
				osc++;
				sumR += (int16_t) (gain
						* osc->fn(osc, lfoVPitchVal, lfoVMorphVal));
			}
			voice--;
		}
		sumL += *(fx->readPtr);
		clamp16(sumL);
		sumR += *(fx->readPtr);
		clamp16(sumR);
#ifdef SYNTH_USE_DELAY
		fx->readPtr++;
		fx->readPos++;
		if (fx->readPos >= fx->len) {
			fx->readPos = 0;
			fx->readPtr = &(fx->buf[0]);
		}
#endif
		sumL = (sumL + sumR) / 2;
		*ptr = sumL;
		ptr++;
		*ptr = sumL;
		ptr++;
#ifdef SYNTH_USE_DELAY
		*(fx->writePtr) = ((sumL + sumR) >> fx->decay);
		fx->writePtr++;
		fx->writePos++;
		if (fx->writePos >= fx->len) {
			fx->writePos = 0;
			fx->writePtr = &(fx->buf[0]);
		}
#endif
	}
}
