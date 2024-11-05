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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "cfile/cfile.h"
#include "platform/platform_filesys.h"
#include "platform/posixstub.h"
#include "mission.h"
#include "songs.h"
#include "platform/mono.h"
#include "misc/error.h"
#include "platform/findfile.h"
#include "inferno.h"
#include "gameseq.h"
#include "titles.h"
#include "bm.h"

#define SANTA

mle Mission_list[MAX_MISSIONS];

int Current_mission_num;
int	N_secret_levels;		//	Made a global by MK for scoring purposes.  August 1, 1995.
char *Current_mission_filename,*Current_mission_longname;

//this stuff should get defined elsewhere

char Level_names[MAX_LEVELS_PER_MISSION][FILENAME_LEN];
char Secret_level_names[MAX_SECRET_LEVELS_PER_MISSION][FILENAME_LEN];

#define SHAREWARE_MISSION_FILENAME	"d2demo"
#define SHAREWARE_MISSION_NAME		"Descent 2 Demo"

//where the missions go
#ifndef EDITOR
#define MISSION_DIR ".\\missions\\"
#else
#define MISSION_DIR ".\\"
#endif


#define D1_BIM_LAST_LEVEL			27
#define D1_BIM_LAST_SECRET_LEVEL	-3
#define D1_BIM_BRIEFING_FILE		"briefing.tex"
#define D1_BIM_ENDING_FILE			"endreg.tex"

#ifdef SHAREWARE

//
//  Special versions of mission routines for shareware
//

int build_mission_list(int anarchy_mode)
{
	anarchy_mode++;		//kill warning

	strcpy(Mission_list[0].filename,SHAREWARE_MISSION_FILENAME);
	strcpy(Mission_list[0].mission_name,SHAREWARE_MISSION_NAME);
	Mission_list[0].anarchy_only_flag = 0;

	return load_mission(0);
}

int load_mission(int mission_num)
{
	Assert(mission_num == 0);

	Current_mission_num = 0;
	Current_mission_filename = Mission_list[0].filename;
	Current_mission_longname = Mission_list[0].mission_name;

	N_secret_levels = 0;

	Assert(Last_level == 3);

#ifdef MACINTOSH	// mac demo is using the regular hog and rl2 files
	strcpy(Level_names[0],"d2leva-1.rl2");
	strcpy(Level_names[1],"d2leva-2.rl2");
	strcpy(Level_names[2],"d2leva-3.rl2");
#else
	strcpy(Level_names[0],"d2leva-1.sl2");
	strcpy(Level_names[1],"d2leva-2.sl2");
	strcpy(Level_names[2],"d2leva-3.sl2");
#endif 	// end of ifdef macintosh

	return 1;
}

int load_mission_by_name(char *mission_name)
{
	if (strcmp(mission_name,SHAREWARE_MISSION_FILENAME))
		return 0;		//cannot load requested mission
	else
		return load_mission(0);
}

#elif defined(D2_OEM)

//
//  Special versions of mission routines for Diamond/S3 version
//

#define OEM_MISSION_FILENAME	"d2"
#define OEM_MISSION_NAME		"D2 Destination:Quartzon"

int build_mission_list(int anarchy_mode)
{
	anarchy_mode++;		//kill warning

	strcpy(Mission_list[0].filename,OEM_MISSION_FILENAME);
	strcpy(Mission_list[0].mission_name,OEM_MISSION_NAME);
	Mission_list[0].anarchy_only_flag = 0;

	return load_mission(0);
}

int load_mission(int mission_num)
{
	Assert(mission_num == 0);

	Current_mission_num = 0;
	Current_mission_filename = Mission_list[0].filename;
	Current_mission_longname = Mission_list[0].mission_name;

	N_secret_levels = 2;

	Assert(Last_level == 8);

	strcpy(Level_names[0],"d2leva-1.rl2");
	strcpy(Level_names[1],"d2leva-2.rl2");
	strcpy(Level_names[2],"d2leva-3.rl2");
	strcpy(Level_names[3],"d2leva-4.rl2");

	strcpy(Secret_level_names[0],"d2leva-s.rl2");

	strcpy(Level_names[4],"d2levb-1.rl2");
	strcpy(Level_names[5],"d2levb-2.rl2");
	strcpy(Level_names[6],"d2levb-3.rl2");
	strcpy(Level_names[7],"d2levb-4.rl2");

	strcpy(Secret_level_names[1],"d2levb-s.rl2");

	Secret_level_table[0] = 1;
	Secret_level_table[1] = 5;

	return 1;
}

