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
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "platform/mono.h"
#include "platform/key.h"
#include "2d/gr.h"
#include "2d/palette.h"

#include "inferno.h"
#ifdef EDITOR
#include "editor\editor.h"
#endif
#include "misc/error.h"
#include "object.h"
#include "wall.h"
#include "gamemine.h"
#include "robot.h"
#include "player.h"
#include "newmenu.h"
#include "textures.h"

#include "bm.h"
#include "menu.h"
#include "switch.h"
#include "fuelcen.h"
#include "powerup.h"
#include "gameseq.h"
#include "polyobj.h"
#include "gamesave.h"


#ifdef EDITOR

void say_totals(FILE* my_file, char* level_name);
void dump_used_textures_level(FILE* my_file, int level_num);
extern uint8_t bogus_data[64 * 64];
extern grs_bitmap bogus_bitmap;

//	--------------------------------------------------------------------------------
char* object_types(int objnum)
{
	int	type = Objects[objnum].type;

	Assert((type >= 0) && (type < MAX_OBJECT_TYPES));
	return	Object_type_names[type];
}

//	--------------------------------------------------------------------------------
char* object_ids(int objnum)
{
	int	type = Objects[objnum].type;
	int	id = Objects[objnum].id;

	switch (type)
	{
	case OBJ_ROBOT:
		return Robot_names[id];
		break;
	case OBJ_POWERUP:
		return Powerup_names[id];
		break;
	}

	return	NULL;
}

void err_printf(FILE* my_file, char* format, ...)
{
	va_list	args;
	char		message[256];

	va_start(args, format);
	vsnprintf(message, 256, format, args);
	va_end(args);

	mprintf((1, "%s", message));
	fprintf(my_file, "%s", message);
	Errors_in_mine++;
}

void warning_printf(FILE* my_file, char* format, ...)
{
	va_list	args;
	char		message[256];

	va_start(args, format);
	vsnprintf(message, 256, format, args);
	va_end(args);

	mprintf((0, "%s", message));
	fprintf(my_file, "%s", message);
}

//	-----------------------------------------------------------------------------------------------------------
void write_exit_text(FILE* my_file)
{
	int	i, j, count;

	fprintf(my_file, "-----------------------------------------------------------------------------\n");
	fprintf(my_file, "Exit stuff\n");

	//	---------- Find exit triggers ----------
	count = 0;
	for (i = 0; i < Num_triggers; i++)
		if (Triggers[i].type == TT_EXIT) {
			int	count2;

			fprintf(my_file, "Trigger %2i, is an exit trigger with %i links.\n", i, Triggers[i].num_links);
			count++;
			if (Triggers[i].num_links != 0)
				err_printf(my_file, "Error: Exit triggers must have 0 links, this one has %i links.\n", Triggers[i].num_links);

			//	Find wall pointing to this trigger.
			count2 = 0;
			for (j = 0; j < Num_walls; j++)
				if (Walls[j].trigger == i) {
					count2++;
					fprintf(my_file, "Exit trigger %i is in segment %i, on side %i, bound to wall %i\n", i, Walls[j].segnum, Walls[j].sidenum, j);
				}
			if (count2 == 0)
				err_printf(my_file, "Error: Trigger %i is not bound to any wall.\n", i);
			else if (count2 > 1)
				err_printf(my_file, "Error: Trigger %i is bound to %i walls.\n", i, count2);

		}

	if (count == 0)
		err_printf(my_file, "Error: No exit trigger in this mine.\n");
	else if (count != 1)
		err_printf(my_file, "Error: More than one exit trigger in this mine.\n");
	else
		fprintf(my_file, "\n");

	//	---------- Find exit doors ----------
	count = 0;
	for (i = 0; i <= Highest_segment_index; i++)
		for (j = 0; j < MAX_SIDES_PER_SEGMENT; j++)
			if (Segments[i].children[j] == -2) {
				fprintf(my_file, "Segment %3i, side %i is an exit door.\n", i, j);
				count++;
			}

	if (count == 0)
		err_printf(my_file, "Error: No external wall in this mine.\n");
	else if (count != 1) {
		// -- warning_printf(my_file, "Warning: %i external walls in this mine.\n", count);
		// -- warning_printf(my_file, "(If %i are secret exits, then no problem.)\n", count-1); 
	}
	else
		fprintf(my_file, "\n");
}

