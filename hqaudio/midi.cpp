/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License.
*/

#include "midi.h"

#include "platform/mono.h"

#ifdef USE_TSFMIDI

#include "platform/i_sound.h"
#include "platform/s_midi.h"

#define TML_IMPLEMENTATION
#include "lib/tml.h"

namespace chr = std::chrono;

MIDILoader::MIDILoader(const std::string& filename) : SoundLoader(filename) {}

bool MIDILoader::Open() {
	
	midiMessage = tml_load_filename(filename.c_str());
	if (!midiMessage) {
		mprintf((1, "Error opening midi file %s\n", filename.c_str()));
		return false;
	}

	midi = tsf_load_filename(SoundFontFilename);
	if (!midi) {
		mprintf((1, "Error creating midi synthesizer with soundfont %s\n", SoundFontFilename));
		tml_free(midiMessage);
		midiMessage = nullptr;
		return false;
	}

	firstMidiMessage = midiMessage;

	for (int i = 0; i < 16; i++) {
		tsf_channel_set_bank_preset(midi, i, 0, 0);
	}
	tsf_channel_set_bank_preset(midi, 9, 128, 0);

	auto sampleRate = plat_get_preferred_midi_sample_rate();

	tsf_set_output(midi, TSF_STEREO_INTERLEAVED, sampleRate);

	properties.channels = 2;
	properties.format = SF_SHORT;
	properties.sampleRate = sampleRate;

	time = clock.now();

	return true;

}

size_t MIDILoader::GetSamples(void* buffer, size_t bufferSize) {
	
	if (!midiMessage)
		return 0;

	auto now = clock.now();
	auto diff = chr::duration_cast<chr::milliseconds>(now - time).count();
	time = now;

	currentPlaybackTime += (unsigned int)diff;
	
	auto sampleCount = bufferSize / (2 * sizeof(int16_t));

	while (midiMessage && midiMessage->time <= currentPlaybackTime) {
		auto channel = midiMessage->channel;

		switch (midiMessage->type) {
		case TML_NOTE_OFF:
			tsf_channel_note_off(midi, channel, midiMessage->key);
			break;

		case TML_NOTE_ON:
			tsf_channel_note_on(midi, channel, midiMessage->key, midiMessage->velocity / 127.0f);
			break;

		case TML_KEY_PRESSURE:
			mprintf((0, "Key pressure events not supported\n"));
			break;

		case TML_CONTROL_CHANGE:
			tsf_channel_midi_control(midi, channel, midiMessage->control, midiMessage->control_value);
			break;

		case TML_PROGRAM_CHANGE:
			tsf_channel_set_presetnumber(midi, channel, midiMessage->program, (channel == 9));
			break;

		case TML_CHANNEL_PRESSURE:
			tsf_channel_set_volume(midi, channel, midiMessage->channel_pressure / 127.0f);
			break;

		case TML_PITCH_BEND:
			tsf_channel_set_pitchwheel(midi, channel, midiMessage->pitch_bend);
			break;

		case TML_SET_TEMPO: //ignore, TML handles these
			break;
			
		default:
			mprintf((1, "Unrecognized MIDI message %d on channel %d\n", midiMessage->type, channel));
			//return -1;
		}

		midiMessage = midiMessage->next;
	}

	tsf_render_short(midi, (short*)buffer, sampleCount);
	return bufferSize;

}

bool MIDILoader::Rewind() {
	currentPlaybackTime = 0;
	midiMessage = firstMidiMessage;
	time = clock.now();

	return true;
}

void MIDILoader::Close() {

	if (firstMidiMessage) {
		tml_free(firstMidiMessage);
		firstMidiMessage = nullptr;
	}

	if (midi) {
		tsf_close(midi);
		midi = nullptr;
	}
	
}

#endif

