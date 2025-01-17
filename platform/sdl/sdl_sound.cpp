/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License.
*/

#include <array>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <thread>
#include <mutex>

#ifdef USE_SDL

#include <SDL_audio.h>

#include "platform/i_sound.h"
#include "platform/s_midi.h"
#include "platform/s_sequencer.h"
#include "misc/error.h"
#include "platform/mono.h"
#include "misc/args.h"

#include "fix/fix.h"

#define SAMPLE_RATE_11K		11025
#define SAMPLE_RATE_22K		22050

#define STREAM_GET_CALLBACK() [](void* userdata, SDL_AudioStream* audioStream, int additionalAmount, int totalAmount)

SDL_AudioDeviceID musicDevice = 0;
SDL_AudioDeviceID sfxDevice = 0;

SDL_AudioStream* musicStream = NULL;
SDL_AudioStream* movieStream = NULL;

bool soundInitialized = false;

bool LoopMusic;

std::mutex MIDIMutex;
std::thread MIDIThread;

struct SDLMusicSource {
	bool playing;
	bool active = true;
};

struct SDLSound {

#ifndef NDEBUG
	int id;
#endif

    SDL_AudioStream* inputStream = NULL;
	SDL_AudioStream* outputStream = NULL;
    
    unsigned char* rawData = NULL;
    int dataLen;

	float xang;
	float yang;

	bool free;
	bool loop;
	bool initialized;

};

std::array<SDLSound, _MAX_VOICES> soundPool;

bool HQMusicPlaying = false;

int MusicVolume;

MidiPlayer* midiPlayer;

void LockSDLStreams(SDLSound& sound) {
	SDL_LockAudioStream(sound.outputStream);
	SDL_LockAudioStream(sound.inputStream);
}

void UnlockSDLStreams(SDLSound& sound) {
	SDL_UnlockAudioStream(sound.inputStream);
	SDL_UnlockAudioStream(sound.outputStream);
}

const SDL_AudioSpec sfxSpec = { SDL_AUDIO_U8, 1, SAMPLE_RATE_22K };
/*const*/ SDL_AudioSpec intermediateSpec = { SDL_AUDIO_S32, 2, SAMPLE_RATE_22K }; // [DW] SDL3 bug doesn't properly convert intermediate streams, so don't const so it can change
const SDL_AudioSpec musicSpec = { SDL_AUDIO_S16, 2, MIDI_SAMPLERATE };
SDL_AudioSpec outputSpec;

typedef int32_t intermediate_data_t;

//uint8_t silence[4096];

int plat_init_audio() {

    SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &outputSpec, NULL);
	outputSpec.channels = 2;
    mprintf((0, "Audio output spec: format %d, %d channels, %dkhz\n", outputSpec.format, outputSpec.channels, outputSpec.freq));

	intermediateSpec.freq = outputSpec.freq; // [DW] SDL3 bug doesn't properly convert intermediate streams
	if (outputSpec.format == SDL_AUDIO_S16)
		intermediateSpec.freq /= 2;

	mprintf((0, "Corrected intermediate spec: format %d, %d channels, %dkhz\n", intermediateSpec.format, intermediateSpec.channels, intermediateSpec.freq));

    musicDevice = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &outputSpec);
	if (!musicDevice) {
		Warning("Could not initialize audio music device! %s", SDL_GetError());
		return 1;
	}

	musicStream = SDL_CreateAudioStream(&musicSpec, &outputSpec);
	if (!musicStream) {
		Warning("Could not create music stream! %s", SDL_GetError());
		SDL_CloseAudioDevice(musicDevice);
		musicDevice = NULL;
		return 1;
	}
	
	movieStream = SDL_CreateAudioStream(&musicSpec, &outputSpec);
	if (!movieStream) {
		Warning("Could not create movie stream! %s", SDL_GetError());
		SDL_DestroyAudioStream(musicStream);
		SDL_CloseAudioDevice(musicDevice);
		musicStream = NULL;
		musicDevice = NULL;
		return 1;
	}

	SDL_AudioStream* musicStreams[] = { musicStream, movieStream };

	if (!SDL_BindAudioStreams(musicDevice, musicStreams, 2)) {
		Warning("Could not bind music audio streams! %s", SDL_GetError());

		SDL_DestroyAudioStream(musicStream);
		SDL_DestroyAudioStream(movieStream);
		SDL_CloseAudioDevice(musicDevice);

		musicStream = NULL;
		movieStream = NULL;
		musicDevice = NULL;

		return 1;
	}

	sfxDevice = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &outputSpec);
	if (!sfxDevice) {
		Warning("Could not initialize audio SFX device! %s", SDL_GetError());

		SDL_DestroyAudioStream(musicStream);
		SDL_DestroyAudioStream(movieStream);
		SDL_CloseAudioDevice(musicDevice);

		musicStream = NULL;
		movieStream = NULL;
		musicDevice = NULL;

		return 1;
	}

    //auto silentValue = SDL_GetSilenceValueForFormat(sfxSpec.format);
    //memset(silence, silentValue, 4096);

