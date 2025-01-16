/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License.
*/

#include "wav.h"

#include "platform/mono.h"

#ifdef USE_SDL
# include <SDL_audio.h>

WAVLoader::WAVLoader(const std::string& filename) : SoundLoader(filename) {}

bool WAVLoader::LoadCommon(const SDL_AudioSpec& spec) {

	properties.sampleRate = spec.freq;
	properties.channels = spec.channels;

	switch (spec.format) {

	case SDL_AUDIO_S8:
		properties.format = SF_BYTE;
		break;

	case SDL_AUDIO_U8:
		properties.format = SF_UBYTE;
		break;

	case SDL_AUDIO_S16:
		properties.format = SF_SHORT;
		break;

	case SDL_AUDIO_S32:
		properties.format = SF_LONG;
		break;

	case SDL_AUDIO_F32:
		properties.format = SF_FLOAT;
		break;

	default:
		properties.format = SF_UNSUPPORTED;
		SDL_free(soundBuffer);
		return false;

	}

	return true;

}

bool WAVLoader::Open() {
	
	SDL_AudioSpec spec;

	if (!SDL_LoadWAV(filename.c_str(), &spec, &soundBuffer, &soundBufferSize)) {
		mprintf((1, "Error loading WAV: %s\n", SDL_GetError()));
		return false;
	}

	return LoadCommon(spec);

}

bool WAVLoader::OpenMemory(void* memory, size_t len, bool autoFree) {
	SoundLoader::OpenMemory(memory, len, autoFree);

	SDL_AudioSpec spec;

	auto mem = SDL_IOFromMem(memory, len);
	if (!SDL_LoadWAV_IO(mem, false, &spec, &soundBuffer, &soundBufferSize)) {
		mprintf((1, "Error loading WAV: %s\n", SDL_GetError()));
		return false;
	}
	SDL_CloseIO(mem);

	return LoadCommon(spec);

}

size_t WAVLoader::GetSamples(void* buffer, size_t bufferSize) {
	auto max = bufferHead + bufferSize;
	if (max > soundBufferSize) {
		bufferSize = max - soundBufferSize;
	}

	if (bufferSize > 0) {
		memcpy(buffer, soundBuffer + bufferHead, bufferSize);
		bufferHead += bufferSize;
	}

	return bufferSize;
}

bool WAVLoader::Rewind() {
	bufferHead = 0;
	return true;
}

void WAVLoader::Close() {
	SDL_free(soundBuffer);
}

#endif // USE_SDL