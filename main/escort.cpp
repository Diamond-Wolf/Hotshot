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
#endif

#include <stdio.h>		//	for printf()
#include <stdlib.h>		// for P_Rand() and qsort()
#include <string.h>		// for memset()
#include <stdarg.h>

#include "misc/rand.h"

#include "inferno.h"
#include "platform/mono.h"
#include "3d/3d.h"
#include "2d/palette.h"
#include "platform/platform.h"

#include "object.h"
#include "misc/error.h"
#include "ai.h"
#include "robot.h"
#include "fvi.h"
#include "physics.h"
#include "wall.h"
#include "player.h"
#include "fireball.h"
#include "game.h"
#include "powerup.h"
#include "cntrlcen.h"
#include "gameseq.h"
#include "platform/key.h"
#include "fuelcen.h"
#include "sounds.h"
#include "screens.h"
#include "stringtable.h"
#include "gamefont.h"
#include "newmenu.h"
#include "playsave.h"
#include "gauges.h"
#include "automap.h"
#include "laser.h"
#include "newcheat.h"
//#include "pa_enabl.h"

#include "bm.h"

#ifdef EDITOR
#include "editor\editor.h"
#endif

#ifdef NETWORK
extern void multi_send_stolen_items();
#endif

const char *Escort_goal_text[MAX_ESCORT_GOALS] = {
	"BLUE KEY",
	"YELLOW KEY",
	"RED KEY",
	"REACTOR",
	"EXIT",
	"ENERGY",
	"ENERGYCEN",
	"SHIELD",
	"POWERUP",
	"ROBOT",
	"HOSTAGES",
	"SPEW",
	"SCRAM",
	"EXIT",
	"BOSS",
	"MARKER 1",
	"MARKER 2",
	"MARKER 3",
	"MARKER 4",
	"MARKER 5",
	"MARKER 6",
	"MARKER 7",
	"MARKER 8",
	"MARKER 9",
// -- too much work -- 	"KAMIKAZE  "
};

int	Max_escort_length = 200;
int	Escort_kill_object = -1;
uint8_t Stolen_items[MAX_STOLEN_ITEMS];
int	Stolen_item_index;
fix	Escort_last_path_created = 0;
int	Escort_goal_object = ESCORT_GOAL_UNSPECIFIED, Escort_special_goal = -1, Escort_goal_index = -1, Buddy_messages_suppressed = 0;
fix	Buddy_sorry_time;
int	Buddy_objnum, Buddy_allowed_to_talk;
int	Looking_for_marker;
int	Last_buddy_key;

fix	Last_buddy_message_time;

//if change this length, change in playsave.c also
#define GUIDEBOT_NAME_LEN 9
char guidebot_name[GUIDEBOT_NAME_LEN+1] = "GUIDE-BOT";
char real_guidebot_name[GUIDEBOT_NAME_LEN+1] = "GUIDE-BOT";

void init_buddy_for_level(void)
{
	int	i;

	Buddy_allowed_to_talk = 0;
	Buddy_objnum = -1;
	Escort_goal_object = ESCORT_GOAL_UNSPECIFIED;
	Escort_special_goal = -1;
	Escort_goal_index = -1;
	Buddy_messages_suppressed = 0;

	for (i=0; i<=Highest_object_index; i++)
		if (Objects[i].type == OBJ_ROBOT && activeBMTable->robots[Objects[i].id].companion)
			break;
	if (i <= Highest_object_index)
		Buddy_objnum = i;

	Buddy_sorry_time = -F1_0;

	Looking_for_marker = -1;
	Last_buddy_key = -1;
}

//	-----------------------------------------------------------------------------
//	See if segment from curseg through sidenum is reachable.
//	Return true if it is reachable, else return false.
int segment_is_reachable(int curseg, int sidenum)
{
	int		wall_num, rval;
	segment	*segp = &Segments[curseg];

	if (!IS_CHILD(segp->children[sidenum]))
		return 0;

	wall_num = segp->sides[sidenum].wall_num;

	//	If no wall, then it is reachable
	if (wall_num == -1)
		return 1;

	rval = ai_door_is_openable(NULL, segp, sidenum);

	return rval;

// -- MK, 10/17/95 -- 
// -- MK, 10/17/95 -- 	//	Hmm, a closed wall.  I think this mean not reachable.
// -- MK, 10/17/95 -- 	if (Walls[wall_num].type == WALL_CLOSED)
// -- MK, 10/17/95 -- 		return 0;
// -- MK, 10/17/95 -- 
// -- MK, 10/17/95 -- 	if (Walls[wall_num].type == WALL_DOOR) {
// -- MK, 10/17/95 -- 		if (Walls[wall_num].keys == KEY_NONE) {
// -- MK, 10/17/95 -- 			return 1;		//	@MK, 10/17/95: Be consistent with ai_door_is_openable
// -- MK, 10/17/95 -- // -- 			if (Walls[wall_num].flags & WALL_DOOR_LOCKED)
// -- MK, 10/17/95 -- // -- 				return 0;
// -- MK, 10/17/95 -- // -- 			else
// -- MK, 10/17/95 -- // -- 				return 1;
// -- MK, 10/17/95 -- 		} else if (Walls[wall_num].keys == KEY_BLUE)
// -- MK, 10/17/95 -- 			return (Players[Player_num].flags & PLAYER_FLAGS_BLUE_KEY);
// -- MK, 10/17/95 -- 		else if (Walls[wall_num].keys == KEY_GOLD)
// -- MK, 10/17/95 -- 			return (Players[Player_num].flags & PLAYER_FLAGS_GOLD_KEY);
// -- MK, 10/17/95 -- 		else if (Walls[wall_num].keys == KEY_RED)
// -- MK, 10/17/95 -- 			return (Players[Player_num].flags & PLAYER_FLAGS_RED_KEY);
// -- MK, 10/17/95 -- 		else
// -- MK, 10/17/95 -- 			Int3();	//	Impossible!  Doesn't have no key, but doesn't have any key!
// -- MK, 10/17/95 -- 	} else
// -- MK, 10/17/95 -- 		return 1;
// -- MK, 10/17/95 -- 
// -- MK, 10/17/95 -- 	Int3();	//	Hmm, thought 'if' above had to return!
// -- MK, 10/17/95 -- 	return 0;

}


//	-----------------------------------------------------------------------------
//	Create a breadth-first list of segments reachable from current segment.
//	max_segs is maximum number of segments to search.  Use MAX_SEGMENTS to search all.
//	On exit, *length <= max_segs.
//	Input:
//		start_seg
//	Output:
//		bfs_list:	array of shorts, each reachable segment.  Includes start segment.
//		length:		number of elements in bfs_list
void create_bfs_list(int start_seg, short bfs_list[], int *length, int max_segs)
{
	int	i, head, tail;
	int8_t* visited = new int8_t[Segments.size()];

	for (i=0; i<Segments.size(); i++)
		visited[i] = 0;

	head = 0;
	tail = 0;

	bfs_list[head++] = start_seg;
	visited[start_seg] = 1;

	while ((head != tail) && (head < max_segs)) 
	{
		int		i;
		int		curseg;
		segment	*cursegp;

		curseg = bfs_list[tail++];
		cursegp = &Segments[curseg];

		for (i=0; i<MAX_SIDES_PER_SEGMENT; i++) 
		{
			int	connected_seg;

			connected_seg = cursegp->children[i];

			if (IS_CHILD(connected_seg) && (visited[connected_seg] == 0))
			{
				if (segment_is_reachable(curseg, i)) 
				{
					bfs_list[head++] = connected_seg;
					if (head >= max_segs)
						break;
					visited[connected_seg] = 1;
					Assert(head < Segments.size());
				}
			}
		}
	}

	*length = head;

	delete[] visited;
	
}

//	-----------------------------------------------------------------------------
//	Return true if ok for buddy to talk, else return false.
//	Buddy is allowed to talk if the segment he is in does not contain a blastable wall that has not been blasted
//	AND he has never yet, since being initialized for level, been allowed to talk.
int ok_for_buddy_to_talk(void)
{
	int		i;
	segment	*segp;

	if (Buddy_objnum < 0 || Buddy_objnum >= Objects.size() || Objects[Buddy_objnum].type != OBJ_ROBOT) 
	{
		Buddy_allowed_to_talk = 0;
		return 0;
	}

	if (Buddy_allowed_to_talk)
		return 1;

	if ((Objects[Buddy_objnum].type == OBJ_ROBOT) && (Buddy_objnum <= Highest_object_index) && !activeBMTable->robots[Objects[Buddy_objnum].id].companion) {
		for (i=0; i<=Highest_object_index; i++)
			if (activeBMTable->robots[Objects[i].id].companion)
				break;
		if (i > Highest_object_index)
			return 0;
		else
			Buddy_objnum = i;
	}

	segp = &Segments[Objects[Buddy_objnum].segnum];

	for (i=0; i<MAX_SIDES_PER_SEGMENT; i++) 
	{
		int	wall_num = segp->sides[i].wall_num;

		if (wall_num != -1) 
		{
			if ((Walls[wall_num].type == WALL_BLASTABLE) && !(Walls[wall_num].flags & WALL_BLASTED))
				return 0;
		}

		//	Check one level deeper.
		if (IS_CHILD(segp->children[i])) 
		{
			int		j;
			segment	*csegp = &Segments[segp->children[i]];

			for (j=0; j<MAX_SIDES_PER_SEGMENT; j++) 
			{
				int	wall2 = csegp->sides[j].wall_num;

				if (wall2 != -1) 
				{
					if ((Walls[wall2].type == WALL_BLASTABLE) && !(Walls[wall2].flags & WALL_BLASTED))
						return 0;
				}
			}
		}
	}

	Buddy_allowed_to_talk = 1;
	return 1;
}

