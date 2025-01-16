/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License.
*/

#pragma once

#include <mutex>
#include <string>

//Basic HQ music functions

//Plays an OGG file
bool PlayHQSong(const char* filename, bool loop);

//Stops playback of an OGG file
void StopHQSong();

//Redbook music emulation functions

void RBAInit();
bool RBAEnabled();
void RBAStop();
int RBAGetNumberOfTracks();
int RBAGetTrackNum();
int RBAPlayTrack(int track, bool loop);
int RBAPlayTracks(int first, int last);
bool RBAPeekPlayStatus();

//What to do at the end of a song
enum RedbookEndMode {
    REM_CONTINUE,
    REM_LOOP,
};

//How many extra tracks? Just title and credits, or add briefing, endlevel, and endgame
enum RedbookExtraTracksMode {
    RETM_REDBOOK_2,
    RETM_MIDI_5,
};

extern RedbookEndMode rbaEndMode;
extern RedbookExtraTracksMode rbaExtraTracksMode;

inline struct HQAWarning {

	bool show = false;
	std::string message;

	std::mutex m;

	void Put(std::string message) {
		m.lock();
		this->message = message;
		show = true;
		m.unlock();
	}

	std::string Get() {
		std::string message;

		m.lock();
		message = this->message;
		show = false;
		m.unlock();

		return message;
	}

} hqaWarning;