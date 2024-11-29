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

#ifdef WINDOWS
#include "desw.h"
#include "win\ds.h"
#endif

#include <stdexcept>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include "platform/platform_filesys.h"
#include "platform/posixstub.h"
#include "misc/types.h"
#include "inferno.h"
#include "2d/gr.h"
#include "mem/mem.h"
#include "iff/iff.h"
#include "platform/mono.h"
#include "misc/error.h"
#include "sounds.h"
#include "songs.h"
#include "bm.h"
#include "bmread.h"
#include "misc/hash.h"
#include "misc/args.h"
#include "2d/palette.h"
#include "gamefont.h"
#include "2d/rle.h"
#include "screens.h"
#include "piggy.h"
#include "texmerge.h"
#include "paging.h"
#include "game.h"
#include "stringtable.h"
#include "cfile/cfile.h"
#include "newmenu.h"
#include "misc/byteswap.h"
#include "platform/findfile.h"
#include "bm.h"
#include "platform/platform.h"

//#include "unarj.h" //[ISB] goddamnit

//#define NO_DUMP_SOUNDS        1               //if set, dump bitmaps but not sounds

#define DEFAULT_PIGFILE_REGISTERED      "groupa.pig"
#define DEFAULT_PIGFILE_SHAREWARE       "d2demo.pig"


#define DEFAULT_HAMFILE         "descent2.ham"
#define DEFAULT_PIGFILE         DEFAULT_PIGFILE_REGISTERED
#define DEFAULT_SNDFILE	 		((digi_sample_rate==SAMPLE_RATE_22K)?"descent2.s22":"descent2.s11")

uint8_t* BitmapBits = NULL;
uint8_t* SoundBits = NULL;

int Must_write_hamfile = 0;
int Num_bitmap_files_new = 0;
int Num_sound_files_new = 0;

int piggy_low_memory = 0;

int Piggy_bitmap_cache_size = 0;
int Piggy_bitmap_cache_next = 0;
uint8_t* Piggy_bitmap_cache_data = NULL;

#define PIGGY_BUFFER_SIZE (2400*1024)

#ifdef MACINTOSH
#define PIGGY_SMALL_BUFFER_SIZE (1400*1024)		// size of buffer when piggy_low_memory is set

#ifdef SHAREWARE
#undef PIGGY_BUFFER_SIZE
#undef PIGGY_SMALL_BUFFER_SIZE

#define PIGGY_BUFFER_SIZE (2000*1024)
#define PIGGY_SMALL_BUFFER_SIZE (1100 * 1024)
#endif		// SHAREWARE

#endif

int piggy_page_flushed = 0;

#define DBM_FLAG_ABM            64
#define DBM_FLAG_LARGE			128

typedef struct DiskBitmapHeader
{
	char name[8];
	uint8_t dflags;                   //bits 0-5 anim frame num, bit 6 abm flag
	uint8_t width;                    //low 8 bits here, 4 more bits in wh_extra
	uint8_t height;                   //low 8 bits here, 4 more bits in wh_extra
	uint8_t   wh_extra;               //bits 0-3 width, bits 4-7 height
	uint8_t flags;
	uint8_t avg_color;
	int offset;
} DiskBitmapHeader;

//[ISB]: The above structure size can vary from system to system, but the size on disk is constant. Calculate that instead. 
#define BITMAP_HEADER_SIZE_D1 17
#define BITMAP_HEADER_SIZE_D2 18

#define BITMAP_HEADER_SIZE BITMAP_HEADER_SIZE_D2

typedef struct DiskSoundHeader
{
	char name[8];
	int length;
	int data_length;
	int offset;
} DiskSoundHeader;

//Not as much as a problem since its longword aligned, but just in case
#define SOUND_HEADER_SIZE 20

uint8_t BigPig = 0;

void piggy_write_pigfile(const char* filename); 
static void write_int(int i, FILE* file);
int piggy_is_substitutable_bitmap(char* name, char* subst_name);

#ifdef EDITOR
void swap_0_255(grs_bitmap* bmp)
{
	int i;

	for (i = 0; i < bmp->bm_h * bmp->bm_w; i++) {
		if (bmp->bm_data[i] == 0)
			bmp->bm_data[i] = 255;
		else if (bmp->bm_data[i] == 255)
			bmp->bm_data[i] = 0;
	}
}
#endif

#define XLAT_SIZE MAX_BITMAP_FILES

//piggytable::piggytable() {
//	init();
//}

void piggytable::Init() {

	gameSounds.reserve(MAX_SOUND_FILES);
	soundOffsets.reserve(MAX_SOUND_FILES);
	gameBitmaps.reserve(MAX_BITMAP_FILES);
	aliases.reserve(MAX_ALIASES);
	bitmapFiles.reserve(MAX_BITMAP_FILES);
	soundFiles.reserve(MAX_SOUND_FILES);
	gameBitmapOffsets.reserve(MAX_BITMAP_FILES);
	gameBitmapFlags.reserve(MAX_BITMAP_FILES);
	
	gameBitmapXlat.resize(XLAT_SIZE); //we actually want the elements here
	for (int i = 0; i < XLAT_SIZE; i++) {
		gameBitmapXlat[i] = i;
	}

	/*hashtable_init(&bitmapNames, MAX_BITMAP_FILES);
	hashtable_init(&soundNames, MAX_SOUND_FILES);*/

}

void piggytable::SetActive() {
	//piggy_close();
	activePiggyTable = this;
}

piggytable::~piggytable() {

	/*hashtable_free(&bitmapNames);
	hashtable_free(&soundNames);*/

	if (file)
	{
		cfclose(file);
	}

}

bitmap_index piggy_register_bitmap(grs_bitmap* bmp, const char* name, int in_file, uint8_t flag, int offset)
{
	bitmap_index temp;
	//Assert(Num_bitmap_files < MAX_BITMAP_FILES);

	auto Num_bitmap_files = activePiggyTable->bitmapFiles.size();

	temp.index = Num_bitmap_files;

	if (!in_file)
	{
#ifdef EDITOR
		if (FindArg("-macdata"))
			swap_0_255(bmp);
#endif
		if (!BigPig)  
			gr_bitmap_rle_compress(bmp);
		Num_bitmap_files_new++;
	}

	BitmapFile file;
	strncpy(file.name, name, 12);
	//hashtable_insert(&activePiggyTable->bitmapNames, file.name, activePiggyTable->gameBitmaps.size());
	activePiggyTable->bitmapNames[file.name] = activePiggyTable->bitmapFiles.size();
	activePiggyTable->bitmapFiles.push_back(file);

	//activePiggyTable->gameBitmaps[Num_bitmap_files] = *bmp;
	activePiggyTable->gameBitmaps.push_back(*bmp);
	if (!in_file)
	{
		activePiggyTable->gameBitmapOffsets[Num_bitmap_files] = 0;
		activePiggyTable->gameBitmapFlags[Num_bitmap_files] = bmp->bm_flags;
	}

	activePiggyTable->gameBitmapFlags.push_back(flag);
	activePiggyTable->gameBitmapOffsets.push_back(offset);
	//Num_bitmap_files++;

	return temp;
}

int piggy_register_sound(digi_sound* snd, const char* name, int in_file)
{
	int i;

	//Assert(Num_sound_files < MAX_SOUND_FILES);

	SoundFile file;
	strncpy(file.name, name, 12);
	activePiggyTable->soundFiles.push_back(file);
	//hashtable_insert(&activePiggyTable->soundNames, file.name, activePiggyTable->soundFiles.size());
	activePiggyTable->soundNames[file.name] = activePiggyTable->soundFiles.size();
	activePiggyTable->gameSounds.push_back(*snd);
	if (!in_file) {
		activePiggyTable->soundOffsets[activePiggyTable->gameSounds.size() - 1] = 0;
		Num_sound_files_new++;
	}

	//Num_sound_files++;
	return activePiggyTable->gameSounds.size() - 1;
}

bitmap_index piggy_find_bitmap(char* name)
{
	bitmap_index bmp;
	int i;
	char* t;

	bmp.index = 0;

	if (currentGame == G_DESCENT_2) { //D1 never used alias table, skip
		if ((t = strchr(name, '#')) != NULL)
			*t = 0;

		for (i = 0; i < activePiggyTable->aliases.size(); i++)
			if (_strfcmp(name, activePiggyTable->aliases[i].alias_name) == 0)
			{
				if (t) //extra stuff for ABMs
				{
					static char temp[FILENAME_LEN];
					_splitpath(activePiggyTable->aliases[i].file_name, NULL, NULL, temp, NULL);
					name = temp;
					strcat(name, "#");
					strcat(name, t + 1);
				}
				else
					name = activePiggyTable->aliases[i].file_name;
				break;
			}

		if (t)
			*t = '#';
	}

	//i = hashtable_search(&activePiggyTable->bitmapNames, name);

	/*if (!activePiggyTable->bitmapNames.contains(name))
		return bmp;

	i = activePiggyTable->bitmapNames[name];
	/*Assert(i != 0);
	if (i < 0)
		return bmp;*\/

	bmp.index = i;
	return bmp;*/

	try {
		bmp.index = activePiggyTable->bitmapNames.at(name);
	} catch (std::out_of_range) {
		bmp.index = 0;
	}

	return bmp;

}