//	--------------------------------------------------------------------------------------------
void detect_escort_goal_accomplished(int index)
{
	int	i,j;
	int	detected = 0;

	if (!Buddy_allowed_to_talk)
		return;

	//	If goal is to go away, how can it be achieved?
	if (Escort_special_goal == ESCORT_GOAL_SCRAM)
		return;

//	See if goal found was a key.  Need to handle default goals differently.
//	Note, no buddy_met_goal sound when blow up reactor or exit.  Not great, but ok
//	since for reactor, noisy, for exit, buddy is disappearing.
if ((Escort_special_goal == -1) && (Escort_goal_index == index)) {
	detected = 1;
	goto dega_ok;
}

if ((Escort_goal_index <= ESCORT_GOAL_RED_KEY) && (index >= 0)) {
	if (Objects[index].type == OBJ_POWERUP)  {
		if (Objects[index].id == POW_KEY_BLUE) {
			if (Escort_goal_index == ESCORT_GOAL_BLUE_KEY) {
				detected = 1;
				goto dega_ok;
			}
		} else if (Objects[index].id == POW_KEY_GOLD) {
			if (Escort_goal_index == ESCORT_GOAL_GOLD_KEY) {
				detected = 1;
				goto dega_ok;
			}
		} else if (Objects[index].id == POW_KEY_RED) {
			if (Escort_goal_index == ESCORT_GOAL_RED_KEY) {
				detected = 1;
				goto dega_ok;
			}
		}
	}
}
	if (Escort_special_goal != -1)
		if (Escort_special_goal == ESCORT_GOAL_ENERGYCEN) {
			if (index == -4)
				detected = 1;
			else {
				for (i=0; i<MAX_SIDES_PER_SEGMENT; i++)
					if (Segments[index].children[i] == Escort_goal_index) {
						detected = 1;
						goto dega_ok;
					} else {
						for (j=0; j<MAX_SIDES_PER_SEGMENT; j++)
							if (Segments[i].children[j] == Escort_goal_index) {
								detected = 1;
								goto dega_ok;
							}
					}
			}
		} else if ((Objects[index].type == OBJ_POWERUP) && (Escort_special_goal == ESCORT_GOAL_POWERUP))
			detected = 1;	//	Any type of powerup picked up will do.
		else if ((Objects[index].type == Objects[Escort_goal_index].type) && (Objects[index].id == Objects[Escort_goal_index].id)) {
			//	Note: This will help a little bit in making the buddy believe a goal is satisfied.  Won't work for a general goal like "find any powerup"
			// because of the insistence of both type and id matching.
			detected = 1;
		}

dega_ok: ;
	if (detected && ok_for_buddy_to_talk()) {
		digi_play_sample_once(SOUND_BUDDY_MET_GOAL, F1_0);
		Escort_goal_index = -1;
		Escort_goal_object = ESCORT_GOAL_UNSPECIFIED;
		Escort_special_goal = -1;
		Looking_for_marker = -1;
	}

}

void change_guidebot_name()
{
	newmenu_item m;
	char text[GUIDEBOT_NAME_LEN+1]="";
	int item;

	strcpy(text,guidebot_name);

	m.type=NM_TYPE_INPUT; m.text_len = GUIDEBOT_NAME_LEN; m.text = text;
	item = newmenu_do( NULL, "Enter Guide-bot name:", 1, &m, NULL );

	if (item != -1) {
		strcpy(guidebot_name,text);
		strcpy(real_guidebot_name,text);
		write_player_file();
	}
}

//	-----------------------------------------------------------------------------
void buddy_message(const char * format, ... )
{
	if (Buddy_messages_suppressed)
		return;

	if (Game_mode & GM_MULTI)
		return;

	if ((Last_buddy_message_time + F1_0 < GameTime) || (Last_buddy_message_time > GameTime)) {
		if (ok_for_buddy_to_talk()) {
			char	gb_str[16], new_format[128];
			va_list	args;
			int t;

			va_start(args, format );
			vsnprintf(new_format, 128, format, args);
			va_end(args);

			gb_str[0] = 1;
			gb_str[1] = BM_XRGB(28, 0, 0);
			strncpy(&gb_str[2], guidebot_name, 14);
			t = strlen(gb_str);
			gb_str[t] = ':';
			gb_str[t+1] = 1;
			gb_str[t+2] = BM_XRGB(0, 31, 0);
			gb_str[t+3] = 0;

			HUD_init_message("%s %s", gb_str, new_format);

			Last_buddy_message_time = GameTime;
		}
	}

}

//	-----------------------------------------------------------------------------
void thief_message(const char * format, ... )
{

	char	gb_str[16], new_format[128];
	va_list	args;

	va_start(args, format );
	vsnprintf(new_format, 128, format, args);
	va_end(args);

	gb_str[0] = 1;
	gb_str[1] = BM_XRGB(28, 0, 0);
	strncpy(&gb_str[2], "THIEF:", 6);
	gb_str[8] = 1;
	gb_str[9] = BM_XRGB(0, 31, 0);
	gb_str[10] = 0;

	HUD_init_message("%s %s", gb_str, new_format);

}

//	-----------------------------------------------------------------------------
//	Return true if marker #id has been placed.
int marker_exists_in_mine(int id)
{
	int	i;

	for (i=0; i<=Highest_object_index; i++)
		if (Objects[i].type == OBJ_MARKER)
			if (Objects[i].id == id)
				return 1;

	return 0;
}

void say_escort_goal(int goal_num);

//	-----------------------------------------------------------------------------
void set_escort_special_goal(int special_key)
{
	int marker_key;

	Buddy_messages_suppressed = 0;

	if (!Buddy_allowed_to_talk) {
		ok_for_buddy_to_talk();
		if (!Buddy_allowed_to_talk) {
			int	i;

			for (i=0; i<=Highest_object_index; i++)
				if ((Objects[i].type == OBJ_ROBOT) && activeBMTable->robots[Objects[i].id].companion) {
					HUD_init_message("%s has not been released.",guidebot_name);
					break;
				}
			if (i == Highest_object_index+1)
				HUD_init_message("No Guide-Bot in mine.");

			return;
		}
	}

	special_key = special_key & (~KEY_SHIFTED);

	marker_key = special_key;
	
	#ifdef MACINTOSH
	switch(special_key) {
		case KEY_5:
			marker_key = KEY_1+4;
			break;
		case KEY_6:
			marker_key = KEY_1+5;
			break;
		case KEY_7:
			marker_key = KEY_1+6;
			break;
		case KEY_8:
			marker_key = KEY_1+7;
			break;
		case KEY_9:
			marker_key = KEY_1+8;
			break;
		case KEY_0:
			marker_key = KEY_1+9;
			break;
	}
	#endif

	if (Last_buddy_key == special_key)
		if ((Looking_for_marker == -1) && (special_key != KEY_0)) {
			if (marker_exists_in_mine(marker_key - KEY_1))
				Looking_for_marker = marker_key - KEY_1;
			else {
				Last_buddy_message_time = 0;	//	Force this message to get through.
				buddy_message("Marker %i not placed.", marker_key - KEY_1 + 1);
				Looking_for_marker = -1;
			}
		} else
			Looking_for_marker = -1;

	Last_buddy_key = special_key;

	if (special_key == KEY_0)
		Looking_for_marker = -1;
		
	if ( Looking_for_marker != -1 ) {
		Escort_special_goal = ESCORT_GOAL_MARKER1 + marker_key - KEY_1;
	} else {
		switch (special_key) {
			case KEY_1:	Escort_special_goal = ESCORT_GOAL_ENERGY;			break;
			case KEY_2:	Escort_special_goal = ESCORT_GOAL_ENERGYCEN;		break;
			case KEY_3:	Escort_special_goal = ESCORT_GOAL_SHIELD;			break;
			case KEY_4:	Escort_special_goal = ESCORT_GOAL_POWERUP;		break;
			case KEY_5:	Escort_special_goal = ESCORT_GOAL_ROBOT;			break;
			case KEY_6:	Escort_special_goal = ESCORT_GOAL_HOSTAGE;		break;
			case KEY_7:	Escort_special_goal = ESCORT_GOAL_SCRAM;			break;
			case KEY_8:	Escort_special_goal = ESCORT_GOAL_PLAYER_SPEW;	break;
			case KEY_9:	Escort_special_goal = ESCORT_GOAL_EXIT;			break;
			case KEY_0:	Escort_special_goal = -1;								break;
			default:
				Int3();		//	Oops, called with illegal key value.
		}
	}

	Last_buddy_message_time = GameTime - 2*F1_0;	//	Allow next message to come through.

	say_escort_goal(Escort_special_goal);
	// -- Escort_goal_object = escort_set_goal_object();

	Escort_goal_object = ESCORT_GOAL_UNSPECIFIED;
}