#ifndef NDEBUG
	int id = 0;
#endif

    for (auto& sound : soundPool) {

#ifndef NDEBUG
		sound.id = id;
		id++;
#endif

        sound.inputStream = SDL_CreateAudioStream(&sfxSpec, &intermediateSpec);
		if (!sound.inputStream) {
			mprintf((1, "Could not create input stream! %s", SDL_GetError()));
			continue;
		}
		
		sound.outputStream = SDL_CreateAudioStream(&intermediateSpec, &outputSpec);
		if (!sound.outputStream) {
			mprintf((1, "Could not create output stream! %s", SDL_GetError()));
			SDL_DestroyAudioStream(sound.inputStream);
			sound.inputStream = NULL;
			continue;
		}

        sound.free = true;
		sound.initialized = false;

		SDL_SetAudioStreamGetCallback(sound.inputStream,
			STREAM_GET_CALLBACK() {
				auto sound = reinterpret_cast<SDLSound*>(userdata);

				auto available = SDL_GetAudioStreamAvailable(audioStream);
				if (available < 0) {
					mprintf((1, "Error getting available audio stream data for mix: %s", SDL_GetError()));
					Int3();
				}

				if (totalAmount >= available) {

					if (sound->loop) {
						SDL_PutAudioStreamData(audioStream, sound->rawData, sound->dataLen);
					}
					else if (available == 0) {
						if (!sound->free && sound->initialized) {
#ifndef NDEBUG
							//mprintf((0, "Freeing sound %d\n", sound->id));
#endif
							sound->free = true;
							sound->loop = false;
							sound->initialized = false;
							delete[] sound->rawData;
							sound->rawData = NULL;
						}
						//SDL_PutAudioStreamData(audioStream, silence, (totalAmount < 4096 ? totalAmount : 4096));
					}

				}
			},
			&sound);

		SDL_SetAudioStreamGetCallback(sound.outputStream,
			STREAM_GET_CALLBACK() {
				auto sound = reinterpret_cast<SDLSound*>(userdata);

				if (sound->free)
					return;

				intermediate_data_t* read = new intermediate_data_t[(additionalAmount + 1) / sizeof(intermediate_data_t)];

				SDL_LockAudioStream(sound->inputStream);

				int bytes = SDL_GetAudioStreamData(sound->inputStream, read, additionalAmount);
				if (bytes < 0)
					Warning("Error getting stream data! %s", SDL_GetError());

				Assert(bytes <= additionalAmount);

				SDL_UnlockAudioStream(sound->inputStream);

				for (int i = 0; i < bytes / sizeof(intermediate_data_t); i += 2) {
					auto right = read[i];
					auto left = read[i + 1];

					auto pan = (sound->xang + 1) / 2;
					//auto scale = abs(sound->yang);


					right = (intermediate_data_t)(right * sqrtf(pan));
					left = (intermediate_data_t)(left * sqrtf(1 - pan));
					read[i] = right;
					read[i + 1] = left;
				}

				SDL_PutAudioStreamData(audioStream, read, bytes);

				delete[] read;

			},
			&sound);

		SDL_BindAudioStream(sfxDevice, sound.outputStream);

    }

	soundInitialized = true;

	return 0;

}