//	-----------------------------------------------------------------------------------------------------------
void write_key_text(FILE* my_file)
{
	int	i;
	int	red_count, blue_count, gold_count;
	int	red_count2, blue_count2, gold_count2;
	int	blue_segnum = -1, blue_sidenum = -1, red_segnum = -1, red_sidenum = -1, gold_segnum = -1, gold_sidenum = -1;
	int	connect_side;

	fprintf(my_file, "-----------------------------------------------------------------------------\n");
	fprintf(my_file, "Key stuff:\n");

	red_count = 0;
	blue_count = 0;
	gold_count = 0;

	for (i = 0; i < Num_walls; i++) {
		if (Walls[i].keys & KEY_BLUE) {
			fprintf(my_file, "Wall %i (seg=%i, side=%i) is keyed to the blue key.\n", i, Walls[i].segnum, Walls[i].sidenum);
			if (blue_segnum == -1) {
				blue_segnum = Walls[i].segnum;
				blue_sidenum = Walls[i].sidenum;
				blue_count++;
			}
			else {
				connect_side = find_connect_side(&Segments[Walls[i].segnum], &Segments[blue_segnum]);
				if (connect_side != blue_sidenum) {
					warning_printf(my_file, "Warning: This blue door at seg %i, is different than the one at seg %i, side %i\n", Walls[i].segnum, blue_segnum, blue_sidenum);
					blue_count++;
				}
			}
		}
		if (Walls[i].keys & KEY_RED) {
			fprintf(my_file, "Wall %i (seg=%i, side=%i) is keyed to the red key.\n", i, Walls[i].segnum, Walls[i].sidenum);
			if (red_segnum == -1) {
				red_segnum = Walls[i].segnum;
				red_sidenum = Walls[i].sidenum;
				red_count++;
			}
			else {
				connect_side = find_connect_side(&Segments[Walls[i].segnum], &Segments[red_segnum]);
				if (connect_side != red_sidenum) {
					warning_printf(my_file, "Warning: This red door at seg %i, is different than the one at seg %i, side %i\n", Walls[i].segnum, red_segnum, red_sidenum);
					red_count++;
				}
			}
		}
		if (Walls[i].keys & KEY_GOLD) {
			fprintf(my_file, "Wall %i (seg=%i, side=%i) is keyed to the gold key.\n", i, Walls[i].segnum, Walls[i].sidenum);
			if (gold_segnum == -1) {
				gold_segnum = Walls[i].segnum;
				gold_sidenum = Walls[i].sidenum;
				gold_count++;
			}
			else {
				connect_side = find_connect_side(&Segments[Walls[i].segnum], &Segments[gold_segnum]);
				if (connect_side != gold_sidenum) {
					warning_printf(my_file, "Warning: This gold door at seg %i, is different than the one at seg %i, side %i\n", Walls[i].segnum, gold_segnum, gold_sidenum);
					gold_count++;
				}
			}
		}
	}

	if (blue_count > 1)
		warning_printf(my_file, "Warning: %i doors are keyed to the blue key.\n", blue_count);

	if (red_count > 1)
		warning_printf(my_file, "Warning: %i doors are keyed to the red key.\n", red_count);

	if (gold_count > 1)
		warning_printf(my_file, "Warning: %i doors are keyed to the gold key.\n", gold_count);

	red_count2 = 0;
	blue_count2 = 0;
	gold_count2 = 0;

	for (i = 0; i <= Highest_object_index; i++) {
		if (Objects[i].type == OBJ_POWERUP)
			if (Objects[i].id == POW_KEY_BLUE) {
				fprintf(my_file, "The BLUE key is object %i in segment %i\n", i, Objects[i].segnum);
				blue_count2++;
			}
		if (Objects[i].type == OBJ_POWERUP)
			if (Objects[i].id == POW_KEY_RED) {
				fprintf(my_file, "The RED key is object %i in segment %i\n", i, Objects[i].segnum);
				red_count2++;
			}
		if (Objects[i].type == OBJ_POWERUP)
			if (Objects[i].id == POW_KEY_GOLD) {
				fprintf(my_file, "The GOLD key is object %i in segment %i\n", i, Objects[i].segnum);
				gold_count2++;
			}

		if (Objects[i].contains_count) {
			if (Objects[i].contains_type == OBJ_POWERUP) {
				switch (Objects[i].contains_id) {
				case POW_KEY_BLUE:
					fprintf(my_file, "The BLUE key is contained in object %i (a %s %s) in segment %i\n", i, Object_type_names[Objects[i].type], Robot_names[Objects[i].id], Objects[i].segnum);
					blue_count2 += Objects[i].contains_count;
					break;
				case POW_KEY_GOLD:
					fprintf(my_file, "The GOLD key is contained in object %i (a %s %s) in segment %i\n", i, Object_type_names[Objects[i].type], Robot_names[Objects[i].id], Objects[i].segnum);
					gold_count2 += Objects[i].contains_count;
					break;
				case POW_KEY_RED:
					fprintf(my_file, "The RED key is contained in object %i (a %s %s) in segment %i\n", i, Object_type_names[Objects[i].type], Robot_names[Objects[i].id], Objects[i].segnum);
					red_count2 += Objects[i].contains_count;
					break;
				default:
					break;
				}
			}
		}
	}

	if (blue_count)
		if (blue_count2 == 0)
			err_printf(my_file, "Error: There is a door keyed to the blue key, but no blue key!\n");

	if (red_count)
		if (red_count2 == 0)
			err_printf(my_file, "Error: There is a door keyed to the red key, but no red key!\n");

	if (gold_count)
		if (gold_count2 == 0)
			err_printf(my_file, "Error: There is a door keyed to the gold key, but no gold key!\n");

	if (blue_count2 > 1)
		err_printf(my_file, "Error: There are %i blue keys!\n", blue_count2);

	if (red_count2 > 1)
		err_printf(my_file, "Error: There are %i red keys!\n", red_count2);

	if (gold_count2 > 1)
		err_printf(my_file, "Error: There are %i gold keys!\n", gold_count2);
}

//	------------------------------------------------------------------------------------------
void write_control_center_text(FILE* my_file)
{
	int	i, count, objnum, count2;

	fprintf(my_file, "-----------------------------------------------------------------------------\n");
	fprintf(my_file, "Control Center stuff:\n");

	count = 0;
	for (i = 0; i <= Highest_segment_index; i++)
		if (Segment2s[i].special == SEGMENT_IS_CONTROLCEN) {
			count++;
			fprintf(my_file, "Segment %3i is a control center.\n", i);
			objnum = Segments[i].objects;
			count2 = 0;
			while (objnum != -1) {
				if (Objects[objnum].type == OBJ_CNTRLCEN)
					count2++;
				objnum = Objects[objnum].next;
			}
			if (count2 == 0)
				fprintf(my_file, "No control center object in control center segment.\n");
			else if (count2 != 1)
				fprintf(my_file, "%i control center objects in control center segment.\n", count2);
		}

	if (count == 0)
		err_printf(my_file, "Error: No control center in this mine.\n");
	else if (count != 1)
		err_printf(my_file, "Error: More than one control center in this mine.\n");
}

