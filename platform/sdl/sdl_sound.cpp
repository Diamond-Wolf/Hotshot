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

//ALuint bufferNames[_MAX_VOICES];
//ALuint sourceNames[_MAX_VOICES];

//MIDI audio fields
//MAX_BUFFERS_QUEUED is currently in terms of buffers of 4 ticks
//The higher this and the amount of ticks in the system, the higher the latency is.
#define MAX_BUFFERS_QUEUED 4

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

/*
int plat_init_audio()
{
	if (FindArg("-nosndlib"))
		return 2;

	int i; 
	ALDevice = alcOpenDevice(NULL);
	if (ALDevice == NULL)
	{
		Warning("plat_init_audio: Cannot open OpenAL device\n");
		return 1;
	}
	ALContext = alcCreateContext(ALDevice, 0);
	if (ALContext == NULL)
	{
		Warning("plat_init_audio: Cannot create OpenAL context\n");
		plat_close_audio();
		return 1;
	}
	alcMakeContextCurrent(ALContext);

	alGenBuffers(_MAX_VOICES, &bufferNames[0]);
	AL_ErrorCheck("Creating buffers");
	alGenSources(_MAX_VOICES, &sourceNames[0]);
	AL_ErrorCheck("Creating sources");
	for (i = 0; i < _MAX_VOICES; i++)
	{
		AL_InitSource(sourceNames[i]);
	}
	
	if (!alIsExtensionPresent("AL_EXT_FLOAT32"))
	{
		mprintf((1, "Warning: OpenAL implementation doesn't support floating point samples for HQ Music.\n"));
	}
	if (!alIsExtensionPresent("AL_SOFT_loop_points"))
	{
		mprintf((1, "Warning: OpenAL implementation doesn't support OpenAL soft loop points.\n"));
	}
	AL_ErrorCheck("Checking exts");

	AL_initialized = 1;

	return 0;
}

void plat_close_audio()
{
	if (ALDevice)
	{
		alcMakeContextCurrent(NULL);
		if (ALContext)
			alcDestroyContext(ALContext);
		alcCloseDevice(ALDevice);
		AL_initialized = 0;
	}
}*/

/*int plat_get_new_sound_handle()
{
	ALint state;
	for (int i = 0; i < _MAX_VOICES; i++)
	{
		alGetSourcei(sourceNames[i], AL_SOURCE_STATE, &state);
		if (state != AL_PLAYING)
		{
			return i;
		}
	}
	AL_ErrorCheck("Getting handle");
	return _ERR_NO_SLOTS;
}*/

/*void plat_set_sound_data(int handle, unsigned char* data, int length, int sampleRate)
{
	if (handle >= _MAX_VOICES) return;

	alSourcei(sourceNames[handle], AL_BUFFER, 0);
	alBufferData(bufferNames[handle], AL_FORMAT_MONO8, data, length, sampleRate);
	plat_set_sound_loop_points(handle, 0, length);

	AL_ErrorCheck("Setting sound data");
}

void plat_set_sound_position(int handle, int volume, int angle)
{
	if (handle >= _MAX_VOICES) return;
	plat_set_sound_angle(handle, angle);
	plat_set_sound_volume(handle, volume);
}

void plat_set_sound_angle(int handle, int angle)
{
	if (handle >= _MAX_VOICES) return;

	float x, y;
	float flang = (angle / 65536.0f) * (3.1415927f);

	x = (float)cos(flang);
	y = (float)sin(flang);

	alSource3f(sourceNames[handle], AL_POSITION, -x, 0.0f, -y);
	AL_ErrorCheck("Setting sound angle");
}

void plat_set_sound_volume(int handle, int volume)
{
	if (handle >= _MAX_VOICES) return;

	float gain = volume / 32768.0f;
	alSourcef(sourceNames[handle], AL_GAIN, gain);
	AL_ErrorCheck("Setting sound volume");
}

void plat_set_sound_loop_points(int handle, int start, int end)
{
	if (start == -1) start = 0;
	if (end == -1)
	{
		ALint len;
		alGetBufferi(bufferNames[handle], AL_SIZE, &len);
		end = len;
		AL_ErrorCheck("Getting buffer length");
	}
	ALint loopPoints[2];
	loopPoints[0] = start;
	loopPoints[1] = end;
	alBufferiv(bufferNames[handle], AL_LOOP_POINTS_SOFT, &loopPoints[0]);
	AL_ErrorCheck("Setting loop points");
}*/