// -- old, pre-bfs, way -- //	-----------------------------------------------------------------------------
// -- old, pre-bfs, way -- //	Return object of interest.
// -- old, pre-bfs, way -- int exists_in_mine(int objtype, int objid)
// -- old, pre-bfs, way -- {
// -- old, pre-bfs, way -- 	int	i;
// -- old, pre-bfs, way -- 
// -- old, pre-bfs, way -- 	mprintf((0, "exists_in_mine, type == %i, id == %i\n", objtype, objid));
// -- old, pre-bfs, way -- 
// -- old, pre-bfs, way -- 	if (objtype == FUELCEN_CHECK) {
// -- old, pre-bfs, way -- 		for (i=0; i<=Highest_segment_index; i++)
// -- old, pre-bfs, way -- 			if (Segments[i].special == SEGMENT_IS_FUELCEN)
// -- old, pre-bfs, way -- 				return i;
// -- old, pre-bfs, way -- 	} else {
// -- old, pre-bfs, way -- 		for (i=0; i<=Highest_object_index; i++) {
// -- old, pre-bfs, way -- 			if (Objects[i].type == objtype) {
// -- old, pre-bfs, way -- 				//	Don't find escort robots if looking for robot!
// -- old, pre-bfs, way -- 				if ((Objects[i].type == OBJ_ROBOT) && (activeBMTable->robots[Objects[i].id].companion))
// -- old, pre-bfs, way -- 					continue;
// -- old, pre-bfs, way -- 
// -- old, pre-bfs, way -- 				if (objid == -1) {
// -- old, pre-bfs, way -- 					if ((objtype == OBJ_POWERUP) && (Objects[i].id != POW_KEY_BLUE) && (Objects[i].id != POW_KEY_GOLD) && (Objects[i].id != POW_KEY_RED))
// -- old, pre-bfs, way -- 						return i;
// -- old, pre-bfs, way -- 					else
// -- old, pre-bfs, way -- 						return i;
// -- old, pre-bfs, way -- 				} else if (Objects[i].id == objid)
// -- old, pre-bfs, way -- 					return i;
// -- old, pre-bfs, way -- 			}
// -- old, pre-bfs, way -- 		}
// -- old, pre-bfs, way -- 	}
// -- old, pre-bfs, way -- 
// -- old, pre-bfs, way -- 	return -1;
// -- old, pre-bfs, way -- 
// -- old, pre-bfs, way -- }

//	-----------------------------------------------------------------------------
//	Return id of boss.
int get_boss_id(void)
{
	int	i;

	for (i=0; i<=Highest_object_index; i++)
		if (Objects[i].type == OBJ_ROBOT)
			if (activeBMTable->robots[Objects[i].id].boss_flag)
				return Objects[i].id;

	return -1;
}

//	-----------------------------------------------------------------------------
//	Return object index if object of objtype, objid exists in mine, else return -1
//	"special" is used to find objects spewed by player which is hacked into flags field of powerup.
int exists_in_mine_2(int segnum, int objtype, int objid, int special)
{
	if (Segments[segnum].objects != -1) {
		int		objnum = Segments[segnum].objects;

		while (objnum != -1) {
			object	*curobjp = &Objects[objnum];

			if (special == ESCORT_GOAL_PLAYER_SPEW) {
				if (curobjp->flags & OF_PLAYER_DROPPED)
					return objnum;
			}

			if (curobjp->type == objtype) {
				//	Don't find escort robots if looking for robot!
				if ((curobjp->type == OBJ_ROBOT) && (activeBMTable->robots[curobjp->id].companion))
					;
				else if (objid == -1) {
					if ((objtype == OBJ_POWERUP) && (curobjp->id != POW_KEY_BLUE) && (curobjp->id != POW_KEY_GOLD) && (curobjp->id != POW_KEY_RED))
						return objnum;
					else
						return objnum;
				} else if (curobjp->id == objid)
					return objnum;
			}

			if (objtype == OBJ_POWERUP)
				if (curobjp->contains_count)
					if (curobjp->contains_type == OBJ_POWERUP)
						if (curobjp->contains_id == objid)
							return objnum;

			objnum = curobjp->next;
		}
	}

	return -1;
}

//	-----------------------------------------------------------------------------
//	Return nearest object of interest.
//	If special == ESCORT_GOAL_PLAYER_SPEW, then looking for any object spewed by player.
//	-1 means object does not exist in mine.
//	-2 means object does exist in mine, but buddy-bot can't reach it (eg, behind triggered wall)
int exists_in_mine(int start_seg, int objtype, int objid, int special)
{
	int	segindex, segnum;
	short* bfs_list = new short[Segments.size()];
	int	length;

//	mprintf((0, "exists_in_mine, type == %i, id == %i\n", objtype, objid));

	create_bfs_list(start_seg, bfs_list, &length, Segments.size());

	if (objtype == FUELCEN_CHECK) {
		for (segindex=0; segindex<length; segindex++) {
			segnum = bfs_list[segindex];
			if (Segment2s[segnum].special == SEGMENT_IS_FUELCEN) {
				delete[] bfs_list;
				return segnum;
			}
		}
	} else {
		for (segindex=0; segindex<length; segindex++) {
			int	objnum;

			segnum = bfs_list[segindex];

			objnum = exists_in_mine_2(segnum, objtype, objid, special);
			if (objnum != -1) {
				delete[] bfs_list;
				return objnum;
			}

		}
	}

	//	Couldn't find what we're looking for by looking at connectivity.
	//	See if it's in the mine.  It could be hidden behind a trigger or switch
	//	which the buddybot doesn't understand.
	if (objtype == FUELCEN_CHECK) {
		for (segnum=0; segnum<=Highest_segment_index; segnum++)
			if (Segment2s[segnum].special == SEGMENT_IS_FUELCEN) {
				delete[] bfs_list;
				return -2;
			}
	} else {
		for (segnum=0; segnum<=Highest_segment_index; segnum++) {
			int	objnum;

			objnum = exists_in_mine_2(segnum, objtype, objid, special);
			if (objnum != -1) {
				delete[] bfs_list;
				return -2;
			}
		}
	}

	delete[] bfs_list;

	return -1;
}

//	-----------------------------------------------------------------------------
//	Return true if it happened, else return false.
int find_exit_segment(void)
{
	int	i,j;

	//	---------- Find exit doors ----------
	for (i=0; i<=Highest_segment_index; i++)
		for (j=0; j<MAX_SIDES_PER_SEGMENT; j++)
			if (Segments[i].children[j] == -2) {
				return i;
			}

	return -1;
}

#define	BUDDY_MARKER_TEXT_LEN	25

//	-----------------------------------------------------------------------------
void say_escort_goal(int goal_num)
{
	if (Player_is_dead)
		return;

	switch (goal_num) {
		case ESCORT_GOAL_BLUE_KEY:		buddy_message("Finding BLUE KEY");			break;
		case ESCORT_GOAL_GOLD_KEY:		buddy_message("Finding YELLOW KEY");		break;
		case ESCORT_GOAL_RED_KEY:		buddy_message("Finding RED KEY");			break;
		case ESCORT_GOAL_CONTROLCEN:	buddy_message("Finding REACTOR");			break;
		case ESCORT_GOAL_EXIT:			buddy_message("Finding EXIT");				break;
		case ESCORT_GOAL_ENERGY:		buddy_message("Finding ENERGY");				break;
		case ESCORT_GOAL_ENERGYCEN:	buddy_message("Finding ENERGY CENTER");	break;
		case ESCORT_GOAL_SHIELD:		buddy_message("Finding a SHIELD");			break;
		case ESCORT_GOAL_POWERUP:		buddy_message("Finding a POWERUP");			break;
		case ESCORT_GOAL_ROBOT:			buddy_message("Finding a ROBOT");			break;
		case ESCORT_GOAL_HOSTAGE:		buddy_message("Finding a HOSTAGE");			break;
		case ESCORT_GOAL_SCRAM:			buddy_message("Staying away...");			break;
		case ESCORT_GOAL_BOSS:			buddy_message("Finding BOSS robot");		break;
		case ESCORT_GOAL_PLAYER_SPEW:	buddy_message("Finding your powerups");	break;
		case ESCORT_GOAL_MARKER1:
		case ESCORT_GOAL_MARKER2:
		case ESCORT_GOAL_MARKER3:
		case ESCORT_GOAL_MARKER4:
		case ESCORT_GOAL_MARKER5:
		case ESCORT_GOAL_MARKER6:
		case ESCORT_GOAL_MARKER7:
		case ESCORT_GOAL_MARKER8:
		case ESCORT_GOAL_MARKER9:
			{ char marker_text[BUDDY_MARKER_TEXT_LEN];
			strncpy(marker_text, MarkerMessage[goal_num-ESCORT_GOAL_MARKER1], BUDDY_MARKER_TEXT_LEN-1);
			marker_text[BUDDY_MARKER_TEXT_LEN-1] = 0;
			buddy_message("Finding marker %i: '%s'", goal_num-ESCORT_GOAL_MARKER1+1, marker_text);
			break;
			}
	}
}

