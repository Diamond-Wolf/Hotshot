/*
The code contained in this file is not the property of Parallax Software,
and is not under the terms of the Parallax Software Source license.
Instead, it is released under the terms of the MIT License.
*/

#include "hqmusic.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <regex>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

#include "cfile/cfile.h"
#include "hqaudio/sound_loader.h"
#include "inferno.h"
#include "main/config.h"
#include "misc/error.h"
#include "platform/i_sound.h"
#include "platform/mono.h"
#include "platform/platform_filesys.h"
#include "platform/posixstub.h"
#include "platform/timer.h"
#include "songs.h";

SoundLoader* loader = nullptr;
RedbookEndMode rbaEndMode = REM_CONTINUE;
RedbookExtraTracksMode rbaExtraTracksMode = RETM_REDBOOK_2;

bool rbaInitialized = false;

std::vector<std::string> tracks;

int currentTrack;
int firstTrack, lastTrack;

std::thread rbaThread;
bool quitThread = false;
bool threadWasEverInitialized = false;

bool PlayHQSong(const char* filename, bool loop) {
	
	if (!filename)
		return false;

	loader = RequestSoundLoader(filename);
	
	if (!loader) {
		//Warning("Unrecognized filetype for HQ file %s", filename);
		hqaWarning.Put("Unrecognized filetype for HQ file " + std::string(filename));
		mprintf((1, "Unrecognized filetype for HQ file %s", filename));
		return false;
	}

	if (Redbook_enabled) {

		if (!loader->Open()) {
			//Warning("Could not open loader for %s", filename);
			hqaWarning.Put("Could not open loader for " + std::string(filename));
			mprintf((1, "Could not open loader for %s", filename));
			delete loader;
			loader = nullptr;
			return false;
		}

	} else {

		auto cfile = cfopen(filename, "rb");
		if (!cfile) {
			//Warning("Could not open %s", filename);
			hqaWarning.Put("Could not open " + std::string(filename));
			mprintf((1, "Could not open %s", filename));
			delete loader;
			loader = nullptr;
			return false;
		}

		auto size = cfile->size;
		auto data = new char[size];
		auto read = cfread(data, 1, cfile->size, cfile);
		cfclose(cfile);

		if (read < size) {
			mprintf((1, "cfread: Requested %d / %d, got %d", 1, size, read));
			//Warning("Error reading %s", filename);
			hqaWarning.Put("Error reading " + std::string(filename));
			mprintf((1, "Error reading %s", filename));
			delete loader;
			loader = nullptr;
			delete[] data;
			return false;
		}

		if (!loader->OpenMemory(data, size, true)) {
			//Warning("Could not open loader for %s", filename);
			hqaWarning.message = "Could not open loader for " + std::string(filename);
			hqaWarning.show = true;
			delete loader;
			loader = nullptr;
			// [DW] Don't delete data, since the loader's dtor will do that
			return false;
		}

	}

	plat_start_hq_song(loader, loop);
	return true;
}

//Stops playback of an OGG file
void StopHQSong() {
	
	quitThread = true;

	if (threadWasEverInitialized && rbaThread.joinable()) {
		rbaThread.join();
	}

	plat_stop_hq_song();

	if (loader) {
		loader->Close();
		delete loader;
		loader = nullptr;
	}

}

//Redbook music emulation functions