//	------------------------------------------------------------------------------------------
void write_fuelcen_text(FILE* my_file)
{
	int	i;

	fprintf(my_file, "-----------------------------------------------------------------------------\n");
	fprintf(my_file, "Fuel Center stuff: (Note: This means fuel, repair, materialize, control centers!)\n");

	for (i = 0; i < Num_fuelcenters; i++) {
		fprintf(my_file, "Fuelcenter %i: Type=%i (%s), segment = %3i\n", i, Station[i].Type, Special_names[Station[i].Type], Station[i].segnum);
		if (Segment2s[Station[i].segnum].special != Station[i].Type)
			err_printf(my_file, "Error: Conflicting data: Segment %i has special type %i (%s), expected to be %i\n", Station[i].segnum, Segment2s[Station[i].segnum].special, Special_names[Segment2s[Station[i].segnum].special], Station[i].Type);
	}
}

//	------------------------------------------------------------------------------------------
void write_segment_text(FILE* my_file)
{
	int	i, objnum;

	fprintf(my_file, "-----------------------------------------------------------------------------\n");
	fprintf(my_file, "Segment stuff:\n");

	for (i = 0; i <= Highest_segment_index; i++) {

		fprintf(my_file, "Segment %4i: ", i);
		if (Segment2s[i].special != 0)
			fprintf(my_file, "special = %3i (%s), value = %3i ", Segment2s[i].special, Special_names[Segment2s[i].special], Segment2s[i].value);

		if (Segment2s[i].matcen_num != -1)
			fprintf(my_file, "matcen = %3i, ", Segment2s[i].matcen_num);

		fprintf(my_file, "\n");
	}

	for (i = 0; i <= Highest_segment_index; i++) {
		int	depth;

		objnum = Segments[i].objects;
		fprintf(my_file, "Segment %4i: ", i);
		depth = 0;
		if (objnum != -1) {
			fprintf(my_file, "Objects: ");
			while (objnum != -1) {
				fprintf(my_file, "[%8s %8s %3i] ", object_types(objnum), object_ids(objnum), objnum);
				objnum = Objects[objnum].next;
				if (depth++ > 30) {
					fprintf(my_file, "\nAborted after %i links\n", depth);
					break;
				}
			}
		}
		fprintf(my_file, "\n");
	}
}

//	------------------------------------------------------------------------------------------
//	This routine is bogus.  It assumes that all centers are matcens, which is not true.  The setting of segnum is bogus.
void write_matcen_text(FILE* my_file)
{
	int	i, j, k;

	fprintf(my_file, "-----------------------------------------------------------------------------\n");
	fprintf(my_file, "Materialization centers:\n");
	for (i = 0; i < Num_robot_centers; i++) {
		int	trigger_count = 0, segnum, fuelcen_num;

		fprintf(my_file, "FuelCenter[%02i].Segment = %04i  ", i, Station[i].segnum);
		fprintf(my_file, "Segment2[%04i].matcen_num = %02i  ", Station[i].segnum, Segment2s[Station[i].segnum].matcen_num);

		fuelcen_num = RobotCenters[i].fuelcen_num;
		if (Station[fuelcen_num].Type != SEGMENT_IS_ROBOTMAKER)
			err_printf(my_file, "Error: Matcen %i corresponds to Station %i, which has type %i (%s).\n", i, fuelcen_num, Station[fuelcen_num].Type, Special_names[Station[fuelcen_num].Type]);

		segnum = Station[fuelcen_num].segnum;

		//	Find trigger for this materialization center.
		for (j = 0; j < Num_triggers; j++) {
			if (Triggers[j].type == TT_MATCEN) {
				for (k = 0; k < Triggers[j].num_links; k++)
					if (Triggers[j].seg[k] == segnum) {
						fprintf(my_file, "Trigger = %2i  ", j);
						trigger_count++;
					}
			}
		}
		fprintf(my_file, "\n");

		if (trigger_count == 0)
			err_printf(my_file, "Error: Matcen %i in segment %i has no trigger!\n", i, segnum);

	}
}

//	------------------------------------------------------------------------------------------
void write_wall_text(FILE* my_file)
{
	int	i, j;
	int8_t wall_flags[MAX_WALLS];

	fprintf(my_file, "-----------------------------------------------------------------------------\n");
	fprintf(my_file, "Walls:\n");
	for (i = 0; i < Num_walls; i++) {
		int	segnum, sidenum;

		fprintf(my_file, "Wall %03i: seg=%3i, side=%2i, linked_wall=%3i, type=%s, flags=%4x, hps=%3i, trigger=%2i, clip_num=%2i, keys=%2i, state=%i\n", i,
			Walls[i].segnum, Walls[i].sidenum, Walls[i].linked_wall, Wall_names[Walls[i].type], Walls[i].flags, Walls[i].hps >> 16, Walls[i].trigger, Walls[i].clip_num, Walls[i].keys, Walls[i].state);

		if (Walls[i].trigger >= Num_triggers)
			fprintf(my_file, "Wall %03d points to invalid trigger %d\n", i, Walls[i].trigger);

		segnum = Walls[i].segnum;
		sidenum = Walls[i].sidenum;

		if (Segments[segnum].sides[sidenum].wall_num != i)
			err_printf(my_file, "Error: Wall %i points at segment %i, side %i, but that segment doesn't point back (it's wall_num = %i)\n", i, segnum, sidenum, Segments[segnum].sides[sidenum].wall_num);
	}

	for (i = 0; i < MAX_WALLS; i++)
		wall_flags[i] = 0;

	for (i = 0; i <= Highest_segment_index; i++) {
		segment* segp = &Segments[i];
		for (j = 0; j < MAX_SIDES_PER_SEGMENT; j++) {
			side* sidep = &segp->sides[j];
			if (sidep->wall_num != -1)
				if (wall_flags[sidep->wall_num])
					err_printf(my_file, "Error: Wall %i appears in two or more segments, including segment %i, side %i.\n", sidep->wall_num, i, j);
				else
					wall_flags[sidep->wall_num] = 1;
		}
	}

}