int piggy_find_sound(const char* name)
{
	//int i;


	//i = hashtable_search(&activePiggyTable->soundNames, const_cast<char*>(name));

	//if (i < 0)
	//if (!activePiggyTable->soundNames.contains(name))
	//	return 255;

	//return activePiggyTable->soundNames[name];

	try {
		return activePiggyTable->soundNames.at(name); 
	} catch (std::out_of_range) {
		return 255;
	}

}

//CFILE* Piggy_fp = NULL;

#define FILENAME_LEN 13

char Current_pigfile[FILENAME_LEN] = "";

void piggy_close_file()
{
	/*if (activePiggyTable->file)
	{
		cfclose(activePiggyTable->file);
		activePiggyTable->file = NULL;
		Current_pigfile[0] = 0;
	}*/
}

//int Pigfile_initialized = 0;

#define PIGFILE_ID              'GIPP'          //PPIG
#define PIGFILE_VERSION         2

extern char CDROM_dir[];

int request_cd(void);


//copies a pigfile from the CD to the current dir
//retuns file handle of new pig
CFILE* copy_pigfile_from_cd(const char* filename)
{
	Error("Cannot copy PIG file from CD. stub function\n");
	return NULL;
}


//initialize a pigfile, reading headers
//returns the size of all the bitmap data
void piggy_init_pigfile(const char* filename)
{
	int i;
	char temp_name[16];
	char temp_name_read[16];
	grs_bitmap temp_bitmap;
	DiskBitmapHeader bmh;
	int header_size, N_bitmaps, data_size, data_start;
#ifdef MACINTOSH
	char name[255];		// filename + path for the mac
#elif defined(CHOCOLATE_USE_LOCALIZED_PATHS)
	char name[CHOCOLATE_MAX_FILE_PATH_SIZE];
#endif

	piggy_close_file();             //close old pig if still open

#ifdef SHAREWARE                //rename pigfile for shareware
	if (strfcmp(filename, DEFAULT_PIGFILE_REGISTERED) == 0)
		filename = DEFAULT_PIGFILE_SHAREWARE;
#endif

#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
	get_full_file_path(name, filename, CHOCOLATE_SYSTEM_FILE_DIR);
	activePiggyTable->file = cfopen(name, "rb");
#else
	activePiggyTable->file = cfopen(filename, "rb");
#endif

	if (!activePiggyTable->file)
	{
#ifdef EDITOR
		return;         //if editor, ok to not have pig, because we'll build one
#else
		//activePiggyTable->file = copy_pigfile_from_cd(filename); //[ISB] no
#endif
	}

	if (activePiggyTable->file) //make sure pig is valid type file & is up-to-date
	{
		int pig_id, pig_version;

		pig_id = cfile_read_int(activePiggyTable->file);
		pig_version = cfile_read_int(activePiggyTable->file);
		if (pig_id > 0x10000 && pig_id < 0x100000 && pig_version > 100 && pig_version < 800) {
			cfseek(activePiggyTable->file, pig_id, SEEK_SET);
		}
		else if (pig_id != PIGFILE_ID || pig_version != PIGFILE_VERSION)
		{
			cfclose(activePiggyTable->file);              //out of date pig
			activePiggyTable->file = NULL;                        //..so pretend it's not here
		}
	}

	if (!activePiggyTable->file)
	{
#ifdef EDITOR
		return;         //if editor, ok to not have pig, because we'll build one
#else
		Error("Cannot load required file <%s>", filename);
#endif
	}

	strncpy(Current_pigfile, filename, sizeof(Current_pigfile));
	N_bitmaps = cfile_read_int(activePiggyTable->file);

	int N_sounds = (currentGame == G_DESCENT_1 ? cfile_read_int(activePiggyTable->file) : 0);

	header_size = N_bitmaps * (currentGame == G_DESCENT_1 ? BITMAP_HEADER_SIZE_D1 : BITMAP_HEADER_SIZE_D2);
	data_start = header_size + cftell(activePiggyTable->file) + N_sounds * 20;
	data_size = cfilelength(activePiggyTable->file) - data_start;
	//Num_bitmap_files = 1;

	if (currentGame != G_DESCENT_1) {
		for (i = 0; i < N_bitmaps; i++)
		{
			cfread(bmh.name, 8, 1, activePiggyTable->file);
			bmh.dflags = cfile_read_byte(activePiggyTable->file);
			bmh.width = cfile_read_byte(activePiggyTable->file);
			bmh.height = cfile_read_byte(activePiggyTable->file);
			bmh.wh_extra = (currentGame == G_DESCENT_2 ? cfile_read_byte(activePiggyTable->file) : 0);
			bmh.flags = cfile_read_byte(activePiggyTable->file);
			bmh.avg_color = cfile_read_byte(activePiggyTable->file);
			bmh.offset = cfile_read_int(activePiggyTable->file);

			memcpy(temp_name_read, bmh.name, 8);
			temp_name_read[8] = 0;
			if (bmh.dflags & DBM_FLAG_ABM)
				snprintf(temp_name, 16, "%s#%d", temp_name_read, bmh.dflags & 63);
			else
				strcpy(temp_name, temp_name_read);
			memset(&temp_bitmap, 0, sizeof(grs_bitmap));
			temp_bitmap.bm_w = temp_bitmap.bm_rowsize = bmh.width + ((short)(bmh.wh_extra & 0x0f) << 8);
			temp_bitmap.bm_h = bmh.height + ((short)(bmh.wh_extra & 0xf0) << 4);
			temp_bitmap.bm_flags = BM_FLAG_PAGED_OUT;
			temp_bitmap.avg_color = bmh.avg_color;
			temp_bitmap.bm_data = Piggy_bitmap_cache_data;

			uint8_t flag = 0;
			//GameBitmapFlags[i + 1] = 0;
			if (bmh.flags & BM_FLAG_TRANSPARENT) flag |= BM_FLAG_TRANSPARENT;
			if (bmh.flags & BM_FLAG_SUPER_TRANSPARENT) flag |= BM_FLAG_SUPER_TRANSPARENT;
			if (bmh.flags & BM_FLAG_NO_LIGHTING) flag |= BM_FLAG_NO_LIGHTING;
			if (bmh.flags & BM_FLAG_RLE) flag |= BM_FLAG_RLE;
			if (bmh.flags & BM_FLAG_RLE_BIG) flag |= BM_FLAG_RLE_BIG;

			int offset = bmh.offset + data_start;
			//Assert((i + 1) == Num_bitmap_files); 
			piggy_register_bitmap(&temp_bitmap, temp_name, 1, flag, offset);
		}
	}

#ifdef EDITOR
	Piggy_bitmap_cache_size = data_size + (data_size / 10);   //extra mem for new bitmaps
	Assert(Piggy_bitmap_cache_size > 0);
#else
	Piggy_bitmap_cache_size = PIGGY_BUFFER_SIZE;
#endif
	BitmapBits = (uint8_t*)mem_malloc(Piggy_bitmap_cache_size);
	if (BitmapBits == NULL)
		Error("Not enough memory to load bitmaps\n");
	Piggy_bitmap_cache_data = BitmapBits;
	Piggy_bitmap_cache_next = 0;

#if defined(MACINTOSH) && defined(SHAREWARE)
	//	load_exit_models();
#endif

	activePiggyTable->piggyInitialized = true;
}

#define FILENAME_LEN 13
#define MAX_BITMAPS_PER_BRUSH 30

extern int compute_average_pixel(grs_bitmap * newbm);

