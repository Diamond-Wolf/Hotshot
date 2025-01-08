/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License.
*/

#ifndef SOUND_LOADER_H
#define SOUND_LOADER_H

#include <string>

//The sample format that the sound loader will output. Does not necessarily need to match the format of the file itself.
enum SampleFormat {
	SF_BYTE,
	SF_UBYTE,
	SF_SHORT,
	SF_FLOAT,
	SF_LONG,

	SF_UNSUPPORTED
};

//The properties associated with a sound that the audio engine may need to know.
struct SoundProperties {
	int sampleRate;
	int channels;
	SampleFormat format;
};

//Container class for a given HQ audio file. Create a loader by calling RequestSoundLoader.
struct SoundLoader {

	const std::string filename;
	SoundProperties properties;

	//Opens the sound file associated with this loader.
	//Returns true if successful.
	virtual bool Open() = 0;

	//Fills buffer with bufferSize bytes.
	//Returns the number of bytes filled, or -1 on error.
	virtual size_t GetSamples(void* buffer, size_t bufferSize) = 0;

	//Rewinds the sound to the beginning.
	//Returns true if successful.
	virtual bool Rewind() = 0;

	//Closes the sound file and cleans up the loader.
	virtual void Close() = 0;

	virtual ~SoundLoader();

protected:
	SoundLoader(const std::string& filename);

};

SoundLoader* RequestSoundLoader(std::string filename);

#endif