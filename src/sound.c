#include <stdio.h>
#include <string.h>
#include <SDL/SDL.h>
#include "sound.h"
#include "assets.h"
#include "exe.h"
#include "third_party/nuked_opl3/opl3.h"

#define AUDIO_SEGMENT 0x5986
#define AUDIO_BYTES 0xc9c0
#define AUDIO_RATE 44100
#define CHANNELS 30
#define PATTERNS 254

typedef struct {
	uint16_t tracks[18];
} AudioPattern;

typedef struct {
	AudioPattern patterns[PATTERNS];
	uint8_t orders[256];
	uint16_t base;
	uint8_t loop;
	unsigned order_count;
} AudioSong;

typedef struct {
	uint16_t position, delay, frequency;
	int16_t detune, slide;
	uint8_t pan, volume, vibrato, phase, priority;
	int enabled;
} AudioChannel;

static struct {
	opl3_chip chip;
	const uint8_t *data;
	const uint8_t *instruments[256];
	AudioSong song, effects;
	AudioChannel channels[CHANNELS];
	uint8_t registers[512], levels[512];
	int ready, device, effects_enabled, music_enabled;
	int track, rate;
	unsigned clock, tempo, accumulator, order, duration;
	unsigned faults;
} audio;

static const uint8_t *audio_data(unsigned offset, unsigned size)
{
	if (offset <= AUDIO_BYTES && size <= AUDIO_BYTES - offset)
		return audio.data + offset;
	audio.faults++;
	return NULL;
}

static uint16_t channel_register(int channel, uint16_t table)
{
	return ds_word((uint16_t)(table + (channel % 18) * 2));
}

static int physical_channel(int channel)
{
	return channel % 18;
}

static void write_register(unsigned reg, uint8_t value)
{
	if (reg >= 512)
		return;
	audio.registers[reg] = value;
	OPL3_WriteRegBuffered(&audio.chip, (uint16_t)reg, value);
}

static int paired(int channel)
{
	int physical = physical_channel(channel);
	if (physical < 6)
		return (audio.registers[0x104] >> physical) & 1;
	if (physical >= 12)
		return (audio.registers[0x104] >> (17 - physical)) & 1;
	return 0;
}

static void reset_channel(int channel)
{
	AudioChannel *voice = &audio.channels[channel];
	memset(voice, 0, sizeof(*voice));
	voice->pan = 3;
	voice->volume = 64;
	voice->enabled = 1;
}

static void silence_channel(int channel)
{
	unsigned reg = channel_register(channel, 0x5126);
	int op;
	write_register(reg + 0x10, 0);
	write_register(reg, 0);
	write_register(reg + 0x20, 0);
	for (op = 0; op < 2; op++) {
		unsigned slot = channel_register(channel, op ? 0x51b6 : 0x516e);
		write_register(slot, 0);
		write_register(slot + 0x20, 63);
		write_register(slot + 0x40, 255);
		write_register(slot + 0x60, 255);
		write_register(slot + 0xc0, 0);
	}
}

static void note_off(int channel)
{
	unsigned reg = channel_register(channel, 0x5126) + 0x10;
	write_register(reg, audio.registers[reg] & ~0x20);
}

static void update_volume(int channel)
{
	AudioChannel *voice = &audio.channels[channel];
	unsigned reg = channel_register(channel, 0x5126);
	int physical = physical_channel(channel), count = paired(channel) && physical < 6 ? 4 : 2;
	int op;
	if (paired(channel) && physical >= 12)
		return;
	for (op = 0; op < count; op++) {
		static const uint16_t tables[] = { 0x516e, 0x51b6, 0x51fe, 0x5246 };
		unsigned slot = channel_register(channel, tables[op]) + 0x20;
		uint8_t raw = audio.levels[slot];
		int connection = audio.registers[reg + 0x20] & 1;
		int second = audio.registers[reg + 0x23] & 1;
		int scale = count == 2 ? (op == 1 || connection) :
			op == 3 || (op == 0 && connection) ||
			(op == 1 && second && !connection) || (op == 2 && second && connection);
		int level = raw & 63;
		if (scale) {
			int master = channel < 18 ? 62 : 64;
			level = 63 - ((((63 - level) * master) >> 6) * voice->volume >> 6);
		}
		write_register(slot, (uint8_t)((raw & 0xc0) | (level & 63)));
	}
}

