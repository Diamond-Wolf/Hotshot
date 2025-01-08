/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License.
*/

#define FLAC_SUPPORTED 1

#include <string>

#include "hqaudio/sound_loader.h"

struct FLACLoader : SoundLoader {
	virtual bool Open();
	virtual size_t GetSamples(void* buffer, size_t bufferSize);
	virtual bool Rewind();
	virtual void Close();

	FLACLoader(std::string& filename);
};