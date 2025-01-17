/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License,
as described in copying.txt.
*/

// [DW] As of the Redbook audio unification, this now represents the legacy MIDI system.
// Unlike the current Redbook setup, where the audio engine requests samples as needed,
// the legacy system gives the audio system samples on its own terms.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "tsf_midi.h"

#include "platform/s_midi.h"
#include "platform/mono.h"
#include "misc/error.h"

#ifdef USE_TSFMIDI

TSFMidiSynth::TSFMidiSynth() { }

TSFMidiSynth::~TSFMidiSynth() {
	if (engine) {
		tsf_close(engine);
		engine = nullptr;
	}
}

void TSFMidiSynth::SetSampleRate(uint32_t newSampleRate)
{
	sampleRate = newSampleRate;
	if (engine)
		tsf_set_output(engine, TSF_STEREO_INTERLEAVED, newSampleRate);
}

void TSFMidiSynth::CreateSynth()
{
	if (!engine)
		return;
	
	tsf_set_output(engine, TSF_STEREO_INTERLEAVED, sampleRate);
}

void TSFMidiSynth::Shutdown()
{
	
}

void TSFMidiSynth::SetSoundfont(const char* filename)
{
	if (engine)
		tsf_close(engine);

	engine = tsf_load_filename(filename);
	if (!engine)
		Warning("Could not load soundfont %s", filename);
}

void TSFMidiSynth::RenderMIDI(int numSamples, short* buffer)
{
	if (!engine)
		return;
	
	tsf_render_short(engine, buffer, numSamples / 2);

	/*int check = 0;
	for (int i = 0; i < sampleCount; i++) {
		if (buffer[i] != 0)
			check++;
	}
	if (check > 0)
		mprintf((0, "Found %d/%d nonzero samples when rendering midi\n", check, sampleCount));*/
}

//I_MidiEvent(chunk->events[chunk->nextEvent]);
void TSFMidiSynth::DoMidiEvent(midievent_t *ev)
{
	if (!engine)
		return;

	//printf("event %d channel %d param 1 %d param 2 %d\n", ev->type, ev->channel, ev->param1, ev->param2);
	int channel = ev->GetChannel();
	switch (ev->GetType())
	{
	case EVENT_NOTEON:
		tsf_channel_note_on(engine, channel, ev->param1, ev->param2 / 127.0f);
		break;
	case EVENT_NOTEOFF:
		tsf_channel_note_off(engine, channel, ev->param1);
		break;
	case EVENT_AFTERTOUCH:
		//tsf_channel_set_volume(engine, channel, ev->param1, ev->param2);
		//tsf_channel_set_volume(engine, channel, ev->param2);
		mprintf((1, "Aftertouch event not supported\n"));
		break;
	case EVENT_PRESSURE:
		tsf_channel_set_volume(engine, channel, ev->param1 / 127.0f);
		break;
	case EVENT_PITCH:
		tsf_channel_set_pitchwheel(engine, channel, ev->param1 + (ev->param2 << 7));
		break;
	case EVENT_PATCH:
		tsf_channel_set_presetnumber(engine, channel, ev->param1, (channel == 9));
		break;
	case EVENT_CONTROLLER:
		tsf_channel_midi_control(engine, channel, ev->param1, ev->param2);
		break;
		//[ISB] TODO: Sysex
	}
}

void TSFMidiSynth::StopSound()
{
	if (!engine)
		return;

	tsf_note_off_all(engine);

	//SetDefaults();
	/*for (int chan = 0; chan < 16; chan++)
	{
		fluid_synth_cc(FluidSynth, chan, 123, 0);
	}*/
}

void TSFMidiSynth::SetDefaults()
{
	if (!engine)
		return;

	tsf_reset(engine);

	//fluid_synth_system_reset(FluidSynth);
	for (int chan = 0; chan < 16; chan++)
	{
		tsf_channel_set_bank_preset(engine, chan, 0, 0);

		//tsf_channel_midi_control(engine, chan, 7, 0); //volume. Set to 0 by default in HMI
		//tsf_channel_midi_control(engine, chan, 39, 64); //fine volume.
		tsf_channel_midi_control(engine, chan, 1, 0); //modulation wheel
		tsf_channel_midi_control(engine, chan, 11, 127); //expression
		tsf_channel_midi_control(engine, chan, 64, 0); //pedals
		tsf_channel_midi_control(engine, chan, 65, 0);
		tsf_channel_midi_control(engine, chan, 66, 0);
		tsf_channel_midi_control(engine, chan, 67, 0);
		tsf_channel_midi_control(engine, chan, 68, 0);
		tsf_channel_midi_control(engine, chan, 69, 0);
		//tsf_channel_midi_control(engine, chan, 42, 64); //pan
		tsf_channel_set_pan(engine, chan, 0.5f);

		tsf_channel_set_volume(engine, chan, 0);
		//tsf_channel_set_pitchwheel(engine, chan, 0x2000);
		
		//for (int key = 0; key < 128; key++) //this isn't going to cause problems with the polyphony limit is it...
		//	fluid_synth_key_pressure(FluidSynth, chan, key, 0);

		//tsf_channel_midi_control(engine, chan, 121, 0); //send all notes off TODO: This can be adjusted in HMI. 
		//tsf_channel_note_off_all(engine, chan);
	}
	tsf_channel_set_bank_preset(engine, 9, 128, 0);
}

void TSFMidiSynth::PerformBranchResets(BranchEntry* entry, int chan)
{
	if (!engine)
		return;

	int i;
	//Attempting to set a default bank, since occasionally there are undefined ones. If a different bank is needed, it should hopefully be set in the CC messages. 
	//fluid_synth_bank_select(FluidSynth, chan, 0);
	//fluid_synth_program_change(FluidSynth, chan, entry->program);
	for (i = 0; i < entry->controlChangeCount; i++)
	{
		tsf_channel_midi_control(engine, chan, entry->controlChanges[i].controller, entry->controlChanges[i].state);
	}
}

void TSFMidiSynth::SetVolume(int volume)
{
	//I had to revert this, since the MIDI softsynth playback code is also streaming emulated CD music now.
	//Otherwise it is impossible to adjust the volume of CD music emulation. 
	//fluid_settings_setnum(FluidSynthSettings, "synth.gain", 0.5 * volume / 127);
	// [DW] Leaving this unimplemented
}

//Implement here so internal functions aren't exposed
#define TSF_IMPLEMENTATION
#include "tsf.h"

#endif