void ProcessRSNGLine(char* line) {

	static char fullFilePath[CHOCOLATE_MAX_FILE_PATH_SIZE];

	if (!_strnicmp(line, ":", 0)) {
		if (!_strnicmp(line, ":endmode=", 9)) {
			mprintf((0, "Found end mode: %s\n", line));
			if (!_strnicmp(line + 9, "continue", 8))
				rbaEndMode = REM_CONTINUE;
			else if (!_strnicmp(line + 9, "loop", 4))
				rbaEndMode = REM_LOOP;
			else {
				mprintf((1, "Invalid endmode specified in redbook.sng"));
				//tracks.clear();
				//return;
			}
		}
		else if (!_strnicmp(line, ":extras=", 8)) {
			mprintf((0, "Found extras mode: %s\n", line));
			if (!_strnicmp(line + 8, "redbook", 7))
				rbaExtraTracksMode = RETM_REDBOOK_2;
			else if (!_strnicmp(line + 8, "midi", 4))
				rbaExtraTracksMode = RETM_MIDI_5;
			else {
				mprintf((1, "Invalid extra tracks mode specified in redbook.sng"));
				//tracks.clear();
				//return;
			}
		}
		else {
			mprintf((1, "Invalid RSNG directive %s", line));
		}
	}
	else {

#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
		get_full_file_path(fullFilePath, line, CHOCOLATE_MUSIC_DIR);
#else
		snprintf(fullFilePath, CHOCOLATE_MAX_FILE_PATH_SIZE, "CDMusic/%s", line);
		fullFilePath[CHOCOLATE_MAX_FILE_PATH_SIZE - 1] = '\0';
#endif

		tracks.push_back(fullFilePath);

	}
}

void ReadRSNGFromDisk(std::string filename) {

	std::ifstream file(filename);
	if (!file.is_open()) {
		Warning("Error opening redbook.sng");
		return;
	}

	rbaEndMode = REM_CONTINUE;
	rbaExtraTracksMode = RETM_REDBOOK_2;

	char line[512];

	while (file.getline(line, 512)) {
		ProcessRSNGLine(line);
	}

}

void InitDefault() {
	char filename_full_path[CHOCOLATE_MAX_FILE_PATH_SIZE];
	char track_name[16];

	//Simple hack to figure out how many CD tracks there are.
	for (int i = 0; i < 99; i++)
	{
		snprintf(track_name, 15, "track%02d.ogg", i + 1);

#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
		get_full_file_path(filename_full_path, track_name, CHOCOLATE_MUSIC_DIR);
#else
		snprintf(filename_full_path, CHOCOLATE_MAX_FILE_PATH_SIZE, "CDMusic/%s", track_name);
		filename_full_path[CHOCOLATE_MAX_FILE_PATH_SIZE - 1] = '\0';
#endif

		if (fs::is_regular_file(filename_full_path))
			tracks.push_back(filename_full_path);
		else
			break;

	}

	int needed = (rbaExtraTracksMode == RETM_REDBOOK_2 ? 3 : 6);
	if (tracks.size() >= needed) //Need sufficient tracks
		rbaInitialized = true;
	else
		tracks.clear();

}

bool ReadSNGFromCFile(CFILE* cfile, bool useWholeLine, bool allowDirectives, bool d1OverrideHack) {
	if (!cfile)
		return false;

	char line[81];
	memset(line, '\0', 81);
	
	rbaEndMode = REM_LOOP;
	rbaEndMode = REM_CONTINUE;
	rbaExtraTracksMode = RETM_MIDI_5;

	while (cfgets(line, 80, cfile))
	{
		char* p = strchr(line, '\n');
		if (p) *p = '\0';

		if (!useWholeLine) {
			const static std::regex spaceRe("\\s+");
			//strncpy(line, std::regex_replace(line, spaceRe, "\0").c_str(), 80);
			p = strchr(line, ' ');
			if (p) *p = '\0';
			p = strchr(line, '\t');
			if (p) *p = '\0';
		}

		if (strlen(line))
		{
			//song_info song;
			//sscanf(inputline, "%15s %15s %15s", song.filename, song.melodic_bank_file, song.drum_bank_file);
			//Songs.push_back(song);
			if (allowDirectives)
				ProcessRSNGLine(line);
			else
				tracks.push_back(line);
		}

		if (d1OverrideHack && currentGame == G_DESCENT_1 && needToMaybePatchD1SongFile && tracks.size() == SONG_FIRST_LEVEL_SONG + 7) {
			for (int i = 7; i < 22; i++) {
				int index = SONG_FIRST_LEVEL_SONG + i;
				snprintf(line, 11, "game%02u.hmp", i + 1);
				tracks.push_back(line);
			}
		}
	}

	return true;

}