/*void plat_start_sound(int handle, int loop)
{
	if (handle >= _MAX_VOICES) return;
	alSourcei(sourceNames[handle], AL_BUFFER, bufferNames[handle]);
	alSourcei(sourceNames[handle], AL_LOOPING, loop);
	alSourcePlay(sourceNames[handle]);
	AL_ErrorCheck("Playing sound");
}

void plat_stop_sound(int handle)
{
	if (handle >= _MAX_VOICES) return;
	alSourceStop(sourceNames[handle]);
	AL_ErrorCheck("Stopping sound");
}

int plat_check_if_sound_playing(int handle)
{
	if (handle >= _MAX_VOICES) return 0;
	
	int playing;
	alGetSourcei(sourceNames[handle], AL_SOURCE_STATE, &playing);
	return playing == AL_PLAYING;
}

int plat_check_if_sound_finished(int handle)
{
	if (handle >= _MAX_VOICES) return 0;

	int playing;
	alGetSourcei(sourceNames[handle], AL_SOURCE_STATE, &playing);
	return playing == AL_STOPPED;
}*/

//-----------------------------------------------------------------------------
// Emitting pleasing rythmic sound at player
//-----------------------------------------------------------------------------
//bool playing = false;

/*int plat_start_midi(MidiSequencer* sequencer)
{
	return 0;
}

void plat_close_midi()
{
}

void plat_set_music_volume(int volume)
{
	if (!AL_initialized) return;
	//printf("Music volume %d\n", volume);
	MusicVolume = volume;

	//[ISB] Midi volume is now handled at the synth level, not the mixer level. 
	/*if (alIsSource(MusicSource)) //[ISB] TODO okay so this isn't truly thread safe, it likely won't pose a problem, but I should fix it just in case
	{
		alSourcef(MusicSource, AL_GAIN, MusicVolume / 127.0f);
	}*//*
	if (alIsSource(HQMusicSource)) //[ISB] heh
	{
		alSourcef(HQMusicSource, AL_GAIN, MusicVolume / 127.0f);
	}
	AL_ErrorCheck("Setting music volume");
}

void plat_start_hq_song(int sample_rate, std::vector<float>&& song_data, bool loop)
{
	alGenSources(1, &HQMusicSource);
	alSourcef(HQMusicSource, AL_ROLLOFF_FACTOR, 0.0f);
	alSource3f(HQMusicSource, AL_POSITION, 1.0f, 0.0f, 0.0f);
	alSourcef(HQMusicSource, AL_GAIN, MusicVolume / 127.0f);
	alSourcei(HQMusicSource, AL_LOOPING, loop);
	AL_ErrorCheck("Creating HQ music source");

	alGenBuffers(1, &HQMusicBuffer);
	alBufferData(HQMusicBuffer, AL_FORMAT_STEREO_FLOAT32, (ALvoid*)song_data.data(), song_data.size() * sizeof(float), sample_rate);
	AL_ErrorCheck("Creating HQ music buffer");
	alSourcei(HQMusicSource, AL_BUFFER, HQMusicBuffer);
	alSourcePlay(HQMusicSource);
	AL_ErrorCheck("Playing HQ music");
	HQMusicPlaying = true;
}

void plat_stop_hq_song()
{
	if (HQMusicPlaying)
	{
		alSourceStop(HQMusicSource);
		alDeleteSources(1, &HQMusicSource);
		alDeleteBuffers(1, &HQMusicBuffer);
		AL_ErrorCheck("Stopping HQ music");
		HQMusicPlaying = false;
	}
}

void* I_CreateMusicSource()
{
	int i;
	ALMusicSource* source = new ALMusicSource;

	alGenSources(1, &source->MusicSource);
	alSourcef(source->MusicSource, AL_ROLLOFF_FACTOR, 0.0f);
	alSource3f(source->MusicSource, AL_POSITION, 0.0f, 0.0f, 0.0f);
	alSourcef(source->MusicSource, AL_GAIN, MusicVolume / 127.0f);
	source->MusicVolume = MusicVolume;
	memset(&source->BufferQueue[0], 0, sizeof(ALuint) * MAX_BUFFERS_QUEUED);
	alGenBuffers(MAX_BUFFERS_QUEUED, source->SourceBuffers);
	for (i = 0; i < MAX_BUFFERS_QUEUED; i++)
		source->AvailableBuffers[i] = i;
	source->NumFreeBuffers = MAX_BUFFERS_QUEUED;
	source->NumUsedBuffers = 0;
	source->Playing = false;
	AL_ErrorCheck("Creating music source");

	return source;
}

void I_DestroyMusicSource(void* opaquesource)
{
	ALMusicSource* source = (ALMusicSource*)opaquesource;
	int BuffersProcessed;
	alGetSourcei(source->MusicSource, AL_BUFFERS_QUEUED, &BuffersProcessed);
	/*if (BuffersProcessed > 0) //Free any lingering buffers
	{
		alSourceUnqueueBuffers(MusicSource, BuffersProcessed, &BufferQueue[0]);
		alDeleteBuffers(BuffersProcessed, &BufferQueue[0]);
	}
	I_ErrorCheck("Destroying lingering music buffers");*//*
	alDeleteSources(1, &source->MusicSource);
	AL_ErrorCheck("Destroying music source");
	alDeleteBuffers(MAX_BUFFERS_QUEUED, source->SourceBuffers);
	AL_ErrorCheck("Destroying lingering buffers");
	source->MusicSource = 0;

	delete source;
}

*/