void plat_close_audio() {

	soundInitialized = false;

	if (musicStream) {
		SDL_DestroyAudioStream(musicStream);
		musicStream = NULL;
	}

	if (movieStream) {
		SDL_DestroyAudioStream(movieStream);
		movieStream = NULL;
	}
	
    for (auto& sound : soundPool) {

		if (sound.outputStream) {
			SDL_DestroyAudioStream(sound.outputStream);
			sound.outputStream = NULL;
		}

		if (sound.inputStream) {
			SDL_DestroyAudioStream(sound.inputStream);
			sound.inputStream = NULL;
		}

		if (sound.rawData) {
			delete[] sound.rawData;
			sound.rawData = NULL;
		}

    }

	SDL_CloseAudioDevice(musicDevice);
    SDL_CloseAudioDevice(sfxDevice);

}

int plat_get_new_sound_handle() {

	if (!soundInitialized)
		return _ERR_NO_SLOTS;

    for (int i = 0; i < _MAX_VOICES; i++) {
		
		LockSDLStreams(soundPool[i]);
        
		if (soundPool[i].free && soundPool[i].outputStream) {

#ifndef NDEBUG
			//mprintf((0, "Allocating sound %d\n", soundPool[i].id));
#endif

            soundPool[i].free = false;
			soundPool[i].initialized = false;
            SDL_ClearAudioStream(soundPool[i].inputStream);
			
			UnlockSDLStreams(soundPool[i]);
            return i;

        }
		
		UnlockSDLStreams(soundPool[i]);
    
	}

    return _ERR_NO_SLOTS;
}

void plat_set_sound_data(int handle, unsigned char* data, int length, int sampleRate) {
	if (handle >= _ERR_NO_SLOTS || !soundInitialized)
		return;

    auto& sound = soundPool[handle];
    SDL_AudioSpec newFormat = { SDL_AUDIO_U8, 1, sampleRate };
    SDL_SetAudioStreamFormat(sound.inputStream, &newFormat, &outputSpec);

    sound.dataLen = length;
    sound.rawData = new uint8_t[length];
    memcpy(sound.rawData, data, length);

	LockSDLStreams(sound);
	
	SDL_PutAudioStreamData(sound.inputStream, data, length);
	SDL_FlushAudioStream(sound.inputStream);
	
	sound.initialized = true;

	UnlockSDLStreams(sound);
}

void plat_set_sound_position(int handle, int volume, int angle) {
	if (handle >= _ERR_NO_SLOTS || !soundInitialized)
		return;

    plat_set_sound_volume(handle, volume);
    plat_set_sound_angle(handle, angle);
}

void plat_set_sound_angle(int handle, int angle) {
	if (handle >= _ERR_NO_SLOTS || !soundInitialized)
		return;

	auto& sound = soundPool[handle];

	float flang = f2fl(angle) * (3.1415927f);

	sound.xang = (float)cos(flang);
	sound.yang = (float)sin(flang);
}

void plat_set_sound_volume(int handle, int volume) {
	if (handle >= _ERR_NO_SLOTS || !soundInitialized)
		return;

	SDL_SetAudioStreamGain(soundPool[handle].inputStream, volume / 32768.0f);
}

void plat_set_sound_loop_points(int handle, int start, int end) {}

void plat_start_sound(int handle, int loop) {
	if (handle >= _ERR_NO_SLOTS || !soundInitialized)
		return;

    soundPool[handle].loop = loop;
}