//reads in a new pigfile (for new palette)
//returns the size of all the bitmap data
void piggy_new_pigfile(const char* pigname)
{
	int i;
	char temp_name[16];
	char temp_name_read[16];
	grs_bitmap temp_bitmap;
	DiskBitmapHeader bmh;
	int header_size, N_bitmaps, data_size, data_start;
	int must_rewrite_pig = 0;
#ifdef MACINTOSH
	char name[255];
#elif defined(CHOCOLATE_USE_LOCALIZED_PATHS)
	char name[CHOCOLATE_MAX_FILE_PATH_SIZE];
#endif

	if (CurrentDataVersion == DataVer::DEMO)             //rename pigfile for shareware
	{
		if (_strfcmp(pigname, DEFAULT_PIGFILE_REGISTERED) == 0)
			pigname = DEFAULT_PIGFILE_SHAREWARE;
	}

	if (_strnfcmp(Current_pigfile, pigname, sizeof(Current_pigfile)) == 0)
		return;         //already have correct pig

	if (!activePiggyTable->piggyInitialized) //have we ever opened a pigfile?
	{
		piggy_init_pigfile(pigname);            //..no, so do initialization stuff
		return;
	}
	else if (currentGame == G_DESCENT_1) {
		printf("init new in d1");
		return;
	}

	piggy_close_file();             //close old pig if still open

	Piggy_bitmap_cache_next = 0;            //free up cache

	strncpy(Current_pigfile, pigname, sizeof(Current_pigfile));

#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
	get_full_file_path(name, pigname, CHOCOLATE_SYSTEM_FILE_DIR);
	activePiggyTable->file = cfopen(name, "rb");
#else
	activePiggyTable->file = cfopen(pigname, "rb");
#endif

#ifndef EDITOR
	//if (!activePiggyTable->file)
	//	activePiggyTable->file = copy_pigfile_from_cd(pigname);
#endif

	if (activePiggyTable->file) //make sure pig is valid type file & is up-to-date
	{
		int pig_id, pig_version;

		cfseek(activePiggyTable->file, 0, SEEK_SET);

		pig_id = cfile_read_int(activePiggyTable->file);
		pig_version = cfile_read_int(activePiggyTable->file);
		if (pig_id > 0x10000 && pig_id < 0x100000 && pig_version > 100 && pig_version < 800) {
			cfseek(activePiggyTable->file, pig_id, SEEK_SET);
		} else if (pig_id != PIGFILE_ID || pig_version != PIGFILE_VERSION) {
			cfclose(activePiggyTable->file);              //out of date pig
			activePiggyTable->file = NULL;                        //..so pretend it's not here
		}
	}

#ifndef EDITOR
	if (!activePiggyTable->file) Error("activePiggyTable->file not defined in piggy_new_pigfile.");
#endif

	if (activePiggyTable->file)
	{

		N_bitmaps = cfile_read_int(activePiggyTable->file);
		header_size = N_bitmaps * BITMAP_HEADER_SIZE;
		data_start = header_size + cftell(activePiggyTable->file);
		data_size = cfilelength(activePiggyTable->file) - data_start;

		for (i = 1; i <= N_bitmaps; i++)
		{
			cfread(bmh.name, 8, 1, activePiggyTable->file);
			bmh.dflags = cfile_read_byte(activePiggyTable->file);
			bmh.width = cfile_read_byte(activePiggyTable->file);
			bmh.height = cfile_read_byte(activePiggyTable->file);
			bmh.wh_extra = cfile_read_byte(activePiggyTable->file);
			bmh.flags = cfile_read_byte(activePiggyTable->file);
			bmh.avg_color = cfile_read_byte(activePiggyTable->file);
			bmh.offset = cfile_read_int(activePiggyTable->file);

			memcpy(temp_name_read, bmh.name, 8);
			temp_name_read[8] = 0;

			if (bmh.dflags & DBM_FLAG_ABM)
				snprintf(temp_name, 16, "%s#%d", temp_name_read, bmh.dflags & 63);
			else
				strcpy(temp_name, temp_name_read);

			//Make sure name matches
			if (strcmp(temp_name, activePiggyTable->bitmapFiles[i].name))
			{
				//Int3();       //this pig is out of date.  Delete it
				must_rewrite_pig = 1;
			}

			strcpy(activePiggyTable->bitmapFiles[i].name, temp_name);

			memset(&temp_bitmap, 0, sizeof(grs_bitmap));

			temp_bitmap.bm_w = temp_bitmap.bm_rowsize = bmh.width + ((short)(bmh.wh_extra & 0x0f) << 8);
			temp_bitmap.bm_h = bmh.height + ((short)(bmh.wh_extra & 0xf0) << 4);
			temp_bitmap.bm_flags = BM_FLAG_PAGED_OUT;
			temp_bitmap.avg_color = bmh.avg_color;
			temp_bitmap.bm_data = Piggy_bitmap_cache_data;

			activePiggyTable->gameBitmapFlags[i] = 0;

			if (bmh.flags & BM_FLAG_TRANSPARENT) activePiggyTable->gameBitmapFlags[i] |= BM_FLAG_TRANSPARENT;
			if (bmh.flags & BM_FLAG_SUPER_TRANSPARENT) activePiggyTable->gameBitmapFlags[i] |= BM_FLAG_SUPER_TRANSPARENT;
			if (bmh.flags & BM_FLAG_NO_LIGHTING) activePiggyTable->gameBitmapFlags[i] |= BM_FLAG_NO_LIGHTING;
			if (bmh.flags & BM_FLAG_RLE) activePiggyTable->gameBitmapFlags[i] |= BM_FLAG_RLE;
			if (bmh.flags & BM_FLAG_RLE_BIG) activePiggyTable->gameBitmapFlags[i] |= BM_FLAG_RLE_BIG;

			activePiggyTable->gameBitmapOffsets[i] = bmh.offset + data_start;

			activePiggyTable->gameBitmaps[i] = temp_bitmap;
		}
	}
	else
		N_bitmaps = 0;          //no pigfile, so no bitmaps

#ifndef EDITOR

	//Assert(N_bitmaps == Num_bitmap_files - 1);

#else

	if (must_rewrite_pig || (N_bitmaps < Num_bitmap_files - 1)) {
		int size;

		//re-read the bitmaps that aren't in this pig

		for (i = N_bitmaps + 1; i < Num_bitmap_files; i++) {
			char* p;

			p = strchr(AllBitmaps[i].name, '#');

			if (p) {                //this is an ABM
				char abmname[FILENAME_LEN];
				int fnum;
				grs_bitmap* bm[MAX_BITMAPS_PER_BRUSH];
				int iff_error;          //reference parm to avoid warning message
				uint8_t newpal[768];
				char basename[FILENAME_LEN];
				int nframes;

				strcpy(basename, AllBitmaps[i].name);
				basename[p - AllBitmaps[i].name] = 0;             //cut off "#nn" part

				snprintf(abmname, FILENAME_LEN, "%s.abm", basename);

				iff_error = iff_read_animbrush(abmname, bm, MAX_BITMAPS_PER_BRUSH, &nframes, newpal);

				if (iff_error != IFF_NO_ERROR) {
					mprintf((1, "File %s - IFF error: %s", abmname, iff_errormsg(iff_error)));
					Error("File %s - IFF error: %s", abmname, iff_errormsg(iff_error));
				}

				for (fnum = 0; fnum < nframes; fnum++) {
					char tempname[20];
					int SuperX;

					snprintf(tempname, 20, "%s#%d", basename, fnum);

					//SuperX = (GameBitmaps[i+fnum].bm_flags&BM_FLAG_SUPER_TRANSPARENT)?254:-1;
					SuperX = (GameBitmapFlags[i + fnum] & BM_FLAG_SUPER_TRANSPARENT) ? 254 : -1;
					//above makes assumption that supertransparent color is 254

					if (iff_has_transparency)
						gr_remap_bitmap_good(bm[fnum], newpal, iff_transparent_color, SuperX);
					else
						gr_remap_bitmap_good(bm[fnum], newpal, -1, SuperX);

					bm[fnum]->avg_color = compute_average_pixel(bm[fnum]);

#ifdef EDITOR
					if (FindArg("-macdata"))
						swap_0_255(bm[fnum]);
#endif
					if (!BigPig) gr_bitmap_rle_compress(bm[fnum]);

					if (bm[fnum]->bm_flags & BM_FLAG_RLE)
						size = *((int*)bm[fnum]->bm_data);
					else
						size = bm[fnum]->bm_w * bm[fnum]->bm_h;

					memcpy(&Piggy_bitmap_cache_data[Piggy_bitmap_cache_next], bm[fnum]->bm_data, size);
					mem_free(bm[fnum]->bm_data);
					bm[fnum]->bm_data = &Piggy_bitmap_cache_data[Piggy_bitmap_cache_next];
					Piggy_bitmap_cache_next += size;

					GameBitmaps[i + fnum] = *bm[fnum];

					// -- mprintf( (0, "U" ));
					mem_free(bm[fnum]);
				}

				i += nframes - 1;         //filled in multiple bitmaps
			}
			else {          //this is a BBM

				grs_bitmap* newbm;
				uint8_t newpal[256 * 3];
				int iff_error;
				char bbmname[FILENAME_LEN];
				int SuperX;

				MALLOC(newbm, grs_bitmap, 1);

				snprintf(bbmname, FILENAME_LEN, "%s.bbm", AllBitmaps[i].name);
				iff_error = iff_read_bitmap(bbmname, newbm, BM_LINEAR, newpal);

				//newbm->bm_handle = 0;
				if (iff_error != IFF_NO_ERROR) 
				{
					mprintf((1, "File %s - IFF error: %s", bbmname, iff_errormsg(iff_error)));
					Error("File %s - IFF error: %s", bbmname, iff_errormsg(iff_error));
				}

				SuperX = (GameBitmapFlags[i] & BM_FLAG_SUPER_TRANSPARENT) ? 254 : -1;
				//above makes assumption that supertransparent color is 254

				if (iff_has_transparency)
					gr_remap_bitmap_good(newbm, newpal, iff_transparent_color, SuperX);
				else
					gr_remap_bitmap_good(newbm, newpal, -1, SuperX);

				newbm->avg_color = compute_average_pixel(newbm);

#ifdef EDITOR
				if (FindArg("-macdata"))
					swap_0_255(newbm);
#endif
				if (!BigPig)  gr_bitmap_rle_compress(newbm);

				if (newbm->bm_flags & BM_FLAG_RLE)
					size = *((int*)newbm->bm_data);
				else
					size = newbm->bm_w * newbm->bm_h;

				memcpy(&Piggy_bitmap_cache_data[Piggy_bitmap_cache_next], newbm->bm_data, size);
				mem_free(newbm->bm_data);
				newbm->bm_data = &Piggy_bitmap_cache_data[Piggy_bitmap_cache_next];
				Piggy_bitmap_cache_next += size;

				GameBitmaps[i] = *newbm;

				mem_free(newbm);

				// -- mprintf( (0, "U" ));
			}
		}

		//@@Dont' do these things which are done when writing
		//@@for (i=0; i < Num_bitmap_files; i++ )       {
		//@@    bitmap_index bi;
		//@@    bi.index = i;
		//@@    PIGGY_PAGE_IN( bi );
		//@@}
		//@@
		//@@piggy_close_file();

		piggy_write_pigfile(pigname);

		Current_pigfile[0] = 0;                 //say no pig, to force reload

		piggy_new_pigfile(pigname);             //read in just-generated pig
	}
#endif  //ifdef EDITOR

}