int load_mission_by_name(char *mission_name)
{
	if (strcmp(mission_name,OEM_MISSION_FILENAME))
		return 0;		//cannot load requested mission
	else
		return load_mission(0);

}

#else

//strips damn newline from end of line
char *mfgets(char *s,int n,CFILE *f)
{
	char *r;

	r = cfgets(s,n,f);
	if (r && s[strlen(s)-1] == '\n')
		s[strlen(s)-1] = 0;

	return r;
}

//compare a string for a token. returns true if match
int istok(char *buf,const char *tok)
{
	return _strnicmp(buf,tok,strlen(tok)) == 0;

}

//adds a terminating 0 after a string at the first white space
void add_term(char *s)
{
	while (*s && !isspace(*s)) s++;

	*s = 0;		//terminate!
}

//returns ptr to string after '=' & white space, or NULL if no '='
//adds 0 after parm at first white space
char *get_value(char *buf)
{
	char *t;

	t = strchr(buf,'=')+1;

	if (t) {
		while (*t && isspace(*t)) t++;

		if (*t)
			return t;
	}

	return NULL;		//error!
}

//reads a line, returns ptr to value of passed parm.  returns NULL if none
char *get_parm_value(const char *parm,CFILE *f)
{
	static char buf[80];
	
	if (!mfgets(buf,80,f))
		return NULL;

	if (istok(buf,parm))
		return get_value(buf);
	else
		return NULL;
}

int ml_sort_func(const mle e0, const mle e1)
{
	return _strnicmp(e0.mission_name, e1.mission_name, MISSION_NAME_LEN);
}

extern int HoardEquipped();

extern char CDROM_dir[];
#define BUILTIN_MISSION "d2.mn2"

//returns 1 if file read ok, else 0
int read_mission_file(char *filename,int count,int location, uint8_t gameVersion)
{

#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
	char filename2[CHOCOLATE_MAX_FILE_PATH_SIZE];
	CFILE *mfile;

	memset(filename2, 0, CHOCOLATE_MAX_FILE_PATH_SIZE);

	switch (location) {
		case ML_MISSIONDIR:
			get_full_file_path(filename2, filename, CHOCOLATE_MISSIONS_DIR);
			break;

		case ML_CDROM:
			//This isn't really implemented at this point
			songs_stop_redbook();
			Int3();
			break;

		default:
			Int3();

		case ML_CURDIR:
			strncpy(filename2, filename, CHOCOLATE_MAX_FILE_PATH_SIZE - 1);
			break;
	}
#else
	char filename2[512];
	CFILE *mfile;

	switch (location) {
		case ML_MISSIONDIR:
			strcpy(filename2,MISSION_DIR);	
			break;

		case ML_CDROM:			
			songs_stop_redbook();		//so we can read from the CD
			strcpy(filename2,CDROM_dir);		
			break;

		default:					
			Int3();		//fall through

		case ML_CURDIR:		
			strcpy(filename2,"");
			break;
	}
	strcat(filename2,filename);
#endif

	mfile = cfopen(filename2,"rb");

	if (mfile) 
	{
		char *p;
		char temp[FILENAME_LEN],*t;

		strcpy(temp,filename);
		if ((t = strchr(temp,'.')) == NULL)
			return 0;	//missing extension
		*t = 0;			//kill extension

		strncpy( Mission_list[count].filename, temp, MISSION_FILENAME_LEN - 1);
		Mission_list[count].anarchy_only_flag = 0;
		Mission_list[count].location = location;

		p = get_parm_value("name",mfile);

		if (!p) //try enhanced mission
		{		
			cfseek(mfile,0,SEEK_SET);
			p = get_parm_value("xname",mfile);
		}

#ifdef SANTA
		if (HoardEquipped()) 
		{
			if (!p) //try super-enhanced mission!
			{		
				cfseek(mfile,0,SEEK_SET);
				p = get_parm_value("zname",mfile);
			}
		}
#endif

		if (p) 
		{
			char *t;
			if ((t=strchr(p,';'))!=NULL)
				*t=0;
			t = p + strlen(p)-1;
			while (isspace(*t)) t--;
			snprintf(Mission_list[count].mission_name, 5, "(%hhd) ", gameVersion);
			strncpy(Mission_list[count].mission_name + 4, p, MISSION_NAME_LEN - 4);
		}
		else
		{
			cfclose(mfile);
			return 0;
		}

		p = get_parm_value("type",mfile);

		//get mission type 
		if (p)
			Mission_list[count].anarchy_only_flag = istok(p,"anarchy");

		Mission_list[count].gameVersion = gameVersion;

		cfclose(mfile);

		return 1;
	}

	return 0;
}


