/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License.
*/

#ifndef OGG_H
#define OGG_H

#define OGG_SUPPORTED 1

#include <string>

#include "hqaudio/sound_loader.h"
#include "lib/stb_vorbis.h"

struct OGGLoader : SoundLoader {
	virtual bool Open();
	virtual bool OpenMemory(void* memory, size_t len, bool autoFree);
	virtual size_t GetSamples(void* buffer, size_t bufferSize);
	virtual bool Rewind();
	virtual void Close();

	OGGLoader(const std::string& filename);

	stb_vorbis* ogg;
};

#endif