static void set_operator(int channel, unsigned slot, const uint8_t *patch)
{
	uint8_t flags = (uint8_t)(((patch[0] << 3) | (patch[0] << 6) |
		(patch[1] << 1) | (patch[1] << 4)) & 0xf0);
	write_register(slot, flags | (patch[2] >> 4));
	audio.levels[slot + 0x20] = (uint8_t)(patch[6] | ((patch[2] & 3) << 6));
	write_register(slot + 0x20, audio.levels[slot + 0x20]);
	write_register(slot + 0x40, patch[4]);
	write_register(slot + 0x60, patch[5]);
	write_register(slot + 0xc0, patch[3]);
	(void)channel;
}

static void pitch_update(int channel)
{
	AudioChannel *voice = &audio.channels[channel];
	const uint8_t *wave = ds_ptr(0x55b6, 128);
	int vibrato = ((int8_t)wave[voice->phase & 127] * (voice->vibrato >> 4)) >> 5;
	unsigned frequency = ((voice->frequency >> 2) + vibrato + voice->detune) & 0x3ff;
	unsigned reg = channel_register(channel, 0x5126);
	write_register(reg, (uint8_t)frequency);
	write_register(reg + 0x10, (uint8_t)((audio.registers[reg + 0x10] & 0xfc) | (frequency >> 8)));
}

static void note(int channel, uint8_t key, unsigned instrument)
{
	AudioChannel *voice = &audio.channels[channel];
	const uint8_t *patch = instrument < 256 ? audio.instruments[instrument] : NULL;
	unsigned reg = channel_register(channel, 0x5126), frequency, octave;
	int physical = physical_channel(channel);
	if (instrument == 255 || (instrument && !patch)) {
		if (physical >= 12 && paired(channel))
			return;
		if (physical < 6 && paired(channel)) {
			write_register(0x104, audio.registers[0x104] & ~(1u << physical));
			silence_channel(17 - physical);
		}
		silence_channel(channel);
		return;
	}
	if (!key)
		return;
	key--;
	frequency = ds_word((uint16_t)(0x510e + (key % 12) * 2));
	octave = key / 12;
	voice->frequency = (uint16_t)(frequency << 2);
	voice->vibrato = voice->phase = 0;
	frequency = (frequency + voice->detune) & 0x3ff;
	write_register(reg + 0x10, (uint8_t)((frequency >> 8) | (octave << 2) |
		(instrument ? 0 : audio.registers[reg + 0x10] & 0x20)));
	write_register(reg, (uint8_t)frequency);
	if (!instrument)
		return;
	write_register(reg + 0x20, (uint8_t)((patch[0x12] << 1) | (patch[0x11] & 1) | (voice->pan << 4)));
	if (physical < 6 && patch[0x10]) {
		write_register(0x104, audio.registers[0x104] | (1u << physical));
		write_register(reg + 0x23, (uint8_t)(((patch[0x11] >> 1) & 1) | (voice->pan << 4)));
		set_operator(channel, channel_register(channel, 0x516e), patch + 0x2c);
		set_operator(channel, channel_register(channel, 0x51b6), patch + 0x24);
		set_operator(channel, channel_register(channel, 0x51fe), patch + 0x1c);
		set_operator(channel, channel_register(channel, 0x5246), patch + 0x14);
	} else {
		int primary = physical < 6 ? physical : physical >= 12 ? 17 - physical : -1;
		if (primary >= 0 && paired(channel)) {
			write_register(0x104, audio.registers[0x104] & ~(1u << primary));
			silence_channel(physical < 6 ? 17 - primary : primary);
		}
		set_operator(channel, channel_register(channel, 0x516e), patch + 0x1c);
		set_operator(channel, channel_register(channel, 0x51b6), patch + 0x14);
	}
	update_volume(channel);
	write_register(reg + 0x10, audio.registers[reg + 0x10] | 0x20);
}

