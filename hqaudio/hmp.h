/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License.
*/

#ifndef HMP_H
#define HMP_H

#include "sound_loader.h"

#include <string>

#ifdef USE_TSFMIDI
#define HMP_SUPPORTED 1

#include <chrono>

#include "platform/s_midi.h"
#include "platform/tsfmidi/tsf.h"

struct HMPLoader : SoundLoader {
	virtual bool Open();
	virtual bool OpenMemory(void* memory, size_t len, bool autoFree);
	virtual size_t GetSamples(void* buffer, size_t bufferSize);
	virtual bool Rewind();
	virtual void Close();

	void BranchRewind();
	bool CheckDone();
	void Reset();
	void ResetBranch(BranchEntry* branch, int channel);
	void Tick();
	void RunEvent(midievent_t* ev);

	HMPLoader(const std::string& filename);
	
	HMPFile* hmp = nullptr;
	tsf* midi = nullptr;

	unsigned long long currentPlaybackTime = 0;
	std::chrono::steady_clock clock;
	std::chrono::time_point<std::chrono::steady_clock> time;

};

#endif

#else
# define MIDI_SUPPORTED 0
#endif