uint8_t bogus_data[64 * 64];
grs_bitmap bogus_bitmap;
//uint8_t bogus_bitmap_initialized = 0;
digi_sound bogus_sound;

extern void bm_read_all(CFILE* fp);

#define HAMFILE_ID              '!MAH'          //HAM!
#define HAMFILE_VERSION 3
//version 1 -> 2:  save marker_model_num
//version 2 -> 3:  removed sound files

#define SNDFILE_ID              'DNSD'          //DSND
#define SNDFILE_VERSION 1

int read_hamfile()
{
	CFILE* ham_fp = NULL;
	int ham_id, ham_version;
	int expected_ham_version = HAMFILE_VERSION;
#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
	char name[CHOCOLATE_MAX_FILE_PATH_SIZE];
#else
	char name[256];
#endif

	strcpy(name, DEFAULT_HAMFILE);

	if (CurrentDataVersion == DataVer::DEMO)
	{
		strcpy(name, "d2demo.ham");
		expected_ham_version = 2;
	}

#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
	get_full_file_path(name, DEFAULT_HAMFILE, CHOCOLATE_SYSTEM_FILE_DIR);
#endif
	ham_fp = cfopen(name, "rb");

	if (ham_fp == NULL) 
	{
		Must_write_hamfile = 1;
		return 0;
	}

	//make sure ham is valid type file & is up-to-date
	ham_id = cfile_read_int(ham_fp);
	ham_version = cfile_read_int(ham_fp);
	if (ham_id != HAMFILE_ID || ham_version != expected_ham_version)
	{
		Must_write_hamfile = 1;
		cfclose(ham_fp);						//out of date ham
		return 0;
	}

	if (CurrentDataVersion == DataVer::DEMO)
		cfile_read_int(ham_fp); //skip data ptr

#ifndef EDITOR
	{
		bm_read_all(ham_fp);  // Note connection to above if!!!
		uint16_t xlatRaw[MAX_BITMAP_FILES];
		cfread(xlatRaw, sizeof(uint16_t) * MAX_BITMAP_FILES, 1, ham_fp);
		activePiggyTable->gameBitmapXlat.reserve(MAX_BITMAP_FILES);
		//for (auto& e : xlatRaw) {
		for (int i = 0; i < MAX_BITMAP_FILES; i++) {
			//activePiggyTable->gameBitmapXlat.push_back(e);
			activePiggyTable->gameBitmapXlat[i] = xlatRaw[i];
		}
	}
#endif

	cfclose(ham_fp);

	return 1;
}

int read_sndfile()
{
	CFILE* snd_fp = NULL;
	int snd_id, snd_version;
	int N_sounds;
	int sound_start;
	int header_size;
	int i, size, length;
	DiskSoundHeader sndh;
	digi_sound temp_sound;
	char temp_name_read[16];
	int sbytes = 0;
	int data_offset;

	int expected_header = SNDFILE_ID;
	int expected_version = SNDFILE_VERSION;

#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
	char name[CHOCOLATE_MAX_FILE_PATH_SIZE];
#else
	char name[256];
#endif

	if (CurrentDataVersion == DataVer::DEMO)
	{
		expected_header = HAMFILE_ID;
		expected_version = 2;
	}

#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
	if (CurrentDataVersion == DataVer::DEMO)
		get_full_file_path(name, "d2demo.ham", CHOCOLATE_SYSTEM_FILE_DIR);
	else
		get_full_file_path(name, DEFAULT_SNDFILE, CHOCOLATE_SYSTEM_FILE_DIR);
#else
	if (CurrentDataVersion == DataVer::DEMO)
		strcpy(name, "d2demo.ham");
	else
		strcpy(name, DEFAULT_SNDFILE);
#endif

	snd_fp = cfopen(name, "rb");

	if (snd_fp == NULL)
		return 0;

	//make sure soundfile is valid type file & is up-to-date
	snd_id = cfile_read_int(snd_fp);
	snd_version = cfile_read_int(snd_fp);
	if (snd_id != expected_header || snd_version != expected_version)
	{
		cfclose(snd_fp);						//out of date sound file
		return 0;
	}

	if (CurrentDataVersion == DataVer::DEMO)
	{
		data_offset = cfile_read_int(snd_fp);
		cfseek(snd_fp, data_offset, SEEK_SET);
	}

	N_sounds = cfile_read_int(snd_fp);

	sound_start = cftell(snd_fp);
	size = cfilelength(snd_fp) - sound_start;
	length = size;
	mprintf((0, "\nReading data (%d KB) ", size / 1024));

	header_size = N_sounds * SOUND_HEADER_SIZE;

	//Read sounds

	for (i = 0; i < N_sounds; i++)
	{
		cfread(sndh.name, 8, 1, snd_fp);
		sndh.length = cfile_read_int(snd_fp);
		sndh.data_length = cfile_read_int(snd_fp);
		sndh.offset = cfile_read_int(snd_fp);
		//		cfread( &sndh, sizeof(DiskSoundHeader), 1, snd_fp );
				//size -= sizeof(DiskSoundHeader);
		temp_sound.length = sndh.length;
		temp_sound.data = (uint8_t*)(sndh.offset + header_size + sound_start);
		//SoundOffset[Num_sound_files] = sndh.offset + header_size + sound_start;
		activePiggyTable->soundOffsets.push_back(sndh.offset + header_size + sound_start);
		memcpy(temp_name_read, sndh.name, 8);
		temp_name_read[8] = 0;
		piggy_register_sound(&temp_sound, temp_name_read, 1);
#ifdef MACINTOSH
		if (piggy_is_needed(i))
#endif		// note link to if.
			sbytes += sndh.length;
		//mprintf(( 0, "%d bytes of sound\n", sbytes ));
	}

	SoundBits = (uint8_t*)mem_malloc(sbytes + 16);
	if (SoundBits == NULL)
		Error("Not enough memory to load sounds\n");

	mprintf((0, "\nBitmaps: %d KB   Sounds: %d KB\n", Piggy_bitmap_cache_size / 1024, sbytes / 1024));

	//	piggy_read_sounds(snd_fp);

	cfclose(snd_fp);

	return 1;
}