static int load_song(AudioSong *song, unsigned start)
{
	unsigned cursor = start + 21, index, count = 0, last = 0;
	const uint8_t *header = audio_data(start, 21), *presence;
	if (!header)
		return 0;
	memset(song, 0, sizeof(*song));
	song->loop = header[20];
	do {
		const uint8_t *order = audio_data(cursor++, 1);
		if (!order || count >= 256)
			return 0;
		song->orders[count++] = *order;
	} while (song->orders[count - 1] != 255);
	song->order_count = count;
	presence = audio_data(cursor, 32);
	if (!presence)
		return 0;
	cursor += 32;
	for (index = 0; index < PATTERNS; index++) {
		if (presence[index >> 3] & (1u << (index & 7))) {
			const uint8_t *pointer = audio_data(cursor, 2), *record;
			unsigned mask, channel, position;
			if (!pointer)
				return 0;
			cursor += 2;
			position = start + rd16(pointer);
			record = audio_data(position, 3);
			if (!record)
				return 0;
			mask = record[0] | (unsigned)record[1] << 8 | (unsigned)record[2] << 16;
			position += 3;
			for (channel = 0; channel < 18; channel++)
				if (mask & (1u << channel)) {
					pointer = audio_data(position, 2);
					if (!pointer)
						return 0;
					song->patterns[index].tracks[channel] = rd16(pointer);
					position += 2;
				}
			if (!audio_data(position, 2))
				return 0;
			last = position + 2;
		}
	}
	song->base = (uint16_t)(last ? last : cursor);
	return 1;
}

static int next_pattern(void)
{
	unsigned attempts, channel;
	for (attempts = 0; attempts < 256; attempts++) {
		unsigned index;
		if (audio.order >= audio.song.order_count)
			return 0;
		index = audio.song.orders[audio.order++];
		if (index == 255) {
			if (audio.song.loop == 255)
				return 0;
			audio.order = audio.song.loop;
			continue;
		}
		if (index >= PATTERNS)
			return 0;
		audio.duration = 512;
		for (channel = 0; channel < 18; channel++) {
			uint16_t offset = audio.song.patterns[index].tracks[channel];
			int enabled = audio.channels[channel].enabled;
			if (offset) {
				reset_channel(channel);
				audio.channels[channel].enabled = enabled;
				audio.channels[channel].position = (uint16_t)(audio.song.base + offset - 4);
				audio.channels[channel].delay = 1;
			} else {
				audio.channels[channel].delay = 0;
				audio.channels[channel].slide = 0;
				audio.channels[channel].vibrato = 0;
			}
		}
		return 1;
	}
	return 0;
}

static int operator_for_command(int channel, uint8_t value, int volume)
{
	static const uint16_t tables[] = { 0x5246, 0x51fe, 0x51b6, 0x516e };
	int physical = physical_channel(channel);
	if (paired(channel) && physical >= 12)
		return -1;
	if (paired(channel) && physical < 6)
		return channel_register(channel, tables[(value >> (volume ? 6 : 4)) & 3]);
	return channel_register(channel, value & (volume ? 0x40 : 0x10) ? 0x516e : 0x51b6);
}