//	-----------------------------------------------------------------------------
void escort_create_path_to_goal(object *objp)
{
	int	goal_seg = -1;
	int			objnum = objp-Objects.data();
	ai_static	*aip = &objp->ctype.ai_info;
	ai_local		*ailp = &Ai_local_info[objnum];

	if (Escort_special_goal != -1)
		Escort_goal_object = Escort_special_goal;

	Escort_kill_object = -1;

	if (Looking_for_marker != -1) {

		Escort_goal_index = exists_in_mine(objp->segnum, OBJ_MARKER, Escort_goal_object-ESCORT_GOAL_MARKER1, -1);
		if (Escort_goal_index > -1)
			goal_seg = Objects[Escort_goal_index].segnum;
	} else {
		switch (Escort_goal_object) {
			case ESCORT_GOAL_BLUE_KEY:
				Escort_goal_index = exists_in_mine(objp->segnum, OBJ_POWERUP, POW_KEY_BLUE, -1);
				if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
				break;
			case ESCORT_GOAL_GOLD_KEY:
				Escort_goal_index = exists_in_mine(objp->segnum, OBJ_POWERUP, POW_KEY_GOLD, -1);
				if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
				break;
			case ESCORT_GOAL_RED_KEY:
				Escort_goal_index = exists_in_mine(objp->segnum, OBJ_POWERUP, POW_KEY_RED, -1);
				if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
				break;
			case ESCORT_GOAL_CONTROLCEN:
				Escort_goal_index = exists_in_mine(objp->segnum, OBJ_CNTRLCEN, -1, -1);
				if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
				break;
			case ESCORT_GOAL_EXIT:
			case ESCORT_GOAL_EXIT2:
				goal_seg = find_exit_segment();
				Escort_goal_index = goal_seg;
				break;
			case ESCORT_GOAL_ENERGY:
				Escort_goal_index = exists_in_mine(objp->segnum, OBJ_POWERUP, POW_ENERGY, -1);
				if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
				break;
			case ESCORT_GOAL_ENERGYCEN:
				goal_seg = exists_in_mine(objp->segnum, FUELCEN_CHECK, -1, -1);
				Escort_goal_index = goal_seg;
				break;
			case ESCORT_GOAL_SHIELD:
				Escort_goal_index = exists_in_mine(objp->segnum, OBJ_POWERUP, POW_SHIELD_BOOST, -1);
				if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
				break;
			case ESCORT_GOAL_POWERUP:
				Escort_goal_index = exists_in_mine(objp->segnum, OBJ_POWERUP, -1, -1);
				if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
				break;
			case ESCORT_GOAL_ROBOT:
				Escort_goal_index = exists_in_mine(objp->segnum, OBJ_ROBOT, -1, -1);
				if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
				break;
			case ESCORT_GOAL_HOSTAGE:
				Escort_goal_index = exists_in_mine(objp->segnum, OBJ_HOSTAGE, -1, -1);
				if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
				break;
			case ESCORT_GOAL_PLAYER_SPEW:
				Escort_goal_index = exists_in_mine(objp->segnum, -1, -1, ESCORT_GOAL_PLAYER_SPEW);
				if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
				break;
			case ESCORT_GOAL_SCRAM:
				goal_seg = -3;		//	Kinda a hack.
				Escort_goal_index = goal_seg;
				break;
			case ESCORT_GOAL_BOSS: {
				int	boss_id;
	
				boss_id = get_boss_id();
				Assert(boss_id != -1);
				Escort_goal_index = exists_in_mine(objp->segnum, OBJ_ROBOT, boss_id, -1);
				if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
				break;
			}
			default:
				Int3();	//	Oops, Illegal value in Escort_goal_object.
				goal_seg = 0;
				break;
		}
	}

	// -- mprintf((0, "Creating path from escort to goal #%i in segment #%i.\n", Escort_goal_object, goal_seg));
	if ((Escort_goal_index < 0) && (Escort_goal_index != -3)) {	//	I apologize for this statement -- MK, 09/22/95
		if (Escort_goal_index == -1) {
			Last_buddy_message_time = 0;	//	Force this message to get through.
			buddy_message("No %s in mine.", Escort_goal_text[Escort_goal_object-1]);
			Looking_for_marker = -1;
		} else if (Escort_goal_index == -2) {
			Last_buddy_message_time = 0;	//	Force this message to get through.
			buddy_message("Can't reach %s.", Escort_goal_text[Escort_goal_object-1]);
			Looking_for_marker = -1;
		} else
			Int3();

		Escort_goal_object = ESCORT_GOAL_UNSPECIFIED;
		Escort_special_goal = -1;
	} else {
		if (goal_seg == -3) {
			create_n_segment_path(objp, 16 + P_Rand() * 16, -1);
			aip->path_length = polish_path(objp, &Point_segs[aip->hide_index], aip->path_length);
		} else {
			create_path_to_segment(objp, goal_seg, Max_escort_length, 1);	//	MK!: Last parm (safety_flag) used to be 1!!
			if (aip->path_length > 3)
				aip->path_length = polish_path(objp, &Point_segs[aip->hide_index], aip->path_length);
			if ((aip->path_length > 0) && (Point_segs[aip->hide_index + aip->path_length - 1].segnum != goal_seg)) {
				fix	dist_to_player;
				Last_buddy_message_time = 0;	//	Force this message to get through.
				buddy_message("Can't reach %s.", Escort_goal_text[Escort_goal_object-1]);
				Looking_for_marker = -1;
				Escort_goal_object = ESCORT_GOAL_SCRAM;
				dist_to_player = find_connected_distance(objp->pos, objp->segnum, Believed_player_pos, Believed_player_seg, 100, WID_FLY_FLAG);
				if (dist_to_player > MIN_ESCORT_DISTANCE)
					create_path_to_player(objp, Max_escort_length, 1);	//	MK!: Last parm used to be 1!
				else {
					create_n_segment_path(objp, 8 + P_Rand() * 8, -1);
					aip->path_length = polish_path(objp, &Point_segs[aip->hide_index], aip->path_length);
				}
			}
		}

		ailp->mode = AIM_GOTO_OBJECT;

		say_escort_goal(Escort_goal_object);
	}

}

//	-----------------------------------------------------------------------------
//	Escort robot chooses goal object based on player's keys, location.
//	Returns goal object.
int escort_set_goal_object(void)
{
	if (Escort_special_goal != -1)
		return ESCORT_GOAL_UNSPECIFIED;
	else if (!(ConsoleObject->flags & PLAYER_FLAGS_BLUE_KEY) && (exists_in_mine(ConsoleObject->segnum, OBJ_POWERUP, POW_KEY_BLUE, -1) != -1))
		return ESCORT_GOAL_BLUE_KEY;
	else if (!(ConsoleObject->flags & PLAYER_FLAGS_GOLD_KEY) && (exists_in_mine(ConsoleObject->segnum, OBJ_POWERUP, POW_KEY_GOLD, -1) != -1))
		return ESCORT_GOAL_GOLD_KEY;
	else if (!(ConsoleObject->flags & PLAYER_FLAGS_RED_KEY) && (exists_in_mine(ConsoleObject->segnum, OBJ_POWERUP, POW_KEY_RED, -1) != -1))
		return ESCORT_GOAL_RED_KEY;
	else if (Control_center_destroyed == 0) {
		if (Boss_teleport_segs.size())
			return ESCORT_GOAL_BOSS;
		else
			return ESCORT_GOAL_CONTROLCEN;
	} else
		return ESCORT_GOAL_EXIT;
	
}

#define	MAX_ESCORT_TIME_AWAY		(F1_0*4)

fix	Buddy_last_seen_player = 0, Buddy_last_player_path_created;

//	-----------------------------------------------------------------------------
int time_to_visit_player(object *objp, ai_local *ailp, ai_static *aip)
{
	//	Note: This one has highest priority because, even if already going towards player,
	//	might be necessary to create a new path, as player can move.
	if (GameTime - Buddy_last_seen_player > MAX_ESCORT_TIME_AWAY)
		if (GameTime - Buddy_last_player_path_created > F1_0)
			return 1;

	if (ailp->mode == AIM_GOTO_PLAYER)
		return 0;

	if (objp->segnum == ConsoleObject->segnum)
		return 0;

	if (aip->cur_path_index < aip->path_length/2)
		return 0;
	
	return 1;
}

//int	Buddy_objnum; //[ISB] what are old c compilers
fix	Last_come_back_message_time = 0;

fix	Buddy_last_missile_time;

//	-----------------------------------------------------------------------------
void bash_buddy_weapon_info(int weapon_objnum)
{
	object	*objp = &Objects[weapon_objnum];

	objp->ctype.laser_info.parent_num = ConsoleObject-Objects.data();
	objp->ctype.laser_info.parent_type = OBJ_PLAYER;
	objp->ctype.laser_info.parent_signature = ConsoleObject->signature;
}

//	-----------------------------------------------------------------------------
int maybe_buddy_fire_mega(int objnum)
{
	object	*objp = &Objects[objnum];
	object	*buddy_objp = &Objects[Buddy_objnum];
	fix		dist, dot;
	vms_vector	vec_to_robot;
	int		weapon_objnum;

	vm_vec_sub(&vec_to_robot, &buddy_objp->pos, &objp->pos);
	dist = vm_vec_normalize_quick(&vec_to_robot);

	if (dist > F1_0*100)
		return 0;

	dot = vm_vec_dot(&vec_to_robot, &buddy_objp->orient.fvec);

	if (dot < F1_0/2)
		return 0;

	if (!object_to_object_visibility(buddy_objp, objp, FQ_TRANSWALL))
		return 0;

	mprintf((0, "Buddy firing mega in frame %i\n", FrameCount));

	buddy_message("GAHOOGA!");

	weapon_objnum = Laser_create_new_easy( buddy_objp->orient.fvec, buddy_objp->pos, objnum, MEGA_ID, 1);

	if (weapon_objnum != -1)
		bash_buddy_weapon_info(weapon_objnum);

	return 1;
}

