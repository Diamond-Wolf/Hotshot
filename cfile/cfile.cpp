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
COPYRIGHT 1993-1998 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

#include "platform/platform_filesys.h"
#include "platform/posixstub.h"
#include "cfile/cfile.h"
#include "mem/mem.h"
#include "misc/error.h"
#include "fix/fix.h"
#include "vecmat/vecmat.h"

#include "main/inferno.h"
#include "main/songs.h" //for D1 descent.sng patch hack
#include "platform/mono.h"

extern bool noHog;

typedef struct hogfile
{
	char	name[13];
	int	offset;
	int 	length;
} hogfile;

#define MAX_HOGFILES 250

#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
#define HOG_FILENAME_MAX CHOCOLATE_MAX_FILE_PATH_SIZE
#else
#define HOG_FILENAME_MAX 64
#endif

hogfile D1HogFiles[MAX_HOGFILES];
bool d1HogInitialized = 0;
int numD1Hogfiles = 0;

hogfile D2HogFiles[MAX_HOGFILES];
bool d2HogInitialized = 0;
int Num_hogfiles = 0;

hogfile AltHogFiles[MAX_HOGFILES];
bool AltHogfile_initialized = 0;
int AltNum_hogfiles = 0;

hogfile VertigoHogFiles[MAX_HOGFILES];
bool VertigoHogfile_initialized = 0;
int VertigoNum_hogfiles = 0;

char d1HogFilename[HOG_FILENAME_MAX];
char d2HogFilename[HOG_FILENAME_MAX];
char AltHogFilename[HOG_FILENAME_MAX];
char vertigoHogFilename[HOG_FILENAME_MAX];

char AltHogDir[HOG_FILENAME_MAX];
bool AltHogdir_initialized = 0;

void cfile_use_alternate_hogdir(const char* path)
{
	if (path)
	{
		strncpy(AltHogDir, path, HOG_FILENAME_MAX-1);
		AltHogdir_initialized = 1;
	}
	else 
	{
		AltHogdir_initialized = 0;
	}
}

//extern int descent_critical_error;

FILE* cfile_get_filehandle(const char* filename, const char* mode)
{
	FILE* fp;

#ifdef CHOCOLATE_USE_LOCALIZED_PATHS
	char temp[CHOCOLATE_MAX_FILE_PATH_SIZE];
#else
	char temp[HOG_FILENAME_MAX * 2];
#endif

#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
	/*if (!_strnicmp(filename, "descent.hog", 11)) { //Check data dir instead for descent.hog
			get_full_file_path(temp, "descent.hog", CHOCOLATE_SYSTEM_FILE_DIR);
			return cfile_get_filehandle(temp, mode);
	}*/

	get_full_file_path(temp, filename, CHOCOLATE_SYSTEM_FILE_DIR);
	fp = fopen(temp, mode);

	if (!fp) {
		get_full_file_path(temp, filename, CHOCOLATE_MISSIONS_DIR);
		fp = fopen(temp, mode);
	}
#else
	fp = fopen(filename, mode);

	if (!fp) {
#ifdef _WIN32
		snprintf(temp, HOG_FILENAME_MAX * 2, ".\\missions\\%s", filename);
#else
		snprintf(temp, HOG_FILENAME_MAX * 2, "./missions/%s", filename);
#endif
		fp = fopen(temp, mode);
	}
#endif

#ifndef _WINDOWS
	if (!fp)
	{
		//[ISB] Try either uppercase or lowercase conversion if the file isn't found. 
		strncpy(temp, filename, HOG_FILENAME_MAX);
		_strupr(temp);
		fp = fopen(temp, mode);
		if (!fp)
		{
			strncpy(temp, filename, HOG_FILENAME_MAX);
			_strlwr(temp);
			fp = fopen(temp, mode);
		}
		
	}
#endif

	if ((fp == NULL) && (AltHogdir_initialized)) 
	{
		strncpy(temp, AltHogDir, HOG_FILENAME_MAX);
		strncat(temp, AltHogDir, HOG_FILENAME_MAX);
		fp = fopen(temp, mode);
	}

	return fp;
}