static int channel_step(int channel)
{
	AudioChannel *voice = &audio.channels[channel];
	unsigned commands = 0;
	if (voice->delay && !--voice->delay) {
		while (!voice->delay && commands++ < 4096) {
			const uint8_t *command = audio_data(voice->position, 1);
			uint8_t opcode, value = 0;
			unsigned reg = channel_register(channel, 0x5126);
			int slot;
			if (!command)
				break;
			opcode = *command & 15;
			voice->delay = (*command & 0xf0) >> 1;
			voice->position++;
			if (opcode) {
				command = audio_data(voice->position++, 1);
				if (!command)
					break;
				value = *command;
			}
			switch (opcode) {
			case 1:
				voice->pan = (uint8_t)(((value & 1) << 1) | ((value >> 4) & 1));
				if (voice->enabled && !(paired(channel) && physical_channel(channel) >= 12)) {
					reg += paired(channel) ? 0x23 : 0x20;
					write_register(reg, (uint8_t)((audio.registers[reg] & 0xcf) | (voice->pan << 4)));
				}
				break;
			case 2: voice->detune = (int8_t)value; if (voice->enabled) pitch_update(channel); break;
			case 3:
				voice->vibrato = value;
				if (!value) { voice->phase = 0; if (voice->enabled) pitch_update(channel); }
				break;
			case 4: voice->slide = (int8_t)value; break;
			case 5: if (voice->enabled) note_off(channel); break;
			case 6:
				if (voice->enabled)
					write_register(reg + 0x20, (uint8_t)((audio.registers[reg + 0x20] & 0xf1) | ((value & 7) << 1)));
				break;
			case 7:
				slot = operator_for_command(channel, value, 0);
				if (voice->enabled && slot >= 0)
					write_register(slot, (uint8_t)((audio.registers[slot] & 0xf0) | (value & 15)));
				break;
			case 8:
				slot = operator_for_command(channel, value, 1);
				if (voice->enabled && slot >= 0) {
					slot += 0x20;
					audio.levels[slot] = (uint8_t)((audio.levels[slot] & 0xc0) | (value & 63));
					update_volume(channel);
				}
				break;
			case 9: voice->volume = value & 127; if (voice->enabled) update_volume(channel); break;
			case 10: voice->delay = (uint16_t)(voice->delay + ((value & 15) < 9 ? value & 15 : (value & 15) - 16)); break;
			case 12:
				if (!value)
					return 1;
				if (value == 1) {
					reset_channel(channel);
					voice->delay = 0;
					if (channel >= 18) {
						audio.channels[channel - 18].enabled = 1;
						if (channel < 24)
							audio.channels[17 - (channel - 18)].enabled = 1;
					}
					return 0;
				}
				break;
			case 13: audio.tempo = value; break;
			case 14: if (voice->enabled) note(channel, value, 0); break;
			case 15:
				command = audio_data(voice->position++, 1);
				if (!command) { voice->delay = 0; return 0; }
				if (voice->enabled)
					note(channel, value, *command);
				break;
			}
		}
		if (commands > 4096) {
			voice->delay = 0;
			audio.faults++;
		}
	}
	voice->frequency = (uint16_t)(voice->frequency + voice->slide);
	if (voice->vibrato)
		voice->phase += (voice->vibrato & 15) * 2;
	if (voice->enabled && (voice->vibrato || voice->slide))
		pitch_update(channel);
	return 0;
}

static void sequence_tick(void)
{
	unsigned channel, budget = 256;
	if (audio.music_enabled && audio.track >= 0) {
		audio.accumulator += audio.tempo;
		while (audio.accumulator >= 64 && budget--) {
			audio.accumulator -= 64;
			if (!audio.duration || !--audio.duration) {
				if (!next_pattern()) {
					audio.track = -1;
					for (channel = 0; channel < 18; channel++)
						if (audio.channels[channel].enabled)
							note_off(channel);
					break;
				}
			}
			channel = 0;
			while (channel < 18 && budget) {
				if (channel_step(channel)) {
					budget--;
					if (!next_pattern()) {
						audio.track = -1;
						break;
					}
					channel = 0;
				} else {
					channel++;
				}
			}
			if (audio.track < 0)
				for (channel = 0; channel < 18; channel++)
					if (audio.channels[channel].enabled)
						note_off(channel);
		}
	}
	if (audio.effects_enabled)
		for (channel = 18; channel < CHANNELS; channel++)
			channel_step(channel);
}