//	-----------------------------------------------------------------------------
int maybe_buddy_fire_smart(int objnum)
{
	object	*objp = &Objects[objnum];
	object	*buddy_objp = &Objects[Buddy_objnum];
	fix		dist;
	int		weapon_objnum;

	dist = vm_vec_dist_quick(&buddy_objp->pos, &objp->pos);

	if (dist > F1_0*80)
		return 0;

	if (!object_to_object_visibility(buddy_objp, objp, FQ_TRANSWALL))
		return 0;

	mprintf((0, "Buddy firing smart missile in frame %i\n", FrameCount));

	buddy_message("WHAMMO!");

	weapon_objnum = Laser_create_new_easy( buddy_objp->orient.fvec, buddy_objp->pos, objnum, SMART_ID, 1);

	if (weapon_objnum != -1)
		bash_buddy_weapon_info(weapon_objnum);

	return 1;
}

//	-----------------------------------------------------------------------------
void do_buddy_dude_stuff(void)
{
	int	i;

	if (!ok_for_buddy_to_talk())
		return;

	if (Buddy_last_missile_time > GameTime)
		Buddy_last_missile_time = 0;

	if (Buddy_last_missile_time + F1_0*2 < GameTime) {
		//	See if a robot potentially in view cone
		for (i=0; i<=Highest_object_index; i++)
			if ((Objects[i].type == OBJ_ROBOT) && !activeBMTable->robots[Objects[i].id].companion)
				if (maybe_buddy_fire_mega(i)) {
					Buddy_last_missile_time = GameTime;
					return;
				}

		//	See if a robot near enough that buddy should fire smart missile
		for (i=0; i<=Highest_object_index; i++)
			if ((Objects[i].type == OBJ_ROBOT) && !activeBMTable->robots[Objects[i].id].companion)
				if (maybe_buddy_fire_smart(i)) {
					Buddy_last_missile_time = GameTime;
					return;
				}

	}
}

//	-----------------------------------------------------------------------------
//	Called every frame (or something).
void do_escort_frame(object *objp, fix dist_to_player, int player_visibility)
{
	int			objnum = objp-Objects.data();
	ai_static	*aip = &objp->ctype.ai_info;
	ai_local		*ailp = &Ai_local_info[objnum];

	Buddy_objnum = objnum;

	if (player_visibility) {
		Buddy_last_seen_player = GameTime;
		if (Players[Player_num].flags & PLAYER_FLAGS_HEADLIGHT_ON)	//	DAMN! MK, stupid bug, fixed 12/08/95, changed PLAYER_FLAGS_HEADLIGHT to PLAYER_FLAGS_HEADLIGHT_ON
			if (f2i(Players[Player_num].energy) < 40)
				if ((f2i(Players[Player_num].energy)/2) & 2)
					if (!Player_is_dead)
						buddy_message("Hey, your headlight's on!");

	}

	if (cheatValues[CI_WINGNUT]) {
		do_buddy_dude_stuff();
		objp = &Objects[objnum];
	}

	if (Buddy_sorry_time + F1_0 > GameTime) {
		Last_buddy_message_time = 0;	//	Force this message to get through.
		if (Buddy_sorry_time < GameTime + F1_0*2)
			buddy_message("Oops, sorry 'bout that...");
		Buddy_sorry_time = -F1_0*2;
	}

	//	If buddy not allowed to talk, then he is locked in his room.  Make him mostly do nothing unless you're nearby.
	if (!Buddy_allowed_to_talk)
		if (dist_to_player > F1_0*100)
			aip->SKIP_AI_COUNT = (F1_0/4)/FrameTime;

	// -- mprintf((0, "%10s: Dist to player = %7.3f, segnum = %4i\n", mode_text[ailp->mode], f2fl(dist_to_player), objp->segnum));

	//	AIM_WANDER has been co-opted for buddy behavior (didn't want to modify aistruct.h)
	//	It means the object has been told to get lost and has come to the end of its path.
	//	If the player is now visible, then create a path.
	if (ailp->mode == AIM_WANDER)
		if (player_visibility) {
			// -- mprintf((0, "Buddy: Going from wander to path following!\n"));
			create_n_segment_path(objp, 16 + P_Rand() * 16, -1);
			aip->path_length = polish_path(objp, &Point_segs[aip->hide_index], aip->path_length);
		}

	if (Escort_special_goal == ESCORT_GOAL_SCRAM) {
		if (player_visibility)
			if (Escort_last_path_created + F1_0*3 < GameTime) {
				mprintf((0, "Frame %i: Buddy creating new scram path.\n", FrameCount));
				create_n_segment_path(objp, 10 + P_Rand() * 16, ConsoleObject->segnum);
				Escort_last_path_created = GameTime;
			}

		// -- Int3();
		// -- mprintf((0, "Buddy: Seg = %3i, dist = %7.3f\n", objp->segnum, f2fl(dist_to_player)));
		return;
	}

	//	Force checking for new goal every 5 seconds, and create new path, if necessary.
	if (((Escort_special_goal != ESCORT_GOAL_SCRAM) && ((Escort_last_path_created + F1_0*5) < GameTime)) ||
		((Escort_special_goal == ESCORT_GOAL_SCRAM) && ((Escort_last_path_created + F1_0*15) < GameTime))) {
		Escort_goal_object = ESCORT_GOAL_UNSPECIFIED;
		Escort_last_path_created = GameTime;
	}

	if ((Escort_special_goal != ESCORT_GOAL_SCRAM) && time_to_visit_player(objp, ailp, aip)) {
		int	max_len;

		Buddy_last_player_path_created = GameTime;
		ailp->mode = AIM_GOTO_PLAYER;
		if (!player_visibility) {
			if ((Last_come_back_message_time + F1_0 < GameTime) || (Last_come_back_message_time > GameTime)) {
				buddy_message("Coming back to get you.");
				Last_come_back_message_time = GameTime;
			}
		}
		//	No point in Buddy creating very long path if he's not allowed to talk.  Really kills framerate.
		max_len = Max_escort_length;
		if (!Buddy_allowed_to_talk)
			max_len = 3;
		create_path_to_player(objp, max_len, 1);	//	MK!: Last parm used to be 1!
		aip->path_length = polish_path(objp, &Point_segs[aip->hide_index], aip->path_length);
		// -- mprintf((0, "Creating path to player, length = %i\n", aip->path_length));
		ailp->mode = AIM_GOTO_PLAYER;
	}	else if (GameTime - Buddy_last_seen_player > MAX_ESCORT_TIME_AWAY) {
		//	This is to prevent buddy from looking for a goal, which he will do because we only allow path creation once/second.
		return;
	} else if ((ailp->mode == AIM_GOTO_PLAYER) && (dist_to_player < MIN_ESCORT_DISTANCE)) {
		Escort_goal_object = escort_set_goal_object();
		ailp->mode = AIM_GOTO_OBJECT;		//	May look stupid to be before path creation, but ai_door_is_openable uses mode to determine what doors can be got through
		escort_create_path_to_goal(objp);
		aip->path_length = polish_path(objp, &Point_segs[aip->hide_index], aip->path_length);
		// mprintf((0, "Creating path to goal, length = %i\n", aip->path_length));
		if (aip->path_length < 3) {
			create_n_segment_path(objp, 5, Believed_player_seg);
			// mprintf((0, "Path to goal has length %i, just wandering...\n", aip->path_length));
		}
		ailp->mode = AIM_GOTO_OBJECT;
	} else if (Escort_goal_object == ESCORT_GOAL_UNSPECIFIED) {
		if ((ailp->mode != AIM_GOTO_PLAYER) || (dist_to_player < MIN_ESCORT_DISTANCE)) {
			Escort_goal_object = escort_set_goal_object();
			ailp->mode = AIM_GOTO_OBJECT;		//	May look stupid to be before path creation, but ai_door_is_openable uses mode to determine what doors can be got through
			escort_create_path_to_goal(objp);
			aip->path_length = polish_path(objp, &Point_segs[aip->hide_index], aip->path_length);
			// mprintf((0, "Creating path to goal, length = %i\n", aip->path_length));
			if (aip->path_length < 3) {
				create_n_segment_path(objp, 5, Believed_player_seg);
				// mprintf((0, "Path to goal has length %i, just wandering...\n", aip->path_length));
			}
			ailp->mode = AIM_GOTO_OBJECT;
		}
	} else
		; // mprintf((0, "!"));

}

void invalidate_escort_goal(void)
{
	Escort_goal_object = -1;
}