//typedef struct trigger {
//	byte		type;
//	short		flags;
//	fix		value;
//	fix		time;
//	byte		link_num;
//	short 	num_links;
//	short 	seg[MAX_WALLS_PER_LINK];
//	short		side[MAX_WALLS_PER_LINK];
//	} trigger;

//	------------------------------------------------------------------------------------------
void write_player_text(FILE* my_file)
{
	int	i, num_players = 0;

	fprintf(my_file, "-----------------------------------------------------------------------------\n");
	fprintf(my_file, "Players:\n");
	for (i = 0; i <= Highest_object_index; i++) {
		if (Objects[i].type == OBJ_PLAYER) {
			num_players++;
			fprintf(my_file, "Player %2i is object #%3i in segment #%3i.\n", Objects[i].id, i, Objects[i].segnum);
		}
	}

	if (num_players != MAX_PLAYERS)
		err_printf(my_file, "Error: %i player objects.  %i are required.\n", num_players, MAX_PLAYERS);
	if (num_players > MAX_MULTI_PLAYERS)
		err_printf(my_file, "Error: %i player objects.  %i are required.\n", num_players, MAX_PLAYERS);
}

//	------------------------------------------------------------------------------------------
void write_trigger_text(FILE* my_file)
{
	int	i, j, w;

	fprintf(my_file, "-----------------------------------------------------------------------------\n");
	fprintf(my_file, "Triggers:\n");
	for (i = 0; i < Num_triggers; i++) {
		fprintf(my_file, "Trigger %03i: type=%02x flags=%04x, value=%08x, time=%8x, num_links=%i ", i,
			Triggers[i].type, Triggers[i].flags, Triggers[i].value, Triggers[i].time, Triggers[i].num_links);

		for (j = 0; j < Triggers[i].num_links; j++)
			fprintf(my_file, "[%03i:%i] ", Triggers[i].seg[j], Triggers[i].side[j]);

		//	Find which wall this trigger is connected to.
		for (w = 0; w < Num_walls; w++)
			if (Walls[w].trigger == i)
				break;

		if (w == Num_walls)
			err_printf(my_file, "\nError: Trigger %i is not connected to any wall, so it can never be triggered.\n", i);
		else
			fprintf(my_file, "Attached to seg:side = %i:%i, wall %i\n", Walls[w].segnum, Walls[w].sidenum, Segments[Walls[w].segnum].sides[Walls[w].sidenum].wall_num);

	}

}

//	------------------------------------------------------------------------------------------
void write_game_text_file(char* filename)
{
	char	my_filename[128];
	int	namelen;
	FILE* my_file;

	Errors_in_mine = 0;

	// mprintf((0, "Writing text file for mine [%s]\n", filename));

	namelen = strlen(filename);

	Assert(namelen > 4);

	Assert(filename[namelen - 4] == '.');

	strcpy(my_filename, filename);
	strcpy(&my_filename[namelen - 4], ".txm");

	// mprintf((0, "Writing text file [%s]\n", my_filename));

	my_file = fopen(my_filename, "wt");
	// -- mprintf((1, "Fileno = %i\n", fileno(my_file)));

	if (!my_file) {
		char  ErrorMessage[200];

		snprintf(ErrorMessage, 200, "ERROR: Unable to open %s\nErrno = %i", my_file, errno);
		stop_time();
		gr_palette_load(gr_palette);
		nm_messagebox(NULL, 1, "Ok", ErrorMessage);
		start_time();

		return;
	}

	dump_used_textures_level(my_file, 0);
	say_totals(my_file, Gamesave_current_filename);

	fprintf(my_file, "\nNumber of segments:   %4i\n", Highest_segment_index + 1);
	fprintf(my_file, "Number of objects:    %4i\n", Highest_object_index + 1);
	fprintf(my_file, "Number of walls:      %4i\n", Num_walls);
	fprintf(my_file, "Number of open doors: %4i\n", Num_open_doors);
	fprintf(my_file, "Number of triggers:   %4i\n", Num_triggers);
	fprintf(my_file, "Number of matcens:    %4i\n", Num_robot_centers);
	fprintf(my_file, "\n");

	write_segment_text(my_file);

	write_fuelcen_text(my_file);

	write_matcen_text(my_file);

	write_player_text(my_file);

	write_wall_text(my_file);

	write_trigger_text(my_file);

	write_exit_text(my_file);

	//	---------- Find control center segment ----------
	write_control_center_text(my_file);

	//	---------- Show keyed walls ----------
	write_key_text(my_file);

	{ int r;
	r = fclose(my_file);
	mprintf((1, "Close value = %i\n", r));
	if (r)
		Int3();
	}
}

// -- //	---------------
// -- //	Note: This only works for a loaded level because the objects array must be valid.
// -- void determine_used_textures_robots(int *tmap_buf)
// -- {
// -- 	int	i, objnum;
// -- 	polymodel	*po;
// -- 
// -- 	Assert(N_polygon_models);
// -- 
// -- 	for (objnum=0; objnum <= Highest_object_index; objnum++) {
// -- 		int	model_num;
// -- 
// -- 		if (Objects[objnum].render_type == RT_POLYOBJ) {
// -- 			model_num = Objects[objnum].rtype.pobj_info.model_num;
// -- 
// -- 			po=&Polygon_models[model_num];
// -- 
// -- 			for (i=0;i<po->n_textures;i++)	{
// -- 				int	tli;
// -- 
// -- 				tli = ObjBitmaps[ObjBitmapPtrs[po->first_texture+i]];
// -- 				Assert((tli>=0) && (tli<= Num_tmaps));
// -- 				tmap_buf[tli]++;
// -- 			}
// -- 		}
// -- 	}
// -- 
// -- }

