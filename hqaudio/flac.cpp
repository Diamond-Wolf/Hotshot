/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License.
*/

#include "flac.h"

#define DR_FLAC_IMPLEMENTATION
#include "lib/dr_flac.h"

#include "platform/mono.h"


FLACLoader::FLACLoader(const std::string& filename) : SoundLoader(filename) {}

bool FLACLoader::Open() {
	
	flac = drflac_open_file(filename.c_str(), NULL);

	if (!flac) {
		mprintf((1, "Error opening FLAC\n"));
		return false;
	}

	properties.sampleRate = flac->sampleRate;
	properties.channels = flac->channels;
	properties.format = SF_LONG;

	return true;

}

bool FLACLoader::OpenMemory(void* memory, size_t len, bool autoFree) {
	SoundLoader::OpenMemory(memory, len, autoFree);

	flac = drflac_open_memory(memory, len, NULL);

	if (!flac) {
		mprintf((1, "Error opening FLAC\n"));
		return false;
	}

	properties.sampleRate = flac->sampleRate;
	properties.channels = flac->channels;
	properties.format = SF_LONG;

	return true;

}

size_t FLACLoader::GetSamples(void* buffer, size_t bufferSize) {
	auto read = drflac_read_pcm_frames_s32(flac, bufferSize / sizeof(int32_t) / properties.channels, reinterpret_cast<drflac_int32*>(buffer));
	return read * sizeof(int32_t) * properties.channels;
}

bool FLACLoader::Rewind() {
	return drflac_seek_to_pcm_frame(flac, 0);
}

void FLACLoader::Close() {
	drflac_close(flac);
}