//	-------------------------------------------------------------------------------------------------
void do_snipe_frame(object *objp, fix dist_to_player, int player_visibility, vms_vector vec_to_player)
{
	int			objnum = objp-Objects.data();
	ai_local		*ailp = &Ai_local_info[objnum];
	fix			connected_distance;

	if (dist_to_player > F1_0*500)
		return;

	// -- mprintf((0, "Mode: %10s, Dist: %7.3f\n", mode_text[ailp->mode], f2fl(dist_to_player)));

	switch (ailp->mode) {
		case AIM_SNIPE_WAIT:
			if ((dist_to_player > F1_0*50) && (ailp->next_action_time > 0))
				return;

			ailp->next_action_time = SNIPE_WAIT_TIME;

			connected_distance = find_connected_distance(objp->pos, objp->segnum, Believed_player_pos, Believed_player_seg, 30, WID_FLY_FLAG);
			if (connected_distance < F1_0*500) {
				// -- mprintf((0, "Object #%i entering attack mode.\n", objnum));
				create_path_to_player(objp, 30, 1);
				ailp->mode = AIM_SNIPE_ATTACK;
				ailp->next_action_time = SNIPE_ATTACK_TIME;	//	have up to 10 seconds to find player.
			}
			break;

		case AIM_SNIPE_RETREAT:
		case AIM_SNIPE_RETREAT_BACKWARDS:
			if (ailp->next_action_time < 0) {
				ailp->mode = AIM_SNIPE_WAIT;
				ailp->next_action_time = SNIPE_WAIT_TIME;
				// -- mprintf((0, "Object #%i going from retreat to wait.\n", objnum));
			} else if ((player_visibility == 0) || (ailp->next_action_time > SNIPE_ABORT_RETREAT_TIME)) {
				ai_follow_path(objp, player_visibility, player_visibility, vec_to_player);
				ailp->mode = AIM_SNIPE_RETREAT_BACKWARDS;
			} else {
				// -- mprintf((0, "Object #%i going from retreat to fire.\n", objnum));
				ailp->mode = AIM_SNIPE_FIRE;
				ailp->next_action_time = SNIPE_FIRE_TIME/2;
			}
			break;

		case AIM_SNIPE_ATTACK:
			if (ailp->next_action_time < 0) {
				// -- mprintf((0, "Object #%i timed out from attack to retreat mode.\n", objnum));
				ailp->mode = AIM_SNIPE_RETREAT;
				ailp->next_action_time = SNIPE_WAIT_TIME;
			} else {
				// -- mprintf((0, "Object #%i attacking: visibility = %i\n", player_visibility));
				ai_follow_path(objp, player_visibility, player_visibility, vec_to_player);
				if (player_visibility) {
					ailp->mode = AIM_SNIPE_FIRE;
					ailp->next_action_time = SNIPE_FIRE_TIME;
				} else
					ailp->mode = AIM_SNIPE_ATTACK;
			}
			break;

		case AIM_SNIPE_FIRE:
			if (ailp->next_action_time < 0) {
				ai_static	*aip = &objp->ctype.ai_info;
				// -- mprintf((0, "Object #%i going from fire to retreat.\n", objnum));
				create_n_segment_path(objp, 10 + P_Rand()/2048, ConsoleObject->segnum);
				aip->path_length = polish_path(objp, &Point_segs[aip->hide_index], aip->path_length);
				if (P_Rand() < 8192)
					ailp->mode = AIM_SNIPE_RETREAT_BACKWARDS;
				else
					ailp->mode = AIM_SNIPE_RETREAT;
				ailp->next_action_time = SNIPE_RETREAT_TIME;
			} else {
			}
			break;

		default:
			Int3();	//	Oops, illegal mode for snipe behavior.
			ailp->mode = AIM_SNIPE_ATTACK;
			ailp->next_action_time = F1_0;
			break;
	}

}

#define	THIEF_DEPTH	20

extern int pick_connected_segment(object *objp, int max_depth);

//	------------------------------------------------------------------------------------------------------
//	Choose segment to recreate thief in.
int choose_thief_recreation_segment(void)
{
	int	segnum = -1;
	int	cur_drop_depth;

	cur_drop_depth = THIEF_DEPTH;

	while ((segnum == -1) && (cur_drop_depth > THIEF_DEPTH/2)) {
		segnum = pick_connected_segment(&Objects[Players[Player_num].objnum], cur_drop_depth);
		if (Segment2s[segnum].special == SEGMENT_IS_CONTROLCEN)
			segnum = -1;
		cur_drop_depth--;
	}

	if (segnum == -1) {
		mprintf((1, "Warning: Unable to find a connected segment for thief recreation.\n"));
		return (P_Rand() * Highest_segment_index) >> 15;
	} else
		return segnum;

}

extern object * create_morph_robot( segment *segp, vms_vector object_pos, int object_id);

fix	Re_init_thief_time = 0x3f000000;

//	----------------------------------------------------------------------
void recreate_thief(object *objp)
{
	int			segnum;
	vms_vector	center_point;
	object		*new_obj;

	segnum = choose_thief_recreation_segment();
	compute_segment_center(&center_point, &Segments[segnum]);

	new_obj = create_morph_robot( &Segments[segnum], center_point, objp->id);
	init_ai_object(new_obj-Objects.data(), AIB_SNIPE, -1);
	Re_init_thief_time = GameTime + F1_0*10;		//	In 10 seconds, re-initialize thief.
}

//	----------------------------------------------------------------------------
#define	THIEF_ATTACK_TIME		(F1_0*10)

fix	Thief_wait_times[NDL] = {F1_0*30, F1_0*25, F1_0*20, F1_0*15, F1_0*10};

//	-------------------------------------------------------------------------------------------------
void do_thief_frame(object *objp, fix dist_to_player, int player_visibility, vms_vector vec_to_player)
{
	int			objnum = objp-Objects.data();
	ai_local		*ailp = &Ai_local_info[objnum];
	fix			connected_distance;

	// -- mprintf((0, "%10s: Action Time: %7.3f\n", mode_text[ailp->mode], f2fl(ailp->next_action_time)));

	if ((Current_level_num < 0) && (Re_init_thief_time < GameTime)) {
		if (Re_init_thief_time > GameTime - F1_0*2)
			init_thief_for_level();
		Re_init_thief_time = 0x3f000000;
	}

	if ((dist_to_player > F1_0*500) && (ailp->next_action_time > 0))
		return;

	if (Player_is_dead)
		ailp->mode = AIM_THIEF_RETREAT;

	switch (ailp->mode) {
		case AIM_THIEF_WAIT:
			// -- mprintf((0, "WAIT\n"));

			if (ailp->player_awareness_type >= PA_PLAYER_COLLISION) {
				ailp->player_awareness_type = 0;
				// -- mprintf((0, "Thief: Awareness = %i ", ailp->player_awareness_type));

				// -- mprintf((0, "ATTACK\n"));
				create_path_to_player(objp, 30, 1);
				ailp->mode = AIM_THIEF_ATTACK;
				ailp->next_action_time = THIEF_ATTACK_TIME/2;
				return;
			} else if (player_visibility) {
				// -- mprintf((0, "RETREAT\n"));
				create_n_segment_path(objp, 15, ConsoleObject->segnum);
				ailp->mode = AIM_THIEF_RETREAT;
				return;
			}

			if ((dist_to_player > F1_0*50) && (ailp->next_action_time > 0))
				return;

			ailp->next_action_time = Thief_wait_times[Difficulty_level]/2;

			connected_distance = find_connected_distance(objp->pos, objp->segnum, Believed_player_pos, Believed_player_seg, 30, WID_FLY_FLAG);
			if (connected_distance < F1_0*500) {
				// -- mprintf((0, "Thief creating path to player.\n", objnum));
				create_path_to_player(objp, 30, 1);
				ailp->mode = AIM_THIEF_ATTACK;
				ailp->next_action_time = THIEF_ATTACK_TIME;	//	have up to 10 seconds to find player.
			}

			break;

		case AIM_THIEF_RETREAT:
			// -- mprintf((0, "RETREAT\n"));

			if (ailp->next_action_time < 0) {
				ailp->mode = AIM_THIEF_WAIT;
				ailp->next_action_time = Thief_wait_times[Difficulty_level];
			} else if ((dist_to_player < F1_0*100) || player_visibility || (ailp->player_awareness_type >= PA_PLAYER_COLLISION)) {
				ai_follow_path(objp, player_visibility, player_visibility, vec_to_player);
				if ((dist_to_player < F1_0*100) || (ailp->player_awareness_type >= PA_PLAYER_COLLISION)) {
					ai_static	*aip = &objp->ctype.ai_info;
					if (((aip->cur_path_index <=1) && (aip->PATH_DIR == -1)) || ((aip->cur_path_index >= aip->path_length-1) && (aip->PATH_DIR == 1))) {
						ailp->player_awareness_type = 0;
						create_n_segment_path(objp, 10, ConsoleObject->segnum);

						//	If path is real short, try again, allowing to go through player's segment
						if (aip->path_length < 4) {
							// -- mprintf((0, "Thief is cornered.  Willing to fly through player.\n"));
							create_n_segment_path(objp, 10, -1);
						} else if (objp->shields* 4 < activeBMTable->robots[objp->id].strength) {
							//	If robot really low on hits, will run through player with even longer path
							if (aip->path_length < 8) {
								create_n_segment_path(objp, 10, -1);
							}
						}

						ailp->mode = AIM_THIEF_RETREAT;
						// -- mprintf((0, "Thief creating new RETREAT path.\n"));
					}
				} else
					ailp->mode = AIM_THIEF_RETREAT;

			}

			break;

		//	This means the thief goes from wherever he is to the player.
		//	Note: When thief successfully steals something, his action time is forced negative and his mode is changed
		//			to retreat to get him out of attack mode.
		case AIM_THIEF_ATTACK:
			// -- mprintf((0, "ATTACK\n"));

			if (ailp->player_awareness_type >= PA_PLAYER_COLLISION) {
				ailp->player_awareness_type = 0;
				if (P_Rand() > 8192) {
					// --- mprintf((0, "RETREAT!!\n"));
					create_n_segment_path(objp, 10, ConsoleObject->segnum);
					Ai_local_info[objp-Objects.data()].next_action_time = Thief_wait_times[Difficulty_level]/2;
					Ai_local_info[objp-Objects.data()].mode = AIM_THIEF_RETREAT;
				}
			} else if (ailp->next_action_time < 0) {
				//	This forces him to create a new path every second.
				ailp->next_action_time = F1_0;
				create_path_to_player(objp, 100, 0);
				ailp->mode = AIM_THIEF_ATTACK;
				// -- mprintf((0, "Creating path to player.\n"));
			} else {
				if (player_visibility && (dist_to_player < F1_0*100)) {
					//	If the player is close to looking at the thief, thief shall run away.
					//	No more stupid thief trying to sneak up on you when you're looking right at him!
					if (dist_to_player > F1_0*60) {
						fix	dot = vm_vec_dot(&vec_to_player, &ConsoleObject->orient.fvec);
						if (dot < -F1_0/2) {	//	Looking at least towards thief, so thief will run!
							create_n_segment_path(objp, 10, ConsoleObject->segnum);
							Ai_local_info[objp-Objects.data()].next_action_time = Thief_wait_times[Difficulty_level]/2;
							Ai_local_info[objp-Objects.data()].mode = AIM_THIEF_RETREAT;
						}
					} 
					ai_turn_towards_vector(vec_to_player, objp, F1_0/4);
					move_towards_player(objp, vec_to_player);
				} else {
					ai_static	*aip = &objp->ctype.ai_info;
					//	If path length == 0, then he will keep trying to create path, but he is probably stuck in his closet.
					if ((aip->path_length > 1) || ((FrameCount & 0x0f) == 0)) {
						ai_follow_path(objp, player_visibility, player_visibility, vec_to_player);
						ailp->mode = AIM_THIEF_ATTACK;
					}
				}
			}
			break;

		default:
			mprintf ((0,"Thief mode (broken) = %d\n",ailp->mode));
			// -- Int3();	//	Oops, illegal mode for thief behavior.
			ailp->mode = AIM_THIEF_ATTACK;
			ailp->next_action_time = F1_0;
			break;
	}

}

