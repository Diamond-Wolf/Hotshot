/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License.
*/

#include "hqaudio/sound_loader.h"

#include <string>

#ifndef WAV_H
#define WAV_H

#ifdef USE_SDL
# define WAV_SUPPORTED 1

struct WAVLoader : SoundLoader {
	virtual bool Open();
	virtual size_t GetSamples(void* buffer, size_t bufferSize);
	virtual bool Rewind();
	virtual void Close();

	WAVLoader(const std::string& filename);

	uint8_t* soundBuffer;
	uint32_t soundBufferSize;
	size_t bufferHead;
};

#else
# define WAV_SUPPORTED 0
#endif

#endif