int PiggyInitD1()
{
	int sbytes = 0;
	char temp_name_read[16];
	char temp_name[16];
	grs_bitmap temp_bitmap;
	digi_sound temp_sound;
	DiskBitmapHeader bmh;
	DiskSoundHeader sndh;
	int header_size, N_bitmaps, N_sounds;
	int i, size, length, x, y;
	const char* filename;
	int read_sounds = 1;
	int Pigdata_start;
#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
	char filename_full_path[CHOCOLATE_MAX_FILE_PATH_SIZE];
#endif

	if (FindArg("-nosound") || (digi_driver_board < 1)) 
	{
		read_sounds = 0;
		mprintf((0, "Not loading sound data!!!!!\n"));
	}

	activePiggyTable->gameBitmapXlat.resize(MAX_BITMAP_FILES_D1);

	for (i = 0; i < MAX_BITMAP_FILES_D1; i++) 
	{
		activePiggyTable->gameBitmapXlat[i] = i;
		//activePiggyTable->gameBitmaps[i].bm_flags = BM_FLAG_PAGED_OUT;
	}

	if (!activePiggyTable->bogusInitialized) 
	{
		int i;
		uint8_t c;
		activePiggyTable->bogusInitialized = true;
		memset(&bogus_bitmap, 0, sizeof(grs_bitmap));
		bogus_bitmap.bm_w = bogus_bitmap.bm_h = bogus_bitmap.bm_rowsize = 64;
		bogus_bitmap.bm_data = bogus_data;
		c = gr_find_closest_color(0, 0, 63);
		for (i = 0; i < 4096; i++) bogus_data[i] = c;
		c = gr_find_closest_color(63, 0, 0);
		// Make a big red X !
		for (i = 0; i < 64; i++) 
		{
			bogus_data[i * 64 + i] = c;
			bogus_data[i * 64 + (63 - i)] = c;
		}
		piggy_register_bitmap(&bogus_bitmap, "bogus", 1, BM_FLAG_PAGED_OUT, 0);
		bogus_sound.length = 64 * 64;
		bogus_sound.data = bogus_data;
		activePiggyTable->gameBitmapOffsets[0] = 0;
	}

#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
	get_full_file_path(filename_full_path, "descent.pig", CHOCOLATE_SYSTEM_FILE_DIR);
#else
	filename = "DESCENT.PIG";
#endif

	if (FindArg("-bigpig"))
		BigPig = 1;

	if (FindArg("-lowmem"))
		piggy_low_memory = 1;

	if (FindArg("-nolowmem"))
		piggy_low_memory = 0;

	if (piggy_low_memory)
		digi_lomem = 1;

	if ((i = FindArg("-piggy"))) 
	{
		filename = Args[i + 1];
		mprintf((0, "Using alternate pigfile, '%s'\n", filename));
	}
#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
	activePiggyTable->file = cfopen(filename_full_path, "rb");
#else
	activePiggyTable->file = cfopen(filename, "rb");
#endif
	if (activePiggyTable->file == NULL) return 0;

	//cfread(&Pigdata_start, sizeof(int), 1, activePiggyTable->file);
	Pigdata_start = cfile_read_int(activePiggyTable->file);
#ifdef EDITOR
	if (FindArg("-nobm"))
#endif
	{
		bm_read_all(activePiggyTable->file);	// Note connection to above if!!!
		uint16_t xlat[MAX_BITMAP_FILES];
		cfread(xlat, sizeof(uint16_t) * MAX_BITMAP_FILES, 1, activePiggyTable->file);
		activePiggyTable->gameBitmapXlat.assign(xlat, xlat + MAX_BITMAP_FILES);
	}

	cfseek(activePiggyTable->file, Pigdata_start, SEEK_SET);
	size = cfilelength(activePiggyTable->file) - Pigdata_start;
	length = size;
	mprintf((0, "\nReading data (%d KB) ", size / 1024));

	cfread(&N_bitmaps, sizeof(int), 1, activePiggyTable->file);
	size -= sizeof(int);
	cfread(&N_sounds, sizeof(int), 1, activePiggyTable->file);
	size -= sizeof(int);

	header_size = (N_bitmaps * BITMAP_HEADER_SIZE_D1) + (N_sounds * SOUND_HEADER_SIZE);

	x = 60; y = 189;

	gr_set_curfont(Gamefonts[GFONT_SMALL]);
	gr_set_fontcolor(gr_find_closest_color_current(20, 20, 20), -1);
	gr_printf(0x8000, y - 10, "%s...", TXT_LOADING_DATA);
	plat_present_canvas(0);

	printf("\n   D1: %d bitmaps to read\nStarting with %d bitmaps\n", N_bitmaps, activePiggyTable->gameBitmaps.size());

	for (i = 0; i < N_bitmaps; i++) 
	{
		cfread(&bmh.name[0], 8 * sizeof(char), 1, activePiggyTable->file);
		bmh.dflags = cfile_read_byte(activePiggyTable->file);
		bmh.width = cfile_read_byte(activePiggyTable->file);
		bmh.height = cfile_read_byte(activePiggyTable->file);
		bmh.flags = cfile_read_byte(activePiggyTable->file);
		bmh.avg_color = cfile_read_byte(activePiggyTable->file);
		bmh.offset = cfile_read_int(activePiggyTable->file);
		memcpy(temp_name_read, bmh.name, 8);
		temp_name_read[8] = 0;
		if (bmh.dflags & DBM_FLAG_ABM)
			snprintf(temp_name, 16, "%s#%d", temp_name_read, bmh.dflags & 63);
		else
			strncpy(temp_name, temp_name_read, 16);
		memset(&temp_bitmap, 0, sizeof(grs_bitmap));
		if (bmh.dflags & DBM_FLAG_LARGE)
			temp_bitmap.bm_w = temp_bitmap.bm_rowsize = bmh.width + 256;
		else
			temp_bitmap.bm_w = temp_bitmap.bm_rowsize = bmh.width;
		temp_bitmap.bm_h = bmh.height;
		temp_bitmap.bm_flags = BM_FLAG_PAGED_OUT;
		temp_bitmap.avg_color = bmh.avg_color;
		temp_bitmap.bm_data = Piggy_bitmap_cache_data;

		//activePiggyTable->gameBitmapFlags[i + 1] = 0;
		uint8_t flags = 0;
		if (bmh.flags & BM_FLAG_TRANSPARENT) flags |= BM_FLAG_TRANSPARENT;
		if (bmh.flags & BM_FLAG_SUPER_TRANSPARENT) flags |= BM_FLAG_SUPER_TRANSPARENT;
		if (bmh.flags & BM_FLAG_NO_LIGHTING) flags |= BM_FLAG_NO_LIGHTING;
		if (bmh.flags & BM_FLAG_RLE) flags |= BM_FLAG_RLE;

		int offset = bmh.offset + header_size + (sizeof(int) * 2) + Pigdata_start;
		//Assert((i + 1) == Num_bitmap_files);
		piggy_register_bitmap(&temp_bitmap, temp_name, 1, flags, offset);
	}

	printf("Finished with %d bitmaps\n", activePiggyTable->gameBitmaps.size());

	for (i = 0; i < N_sounds; i++) 
	{
		//cfread(&sndh, sizeof(DiskSoundHeader), 1, activePiggyTable->file);
		//[ISB] fix platform bugs, hopefully
		cfread(&sndh.name, 8 * sizeof(char), 1, activePiggyTable->file);
		sndh.length = cfile_read_int(activePiggyTable->file);
		sndh.data_length = cfile_read_int(activePiggyTable->file);
		sndh.offset = cfile_read_int(activePiggyTable->file);
		//size -= sizeof(DiskSoundHeader);
		temp_sound.length = sndh.length;
		temp_sound.data = (uint8_t*)(sndh.offset + header_size + (sizeof(int) * 2) + Pigdata_start);
		activePiggyTable->soundOffsets.push_back(sndh.offset + header_size + (sizeof(int) * 2) + Pigdata_start);
		memcpy(temp_name_read, sndh.name, 8);
		temp_name_read[8] = 0;
		piggy_register_sound(&temp_sound, temp_name_read, 1);
		sbytes += sndh.length;
		//mprintf(( 0, "%d bytes of sound\n", sbytes ));
	}

	ClearPiggyCache();

	SoundBits = (uint8_t*)malloc(sbytes + 16);
	if (SoundBits == NULL)
		Error("Not enough memory to load DESCENT.PIG sounds\n");

#ifdef EDITOR
	Piggy_bitmap_cache_size = size - header_size - sbytes + 16;
	Assert(Piggy_bitmap_cache_size > 0);
#else
	Piggy_bitmap_cache_size = PIGGY_BUFFER_SIZE;
#endif
	BitmapBits = (uint8_t*)malloc(Piggy_bitmap_cache_size);
	if (BitmapBits == NULL)
		Error("Not enough memory to load DESCENT.PIG bitmaps\n");
	Piggy_bitmap_cache_data = BitmapBits;
	Piggy_bitmap_cache_next = 0;

	mprintf((0, "\nBitmaps: %d KB   Sounds: %d KB\n", Piggy_bitmap_cache_size / 1024, sbytes / 1024));

	atexit(piggy_close_file);

	//	mprintf( (0, "<<<<Paging in all piggy bitmaps...>>>>>" ));
	//	for (i=0; i < Num_bitmap_files; i++ )	{
	//		bitmap_index bi;
	//		bi.index = i;
	//		PIGGY_PAGE_IN( bi );
	//	}
	//	mprintf( (0, "\n (USed %d / %d KB)\n", Piggy_bitmap_cache_next/1024, (size - header_size - sbytes + 16)/1024 ));
	//	key_getch();

	return 1;
}