// --05/17/95--//	-----------------------------------------------------------------------------
// --05/17/95--void determine_used_textures_level(int load_level_flag, int shareware_flag, int level_num, int *tmap_buf, int *wall_buf, byte *level_tmap_buf, int max_tmap)
// --05/17/95--{
// --05/17/95--	int	segnum, sidenum;
// --05/17/95--	int	i, j;
// --05/17/95--
// --05/17/95--	for (i=0; i<max_tmap; i++)
// --05/17/95--		tmap_buf[i] = 0;
// --05/17/95--
// --05/17/95--	if (load_level_flag) {
// --05/17/95--		if (shareware_flag)
// --05/17/95--			load_level(Shareware_level_names[level_num]);
// --05/17/95--		else
// --05/17/95--			load_level(Registered_level_names[level_num]);
// --05/17/95--	}
// --05/17/95--
// --05/17/95--	for (segnum=0; segnum<=Highest_segment_index; segnum++) {
// --05/17/95--		segment	*segp = &Segments[segnum];
// --05/17/95--
// --05/17/95--		for (sidenum=0; sidenum<MAX_SIDES_PER_SEGMENT; sidenum++) {
// --05/17/95--			side	*sidep = &segp->sides[sidenum];
// --05/17/95--
// --05/17/95--			if (sidep->wall_num != -1) {
// --05/17/95--				int clip_num = Walls[sidep->wall_num].clip_num;
// --05/17/95--				if (clip_num != -1) {
// --05/17/95--
// --05/17/95--					int num_frames = WallAnims[clip_num].num_frames;
// --05/17/95--
// --05/17/95--					wall_buf[clip_num] = 1;
// --05/17/95--
// --05/17/95--					for (j=0; j<num_frames; j++) {
// --05/17/95--						int	tmap_num;
// --05/17/95--
// --05/17/95--						tmap_num = WallAnims[clip_num].frames[j];
// --05/17/95--						tmap_buf[tmap_num]++;
// --05/17/95--						if (level_tmap_buf[tmap_num] == -1)
// --05/17/95--							level_tmap_buf[tmap_num] = level_num + (!shareware_flag) * NUM_SHAREWARE_LEVELS;
// --05/17/95--					}
// --05/17/95--				}
// --05/17/95--			}
// --05/17/95--
// --05/17/95--			if (sidep->tmap_num >= 0)
// --05/17/95--				if (sidep->tmap_num < max_tmap) {
// --05/17/95--					tmap_buf[sidep->tmap_num]++;
// --05/17/95--					if (level_tmap_buf[sidep->tmap_num] == -1)
// --05/17/95--						level_tmap_buf[sidep->tmap_num] = level_num + (!shareware_flag) * NUM_SHAREWARE_LEVELS;
// --05/17/95--				} else
// --05/17/95--					Int3();	//	Error, bogus texture map.  Should not be greater than max_tmap.
// --05/17/95--
// --05/17/95--			if ((sidep->tmap_num2 & 0x3fff) != 0)
// --05/17/95--				if ((sidep->tmap_num2 & 0x3fff) < max_tmap) {
// --05/17/95--					tmap_buf[sidep->tmap_num2 & 0x3fff]++;
// --05/17/95--					if (level_tmap_buf[sidep->tmap_num2 & 0x3fff] == -1)
// --05/17/95--						level_tmap_buf[sidep->tmap_num2 & 0x3fff] = level_num + (!shareware_flag) * NUM_SHAREWARE_LEVELS;
// --05/17/95--				} else
// --05/17/95--					Int3();	//	Error, bogus texture map.  Should not be greater than max_tmap.
// --05/17/95--		}
// --05/17/95--	}
// --05/17/95--}

//	Adam: Change NUM_ADAM_LEVELS to the number of levels.
#define	NUM_ADAM_LEVELS	30

//	Adam: Stick the names here.
char* Adam_level_names[NUM_ADAM_LEVELS] = {
	"D2LEVA-1.LVL",
	"D2LEVA-2.LVL",
	"D2LEVA-3.LVL",
	"D2LEVA-4.LVL",
	"D2LEVA-S.LVL",

	"D2LEVB-1.LVL",
	"D2LEVB-2.LVL",
	"D2LEVB-3.LVL",
	"D2LEVB-4.LVL",
	"D2LEVB-S.LVL",

	"D2LEVC-1.LVL",
	"D2LEVC-2.LVL",
	"D2LEVC-3.LVL",
	"D2LEVC-4.LVL",
	"D2LEVC-S.LVL",

	"D2LEVD-1.LVL",
	"D2LEVD-2.LVL",
	"D2LEVD-3.LVL",
	"D2LEVD-4.LVL",
	"D2LEVD-S.LVL",

	"D2LEVE-1.LVL",
	"D2LEVE-2.LVL",
	"D2LEVE-3.LVL",
	"D2LEVE-4.LVL",
	"D2LEVE-S.LVL",

	"D2LEVF-1.LVL",
	"D2LEVF-2.LVL",
	"D2LEVF-3.LVL",
	"D2LEVF-4.LVL",
	"D2LEVF-S.LVL",
};

typedef struct BitmapFile {
	char			name[15];
} BitmapFile;

extern BitmapFile AllBitmaps[MAX_BITMAP_FILES];

int	Ignore_tmap_num2_error;