void RBAInit() {

	rbaInitialized = false;
	tracks.clear();

	char filename_full_path[CHOCOLATE_MAX_FILE_PATH_SIZE];

	if (Redbook_enabled) {

#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
		get_full_file_path(filename_full_path, "redbook.sng", CHOCOLATE_MUSIC_DIR);
#else
		snprintf(filename_full_path, CHOCOLATE_MAX_FILE_PATH_SIZE, "CDMusic/%s", "track_name");
#endif
		if (fs::is_regular_file(filename_full_path)) {
			ReadRSNGFromDisk(filename_full_path);
			rbaInitialized = true;
		} else {
			InitDefault();
		}

	} else {
		
		if (!ReadSNGFromCFile(cfopen("hotshot.sng", "rb"), true, true, false))
			if (!ReadSNGFromCFile(cfopen("dxx-r.sng", "rb"), true, false, false))
				if (!ReadSNGFromCFile(cfopen("descent.sng", "rb"), false, false, true)) {
					Warning("Could not find valid SNG. Need one of hotshot.sng, dxx-r.sng, or descent.sng.");
				}
					
	}

	music_set_volume(Config_midi_volume * 127 / 8);

	rbaInitialized = true;

}

bool RBAEnabled() { 
	return rbaInitialized; 
}

void RBAStop() {

	rbaInitialized = false;
	StopHQSong();

}

int RBAGetNumberOfTracks() { 
	return tracks.size(); 
}

int RBAGetTrackNum() { 
	return currentTrack; 
}

int RBAPlayTrack(int track, bool loop) { 
	if (!rbaInitialized)
		return false;

	mprintf((0, "Track %d requested\n", track));
	return PlayHQSong(tracks[track - 1].c_str(), loop);
}

//std::atomic<bool> threadStarted = false;
bool threadStarted = false;
int rbaTrackActive = 0;

int RBAPlayTracks(int first, int last) { 
	if (!rbaInitialized)
		return false;

	if (first == last)
		return RBAPlayTrack(first, true);

	if (threadWasEverInitialized && rbaThread.joinable()) {
		StopHQSong();
	}

	quitThread = false;
	threadStarted = false;
	rbaTrackActive = 0;

	rbaThread = std::thread([first, last]() {
		
		currentTrack = first;

		while (!quitThread && currentTrack <= last) {

			if (!plat_is_hq_song_playing()) {
				if (RBAPlayTrack(currentTrack, false))
					rbaTrackActive = currentTrack;
				else
					rbaTrackActive = 0;

				currentTrack++;
			}

			threadStarted = true;
			//threadStarted.notify_all();

			I_DelayUS(1000);

		}

		threadStarted = true;

	});

	threadWasEverInitialized = true;

	while (threadStarted && rbaThread.joinable()) {
		if (hqaWarning.show) {
			Warning(hqaWarning.Get().c_str());
		}
	}

	if (hqaWarning.show) {
		Warning(hqaWarning.Get().c_str());
	}

	return true;

}

bool RBAPeekPlayStatus() { 
	return rbaInitialized && plat_is_hq_song_playing(); 
}

/*

bool PlayHQSong(const char* filename, bool loop)
{
	// Load ogg into memory:

	//std::string name = filename;
	//name = name.substr(0, name.size() - 4); // cut off extension
	fs::path filepath = filename;
	std::string name = filepath.filename().string();
	std::string ext = filepath.extension().string();

	FILE* file = fopen(("music/" + name + ".ogg").c_str(), "rb");
	if (!file) return false;
	fseek(file, 0, SEEK_END);
	auto size = ftell(file);
	fseek(file, 0, SEEK_SET);
	std::vector<uint8_t> filedata(size);
	fread(filedata.data(), filedata.size(), 1, file);
	fclose(file);

	// Decompress it:

	int error = 0;
	int stream_byte_offset = 0;
	stb_vorbis* handle = stb_vorbis_open_pushdata(filedata.data(), filedata.size(), &stream_byte_offset, &error, nullptr);
	if (handle == nullptr)
		return false;

	stb_vorbis_info stream_info = stb_vorbis_get_info(handle);
	int song_sample_rate = stream_info.sample_rate;
	int song_channels = stream_info.channels;
	std::vector<float> song_data;

	while (true)
	{
		float** pcm = nullptr;
		int pcm_samples = 0;
		int bytes_used = stb_vorbis_decode_frame_pushdata(handle, filedata.data() + stream_byte_offset, filedata.size() - stream_byte_offset, nullptr, &pcm, &pcm_samples);

		if (song_channels > 1)
		{
			for (int i = 0; i < pcm_samples; i++)
			{
				song_data.push_back(pcm[0][i]);
				song_data.push_back(pcm[1][i]);
			}
		}
		else
		{
			for (int i = 0; i < pcm_samples; i++)
			{
				song_data.push_back(pcm[0][i]);
				song_data.push_back(pcm[0][i]);
			}
		}

		stream_byte_offset += bytes_used;
		if (bytes_used == 0 || stream_byte_offset == filedata.size())
			break;
	}

	stb_vorbis_close(handle);

	plat_start_hq_song(song_sample_rate, std::move(song_data), loop);

	return true;
}

void StopHQSong()
{
	plat_stop_hq_song();
}

*/