int cfile_init_hogfile(const char* fname, hogfile* hog_files, int* nfiles)
{
	char id[4];
	FILE* fp;
	int i, len;

	*nfiles = 0;

	fp = cfile_get_filehandle(fname, "rb");
	if (fp == NULL)
	{
		Warning("cfile_init_hogfile: Can't open hogfile: %s\n", fname);
		return 0;
	}

	fread(id, 3, 1, fp);
	id[3] = '\0'; //[ISB] solve compiler warning
	if (strncmp(id, "DHF", 3)) 
	{
		fclose(fp);
		return 0;
	}

	while (1)
	{
		if (*nfiles >= MAX_HOGFILES)
		{
			Warning("ERROR: HOGFILE IS LIMITED TO %d FILES\n", MAX_HOGFILES);
			fclose(fp);
			exit(1);
		}
		i = fread(hog_files[*nfiles].name, 13, 1, fp);
		if (i != 1) 
		{
			fclose(fp);
			return 1;
		}
		i = fread(&len, 4, 1, fp);
		if (i != 1) 
		{
			fclose(fp);
			return 0;
		}
		hog_files[*nfiles].length = (len);
		if (hog_files[*nfiles].length < 0)
			Warning("Hogfile length < 0");
		hog_files[*nfiles].offset = ftell(fp);
		*nfiles = (*nfiles) + 1;
		// Skip over
		i = fseek(fp, (len), SEEK_CUR);
	}
	return 0;
}

//Specify the name of the hogfile.  Returns 1 if hogfile found & had files
int cfile_init_d2(const char* hogname)
{
	Assert(d2HogInitialized == 0);

	if (cfile_init_hogfile(hogname, D2HogFiles, &Num_hogfiles)) 
	{
		strcpy(d2HogFilename, hogname);
		d2HogInitialized = 1;
		return 1;
	}
	else
		return 0;	//not loaded!
}

int cfile_init_d1(const char* hogname)
{
	Assert(d1HogInitialized == 0);

	if (cfile_init_hogfile(hogname, D1HogFiles, &numD1Hogfiles))
	{
		strcpy(d1HogFilename, hogname);
		d1HogInitialized = 1;
		return 1;
	}
	else
		return 0;	//not loaded!
}

int cfile_init_vertigo(const char* hogname)
{
	if (!d2HogInitialized)
		return 0;

	Assert(VertigoHogfile_initialized == 0);

	if (cfile_init_hogfile(hogname, VertigoHogFiles, &VertigoNum_hogfiles))
	{
		strcpy(vertigoHogFilename, hogname);
		VertigoHogfile_initialized = 1;
		return 1;
	}
	else
		return 0;	//not loaded!
}

FILE* FindFileInD1(const char* name, int* length) {

	if (!d1HogInitialized)
		return NULL;

	//Hack to patch v1.5 descent.sng if needed
	if (currentGame == G_DESCENT_1 && !_strnicmp(name, "descent.sng", 11))
		needToMaybePatchD1SongFile = true;

	FILE* fp;
	for (int i = 0; i < numD1Hogfiles; i++)
	{
		if (!_stricmp(D1HogFiles[i].name, name))
		{
			fp = cfile_get_filehandle(d1HogFilename, "rb");
			if (fp == NULL) return NULL;
			fseek(fp, D1HogFiles[i].offset, SEEK_SET);
			*length = D1HogFiles[i].length;
			return fp;
		}
	}

	needToMaybePatchD1SongFile = false;
	return NULL;

}

FILE* FindFileInD2(const char* name, int* length) {

	if (!d2HogInitialized)
		return NULL;

	FILE* fp;
	for (int i = 0; i < Num_hogfiles; i++) 
	{
		if (!_stricmp(D2HogFiles[i].name, name))
		{
			fp = cfile_get_filehandle(d2HogFilename, "rb");
			if (fp == NULL) return NULL;
			fseek(fp, D2HogFiles[i].offset, SEEK_SET);
			*length = D2HogFiles[i].length;
			return fp;
		}
	}

	return NULL;

}

FILE* FindFileInVertigo(const char* name, int* length) {

	if (!VertigoHogfile_initialized)
		return NULL;

	FILE* fp;
	for (int i = 0; i < Num_hogfiles; i++)
	{
		if (!_stricmp(VertigoHogFiles[i].name, name))
		{
			fp = cfile_get_filehandle(vertigoHogFilename, "rb");
			if (fp == NULL) return NULL;
			fseek(fp, VertigoHogFiles[i].offset, SEEK_SET);
			*length = VertigoHogFiles[i].length;
			return fp;
		}
	}

	return NULL;

}