/*

void midi_set_music_samplerate(void* opaquesource, uint32_t samplerate)
{
	ALMusicSource* source = (ALMusicSource*)opaquesource;
	source->MusicSampleRate = samplerate;
}

void midi_check_status(void* opaquesource)
{
	ALMusicSource* source = (ALMusicSource*)opaquesource;
	ALenum playstatus;
	if (!source->Playing)
	{
		source->Playing = true;
		alSourcePlay(source->MusicSource);
		AL_ErrorCheck("Playing music source");
	}
	else
	{
		alGetSourcei(source->MusicSource, AL_SOURCE_STATE, &playstatus);
		if (playstatus != AL_PLAYING)
		{
			//If this happens, the buffer starved, kick it back up
			//This should happen as rarely as humanly possible, otherwise there's a pop because of the brief stall in the stream.
			//I need to find ways to reduce the amount of overhead in the setup. 
			printf("midi buffer starved\n");
			alSourcePlay(source->MusicSource);
		}
	}
}

bool midi_check_finished(void* opaquesource)
{
	ALMusicSource* source = (ALMusicSource*)opaquesource;
	if (source->MusicSource == 0) return true;

	ALenum playstatus;
	alGetSourcei(source->MusicSource, AL_SOURCE_STATE, &playstatus);

	return playstatus != AL_PLAYING;
}

bool midi_queue_slots_available(void* opaquesource)
{
	if (!AL_initialized) return false;
	ALMusicSource* source = (ALMusicSource*)opaquesource;
	/*alGetSourcei(MusicSource, AL_BUFFERS_QUEUED, &CurrentBuffers);
	AL_ErrorCheck("Checking can queue buffers");
	return CurrentBuffers < MAX_BUFFERS_QUEUED;*//*
	return source->NumFreeBuffers > 0;
}

void midi_dequeue_midi_buffers(void* opaquesource)
{
	int i;
	if (!AL_initialized) return;
	ALMusicSource* source = (ALMusicSource*)opaquesource;
	//I should probably keep track of all active music sources to do this immediately, but this will work for now. 
	//Check if the music volume changed and keep it up to date.
	if (source->MusicVolume != MusicVolume)
	{
		alSourcef(source->MusicSource, AL_GAIN, MusicVolume / 127.0f);
		source->MusicVolume = MusicVolume;
	}

	int BuffersProcessed;
	alGetSourcei(source->MusicSource, AL_BUFFERS_PROCESSED, &BuffersProcessed);
	if (BuffersProcessed > 0)
	{
		alSourceUnqueueBuffers(source->MusicSource, BuffersProcessed, &source->BufferQueue[0]);
		for (i = 0; i < BuffersProcessed; i++)
		{
			source->AvailableBuffers[source->NumFreeBuffers] = source->UsedBuffers[i];

			//printf("dq %d\n", source->UsedBuffers[i]);

			source->NumFreeBuffers++;
			source->NumUsedBuffers--;
		}
		for (i = 0; i < MAX_BUFFERS_QUEUED - BuffersProcessed; i++)
		{
			source->BufferQueue[i] = source->BufferQueue[i + BuffersProcessed];
			source->UsedBuffers[i] = source->UsedBuffers[i + BuffersProcessed];
		}
		//printf("Killing %d buffers\n", BuffersProcessed);
	}
	AL_ErrorCheck("Unqueueing music buffers");
}

void midi_queue_buffer(void* opaquesource, int numTicks, uint16_t *data)
{
	if (!AL_initialized) return;
	ALMusicSource* source = (ALMusicSource*)opaquesource;

	//printf("Queuing %d ticks\n", numTicks);
	if (source->NumFreeBuffers > 0)
	{
		int freeBufferNum = source->AvailableBuffers[source->NumFreeBuffers - 1];
		source->UsedBuffers[source->NumUsedBuffers] = freeBufferNum;
		
		source->BufferQueue[source->NumUsedBuffers] = source->SourceBuffers[freeBufferNum];
		alBufferData(source->BufferQueue[source->NumUsedBuffers], AL_FORMAT_STEREO16, data, numTicks * sizeof(ALushort) * 2, source->MusicSampleRate);
		alSourceQueueBuffers(source->MusicSource, 1, &source->BufferQueue[source->NumUsedBuffers]);
		AL_ErrorCheck("Queueing music buffers");

		//printf("q %d\n", freeBufferNum);

		source->NumUsedBuffers++;
		source->NumFreeBuffers--;
	}
}

void plat_start_midi_song(HMPFile* song, bool loop)
{
	//Hey, how'd I get here? What do I do?
}

void plat_stop_midi_song()
{
}

uint32_t plat_get_preferred_midi_sample_rate()
{
	return MIDI_SAMPLERATE;
}

void* midi_start_source()
{
	if (!AL_initialized) return nullptr;
	void* source = I_CreateMusicSource();
	AL_ErrorCheck("Creating source");
	return source;
}

void midi_stop_source(void* opaquesource)
{
	if (!AL_initialized) return;
	ALMusicSource* source = (ALMusicSource*)opaquesource;
	alSourceStop(source->MusicSource);
	midi_dequeue_midi_buffers(opaquesource);
	I_DestroyMusicSource(opaquesource);
	AL_ErrorCheck("Destroying source");
}

*/ 

