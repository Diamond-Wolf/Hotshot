/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License.
*/

#include "mp3.h"

#define MINIMP3_IMPLEMENTATION
#include "lib/minimp3.h"


MP3Loader::MP3Loader(std::string& filename) : SoundLoader(filename) {}

bool MP3Loader::Open() {
	return false;
}

size_t MP3Loader::GetSamples(void* buffer, size_t bufferSize) {
	return -1;
}

bool MP3Loader::Rewind() {
	return false;
}

void MP3Loader::Close() {

}