int PiggyInitD2()
{
	int ham_ok = 0, snd_ok = 0;
	int i;

	if (!activePiggyTable->bogusInitialized)
	{
		int i;
		uint8_t c;
		activePiggyTable->bogusInitialized = true;
		memset(&bogus_bitmap, 0, sizeof(grs_bitmap));
		bogus_bitmap.bm_w = bogus_bitmap.bm_h = bogus_bitmap.bm_rowsize = 64;
		bogus_bitmap.bm_data = bogus_data;
		c = gr_find_closest_color(0, 0, 63);
		for (i = 0; i < 4096; i++) bogus_data[i] = c;
		c = gr_find_closest_color(63, 0, 0);
		// Make a big red X !
		for (i = 0; i < 64; i++)
		{
			bogus_data[i * 64 + i] = c;
			bogus_data[i * 64 + (63 - i)] = c;
		}
		piggy_register_bitmap(&bogus_bitmap, "bogus", 1, 0, 0);
		bogus_sound.length = 64 * 64;
		bogus_sound.data = bogus_data;
	}

	if (FindArg("-bigpig"))
		BigPig = 1;

	if (FindArg("-lowmem"))
		piggy_low_memory = 1;

	if (FindArg("-nolowmem"))
		piggy_low_memory = 0;

	if (piggy_low_memory)
		digi_lomem = 1;

	if ((i = FindArg("-piggy")))
		Error("-piggy no longer supported");

	WIN(DDGRLOCK(dd_grd_curcanv));
	gr_set_curfont(SMALL_FONT);
	gr_set_fontcolor(gr_find_closest_color_current(20, 20, 20), -1);
	gr_printf(0x8000, grd_curcanv->cv_h - 20, "%s...", TXT_LOADING_DATA);
	WIN(DDGRUNLOCK(dd_grd_curcanv));

#ifdef EDITOR
	piggy_init_pigfile(DEFAULT_PIGFILE);
#endif

	ham_ok = read_hamfile();

	snd_ok = read_sndfile();

	atexit(piggy_close);

	mprintf((0, "HamOk=%d SndOk=%d\n", ham_ok, snd_ok));
	return (ham_ok && snd_ok);               //read ok
}

int piggy_init() {
	if (currentGame == G_DESCENT_1)
		return PiggyInitD1();
	else if (currentGame == G_DESCENT_2)
		return PiggyInitD2();
	else
		Int3();
}

int piggy_is_needed(int soundnum)
{
	int i;

	if (!digi_lomem) return 1;

	for (i = 0; i < MAX_SOUNDS; i++)
	{
		if ((activeBMTable->altSounds[i] < 255) && (activeBMTable->sounds[activeBMTable->altSounds[i]] == soundnum))
			return 1;
	}
	return 0;
}

void ReadSoundsD1() {
	uint8_t* ptr;
	int i, sbytes;

	ptr = SoundBits;
	sbytes = 0;

	for (i = 0; i < activePiggyTable->soundFiles.size(); i++)
	{
		digi_sound* snd = &activePiggyTable->gameSounds[i];
		if (activePiggyTable->soundOffsets[i] > 0) 
		{
			if (piggy_is_needed(i)) 
			{
				cfseek(activePiggyTable->file, activePiggyTable->soundOffsets[i], SEEK_SET);

				// Read in the sound data!!!
				snd->data = ptr;
				ptr += snd->length;
				sbytes += snd->length;
				cfread(snd->data, snd->length, 1, activePiggyTable->file);
			}
		}
	}

	mprintf((0, "\nActual Sound usage: %d KB\n", sbytes / 1024));

}

void ReadSoundsD2(void) {
	CFILE* fp = NULL;
	uint8_t* ptr;
	int i, sbytes;
	int expected_header = SNDFILE_ID;
	int expected_version = SNDFILE_VERSION;

#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
	char name[CHOCOLATE_MAX_FILE_PATH_SIZE];
#else
	char name[256];
#endif

	if (CurrentDataVersion == DataVer::DEMO)
	{
		expected_header = HAMFILE_ID;
		expected_version = 2;
	}

	ptr = SoundBits;
	sbytes = 0;

#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
	if (CurrentDataVersion == DataVer::DEMO)
		get_full_file_path(name, "d2demo.ham", CHOCOLATE_SYSTEM_FILE_DIR);
	else
		get_full_file_path(name, DEFAULT_SNDFILE, CHOCOLATE_SYSTEM_FILE_DIR);
#else
	if (CurrentDataVersion == DataVer::DEMO)
		strcpy(name, "d2demo.ham");
	else
		strcpy(name, DEFAULT_SNDFILE);
#endif

	fp = cfopen(name, "rb");

	if (fp == NULL)
		return;

	for (i = 0; i < activePiggyTable->soundFiles.size(); i++)
	{
		digi_sound* snd = &activePiggyTable->gameSounds[i];

		if (activePiggyTable->soundOffsets[i] > 0) 
		{
			if (piggy_is_needed(i))
			{
				cfseek(fp, activePiggyTable->soundOffsets[i], SEEK_SET);

				// Read in the sound data!!!
				snd->data = ptr;
				ptr += snd->length;
				sbytes += snd->length;
				cfread(snd->data, snd->length, 1, fp);
			}
			else
				snd->data = (uint8_t*)-1;
		}
	}

	cfclose(fp);

	mprintf((0, "\nActual Sound usage: %d KB\n", sbytes / 1024));

}

void piggy_read_sounds() {
	if (currentGame == G_DESCENT_1)
		ReadSoundsD1();
	else if (currentGame == G_DESCENT_2)
		ReadSoundsD2();
	else
		Int3();
}

extern int descent_critical_error;
extern unsigned descent_critical_deverror;
extern unsigned descent_critical_errcode;

const char* crit_errors[13] = { "Write Protected", "Unknown Unit", "Drive Not Ready", "Unknown Command", "CRC Error", \
"Bad struct length", "Seek Error", "Unknown media type", "Sector not found", "Printer out of paper", "Write Fault", \
"Read fault", "General Failure" };

void piggy_critical_error()
{
	grs_canvas* save_canv;
	grs_font* save_font;
	int i;
	save_canv = grd_curcanv;
	save_font = grd_curcanv->cv_font;
	gr_palette_load(gr_palette);
	i = nm_messagebox("Disk Error", 2, "Retry", "Exit", "%s\non drive %c:", crit_errors[descent_critical_errcode & 0xf], (descent_critical_deverror & 0xf) + 'A');
	if (i == 1)
		exit(1);
	gr_set_current_canvas(save_canv);
	grd_curcanv->cv_font = save_font;
}