//	----------------------------------------------------------------------------
//	Return true if this item (whose presence is indicated by Players[player_num].flags) gets stolen.
int maybe_steal_flag_item(int player_num, int flagval)
{
	if (Players[player_num].flags & flagval) {
		if (P_Rand() < THIEF_PROBABILITY) {
			int	powerup_index=-1;
			Players[player_num].flags &= (~flagval);
			// -- mprintf((0, "You lost your %4x capability!\n", flagval));
			switch (flagval) {
				case PLAYER_FLAGS_INVULNERABLE:
					powerup_index = POW_INVULNERABILITY;
					thief_message("Invulnerability stolen!");
					break;
				case PLAYER_FLAGS_CLOAKED:
					powerup_index = POW_CLOAK;
					thief_message("Cloak stolen!");
					break;
				case PLAYER_FLAGS_MAP_ALL:
					powerup_index = POW_FULL_MAP;
					thief_message("Full map stolen!");
					break;
				case PLAYER_FLAGS_QUAD_LASERS:
					powerup_index = POW_QUAD_FIRE;
					thief_message("Quad lasers stolen!");
					break;
				case PLAYER_FLAGS_AFTERBURNER:
					powerup_index = POW_AFTERBURNER;
					thief_message("Afterburner stolen!");
					break;
// --				case PLAYER_FLAGS_AMMO_RACK:
// --					powerup_index = POW_AMMO_RACK;
// --					thief_message("Ammo Rack stolen!");
// --					break;
				case PLAYER_FLAGS_CONVERTER:
					powerup_index = POW_CONVERTER;
					thief_message("Converter stolen!");
					break;
				case PLAYER_FLAGS_HEADLIGHT:
					powerup_index = POW_HEADLIGHT;
					thief_message("Headlight stolen!");
				   Players[Player_num].flags &= ~PLAYER_FLAGS_HEADLIGHT_ON;
					break;
			}
			Assert(powerup_index != -1);
			Stolen_items[Stolen_item_index] = powerup_index;

			digi_play_sample_once(SOUND_WEAPON_STOLEN, F1_0);
			return 1;
		}
	}

	return 0;
}

//	----------------------------------------------------------------------------
int maybe_steal_secondary_weapon(int player_num, int weapon_num)
{
	if ((Players[player_num].secondary_weapon_flags & HAS_FLAG(weapon_num)) && Players[player_num].secondary_ammo[weapon_num])
		if (P_Rand() < THIEF_PROBABILITY) {
			if (weapon_num == PROXIMITY_INDEX)
				if (P_Rand() > 8192)		//	Come in groups of 4, only add 1/4 of time.
					return 0;
			Players[player_num].secondary_ammo[weapon_num]--;

			//	Smart mines and proxbombs don't get dropped because they only come in 4 packs.
			if ((weapon_num != PROXIMITY_INDEX) && (weapon_num != SMART_MINE_INDEX)) {
				Stolen_items[Stolen_item_index] = Secondary_weapon_to_powerup[weapon_num];
			}

			thief_message("%s stolen!", Text_string[114+weapon_num]);		//	Danger! Danger! Use of literal!  Danger!
			if (Players[Player_num].secondary_ammo[weapon_num] == 0)
				auto_select_weapon(1);

			// -- compress_stolen_items();
			digi_play_sample_once(SOUND_WEAPON_STOLEN, F1_0);
			return 1;
		}

	return 0;
}

//	----------------------------------------------------------------------------
int maybe_steal_primary_weapon(int player_num, int weapon_num)
{
	if ((Players[player_num].primary_weapon_flags & HAS_FLAG(weapon_num)) && Players[player_num].primary_ammo[weapon_num]) {
		if (P_Rand() < THIEF_PROBABILITY) {
			if (weapon_num == 0) {
				if (Players[player_num].laser_level > 0) {
					if (Players[player_num].laser_level > 3) {
						Stolen_items[Stolen_item_index] = POW_SUPER_LASER;
					} else {
						Stolen_items[Stolen_item_index] = Primary_weapon_to_powerup[weapon_num];
					}
					thief_message("%s level decreased!", Text_string[104+weapon_num]);		//	Danger! Danger! Use of literal!  Danger!
					Players[player_num].laser_level--;
					digi_play_sample_once(SOUND_WEAPON_STOLEN, F1_0);
					return 1;
				}
			} else if (Players[player_num].primary_weapon_flags & (1 << weapon_num)) {
				Players[player_num].primary_weapon_flags &= ~(1 << weapon_num);
				Stolen_items[Stolen_item_index] = Primary_weapon_to_powerup[weapon_num];

				thief_message("%s stolen!", Text_string[104+weapon_num]);		//	Danger! Danger! Use of literal!  Danger!
				auto_select_weapon(0);
				digi_play_sample_once(SOUND_WEAPON_STOLEN, F1_0);
				return 1;
			}
		}
	}

	return 0;
}



//	----------------------------------------------------------------------------
//	Called for a thief-type robot.
//	If a item successfully stolen, returns true, else returns false.
//	If a wapon successfully stolen, do everything, removing it from player,
//	updating Stolen_items information, deselecting, etc.
int attempt_to_steal_item_3(object *objp, int player_num)
{
	int	i;

	if (Ai_local_info[objp-Objects.data()].mode != AIM_THIEF_ATTACK)
		return 0;

	//	First, try to steal equipped items.

	if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_INVULNERABLE))
		return 1;

	//	If primary weapon = laser, first try to rip away those nasty quad lasers!
	if (Primary_weapon == 0)
		if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_QUAD_LASERS))
			return 1;

	//	Makes it more likely to steal primary than secondary.
	for (i=0; i<2; i++)
		if (maybe_steal_primary_weapon(player_num, Primary_weapon))
			return 1;

	if (maybe_steal_secondary_weapon(player_num, Secondary_weapon))
		return 1;

	//	See what the player has and try to snag something.
	//	Try best things first.
	if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_INVULNERABLE))
		return 1;
	if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_CLOAKED))
		return 1;
	if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_QUAD_LASERS))
		return 1;
	if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_AFTERBURNER))
		return 1;
	if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_CONVERTER))
		return 1;
// --	if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_AMMO_RACK))	//	Can't steal because what if have too many items, say 15 homing missiles?
// --		return 1;
	if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_HEADLIGHT))
		return 1;
	if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_MAP_ALL))
		return 1;

	for (i=MAX_SECONDARY_WEAPONS-1; i>=0; i--) {
		if (maybe_steal_primary_weapon(player_num, i))
			return 1;
		if (maybe_steal_secondary_weapon(player_num, i))
			return 1;
	}

	return 0;
}

//	----------------------------------------------------------------------------
int attempt_to_steal_item_2(object *objp, int player_num)
{
	int	rval;

	rval = attempt_to_steal_item_3(objp, player_num);

	if (rval) {
		Stolen_item_index = (Stolen_item_index+1) % MAX_STOLEN_ITEMS;
		if (P_Rand() > 20000)	//	Occasionally, boost the value again
			Stolen_item_index = (Stolen_item_index+1) % MAX_STOLEN_ITEMS;
	}

	return rval;
}

//	----------------------------------------------------------------------------
//	Called for a thief-type robot.
//	If a item successfully stolen, returns true, else returns false.
//	If a wapon successfully stolen, do everything, removing it from player,
//	updating Stolen_items information, deselecting, etc.
int attempt_to_steal_item(object *objp, int player_num)
{
	int	i;
	int	rval = 0;

	if (objp->ctype.ai_info.dying_start_time)
		return 0;

	rval += attempt_to_steal_item_2(objp, player_num);

	for (i=0; i<3; i++) {
		if (!rval || (P_Rand() < 11000)) {	//	about 1/3 of time, steal another item
			rval += attempt_to_steal_item_2(objp, player_num);
		} else
			break;
	}
	// -- mprintf((0, "%i items were stolen!\n", rval));

	create_n_segment_path(objp, 10, ConsoleObject->segnum);
	Ai_local_info[objp-Objects.data()].next_action_time = Thief_wait_times[Difficulty_level]/2;
	Ai_local_info[objp-Objects.data()].mode = AIM_THIEF_RETREAT;
	if (rval) {
		PALETTE_FLASH_ADD(30, 15, -20);
		update_laser_weapon_info();
//		digi_link_sound_to_pos( SOUND_NASTY_ROBOT_HIT_1, objp->segnum, 0, &objp->pos, 0 , DEFAULT_ROBOT_SOUND_VOLUME);
//	I removed this to make the "steal sound" more obvious -AP
#ifdef NETWORK
                if (Game_mode & GM_NETWORK)
                 multi_send_stolen_items();
#endif
	}
	return rval;
}