int sound_prepare(int rate)
{
	unsigned index, cursor = 0x38;
	const uint8_t *presence;
	if (rate <= 0 || audio.device)
		return 0;
	memset(&audio, 0, sizeof(audio));
	audio.data = exe_ptr(AUDIO_SEGMENT, 0, AUDIO_BYTES);
	if (!audio.data)
		return 0;
	presence = audio_data(0x18, 32);
	for (index = 1; index < 254; index++)
		if (presence[index >> 3] & (1u << (index & 7))) {
			audio.instruments[index] = audio_data(cursor, 64);
			if (!audio.instruments[index])
				return 0;
			cursor += 64;
		}
	if (!load_song(&audio.effects, 0xfb4))
		return 0;
	OPL3_Reset(&audio.chip, (uint32_t)rate);
	write_register(0x105, 1);
	write_register(1, 0x20);
	write_register(0xbd, 0xc0);
	for (index = 0; index < CHANNELS; index++)
		reset_channel(index);
	audio.effects_enabled = audio.music_enabled = 1;
	audio.track = -1;
	audio.rate = rate;
	audio.ready = 1;
	return 1;
}

static void audio_callback(void *unused, Uint8 *stream, int length)
{
	(void)unused;
	sound_render((int16_t *)stream, length / (2 * sizeof(int16_t)));
}

int sound_init(void)
{
	SDL_AudioSpec desired;
	if (!sound_prepare(AUDIO_RATE)) {
		fprintf(stderr, "Could not decode embedded audio.\n");
		return 0;
	}
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
		audio.ready = 0;
		return 0;
	}
	memset(&desired, 0, sizeof(desired));
	desired.freq = AUDIO_RATE;
	desired.format = AUDIO_S16SYS;
	desired.channels = 2;
	desired.samples = 1024;
	desired.callback = audio_callback;
	if (SDL_OpenAudio(&desired, NULL) < 0) {
		fprintf(stderr, "SDL audio unavailable: %s\n", SDL_GetError());
		audio.ready = 0;
		return 0;
	}
	audio.device = 1;
	SDL_PauseAudio(0);
	return 1;
}

void sound_shutdown(void)
{
	if (audio.device)
		SDL_CloseAudio();
	audio.device = audio.ready = 0;
}

void sound_play(int id)
{
	int channel;
	if (!audio.ready || !audio.effects_enabled || id < 0 || id >= SOUND_EFFECT_COUNT)
		return;
	if (audio.device)
		SDL_LockAudio();
	for (channel = 18; channel < CHANNELS; channel++) {
		unsigned offset = audio.effects.patterns[id].tracks[channel - 18];
		unsigned priority = audio.effects.orders[id];
		AudioChannel *voice = &audio.channels[channel];
		if (offset && voice->priority <= priority) {
			reset_channel(channel);
			voice->position = (uint16_t)(audio.effects.base + offset - 4);
			voice->delay = 1;
			voice->priority = (uint8_t)priority;
			audio.channels[channel - 18].enabled = 0;
			if (channel < 24)
				audio.channels[17 - (channel - 18)].enabled = 0;
		}
	}
	if (audio.device)
		SDL_UnlockAudio();
}

void sound_music(int track)
{
	int channel;
	if (!audio.ready || track < -1 || track > SOUND_MUSIC_COUNT)
		return;
	if (audio.device)
		SDL_LockAudio();
	for (channel = 0; channel < 18; channel++) {
		if (audio.channels[channel].enabled)
			silence_channel(channel);
		reset_channel(channel);
		if ((channel < 12 && audio.channels[channel + 18].delay) ||
		    (channel >= 12 && audio.channels[35 - channel].delay))
			audio.channels[channel].enabled = 0;
	}
	audio.track = -1;
	if (track >= 0 && load_song(&audio.song, rd16(audio.data + track * 2)))
		audio.track = track;
	audio.order = 0;
	audio.duration = 1;
	audio.tempo = 64;
	audio.accumulator = 0;
	if (audio.device)
		SDL_UnlockAudio();
}

