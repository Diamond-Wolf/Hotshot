/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License,
as described in copying.txt.
*/

#pragma once

#ifdef USE_TSFMIDI

#include "platform/s_midi.h"
#include "tsf.h"

class TSFMidiSynth : MidiSynth
{
	tsf* engine = nullptr;
	int sampleRate = MIDI_SAMPLERATE;
public:
	TSFMidiSynth();
	~TSFMidiSynth() override;
	void SetSoundfont(const char* filename);
	void SetSampleRate(uint32_t newSampleRate) override;
	void CreateSynth() override;
	int ClassifySynth() override
	{
		return MIDISYNTH_SOFT;
	}
	void DoMidiEvent(midievent_t* ev) override;
	void RenderMIDI(int numTicks, short* buffer) override;
	void StopSound() override;
	void Shutdown() override;
	void SetDefaults() override;
	void PerformBranchResets(BranchEntry* entry, int chan) override;
	void SetVolume(int volume) override;
};

#endif
