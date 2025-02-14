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

#include "misc/types.h"
#include "segment.h"
#include "player.h"

extern void do_automap(int key_code);
extern void automap_clear_visited();
extern std::vector<uint8_t> Automap_visited;
extern void modex_print_message(int x, int y, char *str);
void DropBuddyMarker(object *objp);

extern int Automap_active;

#define NUM_MARKERS 				16
#define MARKER_MESSAGE_LEN		40

extern char MarkerMessage[NUM_MARKERS][MARKER_MESSAGE_LEN];
extern char MarkerOwner[NUM_MARKERS][CALLSIGN_LEN+1];
extern int	MarkerObject[NUM_MARKERS];


void adjust_segment_limit(int SegmentLimit); 
void draw_all_edges();
void automap_build_edge_list();