void sound_enable(int effects, int music)
{
	int channel;
	if (!audio.ready)
		return;
	if (audio.device)
		SDL_LockAudio();
	if (audio.effects_enabled && !effects)
		for (channel = 18; channel < CHANNELS; channel++) {
			note_off(channel);
			reset_channel(channel);
			audio.channels[channel - 18].enabled = 1;
			if (channel < 24)
				audio.channels[17 - (channel - 18)].enabled = 1;
		}
	if (audio.music_enabled && !music)
		for (channel = 0; channel < 18; channel++)
			if (audio.channels[channel].enabled)
				note_off(channel);
	if (!audio.music_enabled && music) {
		audio.order = 0;
		audio.duration = 1;
	}
	audio.effects_enabled = effects != 0;
	audio.music_enabled = music != 0;
	if (audio.device)
		SDL_UnlockAudio();
}

unsigned sound_faults(void)
{
	unsigned faults;
	if (audio.device)
		SDL_LockAudio();
	faults = audio.faults;
	if (audio.device)
		SDL_UnlockAudio();
	return faults;
}

void sound_render(int16_t *samples, int frames)
{
	int frame;
	if (!samples || frames <= 0)
		return;
	if (!audio.ready) {
		memset(samples, 0, (size_t)frames * 2 * sizeof(*samples));
		return;
	}
	for (frame = 0; frame < frames; frame++) {
		if (audio.clock < 70)
			sequence_tick();
		OPL3_GenerateResampled(&audio.chip, samples + frame * 2);
		samples[frame * 2] /= 2;
		samples[frame * 2 + 1] /= 2;
		if (!audio.effects_enabled && !audio.music_enabled)
			samples[frame * 2] = samples[frame * 2 + 1] = 0;
		audio.clock += 70;
		if (audio.clock >= (unsigned)audio.rate)
			audio.clock -= audio.rate;
	}
}

static int write_word(FILE *file, unsigned value, int bytes)
{
	int byte;
	for (byte = 0; byte < bytes; byte++)
		if (fputc((value >> (byte * 8)) & 255, file) == EOF)
			return 0;
	return 1;
}

int sound_export_wav(const char *path, int track, int effect, int seconds)
{
	int16_t samples[1024 * 2];
	FILE *file;
	unsigned remaining, frames, sample;
	int ok;
	if (seconds < 1 || seconds > 120 || track < -1 || track > SOUND_MUSIC_COUNT ||
	    effect < -1 || effect >= SOUND_EFFECT_COUNT || !sound_prepare(AUDIO_RATE))
		return 0;
	file = fopen(path, "wb");
	if (!file)
		return 0;
	remaining = (unsigned)seconds * AUDIO_RATE;
	ok = fwrite("RIFF", 1, 4, file) == 4 && write_word(file, remaining * 4 + 36, 4) &&
		fwrite("WAVEfmt ", 1, 8, file) == 8 && write_word(file, 16, 4) &&
		write_word(file, 1, 2) && write_word(file, 2, 2) &&
		write_word(file, AUDIO_RATE, 4) && write_word(file, AUDIO_RATE * 4, 4) &&
		write_word(file, 4, 2) && write_word(file, 16, 2) &&
		fwrite("data", 1, 4, file) == 4 && write_word(file, remaining * 4, 4);
	sound_music(track);
	sound_play(effect);
	while (remaining && ok) {
		frames = remaining > 1024 ? 1024 : remaining;
		sound_render(samples, (int)frames);
		for (sample = 0; sample < frames * 2 && ok; sample++)
			ok = write_word(file, (uint16_t)samples[sample], 2);
		remaining -= frames;
	}
	if (fclose(file))
		ok = 0;
	sound_shutdown();
	return ok;
}