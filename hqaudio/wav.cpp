/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License.
*/

#include "wav.h"

#ifdef USE_SDL
#include <SDL_audio.h>


WAVLoader::WAVLoader(std::string& filename) : SoundLoader(filename) {

}

bool WAVLoader::Open() {
	return false;
}

size_t WAVLoader::GetSamples(void* buffer, size_t bufferSize) {
	return -1;
}

bool WAVLoader::Rewind() {
	return false;
}

void WAVLoader::Close() {

}

#endif