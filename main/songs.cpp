/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.  
COPYRIGHT 1993-1999 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "inferno.h"

extern uint8_t Config_redbook_volume;

#include "digi.h"
#include "misc/error.h"
#include "misc/types.h"
#include "misc/args.h"
#include "songs.h"
#include "platform/mono.h"
#include "cfile/cfile.h"
#include "platform/timer.h"
#include "platform/platform_filesys.h"
#include "platform/s_midi.h"
#include "hqmusic.h"

std::vector<song_info> Songs;
int Songs_initialized = 0;

bool No_endlevel_songs;

bool needToMaybePatchD1SongFile = false;

int Redbook_enabled = 1;
//0 if redbook is no playing, else the track number
int Redbook_playing = 0;

#define NumLevelSongs (Songs.size() - SONG_FIRST_LEVEL_SONG)

#define REDBOOK_VOLUME_SCALE  (127/3) //changed to 127 since this uses the same interface as the MIDI sound system

extern void digi_stop_current_song();

//takes volume in range 0..8
void set_redbook_volume(int volume)
{
	/*#ifndef MACINTOSH
	RBASetVolume(0);		// makes the macs sound really funny
	#endif
	RBASetVolume(volume*REDBOOK_VOLUME_SCALE/8);*/
	music_set_volume(volume * REDBOOK_VOLUME_SCALE / 8);
}

extern char CDROM_dir[];

void songs_init()
{
	int i;
	char inputline[80+1];
	CFILE * fp;

	//if ( Songs_initialized ) return;

	fp = cfopen( "descent.sng", "rb" );
	if ( fp == NULL )
	{
		Error( "Couldn't open descent.sng" );
	}
	i = 0;

	Songs.clear();

	while (cfgets(inputline, 80, fp ))
	{
		char *p = strchr(inputline,'\n');
		if (p) *p = '\0';
		if ( strlen( inputline ) )
		{
			song_info song;
			sscanf(inputline, "%15s %15s %15s", song.filename, song.melodic_bank_file, song.drum_bank_file);
			Songs.push_back(song);
		}
	}
	//Num_songs = i;
	if (Songs.size() <= SONG_FIRST_LEVEL_SONG)
		Error("Must have at least %d songs",SONG_FIRST_LEVEL_SONG+1);
	cfclose(fp);

	if (Songs.capacity() > MAX_NUM_SONGS)
		Songs.shrink_to_fit();

	mprintf((1, "Num songs: %d\n", Songs.size()));

	if (currentGame == G_DESCENT_1 && needToMaybePatchD1SongFile && Songs.size() == SONG_FIRST_LEVEL_SONG + 7) {
		for (int i = 7; i < 22; i++) {
			int index = SONG_FIRST_LEVEL_SONG + i;
			song_info song;
			snprintf(song.filename, 11, "game%02u.hmp", i + 1);
			snprintf(song.melodic_bank_file, 12, "melodic.bnk");
			snprintf(song.drum_bank_file, 9, "drum.bnk");
			Songs.push_back(song);
		}
	}

	needToMaybePatchD1SongFile = false;

	if (!Songs_initialized) {
		if (FindArg("-noredbook"))
		{
			Redbook_enabled = 0;
		}
		else	// use redbook
		{
			RBAInit();

			if (RBAEnabled())
			{
				set_redbook_volume(Config_redbook_volume);
			}
		}
		//atexit(RBAStop);	// stop song on exit [DW] digi_close does this automatically with more guarantees on valid order of destroying things
	}

	Songs_initialized = 1;
}

#define FADE_TIME (f1_0/2)

//stop the redbook, so we can read off the CD
void songs_stop_redbook(void)
{
	int old_volume = Config_redbook_volume*REDBOOK_VOLUME_SCALE/8;
	fix old_time = timer_get_fixed_seconds();

	if (Redbook_playing) //fade out volume
	{		
		int new_volume;
		do {
			fix t = timer_get_fixed_seconds();

			new_volume = fixmuldiv(old_volume,(FADE_TIME - (t-old_time)),FADE_TIME);

			if (new_volume < 0)
				new_volume = 0;

			music_set_volume(new_volume);

		} while (new_volume > 0);
	}

	RBAStop();						// Stop CD, if playing

	music_set_volume(old_volume);	//restore volume

	Redbook_playing = 0;
}

//stop any songs - midi or redbook - that are currently playing
void songs_stop_all(void)
{
	digi_stop_current_song();	// Stop midi song, if playing
	songs_stop_redbook();			// Stop CD, if playing
}

int force_rb_register=0;

int reinit_redbook()
{
	RBAInit();

	if (RBAEnabled())
	{
		set_redbook_volume(Config_redbook_volume);
		//RBARegisterCD();
		//force_rb_register=0;
	}

	return 0;
}


