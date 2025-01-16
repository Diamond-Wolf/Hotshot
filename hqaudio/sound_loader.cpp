/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License.
*/

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

#include "sound_loader.h"

#include "flac.h"
#include "hmp.h"
#include "midi.h"
#include "mp3.h"
#include "ogg.h"
#include "wav.h"

#include "cfile/cfile.h"
#include "platform/posixstub.h"

SoundLoader* RequestSoundLoader(const std::string filename) {

	fs::path path = filename;
	std::string ext = path.extension().string();
	const char* extc = ext.c_str();

	SoundLoader* loader;

	if (ext.length() == 4) {

#if MP3_SUPPORTED
		if (!_strnicmp(extc, ".mp3", 4)) {
			return new MP3Loader(filename);
		}
#endif

#if OGG_SUPPORTED
		if (!_strnicmp(extc, ".ogg", 4)) {
			return new OGGLoader(filename);
		}
#endif

#if WAV_SUPPORTED
		if (!_strnicmp(extc, ".wav", 4)) {
			return new WAVLoader(filename);
		}
#endif

	}

#if FLAC_SUPPORTED
	if (strlen(extc) == 5 && !_strnicmp(extc, ".flac", 5)) {
		return new FLACLoader(filename);
	}
#endif

#if MIDI_SUPPORTED
	if (strlen(extc) == 4 && !_strnicmp(extc, ".mid", 4)
	|| strlen(extc) == 5 && !_strnicmp(extc, ".midi", 5)) {
		return new MIDILoader(filename);
	}
#endif

	// [DW] if HMP isn't supported, something is horribly wrong
	if (strlen(extc) == 4 && !_strnicmp(extc, ".hmp", 4)) {
		return new HMPLoader(filename);
	}
	
	return nullptr;

}

bool SoundLoader::OpenMemory(void* memory, size_t len, bool autoFree) {
	this->memory = memory;
	this->memoryLen = len;
	this->autoFree = autoFree;
	return true;
}

SoundLoader::SoundLoader(const std::string& filename) : filename(filename) {};

SoundLoader::~SoundLoader() {
	if (memory && autoFree) {
		delete[] memory;
		memory = nullptr;
		memoryLen = 0;
	}
}