void piggy_bitmap_page_in(bitmap_index bitmap)
{
	grs_bitmap* bmp;
	int i, org_i, temp;

	i = bitmap.index;
	Assert(i >= 0);
	//Assert(i < MAX_BITMAP_FILES);
	Assert(i < activePiggyTable->bitmapFiles.size());
	Assert(Piggy_bitmap_cache_size > 0);

	if (i < 1) return;
	if (i >= MAX_BITMAP_FILES) return;
	if (i >= activePiggyTable->bitmapFiles.size()) return;

	if (activePiggyTable->gameBitmapOffsets[i] == 0) return;         // A read-from-disk bitmap!!!

	if (piggy_low_memory) {
		org_i = i;
		i = activePiggyTable->gameBitmapXlat[i];          // Xlat for low-memory settings!
	}

	bmp = &activePiggyTable->gameBitmaps[i];

	if (bmp->bm_flags & BM_FLAG_PAGED_OUT) {
		stop_time();

	ReDoIt:
		descent_critical_error = 0;
		cfseek(activePiggyTable->file, activePiggyTable->gameBitmapOffsets[i], SEEK_SET);
		if (descent_critical_error) {
			piggy_critical_error();
			goto ReDoIt;
		}

		bmp->bm_data = &Piggy_bitmap_cache_data[Piggy_bitmap_cache_next];
		bmp->bm_flags = activePiggyTable->gameBitmapFlags[i];

		//printf("\n SC %d %d %d", activePiggyTable->gameBitmaps.size(), activePiggyTable->gameBitmapFlags.size(), activePiggyTable->gameBitmapOffsets.size());

		//printf("\n BMP: %hd %hd %hd %hd %hhu %hhu %hd / %hu %hhu", bmp->bm_x, bmp->bm_y, bmp->bm_w, bmp->bm_h, bmp->bm_type, bmp->bm_flags, bmp->bm_rowsize, bmp->bm_selector, bmp->avg_color);

		if (bmp->bm_flags & BM_FLAG_RLE)
		{
			int zsize = 0;
			descent_critical_error = 0;
			zsize = cfile_read_int(activePiggyTable->file);
			if (descent_critical_error)
			{
				piggy_critical_error();
				goto ReDoIt;
			}

			// GET JOHN NOW IF YOU GET THIS ASSERT!!!
			//Assert( Piggy_bitmap_cache_next+zsize < Piggy_bitmap_cache_size );      
			if (Piggy_bitmap_cache_next + zsize >= Piggy_bitmap_cache_size)
			{
				printf("\n Oh no! %d %d %d\n", Piggy_bitmap_cache_next, zsize, Piggy_bitmap_cache_size);
				Int3();
				piggy_bitmap_page_out_all();
				goto ReDoIt;
			}
			memcpy(&Piggy_bitmap_cache_data[Piggy_bitmap_cache_next], &zsize, sizeof(int));
			Piggy_bitmap_cache_next += sizeof(int);
			descent_critical_error = 0;
			temp = cfread(&Piggy_bitmap_cache_data[Piggy_bitmap_cache_next], 1, zsize - 4, activePiggyTable->file);
			if (descent_critical_error)
			{
				piggy_critical_error();
				goto ReDoIt;
			}
			Piggy_bitmap_cache_next += zsize - 4;
		}
		else
		{
			// GET JOHN NOW IF YOU GET THIS ASSERT!!!
			//Assert(Piggy_bitmap_cache_next + (bmp->bm_h * bmp->bm_w) < Piggy_bitmap_cache_size);
			if (Piggy_bitmap_cache_next + (bmp->bm_h * bmp->bm_w) >= Piggy_bitmap_cache_size) {
				printf("\n Oh no! %d %d %d\n", Piggy_bitmap_cache_next, (bmp->bm_h * bmp->bm_w), Piggy_bitmap_cache_size);
				Int3();
				piggy_bitmap_page_out_all();
				goto ReDoIt;
			}
			descent_critical_error = 0;
			temp = cfread(&Piggy_bitmap_cache_data[Piggy_bitmap_cache_next], 1, bmp->bm_h * bmp->bm_w, activePiggyTable->file);
			if (descent_critical_error) {
				piggy_critical_error();
				goto ReDoIt;
			}
			Piggy_bitmap_cache_next += bmp->bm_h * bmp->bm_w;
		}

		//@@if ( bmp->bm_selector ) {
		//@@#if !defined(WINDOWS) && !defined(MACINTOSH)
		//@@	if (!dpmi_modify_selector_base( bmp->bm_selector, bmp->bm_data ))
		//@@		Error( "Error modifying selector base in piggy.c\n" );
		//@@#endif
		//@@}

		start_time();
	}

	if (piggy_low_memory) {
		if (org_i != i)
			activePiggyTable->gameBitmaps[org_i] = activePiggyTable->gameBitmaps[i];
	}

	//@@Removed from John's code:
	//@@#ifndef WINDOWS
	//@@    if ( bmp->bm_selector ) {
	//@@            if (!dpmi_modify_selector_base( bmp->bm_selector, bmp->bm_data ))
	//@@                    Error( "Error modifying selector base in piggy.c\n" );
	//@@    }
	//@@#endif

}

void piggy_bitmap_page_out_all()
{
	int i;

	Piggy_bitmap_cache_next = 0;

	piggy_page_flushed++;

	texmerge_flush();
	rle_cache_flush();

	for (i = 0; i < activePiggyTable->bitmapFiles.size(); i++) {
		if (activePiggyTable->gameBitmapOffsets[i] > 0) {       // Don't page out bitmaps read from disk!!!
			activePiggyTable->gameBitmaps[i].bm_flags = BM_FLAG_PAGED_OUT;
			activePiggyTable->gameBitmaps[i].bm_data = Piggy_bitmap_cache_data;
		}
	}

	mprintf((0, "Flushing piggy bitmap cache\n"));
}

void piggy_load_level_data()
{
	piggy_bitmap_page_out_all();
	paging_touch_all();
}

#ifdef EDITOR

void change_filename_ext(char* dest, const char* src, const char* ext);

void piggy_write_pigfile(const char* filename)
{
	FILE* pig_fp;
	int bitmap_data_start, data_offset;
	DiskBitmapHeader bmh;
	int org_offset;
	char subst_name[32];
	int i;
	FILE* fp1, * fp2;
	char tname[FILENAME_LEN];
#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
	char filename_full_path[CHOCOLATE_MAX_FILE_PATH_SIZE];
#endif

	// -- mprintf( (0, "Paging in all piggy bitmaps..." ));
	for (i = 0; i < Num_bitmap_files; i++) {
		bitmap_index bi;
		bi.index = i;
		PIGGY_PAGE_IN(bi);
	}
	// -- mprintf( (0, "\n" ));

	piggy_close_file();

	// -- mprintf( (0, "Creating %s...",filename ));

#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
	get_full_file_path(filename_full_path, filename, CHOCOLATE_SYSTEM_FILE_DIR);
	pig_fp = fopen(filename_full_path, "wb");
#else
	pig_fp = fopen(filename, "wb");       //open PIG file
#endif
	Assert(pig_fp != NULL);

	write_int(PIGFILE_ID, pig_fp);
	write_int(PIGFILE_VERSION, pig_fp);

	Num_bitmap_files--;
	fwrite(&Num_bitmap_files, sizeof(int), 1, pig_fp);
	Num_bitmap_files++;

	bitmap_data_start = ftell(pig_fp);
	bitmap_data_start += (Num_bitmap_files - 1) * sizeof(DiskBitmapHeader);
	data_offset = bitmap_data_start;

#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
	change_filename_ext(tname, filename, "lst");
	get_full_file_path(filename_full_path, tname, CHOCOLATE_SYSTEM_FILE_DIR);
	fp1 = fopen(filename_full_path, "wt");
	change_filename_ext(tname, filename, "all");
	get_full_file_path(filename_full_path, tname, CHOCOLATE_SYSTEM_FILE_DIR);
	fp2 = fopen(filename_full_path, "wt");
#else
	change_filename_ext(tname, filename, "lst");
	fp1 = fopen(tname, "wt");
	change_filename_ext(tname, filename, "all");
	fp2 = fopen(tname, "wt");
#endif

	for (i = 1; i < Num_bitmap_files; i++) {
		int* size;
		grs_bitmap* bmp;

		{
			char* p, * p1;
			p = strchr(AllBitmaps[i].name, '#');
			if (p) {
				int n;
				p1 = p; p1++;
				n = atoi(p1);
				*p = 0;
				if (fp2 && n == 0)
					fprintf(fp2, "%s.abm\n", AllBitmaps[i].name);
				memcpy(bmh.name, AllBitmaps[i].name, 8);
				Assert(n <= 63);
				bmh.dflags = DBM_FLAG_ABM + n;
				*p = '#';
			}
			else {
				if (fp2)
					fprintf(fp2, "%s.bbm\n", AllBitmaps[i].name);
				memcpy(bmh.name, AllBitmaps[i].name, 8);
				bmh.dflags = 0;
			}
		}
		bmp = &GameBitmaps[i];

		Assert(!(bmp->bm_flags & BM_FLAG_PAGED_OUT));

		if (fp1)
			fprintf(fp1, "BMP: %s, size %d bytes", AllBitmaps[i].name, bmp->bm_rowsize * bmp->bm_h);
		org_offset = ftell(pig_fp);
		bmh.offset = data_offset - bitmap_data_start;
		fseek(pig_fp, data_offset, SEEK_SET);

		if (bmp->bm_flags & BM_FLAG_RLE) {
			size = (int*)bmp->bm_data;
			fwrite(bmp->bm_data, sizeof(uint8_t), *size, pig_fp);
			data_offset += *size;
			if (fp1)
				fprintf(fp1, ", and is already compressed to %d bytes.\n", *size);
		}
		else {
			fwrite(bmp->bm_data, sizeof(uint8_t), bmp->bm_rowsize * bmp->bm_h, pig_fp);
			data_offset += bmp->bm_rowsize * bmp->bm_h;
			if (fp1)
				fprintf(fp1, ".\n");
		}
		fseek(pig_fp, org_offset, SEEK_SET);
		Assert(GameBitmaps[i].bm_w < 4096);
		bmh.width = (GameBitmaps[i].bm_w & 0xff);
		bmh.wh_extra = ((GameBitmaps[i].bm_w >> 8) & 0x0f);
		Assert(GameBitmaps[i].bm_h < 4096);
		bmh.height = GameBitmaps[i].bm_h;
		bmh.wh_extra |= ((GameBitmaps[i].bm_h >> 4) & 0xf0);
		bmh.flags = GameBitmaps[i].bm_flags;
		if (piggy_is_substitutable_bitmap(AllBitmaps[i].name, subst_name)) {
			bitmap_index other_bitmap;
			other_bitmap = piggy_find_bitmap(subst_name);
			activePiggyTable->gameBitmapXlat[i] = other_bitmap.index;
			bmh.flags |= BM_FLAG_PAGED_OUT;
			//mprintf(( 0, "Skipping bitmap %d\n", i ));
			//mprintf(( 0, "Marking '%s' as substitutible\n", AllBitmaps[i].name ));
		}
		else {
			bmh.flags &= ~BM_FLAG_PAGED_OUT;
		}
		bmh.avg_color = GameBitmaps[i].avg_color;
		fwrite(&bmh, sizeof(DiskBitmapHeader), 1, pig_fp);                    // Mark as a bitmap
	}

	fclose(pig_fp);

	mprintf((0, " Dumped %d assorted bitmaps.\n", Num_bitmap_files));
	fprintf(fp1, " Dumped %d assorted bitmaps.\n", Num_bitmap_files);

	fclose(fp1);
	fclose(fp2);

}

