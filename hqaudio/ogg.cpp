/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License.
*/

#include "ogg.h"
#include "lib/stb_vorbis.h"

#include "platform/mono.h"

OGGLoader::OGGLoader(std::string& filename) : SoundLoader(filename) {}

bool OGGLoader::Open() {
	
	int error = 0;
	ogg = stb_vorbis_open_filename(filename.data(), &error, nullptr);

	if (!ogg) {
		mprintf((1, "Could not open OGG file %s", filename.c_str()));
		return false;
	}

	auto info = stb_vorbis_get_info(ogg);

	properties.channels = channels = info.channels;
	properties.sampleRate = info.sample_rate;
	properties.format = SF_SHORT; //Supports either arbitrarily, so may as well give it shorts

	return true;

}

size_t OGGLoader::GetSamples(void* buffer, size_t bufferSize) {

	auto samples = stb_vorbis_get_samples_short_interleaved(ogg, channels, (short*)buffer, bufferSize / sizeof(int16_t));

	auto error = stb_vorbis_get_error(ogg);
	if (error) {
		mprintf((1, "OGG decode: stb_vorbis gave error code %d\n", error));
		return -1;
	}

	return samples * channels * sizeof(int16_t);

}

bool OGGLoader::Rewind() {
	return stb_vorbis_seek(ogg, 0);
}

void OGGLoader::Close() {
	stb_vorbis_close(ogg);
}