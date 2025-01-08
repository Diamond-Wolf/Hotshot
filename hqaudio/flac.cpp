/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License.
*/

#include "flac.h"

#define DR_FLAC_IMPLEMENTATION
#include "lib/dr_flac.h"


FLACLoader::FLACLoader(std::string& filename) : SoundLoader(filename) {

}

bool FLACLoader::Open() {
	return false;
}

size_t FLACLoader::GetSamples(void* buffer, size_t bufferSize) {
	return -1;
}

bool FLACLoader::Rewind() {
	return false;
}

void FLACLoader::Close() {

}