//returns 1 if track started sucessfully
//start at tracknum.  if keep_playing set, play to end of disc.  else
//play only specified track
int play_redbook_track(int tracknum,int keep_playing)
{
	Redbook_playing = 0;

	

	if (!RBAEnabled() && Redbook_enabled && !FindArg("-noredbook"))
		reinit_redbook();

	/*if (force_rb_register)
	{
		RBARegisterCD();			//get new track list for new CD
		force_rb_register = 0;
	}*/

	if (Redbook_enabled && RBAEnabled()) 
	{
		int num_tracks = RBAGetNumberOfTracks();
		if (tracknum <= num_tracks)
			if (RBAPlayTracks(tracknum,keep_playing?num_tracks:tracknum))  
			{
				Redbook_playing = tracknum;
			}
	}

	return (Redbook_playing != 0);
}

#define REDBOOK_TITLE_TRACK			1
#define REDBOOK_CREDITS_TRACK			2
#define REDBOOK_FIRST_LEVEL_TRACK	3 
								//(songs_haved2_cd()?4:1)

// songs_haved2_cd returns 1 if the descent 2 CD is in the drive and
// 0 otherwise

int songs_haved2_cd()
{
	/*
	char temp[128],cwd[128];
	
	getcwd(cwd, 128);

	strcpy(temp,CDROM_dir);

	#ifndef MACINTOSH		//for PC, strip of trailing slash
	if (temp[strlen(temp)-1] == '\\')
		temp[strlen(temp)-1] = 0;
	#endif

	if ( !chdir(temp) )
	{
		chdir(cwd);
		return 1;
	}
	*/
	return 0;
}
	

void songs_play_song( int songnum, int repeat )
{
	if (No_endlevel_songs)
	{
		Assert(songnum != SONG_ENDLEVEL && songnum != SONG_ENDGAME);	//not in full version
	}

	if ( !Songs_initialized ) 
		songs_init();

	//stop any music already playing

	songs_stop_all();

	//do we want any of these to be redbook songs?

	/*if (force_rb_register)
	{
		RBARegisterCD();			//get new track list for new CD
		force_rb_register = 0;
	}*/

	if (songnum == SONG_TITLE)
		play_redbook_track(REDBOOK_TITLE_TRACK,0);
	else if (songnum == SONG_CREDITS)
		play_redbook_track(REDBOOK_CREDITS_TRACK,0);

	if (!Redbook_playing) //not playing redbook, so play midi
	{		
		digi_play_midi_song( Songs[songnum].filename, Songs[songnum].melodic_bank_file, Songs[songnum].drum_bank_file, repeat );
	}
}

int current_song_level;

void songs_play_level_song( int levelnum )
{
	int songnum;
	int n_tracks;

	Assert( levelnum != 0 );

	if ( !Songs_initialized )
		songs_init();

	songs_stop_all();

	current_song_level = levelnum;

	songnum = (levelnum>0)?(levelnum-1):(-levelnum);
	
	
	if (!RBAEnabled() && Redbook_enabled && !FindArg("-noredbook"))
		reinit_redbook();

	/*if (force_rb_register) 
	{
		RBARegisterCD();			//get new track list for new CD
		force_rb_register = 0;
	}*/

	if (Redbook_enabled && RBAEnabled() && (n_tracks = RBAGetNumberOfTracks()) > 1) 
	{
		//try to play redbook
		mprintf((0,"n_tracks = %d\n",n_tracks));
		play_redbook_track(REDBOOK_FIRST_LEVEL_TRACK + (songnum % (n_tracks-REDBOOK_FIRST_LEVEL_TRACK+1)),1);
	}
	
	if (!Redbook_playing) //not playing redbook, so play midi
	{			
		songnum = SONG_FIRST_LEVEL_SONG + (songnum % NumLevelSongs);
		digi_play_midi_song( Songs[songnum].filename, Songs[songnum].melodic_bank_file, Songs[songnum].drum_bank_file, 1 );
	}
}

//this should be called regularly to check for redbook restart
void songs_check_redbook_repeat()
{
	static fix last_check_time;
	fix current_time;

	if (!Redbook_playing || Config_redbook_volume==0) return;

	current_time = timer_get_fixed_seconds();
	if (current_time < last_check_time || (current_time - last_check_time) >= F2_0) 
	{
		if (!RBAPeekPlayStatus()) 
		{
			//stop_time();
			// if title ends, start credit music
			// if credits music ends, restart it
			if (Redbook_playing == REDBOOK_TITLE_TRACK || Redbook_playing == REDBOOK_CREDITS_TRACK)
				play_redbook_track(REDBOOK_CREDITS_TRACK,0);
			else 
			{
				//songs_goto_next_song();
	
				//new code plays all tracks to end of disk, so if disk has
				//stopped we must be at end.  So start again with level 1 song.
	
				songs_play_level_song(1);
			}
			//start_time();
		}
		last_check_time = current_time;
	}
}

//goto the next level song
void songs_goto_next_song()
{
	if (Redbook_playing) 		//get correct track
		current_song_level = RBAGetTrackNum() - REDBOOK_FIRST_LEVEL_TRACK + 1;

	songs_play_level_song(current_song_level+1);
}

//goto the previous level song
void songs_goto_prev_song()
{
	if (Redbook_playing) 		//get correct track
		current_song_level = RBAGetTrackNum() - REDBOOK_FIRST_LEVEL_TRACK + 1;

	if (current_song_level > 1)
		songs_play_level_song(current_song_level-1);
}

//CD music thread nonsense
