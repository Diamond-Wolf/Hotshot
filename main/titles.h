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

#pragma once

#ifndef RELEASE
extern int	Skip_briefing_screens;
#else
#define Skip_briefing_screens 0
#endif

extern char Briefing_text_filename[13];
extern char Ending_text_filename[13];

extern int show_title_screen( const char * filename, int allow_keys, int from_hog_only );
extern int show_briefing_screen(const char* filename, int allow_keys); 
extern void show_title_flick(const char* name, int allow_keys);
extern void do_briefing_screens(const char *filename,int level_num);

extern void do_end_game(void);
extern char* get_briefing_screen(int level_num);

void title_save_game();
extern void show_endgame_briefing(void);

#define	SHAREWARE_ENDING_LEVEL_NUM  0x7f
#define	REGISTERED_ENDING_LEVEL_NUM 0x7e