//	-----------------------------------------------------------------------------
void determine_used_textures_level(int load_level_flag, int shareware_flag, int level_num, int* tmap_buf, int* wall_buf, int8_t* level_tmap_buf, int max_tmap)
{
	int	segnum, sidenum, objnum = max_tmap;
	int	i, j;

	Assert(shareware_flag != -17);

	for (i = 0; i < MAX_BITMAP_FILES; i++)
		tmap_buf[i] = 0;

	if (load_level_flag) 
	{
		load_level(Adam_level_names[level_num]);
	}


	//	Process robots.
	for (objnum = 0; objnum <= Highest_object_index; objnum++) 
	{
		object* objp = &Objects[objnum];

		if (objp->render_type == RT_POLYOBJ) 
		{
			polymodel* po = &Polygon_models[objp->rtype.pobj_info.model_num];

			for (i = 0; i < po->n_textures; i++) 
			{

				int	tli = ObjBitmaps[ObjBitmapPtrs[po->first_texture + i]].index;
				// -- mprintf((0, "%s  ", AllBitmaps[ObjBitmaps[ObjBitmapPtrs[po->first_texture+i]].index].name));

				if ((tli < MAX_BITMAP_FILES) && (tli >= 0)) 
				{
					tmap_buf[tli]++;
					if (level_tmap_buf[tli] == -1)
						level_tmap_buf[tli] = level_num;
				}
				else
					Int3();	//	Hmm, It seems this texture is bogus!
			}

		}
	}


	Ignore_tmap_num2_error = 0;

	//	Process walls and segment sides.
	for (segnum = 0; segnum <= Highest_segment_index; segnum++) {
		segment* segp = &Segments[segnum];

		for (sidenum = 0; sidenum < MAX_SIDES_PER_SEGMENT; sidenum++) {
			side* sidep = &segp->sides[sidenum];

			if (sidep->wall_num != -1) {
				int clip_num = Walls[sidep->wall_num].clip_num;
				if (clip_num != -1) {

					// -- int num_frames = WallAnims[clip_num].num_frames;

					wall_buf[clip_num] = 1;

					for (j = 0; j < 1; j++) {	//	Used to do through num_frames, but don't really want all the door01#3 stuff.
						int	tmap_num;

						tmap_num = Textures[WallAnims[clip_num].frames[j]].index;
						Assert((tmap_num >= 0) && (tmap_num < MAX_BITMAP_FILES));
						tmap_buf[tmap_num]++;
						if (level_tmap_buf[tmap_num] == -1)
							level_tmap_buf[tmap_num] = level_num;
					}
				}
			}
			else if (segp->children[sidenum] == -1) {

				if (sidep->tmap_num >= 0)
					if (sidep->tmap_num < MAX_BITMAP_FILES) {
						Assert(Textures[sidep->tmap_num].index < MAX_BITMAP_FILES);
						tmap_buf[Textures[sidep->tmap_num].index]++;
						if (level_tmap_buf[Textures[sidep->tmap_num].index] == -1)
							level_tmap_buf[Textures[sidep->tmap_num].index] = level_num;
					}
					else
						Int3();	//	Error, bogus texture map.  Should not be greater than max_tmap.

				if ((sidep->tmap_num2 & 0x3fff) != 0)
					if ((sidep->tmap_num2 & 0x3fff) < MAX_BITMAP_FILES) {
						Assert(Textures[sidep->tmap_num2 & 0x3fff].index < MAX_BITMAP_FILES);
						tmap_buf[Textures[sidep->tmap_num2 & 0x3fff].index]++;
						if (level_tmap_buf[Textures[sidep->tmap_num2 & 0x3fff].index] == -1)
							level_tmap_buf[Textures[sidep->tmap_num2 & 0x3fff].index] = level_num;
					}
					else {
						if (!Ignore_tmap_num2_error)
							Int3();	//	Error, bogus texture map.  Should not be greater than max_tmap.
						Ignore_tmap_num2_error = 1;
						sidep->tmap_num2 = 0;
					}
			}
		}
	}
}

//	-----------------------------------------------------------------------------
void merge_buffers(int* dest, int* src, int num)
{
	int	i;

	for (i = 0; i < num; i++)
		if (src[i])
			dest[i] += src[i];
}

//	-----------------------------------------------------------------------------
void say_used_tmaps(FILE* my_file, int* tb)
{
	int	i;
	// -- mk, 08/14/95 -- 	int	count = 0;

	for (i = 0; i < MAX_BITMAP_FILES; i++)
		if (tb[i]) {
			fprintf(my_file, "[%3i %8s (%4i)]\n", i, AllBitmaps[i].name, tb[i]);
			// -- mk, 08/14/95 -- 			if (count++ >= 4) {
			// -- mk, 08/14/95 -- 				fprintf(my_file, "\n");
			// -- mk, 08/14/95 -- 				count = 0;
			// -- mk, 08/14/95 -- 			}
		}
}

// --05/17/95--//	-----------------------------------------------------------------------------
// --05/17/95--void say_used_once_tmaps(FILE *my_file, int *tb, byte *tb_lnum)
// --05/17/95--{
// --05/17/95--	int	i;
// --05/17/95--	char	*level_name;
// --05/17/95--
// --05/17/95--	for (i=0; i<Num_tmaps; i++)
// --05/17/95--		if (tb[i] == 1) {
// --05/17/95--			int	level_num = tb_lnum[i];
// --05/17/95--			if (level_num >= NUM_SHAREWARE_LEVELS) {
// --05/17/95--				Assert((level_num - NUM_SHAREWARE_LEVELS >= 0) && (level_num - NUM_SHAREWARE_LEVELS < NUM_REGISTERED_LEVELS));
// --05/17/95--				level_name = Registered_level_names[level_num - NUM_SHAREWARE_LEVELS];
// --05/17/95--			} else {
// --05/17/95--				Assert((level_num >= 0) && (level_num < NUM_SHAREWARE_LEVELS));
// --05/17/95--				level_name = Shareware_level_names[level_num];
// --05/17/95--			}
// --05/17/95--
// --05/17/95--			fprintf(my_file, "Texture %3i %8s used only once on level %s\n", i, activeBMTable->tmaps[i].filename, level_name);
// --05/17/95--		}
// --05/17/95--}