//fills in the global list of missions.  Returns the number of missions
//in the list.  If anarchy_mode set, don't include non-anarchy levels.
//if there is only one mission, this function will call load_mission on it.

int build_mission_list(int anarchy_mode)
{
	static int num_missions=-1;
	int count=0,special_count=0;
	FILEFINDSTRUCT find;
#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
	char search_name_d1[CHOCOLATE_MAX_FILE_PATH_SIZE];
	char search_name_d2[CHOCOLATE_MAX_FILE_PATH_SIZE];
	char temp_buf[CHOCOLATE_MAX_FILE_PATH_SIZE];
	get_platform_localized_query_string(search_name_d1, CHOCOLATE_MISSIONS_DIR, "*.msn");
	get_platform_localized_query_string(search_name_d2, CHOCOLATE_MISSIONS_DIR, "*.mn2");
#else
	char search_name_d1[256] = MISSION_DIR "*.MSN";
	char search_name_d2[256] = MISSION_DIR "*.MN2";
#endif

	if (CurrentDataVersion == DataVer::DEMO)
	{
		strcpy(Mission_list[0].filename, SHAREWARE_MISSION_FILENAME);
		strcpy(Mission_list[0].mission_name, SHAREWARE_MISSION_NAME);
		Mission_list[0].anarchy_only_flag = 0;

		return load_mission(0);
	}

	//now search for levels on disk

//@@Took out this code because after this routine was called once for
//@@a list of single-player missions, a subsequent call for a list of
//@@anarchy missions would not scan again, and thus would not find the
//@@anarchy-only missions.  If we retain the minimum level of install,
//@@we may want to put the code back in, having it always scan for all
//@@missions, and have the code that uses it sort out the ones it wants.
//@@	if (num_missions != -1) {
//@@		if (Current_mission_num != 0)
//@@			load_mission(0);				//set built-in mission as default
//@@		return num_missions;
//@@	}



	strcpy(Mission_list[0].filename, "");		//no filename for builtin
	strcpy(Mission_list[0].mission_name, "(1) Descent: First Strike");
	Mission_list[0].gameVersion = 1;
	Mission_list[0].location = ML_CURDIR;
	
	count = 1;

	if (!read_mission_file(const_cast<char*>(BUILTIN_MISSION), 1, ML_CURDIR, 2))		//read built-in first
		Error("Could not find required mission file <%s>", BUILTIN_MISSION);

	special_count = count = 2;

	if( !FileFindFirst( search_name_d1, &find ) )
	{
		do	
		{

			//get_full_file_path(temp_buf, find.name, CHOCOLATE_MISSIONS_DIR);
			if (read_mission_file(find.name, count, ML_MISSIONDIR, 1))
			{
				if (anarchy_mode || !Mission_list[count].anarchy_only_flag)
					count++;
			} 

		} while( !FileFindNext( &find ) && count<MAX_MISSIONS);
		FileFindClose();
	}

	if( !FileFindFirst( search_name_d2, &find ) )
	{
		do	
		{
			if (_strfcmp(find.name,BUILTIN_MISSION)==0)
				continue;		//skip the built-in

			//get_full_file_path(temp_buf, find.name, CHOCOLATE_MISSIONS_DIR);
			if (read_mission_file(find.name, count, ML_MISSIONDIR, 2))
			{
				if (anarchy_mode || !Mission_list[count].anarchy_only_flag)
					count++;
			} 

		} while( !FileFindNext( &find ) && count<MAX_MISSIONS);
		FileFindClose();
	}

	int i;

	for (i=special_count;i<count;i++)
	{
# if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
		if (!_strnicmp(Mission_list[i].filename, "d2x", 3))
# else
		if (!_strfcmp(Mission_list[i].filename,"D2X")) //swap!
# endif
		{
			printf("found D2X");
			mle temp;

			temp = Mission_list[special_count];
			Mission_list[special_count] = Mission_list[i];
			Mission_list[i] = temp;

			special_count++;

			break;
		}
	}
	
	if (count>special_count) {
		//qsort(&Mission_list[special_count],count-special_count,sizeof(*Mission_list),
		//		(int (*)( const void *, const void * ))ml_sort_func);
		printf("MC %d %d\n", special_count, count);
		std::sort(Mission_list + special_count, Mission_list + count, ml_sort_func);
	}

	load_mission(1);			//set built-in mission as default
	num_missions = count;

	return count;
}

void init_extra_robot_movie(char *filename);

//values for built-in mission