void plat_stop_sound(int handle) {
	if (handle >= _ERR_NO_SLOTS || !soundInitialized)
		return;

    auto& sound = soundPool[handle];

	LockSDLStreams(sound);

    sound.loop = false;
    sound.free = true;
    SDL_ClearAudioStream(sound.inputStream);
    delete[] sound.rawData;
	sound.rawData = NULL;

	UnlockSDLStreams(sound);
}

int plat_check_if_sound_playing(int handle) { 
	if (handle >= _ERR_NO_SLOTS || !soundInitialized)
		return false;

    return !soundPool[handle].free;
}

int plat_check_if_sound_finished(int handle) {
	if (handle >= _ERR_NO_SLOTS || !soundInitialized)
		return false;

    return !plat_check_if_sound_playing(handle);
}

int plat_start_midi(MidiSequencer *sequencer) { return 0; }

uint32_t plat_get_preferred_midi_sample_rate() {
    return MIDI_SAMPLERATE;
}

void plat_close_midi() {}

void plat_set_music_volume(int volume) {
	if (!soundInitialized)
		return;

	SDL_SetAudioDeviceGain(musicDevice, volume / 127.0f);
}

void plat_start_midi_song(HMPFile* song, bool loop) {}

void plat_stop_midi_song() {
	if (!soundInitialized)
		return;
	
	SDL_ClearAudioStream(musicStream);
}

bool midiStopped = true;

void midi_set_music_samplerate(void* opaquesource, uint32_t samplerate) {
	if (!soundInitialized)
		return;
	
	SDL_AudioSpec midiSpec = { SDL_AUDIO_S16, 2, (int)samplerate };
	SDL_SetAudioStreamFormat(musicStream, &midiSpec, &outputSpec);
}

bool midi_queue_slots_available(void* opaquesource) { return false; }
void midi_dequeue_midi_buffers(void* opaquesource) {}

void midi_queue_buffer(void* opaquesource, int numSamples, int16_t* data) {
	if (midiStopped || !soundInitialized)
		return;

	SDL_PutAudioStreamData(musicStream, data, numSamples * sizeof(*data));
}

void* midi_start_source() {
	if (!soundInitialized)
		return NULL;
	
	auto source = new SDLMusicSource();
	SDL_SetAudioStreamGetCallback(
		musicStream,
		STREAM_GET_CALLBACK() {
			auto source = reinterpret_cast<SDLMusicSource*>(userdata);
			
			auto available = SDL_GetAudioStreamAvailable(audioStream);
			if (available < 0) {
				mprintf((1, "Error getting available audio stream data for midi: %s", SDL_GetError()));
				Int3();
			}

			auto queued = SDL_GetAudioStreamQueued(audioStream);
			if (queued < 0) {
				mprintf((1, "Error getting available queued stream data for midi: %s", SDL_GetError()));
				Int3();
			}

			source->playing = !(available == 0 && queued == 0);
		}
		, source);

	midiStopped = false;

	return source;
}

void midi_stop_source(void* opaquesource) {
	if (!soundInitialized || !musicStream)
		return;
	
	auto source = reinterpret_cast<SDLMusicSource*>(opaquesource);

	midiStopped = true;

	SDL_LockAudioStream(musicStream);

	SDL_SetAudioStreamGetCallback(musicStream, NULL, NULL);
	SDL_ClearAudioStream(musicStream);

	delete source;

	SDL_UnlockAudioStream(musicStream);
}

void midi_check_status(void* opaquesource) {}

bool midi_check_finished(void* opaquesource) {
	if (!soundInitialized)
		return false;
	
	auto source = reinterpret_cast<SDLMusicSource*>(opaquesource);
	return !source->playing;
}

struct HQMusicSource {
	SoundLoader* loader = nullptr;
	bool loop = false;
	bool playing = false;
} activeHQSource;