/*

//Redbook music emulation functions
int RBA_Num_tracks;
bool RBA_Initialized;

int RBA_Start_track, RBA_End_track;
volatile int RBA_Current_track = -1;

std::thread RBA_thread;

//Data of the song currently decoding. ATM loaded all at once.
//TODO: Profile, may be much faster to let stb_vorbis do its own IO.
std::vector<uint8_t> RBA_data;

//Samples of the song currently decoding. Interleaved, stereo.
//TODO: Interface for allowing non-stereo data, since it's quicker
std::vector<uint16_t> RBA_samples;

//Set when the decoder reports no more data. When this is true, the thread
//will wait for the song to end playing, and then load another one. 
bool RBA_out_of_data = false;

//Set to true while the RBA thread should run
volatile bool RBA_active = false;

//Set to !0 if there's an error
volatile int RBA_error = 0;

stb_vorbis* RBA_Current_vorbis_handle = nullptr;
int RBA_Vorbis_stream_offset;
int RBA_Vorbis_error;

stb_vorbis_alloc RBA_Vorbis_alloc_buffer;
//Last frame data.
//TODO: Make this not constant size
constexpr int RBA_NUM_SAMPLES = 4096;
short RBA_Vorbis_frame_data[RBA_NUM_SAMPLES * 2];
int RBA_Vorbis_frame_size;

//Tracks are ones-based, heh.
bool RBAThreadStartTrack(int num, void** mysourceptr)
{
	char filename_full_path[CHOCOLATE_MAX_FILE_PATH_SIZE];
	char track_name[16];

	snprintf(track_name, 15, "track%02d.ogg", num);

#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
	get_full_file_path(filename_full_path, track_name, CHOCOLATE_MUSIC_DIR);
#else
	snprintf(filename_full_path, CHOCOLATE_MAX_FILE_PATH_SIZE, "cdmusic/%s", track_name);
	filename_full_path[CHOCOLATE_MAX_FILE_PATH_SIZE - 1] = '\0';
#endif
	
	FILE* file = fopen(filename_full_path, "rb");
	if (!file) return false;

	fseek(file, 0, SEEK_END);
	auto size = ftell(file);
	fseek(file, 0, SEEK_SET);
	RBA_data.resize(size);
	fread(RBA_data.data(), RBA_data.size(), 1, file);
	fclose(file);

	//RBA_Current_vorbis_handle = stb_vorbis_open_pushdata(RBA_data.data(), RBA_data.size(), &RBA_Vorbis_stream_offset, &RBA_Vorbis_error, nullptr);
	RBA_Current_vorbis_handle = stb_vorbis_open_memory(RBA_data.data(), RBA_data.size(), &RBA_Vorbis_error, &RBA_Vorbis_alloc_buffer);
	if (!RBA_Current_vorbis_handle) return false;

	stb_vorbis_info info = stb_vorbis_get_info(RBA_Current_vorbis_handle);

	void* mysource = midi_start_source();
	midi_set_music_samplerate(mysource, info.sample_rate);
	*mysourceptr = mysource;
	RBA_out_of_data = false;

	return true;
}

//int stb_vorbis_get_samples_short_interleaved(stb_vorbis *f, int channels, short *buffer, int num_shorts);
void RBAThreadDecodeSamples()
{
	RBA_Vorbis_frame_size = stb_vorbis_get_samples_short_interleaved(RBA_Current_vorbis_handle, 2, RBA_Vorbis_frame_data, RBA_NUM_SAMPLES);
	if (RBA_Vorbis_frame_size == 0)
		RBA_out_of_data = true;
}

bool RBAPeekPlayStatus()
{
	return RBA_active;
}

//Before the thread starts, RBA_Current_track and RBA_End_track must be set.
//The thread will run until RBA_Current_track exceeds RBA_End_track.
void RBAThread()
{
	RBA_Current_track = RBA_Start_track;
	void* mysource;
	bool res = RBAThreadStartTrack(RBA_Current_track, &mysource);

	if (!res)
	{
		RBA_active = false;
		RBA_error = 1;
		midi_stop_source(mysource);
		return;
	}

	while (RBA_active)
	{
		if (!RBA_out_of_data)
		{
			midi_dequeue_midi_buffers(mysource);
			if (midi_queue_slots_available(mysource))
			{
				RBAThreadDecodeSamples();
				if (RBA_Vorbis_frame_size > 0)
				{
					midi_queue_buffer(mysource, RBA_Vorbis_frame_size, RBA_Vorbis_frame_data);
				}
				midi_check_status(mysource);
			}
		}
		else
		{
			midi_dequeue_midi_buffers(mysource);
			if (midi_check_finished(mysource))
			{
				RBA_Current_track++;
				if (RBA_Current_track > RBA_End_track)
				{
					RBA_Current_track = -1;
					RBA_active = false;
				}
				else
				{
					midi_stop_source(mysource);
					res = RBAThreadStartTrack(RBA_Current_track, &mysource);
					if (!res)
					{
						RBA_active = true;
						RBA_error = 1;
						return;
					}
				}
			}
		}

		I_DelayUS(4000);
	}

	//If we aborted, still check if the MIDI is actually done playing. 
	midi_stop_source(mysource);
	return;
}
*/