//loads the specfied mission from the mission list.  build_mission_list()
//must have been called.  If build_mission_list() returns 0, this function
//does not need to be called.  Returns true if mission loaded ok, else false.
int load_mission(int mission_num)
{
	SwitchGame(Mission_list[mission_num].gameVersion);

	CFILE *mfile;
# if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
	char buf[CHOCOLATE_MAX_FILE_PATH_SIZE], temp_short_filename[CHOCOLATE_MAX_FILE_PATH_SIZE], *v;
# else
	char buf[80], *v;
# endif
	int found_hogfile;
	int enhanced_mission = 0;

	Current_mission_num = mission_num;

	mprintf(( 0, "Loading mission %d\n", mission_num ));

	printf("\nSwitched mode to game version %hhu\n", Mission_list[mission_num].gameVersion);

	if (currentGame == G_DESCENT_2 && CurrentDataVersion == DataVer::DEMO)
	{
		Assert(mission_num == 0);

		Current_mission_num = 0;
		Current_mission_filename = Mission_list[0].filename;
		Current_mission_longname = Mission_list[0].mission_name;

		N_secret_levels = 0;

		//Assert(Last_level == 3);
		Last_level = 3;

		strcpy(Level_names[0], "d2leva-1.sl2");
		strcpy(Level_names[1], "d2leva-2.sl2");
		strcpy(Level_names[2], "d2leva-3.sl2");

		return 1;
	}

	//read mission from file 

#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)

	snprintf(temp_short_filename, CHOCOLATE_MAX_FILE_PATH_SIZE, 
		(currentGame == G_DESCENT_1 ? "%s.msn" : "%s.mn2"), Mission_list[mission_num].filename);

	switch (Mission_list[mission_num].location)
	{
		case ML_MISSIONDIR:
			get_full_file_path(buf, temp_short_filename, CHOCOLATE_MISSIONS_DIR);
			break;

		case ML_CDROM:
			//This isn't really implemented at this point
			Int3();
			break;

		default:
			Int3();

		case ML_CURDIR:
			strncpy(buf, temp_short_filename, CHOCOLATE_MAX_FILE_PATH_SIZE - 1);
			break;
	}
#else
	switch (Mission_list[mission_num].location) {
		case ML_MISSIONDIR:	strcpy(buf,MISSION_DIR);	break;
		case ML_CDROM:			strcpy(buf,CDROM_dir);		break;
		default:					Int3();							//fall through
		case ML_CURDIR:		strcpy(buf,"");				break;
	}
	strcat(buf,Mission_list[mission_num].filename);
	strcat(buf, (currentGame == G_DESCENT_1 ? ".MSN" : ".MN2"));
#endif

	if (mission_num == 0) 
	{
		int i;

		Last_level = D1_BIM_LAST_LEVEL;
		Last_secret_level = D1_BIM_LAST_SECRET_LEVEL;

		//build level names
		for (i = 0; i < Last_level; i++)
			sprintf(Level_names[i], "LEVEL%02d.RDL", i + 1);
		for (i = 0; i < -Last_secret_level; i++)
			sprintf(Secret_level_names[i], "LEVELS%1d.RDL", i + 1);

		Secret_level_table[0] = 10;
		Secret_level_table[1] = 21;
		Secret_level_table[2] = 24;

		strcpy(Briefing_text_filename, D1_BIM_BRIEFING_FILE);
		strcpy(Ending_text_filename, D1_BIM_ENDING_FILE);
		cfile_use_alternate_hogfile(NULL);		//disable alternate
	} 
	else
	{
		if (mission_num == 1)
			cfile_use_alternate_hogfile(NULL);

		mfile = cfopen(buf,"rb");
		if (mfile == NULL) 
		{
			Current_mission_num = -1;
			printf("\nError opening mfile! %s\n", buf);
			return 0;		//error!
		}

		if (mission_num > 1) //for non-builtin missions, load HOG
		{
#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
			strcpy(buf + strlen(buf) - 4, ".hog");
#else
			strcpy(buf+strlen(buf)-4,".HOG");		//change extension
#endif

			found_hogfile = cfile_use_alternate_hogfile(buf);

			#ifdef RELEASE				//for release, require mission to be in hogfile
			if (! found_hogfile) {
				cfclose(mfile);
				Current_mission_num = -1;
				return 0;
			}
			#endif
		}

		//init vars
		Last_level = 0;
		Last_secret_level = 0;

		if (currentGame == G_DESCENT_1) {
			Briefing_text_filename[0] = 0;
			Ending_text_filename[0] = 0;
		}

		while (mfgets(buf,80,mfile)) 
		{

			if (istok(buf,"name"))
				continue;						//already have name, go to next line
			if (istok(buf,"xname")) 
			{
				enhanced_mission = 1;
				continue;						//already have name, go to next line
			}
			if (istok(buf,"zname"))
			{
				enhanced_mission = 2;
				continue;						//already have name, go to next line
			}
			else if (istok(buf,"type"))
				continue;						//already have name, go to next line				
			else if (istok(buf,"hog")) 
			{
				char	*bufp = buf;

				while (*(bufp++) != '=')
					;

				if (*bufp == ' ')
					while (*(++bufp) == ' ')
						;

				cfile_use_alternate_hogfile(bufp);
				mprintf((0, "Hog file override = [%s]\n", bufp));
			}
			else if (currentGame == G_DESCENT_1 && istok(buf, "briefing")) 
			{
				if ((v = get_value(buf)) != NULL) 
				{
					add_term(v);
					if (strlen(v) < 13)
						strcpy(Briefing_text_filename, v);
				}
			}
			else if (istok(buf, "ending")) 
			{
				if ((v = get_value(buf)) != NULL) 
				{
					add_term(v);
					if (strlen(v) < 13)
						strcpy(Ending_text_filename, v);
				}
			}
			else if (istok(buf,"num_levels")) 
			{
				if ((v=get_value(buf))!=NULL)
				{
					int n_levels,i;

					n_levels = atoi(v);

					for (i=0;i<n_levels && mfgets(buf,80,mfile);i++)
					{

						add_term(buf);
						if (strlen(buf) <= 12 && i < MAX_LEVELS_PER_MISSION) 
						{
							strcpy(Level_names[i],buf);
							Last_level++;
						}
						else
							break;
					}

				}
			}
			else if (istok(buf,"num_secrets")) 
			{
				if ((v=get_value(buf))!=NULL) 
				{
					int i;

					N_secret_levels = atoi(v);

					Assert(N_secret_levels <= MAX_SECRET_LEVELS_PER_MISSION);

					for (i=0;i<N_secret_levels && mfgets(buf,80,mfile);i++) 
					{
						char *t;
						if ((t=strchr(buf,','))!=NULL) *t++=0;
						else
							break;

						add_term(buf);
						if (strlen(buf) <= 12 && i < MAX_SECRET_LEVELS_PER_MISSION)
						{
							strcpy(Secret_level_names[i],buf);
							Secret_level_table[i] = atoi(t);
							if (Secret_level_table[i]<1 || Secret_level_table[i]>Last_level)
								break;
							Last_secret_level--;
						}
						else
							break;
					}
				}
			}
		}

		cfclose(mfile);

	}

	if (Last_level <= 0)
	{
		Current_mission_num = -1;		//no valid mission loaded
		return 0;
	}

	Current_mission_filename = Mission_list[Current_mission_num].filename;
	Current_mission_longname = Mission_list[Current_mission_num].mission_name;

	if (currentGame == G_DESCENT_2 && enhanced_mission) 
	{
		char t[50];
		extern void bm_read_extra_robots(char* fname, int type);
		snprintf(t, 50, "%s.ham", Current_mission_filename);
		bm_read_extra_robots(t,enhanced_mission);
		strncpy(t,Current_mission_filename,6);
		strcat(t,"-l.mvl");
		init_extra_robot_movie(t);
	}

	return 1;
}

//loads the named mission if exists.
//Returns true if mission loaded ok, else false.
int load_mission_by_name(char *mission_name)
{
	int n,i;

	if (currentGame == G_DESCENT_2 && CurrentDataVersion == DataVer::DEMO)
	{
		if (strcmp(mission_name, SHAREWARE_MISSION_FILENAME))
			return 0;		//cannot load requested mission
		else
			return load_mission(0);
	}

	n = build_mission_list(1);

	for (i=0;i<n;i++)
		if (!_strfcmp(mission_name,Mission_list[i].filename))
			return load_mission(i);

	return 0;		//couldn't find mission
}

#endif
//#endif

void SwitchGame(uint8_t gameVersion) {

	switch (gameVersion) {

		case 1:
			currentGame = G_DESCENT_1;
			d1Table.SetActive();
			if (shouldAutoClearBMTable && !activeBMTable->initialized)
				bm_init();
			break;

		case 2:
			currentGame = G_DESCENT_2;
			d2Table.SetActive();
			if (shouldAutoClearBMTable && !activeBMTable->initialized)
				bm_init();
			break;

		default:
			Int3();

	}

}