//	-----------------------------------------------------------------------------
void say_used_once_tmaps(FILE* my_file, int* tb, int8_t* tb_lnum)
{
	int	i;
	char* level_name;

	for (i = 0; i < MAX_BITMAP_FILES; i++)
		if (tb[i] == 1) {
			int	level_num = tb_lnum[i];
			level_name = Adam_level_names[level_num];

			fprintf(my_file, "Texture %3i %8s used only once on level %s\n", i, AllBitmaps[i].name, level_name);
		}
}

//	-----------------------------------------------------------------------------
void say_unused_tmaps(FILE* my_file, int* tb)
{
	int	i;
	int	count = 0;

	for (i = 0; i < MAX_BITMAP_FILES; i++)
		if (!tb[i]) {
			if (GameBitmaps[Textures[i].index].bm_data == bogus_data)
				fprintf(my_file, "U");
			else
				fprintf(my_file, " ");

			fprintf(my_file, "[%3i %8s] ", i, AllBitmaps[i].name);
			if (count++ >= 4) {
				fprintf(my_file, "\n");
				count = 0;
			}
		}
}

//	-----------------------------------------------------------------------------
void say_unused_walls(FILE* my_file, int* tb)
{
	int	i;

	for (i = 0; i < Num_wall_anims; i++)
		if (!tb[i])
			fprintf(my_file, "Wall %3i is unused.\n", i);
}

//	-------------------------------------------------------------------------------------------------
void say_totals(FILE* my_file, char* level_name)
{
	int	i;		//, objnum;
	int	total_robots = 0;
	int	objects_processed = 0;

	int	used_objects[MAX_OBJECTS];

	fprintf(my_file, "\nLevel %s\n", level_name);

	for (i = 0; i < MAX_OBJECTS; i++)
		used_objects[i] = 0;

	while (objects_processed < Highest_object_index + 1) {
		int	j, objtype, objid, objcount, cur_obj_val, min_obj_val, min_objnum;

		//	Find new min objnum.
		min_obj_val = 0x7fff0000;
		min_objnum = -1;

		for (j = 0; j <= Highest_object_index; j++) {
			if (!used_objects[j] && Objects[j].type != OBJ_NONE) {
				cur_obj_val = Objects[j].type * 1000 + Objects[j].id;
				if (cur_obj_val < min_obj_val) {
					min_objnum = j;
					min_obj_val = cur_obj_val;
				}
			}
		}
		if ((min_objnum == -1) || (Objects[min_objnum].type == 255))
			break;

		objcount = 0;

		objtype = Objects[min_objnum].type;
		objid = Objects[min_objnum].id;

		for (i = 0; i <= Highest_object_index; i++) {
			if (!used_objects[i]) {

				if (((Objects[i].type == objtype) && (Objects[i].id == objid)) ||
					((Objects[i].type == objtype) && (objtype == OBJ_PLAYER)) ||
					((Objects[i].type == objtype) && (objtype == OBJ_COOP)) ||
					((Objects[i].type == objtype) && (objtype == OBJ_HOSTAGE))) {
					if (Objects[i].type == OBJ_ROBOT)
						total_robots++;
					used_objects[i] = 1;
					objcount++;
					objects_processed++;
				}
			}
		}

		if (objcount) {
			fprintf(my_file, "Object: ");
			fprintf(my_file, "%8s %8s %3i\n", object_types(min_objnum), object_ids(min_objnum), objcount);
		}
	}

	fprintf(my_file, "Total robots = %3i\n", total_robots);
}

int	First_dump_level = 0;
int	Last_dump_level = NUM_ADAM_LEVELS - 1;

//	-----------------------------------------------------------------------------
void say_totals_all(void)
{
	int	i;
	FILE* my_file;

	my_file = fopen("levels.all", "wt");
	// -- mprintf((1, "Fileno = %i\n", fileno(my_file)));

	if (!my_file) {
		char  ErrorMessage[200];

		snprintf(ErrorMessage, 200, "ERROR: Unable to open levels.all\nErrno=%i", errno);
		stop_time();
		gr_palette_load(gr_palette);
		nm_messagebox(NULL, 1, "Ok", ErrorMessage);
		start_time();

		return;
	}

	for (i = First_dump_level; i <= Last_dump_level; i++) {
		mprintf((0, "Level %i\n", i + 1));
		load_level(Adam_level_names[i]);
		say_totals(my_file, Adam_level_names[i]);
	}

	//--05/17/95--	for (i=0; i<NUM_SHAREWARE_LEVELS; i++) {
	//--05/17/95--		mprintf((0, "Level %i\n", i+1));
	//--05/17/95--		load_level(Shareware_level_names[i]);
	//--05/17/95--		say_totals(my_file, Shareware_level_names[i]);
	//--05/17/95--	}
	//--05/17/95--
	//--05/17/95--	for (i=0; i<NUM_REGISTERED_LEVELS; i++) {
	//--05/17/95--		mprintf((0, "Level %i\n", i+1+NUM_SHAREWARE_LEVELS));
	//--05/17/95--		load_level(Registered_level_names[i]);
	//--05/17/95--		say_totals(my_file, Registered_level_names[i]);
	//--05/17/95--	}

	fclose(my_file);

}