/*

//-----------------------------------------------------------------------------
// Emitting buffered movie sound at player
//-----------------------------------------------------------------------------
#define NUMMVESNDBUFFERS 100

//Next buffer position, and earliest buffer position still currently queued
int mveSndBufferHead, mveSndBufferTail;
//Ring buffer of all the buffer names
ALuint mveSndRingBuffer[NUMMVESNDBUFFERS];

ALenum mveSndFormat;
ALint mveSndSampleRate;
ALuint mveSndSourceName;
ALboolean mveSndPlaying;

void I_CreateMovieSource()
{
	alGenSources(1, &mveSndSourceName);
	alSourcef(mveSndSourceName, AL_ROLLOFF_FACTOR, 0.0f);
	alSource3f(mveSndSourceName, AL_POSITION, 0.0f, 0.0f, 0.0f);
	AL_ErrorCheck("Creating movie source");
}

void mvesnd_init_audio(int format, int samplerate, int stereo)
{
	//printf("format: %d, samplerate: %d, stereo: %d\n", format, samplerate, stereo);
	switch (format)
	{
	case MVESND_U8:
		if (stereo)
			mveSndFormat = AL_FORMAT_STEREO8;
		else
			mveSndFormat = AL_FORMAT_MONO8;
		break;
	case MVESND_S16LSB:
		if (stereo)
			mveSndFormat = AL_FORMAT_STEREO16;
		else
			mveSndFormat = AL_FORMAT_MONO16;
		break;
	}
	mveSndSampleRate = samplerate;

	mveSndBufferHead = 0; mveSndBufferTail = 0;
	mveSndPlaying = AL_FALSE;

	I_CreateMovieSource();
}

void I_DequeueMovieAudioBuffers(int all)
{
	int i, n;
	//Dequeue processed buffers in the ring buffer
	alGetSourcei(mveSndSourceName, AL_BUFFERS_PROCESSED, &n);
	for (i = 0; i < n; i++)
	{
		alSourceUnqueueBuffers(mveSndSourceName, 1, &mveSndRingBuffer[mveSndBufferTail]);
		alDeleteBuffers(1, &mveSndRingBuffer[mveSndBufferTail]);

		mveSndBufferTail++;
		if (mveSndBufferTail == NUMMVESNDBUFFERS)
			mveSndBufferTail = 0;
	}
	AL_ErrorCheck("Dequeing movie buffers");
	//kill all remaining buffers if we're told to stop
	if (all)
	{
		alGetSourcei(mveSndSourceName, AL_BUFFERS_QUEUED, &n);
		for (i = 0; i < n; i++)
		{
			alSourceUnqueueBuffers(mveSndSourceName, 1, &mveSndRingBuffer[mveSndBufferTail]);
			alDeleteBuffers(1, &mveSndRingBuffer[mveSndBufferTail]);

			mveSndBufferTail++;
			if (mveSndBufferTail == NUMMVESNDBUFFERS)
				mveSndBufferTail = 0;
		}
		AL_ErrorCheck("Dequeing excess movie buffers");
	}
}

void mvesnd_queue_audio_buffer(int len, short* data)
{
	//I don't currently have a tick function (should I fix this?), so do this now
	I_DequeueMovieAudioBuffers(0);

	//Generate and fill out a new buffer
	alGenBuffers(1, &mveSndRingBuffer[mveSndBufferHead]);
	alBufferData(mveSndRingBuffer[mveSndBufferHead], mveSndFormat, (ALvoid*)data, len, mveSndSampleRate);

	AL_ErrorCheck("Creating movie buffers");
	//Queue the buffer, and if this source isn't playing, kick it off
	alSourceQueueBuffers(mveSndSourceName, 1, &mveSndRingBuffer[mveSndBufferHead]);
	if (mveSndPlaying == AL_FALSE)
	{
		alSourcePlay(mveSndSourceName);
		mveSndPlaying = AL_TRUE;
	}

	mveSndBufferHead++;
	if (mveSndBufferHead == NUMMVESNDBUFFERS)
		mveSndBufferHead = 0;

	AL_ErrorCheck("Queuing movie buffers");
}

void mvesnd_close()
{
	if (mveSndPlaying)
		alSourceStop(mveSndSourceName);

	I_DequeueMovieAudioBuffers(1);

	alDeleteSources(1, &mveSndSourceName);
}

void mvesnd_pause()
{
	alSourcePause(mveSndSourceName);
}

void mvesnd_resume()
{
	alSourcePlay(mveSndSourceName);
}*/

#endif
