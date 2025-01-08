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
#include "mp3.h"
#include "ogg.h"
#include "wav.h"

#include "platform/posixstub.h"

SoundLoader* RequestSoundLoader(std::string filename) {

	fs::path path = filename;
	std::string ext = path.extension().string();

	SoundLoader* loader;

#if FLAC_SUPPORTED
	if (!_strnicmp(ext.c_str(), ".flac", 5)) {
		return new FLACLoader(filename);
	}
#endif

#if MP3_SUPPORTED
	if (!_strnicmp(ext.c_str(), ".mp3", 4)) {
		return new MP3Loader(filename);
	}
#endif

#if OGG_SUPPORTED
	if (!_strnicmp(ext.c_str(), ".ogg", 4)) {
		return new OGGLoader(filename);
	}
#endif

#if WAV_SUPPORTED
	if (!_strnicmp(ext.c_str(), ".wav", 4)) {
		return new WAVLoader(filename);
	}
#endif

	return nullptr;

}

SoundLoader::SoundLoader(std::string& filename) : filename(filename) {};

SoundLoader::~SoundLoader() {}