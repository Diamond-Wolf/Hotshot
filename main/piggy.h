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

#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "digi.h"
#include "sounds.h"
#include "misc/hash.h"
#include "cfile/cfile.h"

#include "inferno.h"

#define MAX_ALIASES 20

typedef struct alias 
{
	char alias_name[FILENAME_LEN];
	char file_name[FILENAME_LEN];
} alias;

//extern alias alias_list[MAX_ALIASES];
//extern int Num_aliases;

typedef struct bitmap_index 
{
	uint16_t	index;
} bitmap_index;

#if defined(__GNUG__) || defined(__clang__)
# define BITMAP_INDEX(val) (bitmap_index){(uint16_t)val}
#else
# define BITMAP_INDEX(val) (bitmap_index {(uint16_t)val})
#endif

/*#define BITMAP_INDEX(val)
# if defined(_MSC_VER)
2
# else
(bitmap_index){(uint16_t)val}
#endif*/

int piggy_init();
void piggy_close();
void piggy_dump_all();
void ClearPiggyCache();
bitmap_index piggy_register_bitmap( grs_bitmap * bmp, const char * name, int in_file, uint8_t flag, int offset);
int piggy_register_sound( digi_sound * snd, const char * name, int in_file );
bitmap_index piggy_find_bitmap( char * name );
int piggy_find_sound(const char * name );

extern int Pigfile_initialized;

#define PIGGY_PAGE_IN(bmp) \
do { \
	if ( activePiggyTable->gameBitmaps[(bmp).index].bm_flags & BM_FLAG_PAGED_OUT )	{	\
		piggy_bitmap_page_in( bmp ); 						\
	}		\
} while(0)

extern void piggy_bitmap_page_in( bitmap_index bmp );
extern void piggy_bitmap_page_out_all();
extern int piggy_page_flushed;

void piggy_read_bitmap_data(grs_bitmap * bmp);
void piggy_read_sound_data(digi_sound	*snd);

void piggy_load_level_data();

#define MAX_BITMAP_FILES_D1 1800
#define MAX_BITMAP_FILES_D2 2620

#define MAX_BITMAP_FILES MAX_BITMAP_FILES_D2

#define MAX_SOUND_FILES		MAX_SOUNDS

void piggy_read_sounds();

//reads in a new pigfile (for new palette)
//returns the size of all the bitmap data
void piggy_new_pigfile(const char *pigname);

typedef struct BitmapFile {
	char                    name[15];
} BitmapFile;

typedef struct SoundFile {
	char                    name[15];
} SoundFile;

struct piggytable {
	CFILE* file;

	/*hashtable bitmapNames;
	hashtable soundNames;*/

	std::unordered_map<std::string, int> bitmapNames;
	std::unordered_map<std::string, int> soundNames;

	bool bogusInitialized = false;
	bool piggyInitialized = false;

	std::vector<digi_sound> gameSounds;
	std::vector<int> soundOffsets;
	std::vector<grs_bitmap> gameBitmaps;
	std::vector<alias> aliases;
	std::vector<BitmapFile> bitmapFiles;
	std::vector<SoundFile> soundFiles;
	std::vector<int> gameBitmapOffsets;
	std::vector<uint8_t> gameBitmapFlags;
	std::vector<uint16_t> gameBitmapXlat;

	//piggytable();
	void Init();
	void SetActive();
	~piggytable();
};

inline piggytable* activePiggyTable;