/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License.
*/

#ifndef MP3_H
#define MP3_H

#define MP3_SUPPORTED 1

#include <string>

#include "hqaudio/sound_loader.h"
#include "lib/minimp3.h"
#include "lib/minimp3_ex.h"

inline bool mp3Initialized = false;
inline mp3dec_t engine;

struct MP3Loader : SoundLoader {
	virtual bool Open();
	virtual bool OpenMemory(void* memory, size_t len, bool autoFree);
	virtual size_t GetSamples(void* buffer, size_t bufferSize);
	virtual bool Rewind();
	virtual void Close();

	MP3Loader(const std::string& filename);

	mp3dec_ex_t mp3;

};

#endif