FILE* cfile_find_libfile(const char* name, int* length)
{
	FILE* fp;
	int i;

	if (AltHogfile_initialized) 
	{
		for (i = 0; i < AltNum_hogfiles; i++) 
		{
			if (!_stricmp(AltHogFiles[i].name, name))
			{
				fp = cfile_get_filehandle(AltHogFilename, "rb");
				if (fp == NULL) return NULL;
				fseek(fp, AltHogFiles[i].offset, SEEK_SET);
				*length = AltHogFiles[i].length;
				return fp;
			}
		}
	}

	if (currentGame == G_DESCENT_1) { //Search the correct game's files first

		//mprintf((1, "\nCurrent game is D1, trying D1 first (%s)\n", name));

		fp = FindFileInD1(name, length);
		if (fp) 
			return fp;

		fp = FindFileInVertigo(name, length);
		if (fp)
			return fp;

		return FindFileInD2(name, length);

	} else {

		//mprintf((1, "\nCurrent game is D2, trying D2 first (%s)\n", name));

		fp = FindFileInVertigo(name, length);
		if (fp)
			return fp;

		fp = FindFileInD2(name, length);
		if (fp)
			return fp;

		return FindFileInD1(name, length);

	}
	
	return NULL;
}

int cfile_use_alternate_hogfile(const char* name)
{
	if (name)
	{
		strncpy(AltHogFilename, name, HOG_FILENAME_MAX-1);
		cfile_init_hogfile(AltHogFilename, AltHogFiles, &AltNum_hogfiles);
		AltHogfile_initialized = 1;
		return (AltNum_hogfiles > 0);
	}
	else 
	{
		AltHogfile_initialized = 0;
		return 1;
	}
}

int cfexist(const char* filename)
{
	int length;
	FILE* fp;

	//[ISB] descent 2 code for release
	if (filename[0] != '\x01')
		fp = cfile_get_filehandle(filename, "rb");		// Check for non-hog file first...
	else 
	{
		fp = NULL;		//don't look in dir, only in hogfile
		filename++;
	}

	if (fp) 
	{
		fclose(fp);
		return 1;
	}

	fp = cfile_find_libfile(filename, &length);
	if (fp)
	{
		fclose(fp);
		return 2;		// file found in hog
	}

	return 0;		// Couldn't find it.
}

CFILE* cfopen(const char* filename, const char* mode)
{
	int length;
	FILE* fp;
	CFILE* cfile;
#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
	char new_filename[CHOCOLATE_MAX_FILE_PATH_SIZE], * p;
#else
	char new_filename[256], * p;
#endif

	if (_stricmp(mode, "rb")) 
	{
		Warning("CFILES CAN ONLY BE OPENED WITH RB\n");
		exit(1);
	}

#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
	memset(new_filename, 0, CHOCOLATE_MAX_FILE_PATH_SIZE);
	strncpy(new_filename, filename, CHOCOLATE_MAX_FILE_PATH_SIZE - 1);
#else
	memset(new_filename, 0, 256);
	strncpy(new_filename, filename, 255);
#endif
	while ((p = strchr(new_filename, 13)))
		* p = '\0';

	while ((p = strchr(new_filename, 10)))
		* p = '\0';

	if (filename[0] == '\x01')
		filename++;

	fp = cfile_find_libfile(filename, &length);
	if (fp) {
		cfile = (CFILE*)mem_malloc(sizeof(CFILE));
		if (cfile != NULL)
		{
			cfile->file = fp;
			cfile->size = length;
			cfile->lib_offset = ftell(fp);
			cfile->raw_position = 0;
			return cfile;
		} else {
			fclose(fp);
			return NULL;
		}
	} 

	fp = cfile_get_filehandle(filename, "rb");
	if (!fp)
		return NULL;

	cfile = (CFILE*)mem_malloc(sizeof(CFILE));
	if (cfile == NULL) 
	{
		fclose(fp);
		return NULL;
	}
	cfile->file = fp;
	cfile->size = _filelength(_fileno(fp));
	cfile->lib_offset = 0;
	cfile->raw_position = 0;
	return cfile;
}

int cfilelength(CFILE* fp)
{
	return fp->size;
}

int cfgetc(CFILE* fp)
{
	int c;

	if (fp->raw_position >= fp->size) return EOF;

	c = getc(fp->file);
	if (c != EOF)
		fp->raw_position++;

	//	Assert( fp->raw_position==(ftell(fp->file)-fp->lib_offset) );

	return c;
}

char* cfgets(char* buf, size_t n, CFILE* fp)
{
	char* t = buf;
	int i;
	int c;

	if (fp->raw_position >= fp->size)
	{
		*buf = 0;
		return NULL;
	}

	for (i = 0; i < (int)(n - 1); i++) 
	{
		do {
			c = fgetc(fp->file);
			fp->raw_position++;
		} while (c == 13 && fp->raw_position < fp->size);

		if (fp->raw_position >= fp->size)
			break;

		*buf++ = c;
		if (c == 10)
			c = '\n';
		if (c == '\n') break;
	}

	*buf++ = 0;
	return  t;
}

