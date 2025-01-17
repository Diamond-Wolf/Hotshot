/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License.
*/

#ifndef MIDI_H
#define MIDI_H

#include "hqaudio/sound_loader.h"

#include <string>

#ifdef USE_TSFMIDI
#define MIDI_SUPPORTED 1

#include <chrono>

#include "platform/tsfmidi/tsf.h"
#include "lib/tml.h"

struct MIDILoader : SoundLoader {
	virtual bool Open();
	virtual bool OpenMemory(void* memory, size_t len, bool autoFree);
	virtual size_t GetSamples(void* buffer, size_t bufferSize);
	virtual bool Rewind();
	virtual void Close();

	bool LoadCommon();

	MIDILoader(const std::string& filename);

	tml_message* midiMessage = nullptr;
	tml_message* firstMidiMessage = nullptr;
	tsf* midi = nullptr;

	unsigned int currentPlaybackTime = 0;
	std::chrono::steady_clock clock;
	std::chrono::time_point<std::chrono::steady_clock> time;
	
};

#endif

#else
# define MIDI_SUPPORTED 0
#endif