/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License.
*/

#include "hmp.h"

#include <fstream>

#include "platform/mono.h"

namespace chr = std::chrono;
using std::ios;

typedef chr::nanoseconds timediff_t;

const auto ONE_SECOND = chr::duration_cast<timediff_t>(chr::seconds(1)).count();
const auto TICK_TIME = ONE_SECOND / MIDI_FREQUENCY;

HMPLoader::HMPLoader(const std::string& filename) : SoundLoader(filename) {}

bool HMPLoader::Open() {
	
	std::ifstream file(filename, ios::in | ios::ate | ios::binary);
	if (!file) {
		mprintf((1, "Could not open HMP file"));
		return false;
	}

	auto size = file.tellg();
	auto buffer = new char[size];

	file.seekg(0, ios::beg);
	file.read(buffer, size);

	return OpenMemory(buffer, size, true);

}

bool HMPLoader::OpenMemory(void* memory, size_t len, bool autoFree) {
	SoundLoader::OpenMemory(memory, len, autoFree);

	midi = tsf_load_filename(SoundFontFilename);
	if (!midi) {
		mprintf((1, "Could not create midi synth! Is your soundfont valid?"));
		return false;
	}
	tsf_set_output(midi, TSF_STEREO_INTERLEAVED, MIDI_SAMPLERATE);
	
	hmp = new HMPFile(len, (uint8_t*)memory);

	properties.sampleRate = MIDI_SAMPLERATE;
	properties.channels = 2;
	properties.format = SF_SHORT;

	Reset();
	Rewind();

	time = clock.now();
	
	return true;

}

bool HMPLoader::CheckDone() {
	bool remaining = false;

	for (int i = 0; i < hmp->NumTracks(); i++) {
		remaining |= hmp->GetTrack(i)->CheckRemainingEvents();
	}

	return !remaining;
}

void HMPLoader::ResetBranch(BranchEntry* branch, int channel) {
	for (int i = 0; i < branch->controlChangeCount; i++)
		tsf_channel_midi_control(midi, channel, branch->controlChanges[i].controller, branch->controlChanges[i].state);
}

void HMPLoader::Reset() {
	tsf_reset(midi);

	//fluid_synth_system_reset(FluidSynth);
	for (int chan = 0; chan < 16; chan++)
	{
		tsf_channel_set_bank_preset(midi, chan, 0, 0);
		tsf_channel_midi_control(midi, chan, 1, 0); //modulation wheel
		tsf_channel_midi_control(midi, chan, 11, 127); //expression
		tsf_channel_midi_control(midi, chan, 64, 0); //pedals
		tsf_channel_midi_control(midi, chan, 65, 0);
		tsf_channel_midi_control(midi, chan, 66, 0);
		tsf_channel_midi_control(midi, chan, 67, 0);
		tsf_channel_midi_control(midi, chan, 68, 0);
		tsf_channel_midi_control(midi, chan, 69, 0);
		tsf_channel_set_pan(midi, chan, 0.5f);
		tsf_channel_set_volume(midi, chan, 0);
	}
	tsf_channel_set_bank_preset(midi, 9, 128, 0);
}

void HMPLoader::RunEvent(midievent_t* ev) {
	int channel = ev->GetChannel();
	switch (ev->GetType())
	{
	case EVENT_NOTEON:
		tsf_channel_note_on(midi, channel, ev->param1, ev->param2 / 127.0f);
		break;
	case EVENT_NOTEOFF:
		tsf_channel_note_off(midi, channel, ev->param1);
		break;
	case EVENT_AFTERTOUCH:
		mprintf((1, "Aftertouch event not supported\n"));
		break;
	case EVENT_PRESSURE:
		tsf_channel_set_volume(midi, channel, ev->param1 / 127.0f);
		break;
	case EVENT_PITCH:
		tsf_channel_set_pitchwheel(midi, channel, ev->param1 + (ev->param2 << 7));
		break;
	case EVENT_PATCH:
		tsf_channel_set_presetnumber(midi, channel, ev->param1, (channel == 9));
		break;
	case EVENT_CONTROLLER:
		tsf_channel_midi_control(midi, channel, ev->param1, ev->param2);
		break;
		//[ISB] TODO: Sysex
	}
}