/*

void RBAInit()
{
	int i;
	char filename_full_path[CHOCOLATE_MAX_FILE_PATH_SIZE];
	char track_name[16];
	FILE* fp;

	RBA_error = 0;

	//Simple hack to figure out how many CD tracks there are.
	for (i = 0; i < 99; i++)
	{
		snprintf(track_name, 15, "track%02d.ogg", i + 1);

#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
		get_full_file_path(filename_full_path, track_name, CHOCOLATE_MUSIC_DIR);
#else
		snprintf(filename_full_path, CHOCOLATE_MAX_FILE_PATH_SIZE, "cdmusic/%s", track_name);
		filename_full_path[CHOCOLATE_MAX_FILE_PATH_SIZE - 1] = '\0';
#endif

		fp = fopen(filename_full_path, "rb");
		if (fp)
			fclose(fp);
		else
			break;
	}

	RBA_Num_tracks = i;
	if (RBA_Num_tracks >= 3) //Need sufficient tracks
		RBA_Initialized = true;
}

bool RBAEnabled()
{
	return RBA_Initialized && !RBA_error;
}

void RBAStop()
{
	if (RBA_thread.joinable())
	{
		RBA_active = false;
		RBA_thread.join();
	}
}

int RBAGetNumberOfTracks()
{
	return RBA_Num_tracks;
}

int RBAGetTrackNum()
{
	return RBA_Current_track;
}

void RBAStartThread()
{
	//Assert(!RBA_thread.joinable()); //joinable thread is an active thread, so it wasn't cleaned up before.
	RBAStop();
	RBA_active = true;
	RBA_thread = std::thread(&RBAThread);
}

int RBAPlayTrack(int track)
{
	RBA_Start_track = RBA_End_track = track;
	mprintf((0, "Playing Track %d\n", track));
	RBAStartThread();
	return 1;
}

int RBAPlayTracks(int first, int last)
{
	RBA_Start_track = first;
	RBA_End_track = last;

	mprintf((0, "Playing tracks %d to %d\n", first, last));
	RBAStartThread();
	return 1;
}*/