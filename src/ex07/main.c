#include "ex07/main.h"

__IO DMABufferState bufferState = BUFFER_OFFSET_NONE;
__IO int32_t noteDir = 1;

uint8_t audioBuffer[AUDIO_BUFFER_SIZE];

static Synth synth;
static SeqTrack* tracks[3];
static uint8_t keyChanges[] = { 0, 5, 7, 8, 12, 19, 24 };

static void playNoteInst1(Synth* synth, int8_t note, uint32_t tick);
static void playNoteInst2(Synth* synth, int8_t note, uint32_t tick);
static void playNoteInst3(Synth* synth, int8_t note, uint32_t tick);
static void updateAudioBuffer(Synth *synth);

static __IO uint32_t transposeID = 0;

int main(void) {
	HAL_Init();
	SystemClock_Config();
	led_all_init();
	BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_EXTI);
	HAL_Delay(1000);

	if (BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_AUTO, 85, SAMPLERATE) != 0) {
		Error_Handler();
	}

	synth_init(&synth);
	synth_bus_init(&(synth.bus[0]),
			(int16_t*) malloc(sizeof(int16_t) * DELAY_LENGTH),
			DELAY_LENGTH, 2);
	synth_osc_init(&(synth.lfoEnvMod), synth_osc_sin_dc, 0, 0, 0, 1.0f);
	BSP_AUDIO_OUT_Play((uint16_t*) &audioBuffer[0], AUDIO_BUFFER_SIZE);

 	int8_t notes1[16] = {
			36, -1, 12, 12,
			-1, -1, -1, -1,
			48, -1, 17, 12,
			-1, -1, -1, 24 };

	int8_t notes2[16] = {
			0, 12, 0, 12,
			0, 12, 0, 12,
			7, 19, 7, 19,
			7, 19, 7, 19,
	};

	int8_t notes3[8] = {
			-1, -1, -1, -1,
			24, -1, 22,19
		};

	tracks[0] = initTrack((SeqTrack*) malloc(sizeof(SeqTrack)), playNoteInst1,
			notes1, 16, 250);

	tracks[1] = initTrack((SeqTrack*) malloc(sizeof(SeqTrack)), playNoteInst2,
				notes2, 16, 500);

	//tracks[2] = initTrack((SeqTrack*) malloc(sizeof(SeqTrack)), playNoteInst3,
	//			notes3, 8, 1000);

	while (1) {
		uint32_t tick = HAL_GetTick();
		updateAllTracks(&synth, tracks, 2, tick);
		updateAudioBuffer(&synth);
	}
}

void playNoteInst1(Synth* synth, int8_t note, uint32_t tick) {
	float freq = notes[note + keyChanges[transposeID]];
	SynthVoice *voice = synth_new_voice(synth);
	synth_adsr_init(&(voice->env), 0.25f, 0.000025f, 0.005f, 1.0f, 0.95f);
	synth_osc_init(&(voice->lfoPitch), synth_osc_sin, FREQ_TO_RAD(5.0f), 0.0f,
			10.0f, 0.0f);
	synth_osc_init(&(voice->osc[0]), synth_osc_sin, 0.20f, 0.0f, freq, 0.0f);
	synth_osc_init(&(voice->osc[1]), synth_osc_sin, 0.10f, 0.0f, freq,
			0.0f);
	BSP_LED_Toggle(LED_GREEN);
}

void playNoteInst2(Synth* synth, int8_t note, uint32_t tick) {
	float freq = notes[note + keyChanges[transposeID]] * 0.5f;
	SynthVoice *voice = synth_new_voice(synth);
	synth_adsr_init(&(voice->env), 0.25f, 0.0000025f, 0.005f, 1.0f, 0.95f);
	synth_osc_init(&(voice->lfoPitch), synth_osc_sin, FREQ_TO_RAD(5.0f), 0.0f,
			10.0f, 0.0f);
	synth_osc_init(&(voice->osc[0]), synth_osc_sin, 0.30f, 0.0f, freq, 0.0f);
	synth_osc_init(&(voice->osc[1]), synth_osc_sin, 0.30f, 0.0f, freq * 0.51f,
			0.0f);
	BSP_LED_Toggle(LED_ORANGE);
}

void playNoteInst3(Synth* synth, int8_t note, uint32_t tick) {
	float freq = notes[note + keyChanges[transposeID]];
	float freq2 = notes[note + 5 + keyChanges[transposeID]];
	SynthVoice *voice = synth_new_voice(synth);
	synth_adsr_init(&(voice->env), 0.025f, 0.0000025f, 0.00005f, 1.0f, 0.5f);
	synth_osc_init(&(voice->lfoPitch), synth_osc_sin, FREQ_TO_RAD(5.0f), 0.0f, 10.0f, 0.0f);
	synth_osc_init(&(voice->lfoMorph), synth_osc_saw_dc, 0.499f, PI, 4.0f + 3.9f * sinf(tick * 0.0005f), 0.5f);
	uint8_t vid = (tick >> 8) & 1;
	synth_osc_init(&(voice->osc[vid]), synth_osc_saw, 0.15f, 0.0f, freq, 0.0f);
	//synth_osc_set_wavetables(&(voice->osc[vid]), &wtable_harmonics_3[0], &wtable_noise[0]);
	synth_osc_init(&(voice->osc[1 - vid]), synth_osc_tri, 0.15f, 0.0f, freq2, 0.0f);
	//synth_osc_set_wavetables(&(voice->osc[1 - vid]), &wtable_harmonics_3[0], &wtable_harmonics_2[0]);
	BSP_LED_Toggle(LED_ORANGE);
}

void updateAudioBuffer(Synth *synth) {
	if (bufferState == BUFFER_OFFSET_HALF) {
		int16_t *ptr = (int16_t*) &audioBuffer[0];
		synth_render_slice(synth, ptr, AUDIO_BUFFER_SIZE >> 3);
		bufferState = BUFFER_OFFSET_NONE;
	}

	if (bufferState == BUFFER_OFFSET_FULL) {
		int16_t *ptr = (int16_t*) &audioBuffer[AUDIO_BUFFER_SIZE >> 1];
		synth_render_slice(synth, ptr, AUDIO_BUFFER_SIZE >> 3);
		bufferState = BUFFER_OFFSET_NONE;
	}
}

void HAL_GPIO_EXTI_Callback(uint16_t pin) {
	if (pin == KEY_BUTTON_PIN) {
		BSP_LED_Toggle(LED_BLUE);
		transposeID = (transposeID + 1) % 7;
	}
}

void BSP_AUDIO_OUT_HalfTransfer_CallBack(void) {
	bufferState = BUFFER_OFFSET_HALF;
}

void BSP_AUDIO_OUT_TransferComplete_CallBack(void) {
	bufferState = BUFFER_OFFSET_FULL;
	BSP_AUDIO_OUT_ChangeBuffer((uint16_t*) &audioBuffer[0],
	AUDIO_BUFFER_SIZE >> 1);
}

void BSP_AUDIO_OUT_Error_CallBack(void) {
	BSP_LED_On(LED_RED);
	while (1) {
	}
}
