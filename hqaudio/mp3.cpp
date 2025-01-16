/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License.
*/

#include "mp3.h"

#define MINIMP3_IMPLEMENTATION
#include "lib/minimp3.h"
#include "lib/minimp3_ex.h"

#include "platform/mono.h"

MP3Loader::MP3Loader(const std::string& filename) : SoundLoader(filename) {}

bool MP3Loader::Open() {
	
	if (!mp3Initialized) {
		mp3dec_init(&engine);
	}

	if (mp3dec_ex_open(&mp3, filename.c_str(), MP3D_SEEK_TO_SAMPLE) != 0) {
		mprintf((1, "Error loading MP3 file\n"));
		return false;
	}

	properties.channels = mp3.info.channels;
	properties.sampleRate = mp3.info.hz;
	properties.format = SF_SHORT;

	return true;

}

bool MP3Loader::OpenMemory(void* memory, size_t len, bool autoFree) {
	SoundLoader::OpenMemory(memory, len, autoFree);

	if (!mp3Initialized) {
		mp3dec_init(&engine);
	}

	if (mp3dec_ex_open_buf(&mp3, (uint8_t*)memory, len, MP3D_SEEK_TO_SAMPLE) != 0) {
		mprintf((1, "Error loading MP3 file\n"));
		return false;
	}

	properties.channels = mp3.info.channels;
	properties.sampleRate = mp3.info.hz;
	properties.format = SF_SHORT;

	return true;
}

size_t MP3Loader::GetSamples(void* buffer, size_t bufferSize) {
	
	auto samples = bufferSize / sizeof(mp3d_sample_t);
	auto count = mp3dec_ex_read(&mp3, reinterpret_cast<mp3d_sample_t*>(buffer), samples);

	if (count != samples && mp3.last_error) {
		mprintf((1, "Error decoding MP3\n"));
		return -1;
	}
	
	return count * sizeof(mp3d_sample_t);

}

bool MP3Loader::Rewind() {

	if (mp3dec_ex_seek(&mp3, 0)) {
		mprintf((1, "Error rewinding MP3\n"));
		return false;
	}

	return true;

}

void MP3Loader::Close() {
	mp3dec_ex_close(&mp3);
}