void plat_start_hq_song(SoundLoader* loader, bool loop) {
	if (!soundInitialized)
		return;
	
	auto& properties = loader->properties;
	SDL_AudioSpec musicSpec = { SDL_AUDIO_UNKNOWN, properties.channels, properties.sampleRate };

	switch (properties.format) {

		case SF_BYTE:
			musicSpec.format = SDL_AUDIO_S8;
		break;
		
		case SF_UBYTE:
			musicSpec.format = SDL_AUDIO_U8;
		break;
		
		case SF_SHORT:
			musicSpec.format = SDL_AUDIO_S16;
		break;
		
		case SF_LONG:
			musicSpec.format = SDL_AUDIO_S32;
		break;

		case SF_FLOAT:
			musicSpec.format = SDL_AUDIO_F32;
		break;
			
	}

	SDL_SetAudioStreamFormat(musicStream, &musicSpec, &outputSpec);

	activeHQSource.loader = loader;
	activeHQSource.loop = loop;
	
	SDL_SetAudioStreamGetCallback(musicStream,
		STREAM_GET_CALLBACK() {
			auto source = reinterpret_cast<HQMusicSource*>(userdata);
			auto& loader = source->loader;
			
			char* buffer = new char[additionalAmount];
			auto read = loader->GetSamples(buffer, additionalAmount);

			if (read < 0) {
				mprintf((1, "Error in music stream callback\n"));
				source->playing = false;
			} else if (read == 0 && !source->loop) {
				mprintf((1, "HQ audio couldn't get data!"));
				source->playing = false;
			} else {
				if (read > 0)
					SDL_PutAudioStreamData(audioStream, buffer, read);
				
				if (source->loop && read < additionalAmount) {

					if (!loader->Rewind()) {
						mprintf((1, "Error rewinding in music stream callback"));
						source->playing = false;
					} else {

						auto remaining = additionalAmount - read;
						remaining = loader->GetSamples(buffer + read, remaining);
						if (remaining < 0)
							mprintf((1, "Error in music stream callback after rewind\n"));
						else if (remaining > 0)
							SDL_PutAudioStreamData(audioStream, buffer + read, remaining);

					}

				}
			}

			delete[] buffer;
		}
		, &activeHQSource);

	activeHQSource.playing = true;

}

void plat_stop_hq_song() {
	if (!soundInitialized || !musicStream)
		return;
	
	SDL_LockAudioStream(musicStream);

	SDL_SetAudioStreamGetCallback(musicStream, NULL, NULL);
	SDL_ClearAudioStream(musicStream);
	
	activeHQSource.loader = nullptr;
	activeHQSource.loop = false;
	activeHQSource.playing = false;

	SDL_UnlockAudioStream(musicStream);
}

bool plat_is_hq_song_playing() {
	return soundInitialized && activeHQSource.playing;
}

void mvesnd_init_audio(SampleFormat format, int samplerate, int channels) {
	if (!soundInitialized)
		return;

	SDL_AudioSpec movieSpec = { SDL_AUDIO_U8, channels, samplerate };
	if (format == SF_SHORT)
		movieSpec.format = SDL_AUDIO_S16;

	SDL_SetAudioStreamFormat(movieStream, &movieSpec, &outputSpec);

}
void mvesnd_queue_audio_buffer(int len, short* data) {
	if (!soundInitialized)
		return;

	SDL_PutAudioStreamData(movieStream, data, len);
}
void mvesnd_close() {
	if (!soundInitialized)
		return;

	SDL_ClearAudioStream(movieStream);

	/*if (!soundInitialized || !movieStream)
		return;

	SDL_DestroyAudioStream(movieStream);
	movieStream = NULL;*/
}

void mvesnd_pause() {
	if (!soundInitialized)
		return;

	SDL_PauseAudioDevice(musicDevice);

}

void mvesnd_resume() {
	if (!soundInitialized)
		return;

	SDL_ResumeAudioDevice(musicDevice);
}

#endif
