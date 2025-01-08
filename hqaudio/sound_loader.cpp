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

SoundLoader* RequestSoundLoader(std::string filename) {

	fs::path path = filename;
	std::string ext = path.extension().string();
	for (auto& c : ext)
		c = tolower(c);

	SoundLoader* loader;

#if FLAC_SUPPORTED
	if (ext == ".flac") {
		return new FLACLoader(filename);
	}
#endif

#if MP3_SUPPORTED
	if (ext == ".mp3") {
		return new MP3Loader(filename);
	}
#endif

#if OGG_SUPPORTED
	if (ext == ".ogg") {
		return new OGGLoader(filename);
	}
#endif

#if WAV_SUPPORTED
	if (ext == ".wav") {
		return new WAVLoader(filename);
	}
#endif

	return nullptr;

}

SoundLoader::SoundLoader(std::string& filename) : filename(filename) {};

SoundLoader::~SoundLoader() {}