// --------------------------------------------------------------------------------------------------------------
//	Indicate no items have been stolen.
void init_thief_for_level(void)
{
	int	i;

	for (i=0; i<MAX_STOLEN_ITEMS; i++)
		Stolen_items[i] = 255;

	Assert (MAX_STOLEN_ITEMS >= 3*2);	//	Oops!  Loop below will overwrite memory!
  
   if (!(Game_mode & GM_MULTI))    
		for (i=0; i<3; i++) {
			Stolen_items[2*i] = POW_SHIELD_BOOST;
			Stolen_items[2*i+1] = POW_ENERGY;
		}

	Stolen_item_index = 0;
}

// --------------------------------------------------------------------------------------------------------------
void drop_stolen_items(object *objp)
{
	int	i;

        mprintf ((0,"Dropping thief items!\n"));

	// -- compress_stolen_items();

	for (i=0; i<MAX_STOLEN_ITEMS; i++) {
		if (Stolen_items[i] != 255)
			drop_powerup(OBJ_POWERUP, Stolen_items[i], 1, objp->mtype.phys_info.velocity, objp->pos, objp->segnum);
		Stolen_items[i] = 255;
	}

}

void show_escort_menu(char* msg);

// --------------------------------------------------------------------------------------------------------------
void do_escort_menu(void)
{
	int	i;
	char	msg[300];
	int	paused;
	int	key;
	int	next_goal;
	char	goal_str[32], tstr[32];

	if (Game_mode & GM_MULTI) 
	{
		HUD_init_message("No Guide-Bot in Multiplayer!");
		return;
	}

	for (i=0; i<=Highest_object_index; i++) 
	{
		if (Objects[i].type == OBJ_ROBOT)
			if (activeBMTable->robots[Objects[i].id].companion)
				break;
	}

	if (i > Highest_object_index) 
	{
		HUD_init_message("No Guide-Bot present in mine!");

		#ifndef NDEBUG
		//	If no buddy bot, create one!
		HUD_init_message("Debug Version: Creating Guide-Bot!");
		create_buddy_bot();
		#else
		return;
		#endif
	}

	ok_for_buddy_to_talk();	//	Needed here or we might not know buddy can talk when he can.

	if (!Buddy_allowed_to_talk) 
	{
		HUD_init_message("%s has not been released",guidebot_name);
		return;
	}

	digi_pause_digi_sounds();
	stop_time();

	palette_save();
	apply_modified_palette();
	reset_palette_add();

	game_flush_inputs();

	paused = 1;

//	set_screen_mode( SCREEN_MENU );
	set_popup_screen();
	gr_palette_load( gr_palette );

	//	This prevents the buddy from coming back if you've told him to scram.
	//	If we don't set next_goal, we get garbage there.
	if (Escort_special_goal == ESCORT_GOAL_SCRAM)
	{
		Escort_special_goal = -1;	//	Else setting next goal might fail.
		next_goal = escort_set_goal_object();
		Escort_special_goal = ESCORT_GOAL_SCRAM;
	} else 
	{
		Escort_special_goal = -1;	//	Else setting next goal might fail.
		next_goal = escort_set_goal_object();
	}

	switch (next_goal) 
	{
	#ifndef NDEBUG
		case ESCORT_GOAL_UNSPECIFIED:
			Int3();
			snprintf(goal_str, 6, "ERROR");
			break;
	#endif
			
		case ESCORT_GOAL_BLUE_KEY:
			snprintf(goal_str, 9, "blue key");
			break;
		case ESCORT_GOAL_GOLD_KEY:
			snprintf(goal_str, 11, "yellow key");
			break;
		case ESCORT_GOAL_RED_KEY:
			snprintf(goal_str, 8, "red key");
			break;
		case ESCORT_GOAL_CONTROLCEN:
			snprintf(goal_str, 8, "reactor");
			break;
		case ESCORT_GOAL_BOSS:
			snprintf(goal_str, 5, "boss");
			break;
		case ESCORT_GOAL_EXIT:
			snprintf(goal_str, 5, "exit");
			break;
		case ESCORT_GOAL_MARKER1:
		case ESCORT_GOAL_MARKER2:
		case ESCORT_GOAL_MARKER3:
		case ESCORT_GOAL_MARKER4:
		case ESCORT_GOAL_MARKER5:
		case ESCORT_GOAL_MARKER6:
		case ESCORT_GOAL_MARKER7:
		case ESCORT_GOAL_MARKER8:
		case ESCORT_GOAL_MARKER9:
			snprintf(goal_str, 12, "marker %i", next_goal-ESCORT_GOAL_MARKER1+1);
			break;

	}
			
	if (!Buddy_messages_suppressed)
		snprintf(tstr, 9, "Suppress");
	else
		snprintf(tstr, 7, "Enable");

	snprintf(msg, 300, "Select Guide-Bot Command:\n\n"
						"0.  Next Goal: %s" CC_LSPACING_S "3\n"
						"\x84.  Find Energy Powerup" CC_LSPACING_S "3\n"
						"2.  Find Energy Center" CC_LSPACING_S "3\n"
						"3.  Find Shield Powerup" CC_LSPACING_S "3\n"
						"4.  Find Any Powerup" CC_LSPACING_S "3\n"
						"5.  Find a Robot" CC_LSPACING_S "3\n"
						"6.  Find a Hostage" CC_LSPACING_S "3\n"
						"7.  Stay Away From Me" CC_LSPACING_S "3\n"
						"8.  Find My Powerups" CC_LSPACING_S "3\n"
						"9.  Find the exit\n\n"
						"T.  %s Messages\n"
						// -- "9.	Find the exit" CC_LSPACING_S "3\n"
				, goal_str, tstr);

	show_escort_menu(msg);		//TXT_PAUSE);
	plat_present_canvas(0);

	while (paused) 
	{
	#ifdef WINDOWS
		while (!(key = key_inkey()))
		{
			MSG wmsg;
			DoMessageStuff(&wmsg);
		}
	#else
		key = key_getch();
	#endif

		switch (key) 
		{
			case KEY_0:
			case KEY_1:
			case KEY_2:
			case KEY_3:
			case KEY_4:
			case KEY_5:
			case KEY_6:
			case KEY_7:
			case KEY_8:
			case KEY_9:
				Looking_for_marker = -1;
				Last_buddy_key = -1;
				set_escort_special_goal(key);
				Last_buddy_key = -1;
				paused = 0;
				break;

			case KEY_ESC:
			case KEY_ENTER:
				clear_boxed_message();
				paused=0;
				break;

//--10/08/95-- Screwed up font, background.  Why needed, anyway?
//--10/08/95--			case KEY_F1:
//--10/08/95-- 				clear_boxed_message();
//--10/08/95--				do_show_help();
//--10/08/95--				show_boxed_message(msg);
//--10/08/95--				break;

			case KEY_PRINT_SCREEN:
				save_screen_shot(0);
				break;

			#ifndef RELEASE
			case KEY_BACKSP: Int3(); break;
			#endif

			case KEY_T: 
			{
				char msg[32];
				int temp;

				temp = !Buddy_messages_suppressed;

				if (temp)
					strcpy(msg, "suppressed");
				else
					strcpy(msg, "enabled");

				Buddy_messages_suppressed = 1;
				buddy_message("Messages %s.", msg);

				Buddy_messages_suppressed = temp;

				paused = 0;
				break;
			}
			default:
				break;
		}
	}
	game_flush_inputs();

	palette_restore();

	start_time();
	digi_resume_digi_sounds();

}

//	-------------------------------------------------------------------------------
//	Show the Buddy menu!
void show_escort_menu(char *msg)
{	
	int	w,h,aw;
	int	x,y;

	gr_set_current_canvas(&VR_screen_pages);

	gr_set_curfont( GAME_FONT );

	gr_get_string_size(msg,&w,&h,&aw);

	x = (grd_curscreen->sc_w-w)/2;
	y = (grd_curscreen->sc_h-h)/4;

	gr_set_fontcolor( gr_getcolor(0, 28, 0), -1 );
   
	//[ISB] I need to figure out some solution for the poly accel stuff
   //PA_DFX (pa_set_frontbuffer_current());
	//PA_DFX (nm_draw_background(x-15,y-15,x+w+15-1,y+h+15-1));
   //PA_DFX (pa_set_backbuffer_current());
   nm_draw_background(x-15,y-15,x+w+15-1,y+h+15-1);

WIN(DDGRLOCK(dd_grd_curcanv));\
	//PA_DFX (pa_set_frontbuffer_current());
  	//PA_DFX (gr_ustring( x, y, msg ));
	//PA_DFX (pa_set_backbuffer_current());
  	gr_ustring( x, y, msg );
WIN(DDGRUNLOCK(dd_grd_curcanv));

	reset_cockpit();
}