void dump_used_textures_level(FILE* my_file, int level_num)
{
	int	i;
	int	temp_tmap_buf[MAX_BITMAP_FILES];
	int	perm_tmap_buf[MAX_BITMAP_FILES];
	int8_t level_tmap_buf[MAX_BITMAP_FILES];
	int	temp_wall_buf[MAX_BITMAP_FILES];
	int	perm_wall_buf[MAX_BITMAP_FILES];

	for (i = 0; i < MAX_BITMAP_FILES; i++) {
		perm_tmap_buf[i] = 0;
		level_tmap_buf[i] = -1;
	}

	for (i = 0; i < MAX_BITMAP_FILES; i++) {
		perm_wall_buf[i] = 0;
	}

	determine_used_textures_level(0, 1, level_num, temp_tmap_buf, temp_wall_buf, level_tmap_buf, MAX_BITMAP_FILES);
	// -- 	determine_used_textures_robots(temp_tmap_buf);
	fprintf(my_file, "\nTextures used in [%s]\n", Gamesave_current_filename);
	say_used_tmaps(my_file, temp_tmap_buf);
}

FILE* my_file;

//	-----------------------------------------------------------------------------
void dump_used_textures_all(void)
{
	int	i;
	int	temp_tmap_buf[MAX_BITMAP_FILES];
	int	perm_tmap_buf[MAX_BITMAP_FILES];
	int8_t level_tmap_buf[MAX_BITMAP_FILES];
	int	temp_wall_buf[MAX_BITMAP_FILES];
	int	perm_wall_buf[MAX_BITMAP_FILES];

	say_totals_all();

	my_file = fopen("textures.dmp", "wt");
	// -- mprintf((1, "Fileno = %i\n", fileno(my_file)));

	if (!my_file) {
		char  ErrorMessage[200];

		snprintf(ErrorMessage, 200, "ERROR: Can't open textures.dmp\nErrno=%i", errno);
		stop_time();
		gr_palette_load(gr_palette);
		nm_messagebox(NULL, 1, "Ok", ErrorMessage);
		start_time();

		return;
	}

	for (i = 0; i < MAX_BITMAP_FILES; i++) {
		perm_tmap_buf[i] = 0;
		level_tmap_buf[i] = -1;
	}

	for (i = 0; i < MAX_BITMAP_FILES; i++) {
		perm_wall_buf[i] = 0;
	}

	// --05/17/95--	for (i=0; i<NUM_SHAREWARE_LEVELS; i++) {
	// --05/17/95--		determine_used_textures_level(1, 1, i, temp_tmap_buf, temp_wall_buf, level_tmap_buf, MAX_BITMAP_FILES);
	// --05/17/95--		fprintf(my_file, "\nTextures used in [%s]\n", Shareware_level_names[i]);
	// --05/17/95--		say_used_tmaps(my_file, temp_tmap_buf);
	// --05/17/95--		merge_buffers(perm_tmap_buf, temp_tmap_buf, MAX_BITMAP_FILES);
	// --05/17/95--		merge_buffers(perm_wall_buf, temp_wall_buf, MAX_BITMAP_FILES);
	// --05/17/95--	}
	// --05/17/95--
	// --05/17/95--	fprintf(my_file, "\n\nUsed textures in all shareware mines:\n");
	// --05/17/95--	say_used_tmaps(my_file, perm_tmap_buf);
	// --05/17/95--
	// --05/17/95--	fprintf(my_file, "\nUnused textures in all shareware mines:\n");
	// --05/17/95--	say_unused_tmaps(my_file, perm_tmap_buf);
	// --05/17/95--
	// --05/17/95--	fprintf(my_file, "\nTextures used exactly once in all shareware mines:\n");
	// --05/17/95--	say_used_once_tmaps(my_file, perm_tmap_buf, level_tmap_buf);
	// --05/17/95--
	// --05/17/95--	fprintf(my_file, "\nWall anims (eg, doors) unused in all shareware mines:\n");
	// --05/17/95--	say_unused_walls(my_file, perm_wall_buf);

	// --05/17/95--	for (i=0; i<NUM_REGISTERED_LEVELS; i++) {
	// --05/17/95--		determine_used_textures_level(1, 0, i, temp_tmap_buf, temp_wall_buf, level_tmap_buf, MAX_BITMAP_FILES);
	// --05/17/95--		fprintf(my_file, "\nTextures used in [%s]\n", Registered_level_names[i]);
	// --05/17/95--		say_used_tmaps(my_file, temp_tmap_buf);
	// --05/17/95--		merge_buffers(perm_tmap_buf, temp_tmap_buf, MAX_BITMAP_FILES);
	// --05/17/95--	}
	// --05/17/95--
	// --05/17/95--	fprintf(my_file, "\n\nUsed textures in all (including registered) mines:\n");
	// --05/17/95--	say_used_tmaps(my_file, perm_tmap_buf);
	// --05/17/95--
	// --05/17/95--	fprintf(my_file, "\nUnused textures in all (including registered) mines:\n");
	// --05/17/95--	say_unused_tmaps(my_file, perm_tmap_buf);

	for (i = First_dump_level; i <= Last_dump_level; i++) {
		determine_used_textures_level(1, 0, i, temp_tmap_buf, temp_wall_buf, level_tmap_buf, MAX_BITMAP_FILES);
		fprintf(my_file, "\nTextures used in [%s]\n", Adam_level_names[i]);
		say_used_tmaps(my_file, temp_tmap_buf);
		merge_buffers(perm_tmap_buf, temp_tmap_buf, MAX_BITMAP_FILES);
	}

	fprintf(my_file, "\n\nUsed textures in all (including registered) mines:\n");
	say_used_tmaps(my_file, perm_tmap_buf);

	fprintf(my_file, "\nUnused textures in all (including registered) mines:\n");
	say_unused_tmaps(my_file, perm_tmap_buf);

	fclose(my_file);
}

#endif