void HMPLoader::Tick() {

	int i;
	int branchnum;
	HMPTrack* track;
	BranchEntry* branchData;
	midievent_t* ev;
	int loopNum = -1;
	bool eventsRemaining = false;

	for (i = 0; i < hmp->NumTracks(); i++)
	{
		track = hmp->GetTrack(i);
		track->AdvancePlayhead(1);
		eventsRemaining |= track->CheckRemainingEvents();

		ev = track->NextEvent();
		while (ev != nullptr)
		{
			//handle special controllers
			if (ev->GetType() == EVENT_CONTROLLER)
			{
				switch (ev->param1)
				{
				case HMI_CONTROLLER_GLOBAL_LOOP_END:
					loopNum = ev->param2 & 127;
					ev = nullptr;
					break;
				case HMI_CONTROLLER_GLOBAL_BRANCH:
					printf("gbranch\n");
					ev = nullptr;
					break;
				case HMI_CONTROLLER_LOCAL_BRANCH:
					branchnum = ev->param2 - 128;
					if (branchnum >= 0)
					{
						branchData = track->GetLocalBranchData(branchnum);
						//track->SetPlayhead(song->GetBranchTick(i, branchnum));
						track->BranchToByteOffset(branchData->offset);
						//printf("lbranch %d %d %d\n", i, branchnum, branchData->offset);

						ResetBranch(branchData, ev->GetChannel());

						ev = nullptr;
					}
					break;
				default:
					RunEvent(ev);
					ev = track->NextEvent();
					break;
				}
			}
			else
			{
				RunEvent(ev);
				ev = track->NextEvent();
			}
		}
	}

	//Check loop status
	/*if (loop)
	{
		//Needed for looping songs without explicit loop points.
		if (eventsRemaining == false)
		{
			Rewind();
		}
		else*/ 
	if (loopNum != -1) //Explicit loop.
	{
		//printf("new loop code running, loop %d\n", loopNum);
		for (i = 0; i < hmp->NumTracks(); i++)
		{
			track = hmp->GetTrack(i);
			//track->SetPlayhead(song->GetLoopStart(0));
			if (loopNum + 1 <= track->GetNumBranches())
			{
				branchData = track->GetLocalBranchData(loopNum);
				track->BranchToByteOffset(branchData->offset);
				ResetBranch(branchData, track->GetChannel());
			}
		}
		//}
	}

}

size_t HMPLoader::GetSamples(void* buffer, size_t bufferSize) {
	
	if (CheckDone())
		return 0;

	auto now = clock.now();
	auto diff = chr::duration_cast<timediff_t>(now - time).count();
	time = now;

	//mprintf((0, "|%ull|", diff));

	currentPlaybackTime += diff;
	//auto div = currentPlaybackTime / TICK_TIME;

	//mprintf((0, "D/CPT: %llu / %llu\n", div, currentPlaybackTime));

	while (currentPlaybackTime > TICK_TIME) {
		currentPlaybackTime -= TICK_TIME;
		//mprintf((0, "Tick!"));
		Tick();
	}

	auto sampleCount = bufferSize / (2 * sizeof(int16_t));

	tsf_render_short(midi, (short*)buffer, sampleCount);
	return bufferSize;

}

bool HMPLoader::Rewind() {

	for (int i = 0; i < hmp->NumTracks(); i++)
		hmp->GetTrack(i)->StartSequence();

	time = clock.now();

	/*HMPTrack* track;
	int i;
	
	if (!CheckDone()) {
		for (i = 0; i < hmp->NumTracks(); i++)
			hmp->GetTrack(i)->StartSequence();
	} else if (loopNum != -1) {//Explicit loop.
		//printf("new loop code running, loop %d\n", loopNum);
		for (i = 0; i < song->NumTracks(); i++)
		{
			track = song->GetTrack(i);
			//track->SetPlayhead(song->GetLoopStart(0));
			if (loopNum + 1 <= track->GetNumBranches())
			{
				branchData = track->GetLocalBranchData(loopNum);
				track->BranchToByteOffset(branchData->offset);
				synth->PerformBranchResets(branchData, track->GetChannel());
			}
		}
	}*/

	currentPlaybackTime = 0;
	
	return true;
}

void HMPLoader::Close() {

	if (hmp) {
		delete hmp;
	}

	if (midi) {
		tsf_close(midi);
		midi = nullptr;
	}
	
}