size_t cfread(void* buf, size_t elsize, size_t nelem, CFILE* fp)
{
	int i;
	if ((int)(fp->raw_position + (elsize * nelem)) > fp->size) return EOF;
	i = fread(buf, elsize, nelem, fp->file);
	fp->raw_position += i * elsize;
	return i;
}


int cftell(CFILE* fp)
{
	return fp->raw_position;
}

int cfseek(CFILE* fp, long int offset, int where)
{
	int c, goal_position;

	switch (where) 
	{
	case SEEK_SET:
		goal_position = offset;
		break;
	case SEEK_CUR:
		goal_position = fp->raw_position + offset;
		break;
	case SEEK_END:
		goal_position = fp->size + offset;
		break;
	default:
		return 1;
	}
	c = fseek(fp->file, fp->lib_offset + goal_position, SEEK_SET);
	fp->raw_position = ftell(fp->file) - fp->lib_offset;
	return c;
}

void cfclose(CFILE* fp)
{
	fclose(fp->file);
	mem_free(fp);
	return;
}

uint8_t cfile_read_byte(CFILE* fp)
{
	uint8_t b;
	size_t size = cfread(&b, sizeof(uint8_t), 1, fp);
	if (size < 1)
		Error("cfile_read_byte: read %d bytes, expected 1.", size);
	return b;
}

short cfile_read_short(CFILE* fp)
{
	uint8_t b[2];
	short v;
	size_t size = cfread(&b[0], sizeof(uint8_t), 2, fp);
	if (size < 2)
		Error("cfile_read_short: read %d bytes, expected 2.", size);

	v = b[0] + (b[1] << 8);
	return v;
}

int cfile_read_int(CFILE* fp)
{
	uint8_t b[4];
	int v;
	size_t size = cfread(&b[0], sizeof(uint8_t), 4, fp);
	if (size < 4)
		Error("cfile_read_int: read %d bytes, expected 4.", size);

	v = b[0] + (b[1] << 8) + (b[2] << 16) + (b[3] << 24);
	return v;
}

uint8_t file_read_byte(FILE* fp)
{
	uint8_t b;
	fread(&b, sizeof(uint8_t), 1, fp);
	return b;
}

short file_read_short(FILE* fp)
{
	uint8_t b[2];
	short v;
	fread(&b[0], sizeof(uint8_t), 2, fp);
	v = b[0] + (b[1] << 8);
	return v;
}

int file_read_int(FILE* fp)
{
	uint8_t b[4];
	int v;
	fread(&b[0], sizeof(uint8_t), 4, fp);
	v = b[0] + (b[1] << 8) + (b[2] << 16) + (b[3] << 24);
	return v;
}

void file_write_byte(FILE* fp, uint8_t b)
{
	fwrite(&b, sizeof(uint8_t), 1, fp);
}

void file_write_short(FILE* fp, short s)
{
	uint8_t b1 = s & 255;
	uint8_t b2 = (s >> 8) & 255;
	fwrite(&b1, sizeof(uint8_t), 1, fp);
	fwrite(&b2, sizeof(uint8_t), 1, fp);
}

void file_write_int(FILE* fp, int i)
{
	uint8_t b1 = i & 255;
	uint8_t b2 = (i >> 8) & 255;
	uint8_t b3 = (i >> 16) & 255;
	uint8_t b4 = (i >> 24) & 255;
	fwrite(&b1, sizeof(uint8_t), 1, fp);
	fwrite(&b2, sizeof(uint8_t), 1, fp);
	fwrite(&b3, sizeof(uint8_t), 1, fp);
	fwrite(&b4, sizeof(uint8_t), 1, fp);
}

//[ISB] so it turns out there was already a cfgets. OOPS. This is still needed for the level name loader though
void cfile_get_string(char* buffer, int count, CFILE* fp)
{
	int i = 0;
	char c;
	do
	{
		c = cfgetc(fp);
		if (i == count - 1) //At the null terminator, so ensure this character is always null
			* buffer = '\0';
		else if (i < count) //Still space to go
			* buffer++ = c;

		i++;
	} while (c != 0);
}

void cfile_read_vector(vms_vector* vec, CFILE* fp)
{
	vec->x = cfile_read_int(fp);
	vec->y = cfile_read_int(fp);
	vec->z = cfile_read_int(fp);
}

void cfile_read_angvec(vms_angvec *vec, CFILE* fp)
{
	vec->p = cfile_read_short(fp);
	vec->b = cfile_read_short(fp);
	vec->h = cfile_read_short(fp);
}