static void write_int(int i, FILE* file)
{
	if (fwrite(&i, sizeof(i), 1, file) != 1)
		Error("Error reading int in gamesave.c");

}

void piggy_dump_all()
{
	int i, xlat_offset;
	FILE* ham_fp;
	int org_offset, data_offset;
	DiskSoundHeader sndh;
	int sound_data_start;
	FILE* fp1, * fp2;
#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
	char filename_full_path[CHOCOLATE_MAX_FILE_PATH_SIZE];
#endif

#ifdef NO_DUMP_SOUNDS
	Num_sound_files = 0;
	Num_sound_files_new = 0;
#endif

	if (!Must_write_hamfile && (Num_bitmap_files_new == 0) && (Num_sound_files_new == 0))
		return;

	if ((i = FindArg("-piggy")))
		Error("-piggy no longer supported");

#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
	get_full_file_path(filename_full_path, "ham.lst", CHOCOLATE_SYSTEM_FILE_DIR);
	fp1 = fopen(filename_full_path, "wt");
	get_full_file_path(filename_full_path, "ham.all", CHOCOLATE_SYSTEM_FILE_DIR);
	fp2 = fopen(filename_full_path, "wt");
#else
	fp1 = fopen("ham.lst", "wt");
	fp2 = fopen("ham.all", "wt");
#endif

	if (Must_write_hamfile || Num_bitmap_files_new) {

		mprintf((0, "Creating %s...", DEFAULT_HAMFILE));

#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
		get_full_file_path(filename_full_path, DEFAULT_HAMFILE, CHOCOLATE_SYSTEM_FILE_DIR);
		ham_fp = fopen(filename_full_path, "wb");
#else
		ham_fp = fopen(DEFAULT_HAMFILE, "wb");                       //open HAM file
#endif
		Assert(ham_fp != NULL);

		write_int(HAMFILE_ID, ham_fp);
		write_int(HAMFILE_VERSION, ham_fp);

		bm_write_all(ham_fp);
		xlat_offset = ftell(ham_fp);
		fwrite(GameBitmapXlat, sizeof(uint16_t) * MAX_BITMAP_FILES, 1, ham_fp);
		//Dump bitmaps

		if (Num_bitmap_files_new)
			piggy_write_pigfile(DEFAULT_PIGFILE);

		//free up memeory used by new bitmaps
		for (i = Num_bitmap_files - Num_bitmap_files_new; i < Num_bitmap_files; i++)
			mem_free(GameBitmaps[i].bm_data);

		//next thing must be done after pig written
		fseek(ham_fp, xlat_offset, SEEK_SET);
		fwrite(GameBitmapXlat, sizeof(uint16_t) * MAX_BITMAP_FILES, 1, ham_fp);

		fclose(ham_fp);
		mprintf((0, "\n"));
	}

	if (Num_sound_files_new) 
	{
		mprintf((0, "Creating %s...", DEFAULT_HAMFILE));
		// Now dump sound file
#if defined (CHOCOLATE_USE_LOCALIZED_PATHS)
		get_full_file_path(filename_full_path, DEFAULT_SNDFILE, CHOCOLATE_SYSTEM_FILE_DIR);
		ham_fp = fopen(filename_full_path, "wb");
#else
		ham_fp = fopen(DEFAULT_SNDFILE, "wb");
#endif
		Assert(ham_fp != NULL);

		write_int(SNDFILE_ID, ham_fp);
		write_int(SNDFILE_VERSION, ham_fp);

		fwrite(&Num_sound_files, sizeof(int), 1, ham_fp);

		mprintf((0, "\nDumping sounds..."));

		sound_data_start = ftell(ham_fp);
		sound_data_start += Num_sound_files * sizeof(DiskSoundHeader);
		data_offset = sound_data_start;

		for (i = 0; i < Num_sound_files; i++) {
			digi_sound* snd;

			snd = &GameSounds[i];
			strcpy(sndh.name, AllSounds[i].name);
			sndh.length = GameSounds[i].length;
			sndh.offset = data_offset - sound_data_start;

			org_offset = ftell(ham_fp);
			fseek(ham_fp, data_offset, SEEK_SET);

			sndh.data_length = GameSounds[i].length;
			fwrite(snd->data, sizeof(uint8_t), snd->length, ham_fp);
			data_offset += snd->length;
			fseek(ham_fp, org_offset, SEEK_SET);
			fwrite(&sndh, sizeof(DiskSoundHeader), 1, ham_fp);                    // Mark as a bitmap

			fprintf(fp1, "SND: %s, size %d bytes\n", AllSounds[i].name, snd->length);
			fprintf(fp2, "%s.raw\n", AllSounds[i].name);
		}

		fclose(ham_fp);
		mprintf((0, "\n"));

		//[ISB] needs to be here, otherwise it crashes in debug since these variables aren't initalized
		fprintf(fp1, "Total sound size: %d bytes\n", data_offset - sound_data_start);
		mprintf((0, " Dumped %d assorted sounds.\n", Num_sound_files));
		fprintf(fp1, " Dumped %d assorted sounds.\n", Num_sound_files);
	}

	fclose(fp1);
	fclose(fp2);

	// Never allow the game to run after building ham.
	exit(0);
}

#endif

void ClearPiggyCache() {
	if (BitmapBits) {
		mem_free(BitmapBits);
		BitmapBits = NULL;
	}

	if (SoundBits) {
		mem_free(SoundBits);
		SoundBits = NULL;
	}
}

void piggy_close()
{
	piggy_close_file();

	ClearPiggyCache();

}

int piggy_does_bitmap_exist_slow(char* name)
{
	int i;

	for (i = 0; i < activePiggyTable->bitmapFiles.size(); i++) {
		if (!strcmp(activePiggyTable->bitmapFiles[i].name, name))
			return 1;
	}
	return 0;
}


#define NUM_GAUGE_BITMAPS 23
const char* gauge_bitmap_names[NUM_GAUGE_BITMAPS] = {
	"gauge01", "gauge01b",
	"gauge02", "gauge02b",
	"gauge06", "gauge06b",
	"targ01", "targ01b",
	"targ02", "targ02b",
	"targ03", "targ03b",
	"targ04", "targ04b",
	"targ05", "targ05b",
	"targ06", "targ06b",
	"gauge18", "gauge18b",
	"gauss1", "helix1",
	"phoenix1"
};


int piggy_is_gauge_bitmap(char* base_name)
{
	int i;
	for (i = 0; i < NUM_GAUGE_BITMAPS; i++) {
		if (!_stricmp(base_name, gauge_bitmap_names[i]))
			return 1;
	}

	return 0;
}

int piggy_is_substitutable_bitmap(char* name, char* subst_name)
{
	int frame;
	char* p;
	char base_name[16];

	strcpy(subst_name, name);
	p = strchr(subst_name, '#');
	if (p) {
		frame = atoi(&p[1]);
		*p = 0;
		strcpy(base_name, subst_name);
		if (!piggy_is_gauge_bitmap(base_name)) {
			sprintf(subst_name, "%s#%d", base_name, frame + 1);
			if (piggy_does_bitmap_exist_slow(subst_name)) {
				if (frame & 1) {
					sprintf(subst_name, "%s#%d", base_name, frame - 1);
					return 1;
				}
			}
		}
	}
	strcpy(subst_name, name);
	return 0;
}



#ifdef WINDOWS
//	New Windows stuff

//	windows bitmap page in
//		Page in a bitmap, if ddraw, then page it into a ddsurface in 
//		'video' memory.  if that fails, page it in normally.

void piggy_bitmap_page_in_w(bitmap_index bitmap, int ddraw)
{
}


//	Essential when switching video modes!

void piggy_bitmap_page_out_all_w()
{
}


#endif


