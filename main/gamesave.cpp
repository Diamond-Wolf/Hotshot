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
#include <math.h>
#include <string.h>

#include <array>
#include <vector>

#include "platform/posixstub.h"
#include "platform/mono.h"
#include "platform/key.h"
#include "2d/gr.h"
#include "2d/palette.h"
#include "newmenu.h"

#include "inferno.h"
#ifdef EDITOR
#include "editor\editor.h"
#include "player.h"

extern void compress_uv_coordinates_all(void);
void do_load_save_levels(int save);
#endif
#include "misc/error.h"
#include "object.h"
#include "game.h"
#include "screens.h"
#include "wall.h"
#include "gamemine.h"
#include "robot.h"
#include "cfile/cfile.h"
#include "bm.h"
#include "menu.h"
#include "switch.h"
#include "fuelcen.h"
#include "cntrlcen.h"
#include "powerup.h"
#include "weapon.h"
#include "newdemo.h"
#include "gameseq.h"
#include "automap.h"
#include "polyobj.h"
#include "stringtable.h"
#include "gamefont.h"
#include "gamesave.h"
#include "gamepal.h"
#include "laser.h"
#include "misc/byteswap.h"
#include "newcheat.h"
#include "ai.h"

#define SANTA

char Gamesave_current_filename[128];

#define GAME_VERSION					32
#define GAME_COMPATIBLE_VERSION	22

//version 28->29	add delta light support
//version 27->28  controlcen id now is reactor number, not model number
//version 28->29  ??
//version 29->30	changed trigger structure
//version 30->31	changed trigger structure some more
//version 31->32	change segment structure, make it 512 bytes w/o editor, add Segment2s array.

#define MENU_CURSOR_X_MIN			MENU_X
#define MENU_CURSOR_X_MAX			MENU_X+6

//Start old wall structures

typedef struct v16_wall {
	int8_t  type; 			  	// What kind of special wall.
	int8_t	flags;				// Flags for the wall.		
	fix   hps;				  	// "Hit points" of the wall. 
	int8_t	trigger;				// Which trigger is associated with the wall.
	int8_t	clip_num;			// Which	animation associated with the wall. 
	int8_t	keys;
} v16_wall;

typedef struct v19_wall {
	int	segnum, sidenum;	// Seg & side for this wall
	int8_t	type; 			  	// What kind of special wall.
	int8_t	flags;				// Flags for the wall.		
	fix   hps;				  	// "Hit points" of the wall. 
	int8_t	trigger;				// Which trigger is associated with the wall.
	int8_t	clip_num;			// Which	animation associated with the wall. 
	int8_t	keys;
	int	linked_wall;		// number of linked wall
} v19_wall;

typedef struct v19_door {
	int		n_parts;					// for linked walls
	short 	seg[2]; 					// Segment pointer of door.
	short 	side[2];					// Side number of door.
	short 	type[2];					// What kind of door animation.
	fix 		open;						//	How long it has been open.
} v19_door;

//End old wall structures

//old trigger structs

typedef struct v29_trigger {
	int8_t		type;
	short		flags;
	fix		value;
	fix		time;
	int8_t		link_num;
	short 	num_links;
	short 	seg[MAX_WALLS_PER_LINK];
	short		side[MAX_WALLS_PER_LINK];
} v29_trigger;

typedef struct v30_trigger {
	short		flags;
	int8_t	 	num_links;
	int8_t		pad;			//keep alignment
	fix		value;
	fix		time;
	short 	seg[MAX_WALLS_PER_LINK];
	short		side[MAX_WALLS_PER_LINK];
} v30_trigger;

//flags for V30 & below triggers
#define	TRIGGER_CONTROL_DOORS		1	// Control Trigger
#define	TRIGGER_SHIELD_DAMAGE		2	// Shield Damage Trigger
#define	TRIGGER_ENERGY_DRAIN	   	4	// Energy Drain Trigger
#define	TRIGGER_EXIT					8	// End of level Trigger
#define	TRIGGER_ON					  16	// Whether Trigger is active
#define	TRIGGER_ONE_SHOT			  32	// If Trigger can only be triggered once
#define	TRIGGER_MATCEN				  64	// Trigger for materialization centers
#define	TRIGGER_ILLUSION_OFF		 128	// Switch Illusion OFF trigger
#define	TRIGGER_SECRET_EXIT		 256	// Exit to secret level
#define	TRIGGER_ILLUSION_ON		 512	// Switch Illusion ON trigger
#define	TRIGGER_UNLOCK_DOORS		1024	// Unlocks a door
#define	TRIGGER_OPEN_WALL			2048	// Makes a wall open
#define	TRIGGER_CLOSE_WALL		4096	// Makes a wall closed
#define	TRIGGER_ILLUSORY_WALL	8192	// Makes a wall illusory

constexpr std::array<uint32_t, 14> D2_TRIGGER_TABLE{
	HTT_OPEN_DOOR,
	HTT_CLOSE_DOOR,
	HTT_MATCEN,
	HTT_EXIT,
	HTT_SECRET_EXIT,
	HTT_ILLUSION_OFF,
	HTT_ILLUSION_ON,
	HTT_UNLOCK_DOOR,
	HTT_LOCK_DOOR,
	HTT_OPEN_WALL,
	HTT_CLOSE_WALL,
	HTT_ILLUSORY_WALL,
	HTT_LIGHT_OFF,
	HTT_LIGHT_ON,
};

uint16_t MakeD1HTriggerFlags(int flags) {
	return ((flags & TRIGGER_ONE_SHOT) ? TF_ONE_SHOT : 0);
}

uint32_t MakeD1HTrigger(int flags) {
	uint32_t htFlags = 0;

	if (flags & TRIGGER_CONTROL_DOORS)
		htFlags |= HTT_OPEN_DOOR;
	if (flags & TRIGGER_SHIELD_DAMAGE)
		htFlags |= HTT_DRAIN_SHIELDS;
	if (flags & TRIGGER_ENERGY_DRAIN)
		htFlags |= HTT_DRAIN_ENERGY;
	if (flags & TRIGGER_EXIT)
		htFlags |= HTT_EXIT;
	// no TRIGGER_ON
	if (flags & TRIGGER_MATCEN)
		htFlags |= HTT_MATCEN;
	if (flags & TRIGGER_ILLUSION_OFF)
		htFlags |= HTT_ILLUSION_OFF;
	if (flags & TRIGGER_SECRET_EXIT)
		htFlags |= HTT_SECRET_EXIT;
	if (flags & TRIGGER_ILLUSION_ON)
		htFlags |= HTT_ILLUSION_ON;
	if (flags & TRIGGER_UNLOCK_DOORS)
		htFlags |= HTT_UNLOCK_DOOR;
	if (flags & TRIGGER_OPEN_WALL)
		htFlags |= HTT_OPEN_WALL;
	if (flags & TRIGGER_CLOSE_WALL)
		htFlags |= HTT_CLOSE_WALL;
	if (flags & TRIGGER_ILLUSORY_WALL)
		htFlags |= HTT_ILLUSORY_WALL;

	return htFlags;
}

uint32_t MakeD2HTrigger(int type) {
	return D2_TRIGGER_TABLE[type];
}


struct {
	uint16_t 	fileinfo_signature;
	uint16_t	fileinfo_version;
	int		fileinfo_sizeof;
} game_top_fileinfo;    // Should be same as first two fields below...

struct {
	uint16_t 	fileinfo_signature;
	uint16_t	fileinfo_version;
	int		fileinfo_sizeof;
	char		mine_filename[15];
	int		level;
	int		player_offset;				// Player info
	int		player_sizeof;
	int		object_offset;				// Object info
	int		object_howmany;
	int		object_sizeof;
	int		walls_offset;
	int		walls_howmany;
	int		walls_sizeof;
	int		doors_offset;
	int		doors_howmany;
	int		doors_sizeof;
	int		triggers_offset;
	int		triggers_howmany;
	int		triggers_sizeof;
	int		links_offset;
	int		links_howmany;
	int		links_sizeof;
	int		control_offset;
	int		control_howmany;
	int		control_sizeof;
	int		matcen_offset;
	int		matcen_howmany;
	int		matcen_sizeof;
	int		dl_indices_offset;
	int		dl_indices_howmany;
	int		dl_indices_sizeof;
	int		delta_light_offset;
	int		delta_light_howmany;
	int		delta_light_sizeof;
} game_fileinfo;

//	LINT: adding function prototypes
void read_object(object* obj, CFILE* f, int version);
void write_object(object* obj, FILE* f);
void dump_mine_info(void);

#ifdef NETWORK
extern char MaxPowerupsAllowed[MAX_POWERUP_TYPES];
extern char PowerupsInMine[MAX_POWERUP_TYPES];
#endif

#ifdef EDITOR
extern char mine_filename[];
extern int save_mine_data_compiled(FILE* SaveFile);
//--unused-- #else
//--unused-- char mine_filename[128];
#endif

int Gamesave_num_org_robots = 0;
//--unused-- grs_bitmap * Gamesave_saved_bitmap = NULL;

#ifdef EDITOR
//	Return true if this level has a name of the form "level??"
//	Note that a pathspec can appear at the beginning of the filename.
int is_real_level(char* filename)
{
	int	len = strlen(filename);

	if (len < 6)
		return 0;

	//mprintf((0, "String = [%s]\n", &filename[len-11]));
	return !_strnfcmp(&filename[len - 11], "level", 5);
}
#endif

void change_filename_extension(char* dest, const char* src, const char* new_ext)
{
	int i;

	strcpy(dest, src);

	if (new_ext[0] == '.')
		new_ext++;

	for (i = 1; i < strlen(dest); i++)
		if (dest[i] == '.' || dest[i] == ' ' || dest[i] == 0)
			break;

	if (i < 123) {
		dest[i] = '.';
		dest[i + 1] = new_ext[0];
		dest[i + 2] = new_ext[1];
		dest[i + 3] = new_ext[2];
		dest[i + 4] = 0;
		return;
	}
}

//--unused-- vms_angvec zero_angles={0,0,0};

#define vm_angvec_zero(v) do {(v)->p=(v)->b=(v)->h=0;} while (0)

int Gamesave_num_players = 0;

int N_save_pof_names;
std::vector<std::array<char, FILENAME_LEN>> Save_pof_names;

void check_and_fix_matrix(vms_matrix* m);

extern int multi_powerup_is_4pack(int);

void verify_object(object* obj) 
{
	obj->lifeleft = IMMORTAL_TIME;		//all loaded object are immortal, for now

	if (obj->type == OBJ_ROBOT) 
	{
		Gamesave_num_org_robots++;

		// Make sure valid id...
		if (obj->id >= activeBMTable->robots.size())
			obj->id = obj->id % activeBMTable->robots.size();

		// Make sure model number & size are correct...		
		if (obj->render_type == RT_POLYOBJ) 
		{
			Assert(activeBMTable->robots[obj->id].model_num != -1);
			//if you fail this assert, it means that a robot in this level
			//hasn't been loaded, possibly because he's marked as
			//non-shareware.  To see what robot number, print obj->id.

			Assert(activeBMTable->robots[obj->id].always_0xabcd == 0xabcd);
			//if you fail this assert, it means that the robot_ai for
			//a robot in this level hasn't been loaded, possibly because 
			//it's marked as non-shareware.  To see what robot number, 
			//print obj->id.

			obj->rtype.pobj_info.model_num = activeBMTable->robots[obj->id].model_num;
			obj->size = activeBMTable->models[obj->rtype.pobj_info.model_num].rad;

			//@@Took out this ugly hack 1/12/96, because Mike has added code
			//@@that should fix it in a better way.
			//@@//this is a super-ugly hack.  Since the baby stripe robots have
			//@@//their firing point on their bounding sphere, the firing points
			//@@//can poke through a wall if the robots are very close to it. So
			//@@//we make their radii bigger so the guns can't get too close to 
			//@@//the walls
			//@@if (activeBMTable->robots[obj->id].flags & RIF_BIG_RADIUS)
			//@@	obj->size = (obj->size*3)/2;

			//@@if (obj->control_type==CT_AI && activeBMTable->robots[obj->id].attack_type)
			//@@	obj->size = obj->size*3/4;
		}

		if (obj->id == 65)						//special "reactor" robots
			obj->movement_type = MT_NONE;

		if (obj->movement_type == MT_PHYSICS) 
		{
			obj->mtype.phys_info.mass = activeBMTable->robots[obj->id].mass;
			obj->mtype.phys_info.drag = activeBMTable->robots[obj->id].drag;
		}
	}
	else {		//Robots taken care of above

		if (obj->render_type == RT_POLYOBJ && Pof_names.size() > 0)
		{
			int i;

			auto modelNum = obj->rtype.pobj_info.model_num;
			if (modelNum >= 0 && modelNum < Save_pof_names.size()) {
				char* name = Save_pof_names[modelNum].data();

				for (i = 0; i < Pof_names.size(); i++)
					if (!_stricmp(Pof_names[i].data(), name)) //found it!	
					{
						// mprintf((0,"Mapping <%s> to %d (was %d)\n",name,i,obj->rtype.pobj_info.model_num));
						obj->rtype.pobj_info.model_num = i;
						break;
					}
			}
		}
	}

	if (obj->type == OBJ_POWERUP)
	{
		if (obj->id >= activeBMTable->powerups.size()) 
		{
			obj->id = 0;
			Assert(obj->render_type != RT_POLYOBJ);
		}
		obj->control_type = CT_POWERUP;
		obj->size = activeBMTable->powerups[obj->id].size;
		obj->ctype.powerup_info.creation_time = 0;

		if (Game_mode & GM_NETWORK)
		{
#ifdef NETWORK
			if (multi_powerup_is_4pack(obj->id))
			{
				PowerupsInMine[obj->id - 1] += 4;
				MaxPowerupsAllowed[obj->id - 1] += 4;
			}
			PowerupsInMine[obj->id]++;
			MaxPowerupsAllowed[obj->id]++;
#endif
			mprintf((0, "PowerupLimiter: ID=%d\n", obj->id));
			if (obj->id > MAX_POWERUP_TYPES)
				mprintf((1, "POWERUP: Overwriting array bounds!! Get JL!\n"));
		}

	}

	if (obj->type == OBJ_WEAPON)
	{
		if (obj->id >= activeBMTable->weapons.size()) 
		{
			obj->id = 0;
			Assert(obj->render_type != RT_POLYOBJ);
		}

		if (obj->id == PMINE_ID) //make sure pmines have correct values
		{		
			obj->mtype.phys_info.mass = activeBMTable->weapons[obj->id].mass;
			obj->mtype.phys_info.drag = activeBMTable->weapons[obj->id].drag;
			obj->mtype.phys_info.flags |= PF_FREE_SPINNING;

			// Make sure model number & size are correct...		
			Assert(obj->render_type == RT_POLYOBJ);

			obj->rtype.pobj_info.model_num = activeBMTable->weapons[obj->id].model_num;
			obj->size = activeBMTable->models[obj->rtype.pobj_info.model_num].rad;
		}
	}

	if (obj->type == OBJ_CNTRLCEN) 
	{
		obj->render_type = RT_POLYOBJ;
		obj->control_type = CT_CNTRLCEN;

		if (currentGame == G_DESCENT_1) {
			obj->rtype.pobj_info.model_num = activeBMTable->reactors[0].model_num;
			obj->shields = -1;
		}

		//@@// Make model number is correct...	
		//@@for (i=0; i<Num_total_object_types; i++ )	
		//@@	if ( ObjType[i] == OL_CONTROL_CENTER )		{
		//@@		obj->rtype.pobj_info.model_num = ObjId[i];
		//@@		obj->shields = ObjStrength[i];
		//@@		break;		
		//@@	}

#ifdef EDITOR
		{
			int i;
			// Check, and set, strength of reactor
			for (i = 0; i < Num_total_object_types; i++)
				if (ObjType[i] == OL_CONTROL_CENTER && ObjId[i] == obj->id) 
				{
					obj->shields = ObjStrength[i];
					break;
				}
			Assert(i < Num_total_object_types);		//make sure we found it
		}
#endif
	}

	if (obj->type == OBJ_PLAYER) 
	{
		//int i;

		//Assert(obj == Player);

		if (obj == ConsoleObject)
			init_player_object();
		else
			if (obj->render_type == RT_POLYOBJ)	//recover from Matt's pof file matchup bug
				obj->rtype.pobj_info.model_num = Player_ship->model_num;

		//Make sure orient matrix is orthogonal
		check_and_fix_matrix(&obj->orient);

		obj->id = Gamesave_num_players++;
	}

	if (obj->type == OBJ_HOSTAGE) 
	{
		//@@if (obj->id > N_hostage_types)
		//@@	obj->id = 0;

		obj->render_type = RT_HOSTAGE;
		obj->control_type = CT_POWERUP;
	}

}

static int read_int(CFILE* file)
{
	int i = cfile_read_int(file);

	//if (cfread(&i, sizeof(i), 1, file) != 1)
	//	Error("Error reading int in gamesave.c");

	return i;
}

static fix read_fix(CFILE* file)
{
	fix f = cfile_read_int(file);

	//if (cfread(&f, sizeof(f), 1, file) != 1)
	//	Error("Error reading fix in gamesave.c");

	return f;
}

static short read_short(CFILE* file)
{
	short s = cfile_read_short(file);

	//if (cfread(&s, sizeof(s), 1, file) != 1)
	//	Error("Error reading short in gamesave.c");

	return s;
}

static short read_fixang(CFILE* file)
{
	fixang f = cfile_read_short(file);

	//if (cfread(&f, sizeof(f), 1, file) != 1)
	//	Error("Error reading fixang in gamesave.c");

	return f;
}

static int8_t read_byte(CFILE* file)
{
	int8_t b = cfile_read_byte(file);

	//if (cfread(&b, sizeof(b), 1, file) != 1)
	//	Error("Error reading int8_t in gamesave.c");

	return b;
}

static void read_vector(vms_vector* v, CFILE* file)
{
	v->x = read_fix(file);
	v->y = read_fix(file);
	v->z = read_fix(file);
}

static void read_matrix(vms_matrix* m, CFILE* file)
{
	read_vector(&m->rvec, file);
	read_vector(&m->uvec, file);
	read_vector(&m->fvec, file);
}

static void read_angvec(vms_angvec* v, CFILE* file)
{
	v->p = read_fixang(file);
	v->b = read_fixang(file);
	v->h = read_fixang(file);
}

//static gs_skip(int len,CFILE *file)
//{
//
//	cfseek(file,len,SEEK_CUR);
//}

#ifdef EDITOR
static void gs_write_int(int i, FILE* file)
{
	if (fwrite(&i, sizeof(i), 1, file) != 1)
		Error("Error reading int in gamesave.c");

}

static void gs_write_fix(fix f, FILE* file)
{
	if (fwrite(&f, sizeof(f), 1, file) != 1)
		Error("Error reading fix in gamesave.c");

}

static void gs_write_short(short s, FILE* file)
{
	if (fwrite(&s, sizeof(s), 1, file) != 1)
		Error("Error reading short in gamesave.c");

}

static void gs_write_fixang(fixang f, FILE* file)
{
	if (fwrite(&f, sizeof(f), 1, file) != 1)
		Error("Error reading fixang in gamesave.c");

}

static void gs_write_byte(int8_t b, FILE* file)
{
	if (fwrite(&b, sizeof(b), 1, file) != 1)
		Error("Error reading byte in gamesave.c");

}

static void gr_write_vector(vms_vector* v, FILE* file)
{
	gs_write_fix(v->x, file);
	gs_write_fix(v->y, file);
	gs_write_fix(v->z, file);
}

static void gs_write_matrix(vms_matrix* m, FILE* file)
{
	gr_write_vector(&m->rvec, file);
	gr_write_vector(&m->uvec, file);
	gr_write_vector(&m->fvec, file);
}

static void gs_write_angvec(vms_angvec* v, FILE* file)
{
	gs_write_fixang(v->p, file);
	gs_write_fixang(v->b, file);
	gs_write_fixang(v->h, file);
}

#endif

static void read_v16_wall(wall* nwall, CFILE* fp)
{
	nwall->segnum = nwall->sidenum = nwall->linked_wall = 0;
	nwall->type = cfile_read_byte(fp);
	nwall->flags = cfile_read_byte(fp);
	nwall->hps = cfile_read_int(fp);
	nwall->trigger = cfile_read_byte(fp);
	nwall->clip_num = cfile_read_byte(fp);
	nwall->keys = cfile_read_byte(fp);
}

static void read_v19_wall(wall* nwall, CFILE* fp)
{
	nwall->segnum = cfile_read_int(fp);
	nwall->sidenum = cfile_read_int(fp);
	nwall->type = cfile_read_byte(fp);
	nwall->flags = cfile_read_byte(fp);
	nwall->hps = cfile_read_int(fp);
	nwall->trigger = cfile_read_byte(fp);
	nwall->clip_num = cfile_read_byte(fp);
	nwall->keys = cfile_read_byte(fp);
	nwall->linked_wall = cfile_read_int(fp);
	nwall->state = WALL_DOOR_CLOSED;
}

static void read_v19_active_door(active_door* door, CFILE* fp)
{
	v19_door oldDoor;
	int p;

	oldDoor.n_parts = cfile_read_int(fp);
	oldDoor.seg[0] = cfile_read_short(fp);
	oldDoor.seg[1] = cfile_read_short(fp);
	oldDoor.side[0] = cfile_read_short(fp);
	oldDoor.side[1] = cfile_read_short(fp);
	oldDoor.type[0] = cfile_read_short(fp);
	oldDoor.type[1] = cfile_read_short(fp);
	oldDoor.open = cfile_read_int(fp);

	door->n_parts = oldDoor.n_parts;
	for (p = 0; p < oldDoor.n_parts; p++)
	{
		int cseg, cside;

		cseg = Segments[oldDoor.seg[p]].children[oldDoor.side[p]];
		cside = find_connect_side(&Segments[oldDoor.seg[p]], &Segments[cseg]);

		door->front_wallnum[p] = Segments[oldDoor.seg[p]].sides[oldDoor.side[p]].wall_num;
		door->back_wallnum[p] = Segments[cseg].sides[cside].wall_num;
	}
}

//reads one object of the given version from the given file
void read_object(object* obj, CFILE* f, int version)
{
	obj->type = read_byte(f);
	obj->id = read_byte(f);

	if (obj->type == OBJ_CNTRLCEN && version < 28)
		obj->id = 0;		//used to be only one kind of reactor

	obj->control_type = read_byte(f);
	obj->movement_type = read_byte(f);
	obj->render_type = read_byte(f);
	obj->flags = read_byte(f);

	obj->segnum = read_short(f);
	obj->attached_obj = -1;

	read_vector(&obj->pos, f);
	read_matrix(&obj->orient, f);

	obj->size = read_fix(f);
	obj->shields = read_fix(f);

	read_vector(&obj->last_pos, f);

	obj->contains_type = read_byte(f);
	obj->contains_id = read_byte(f);
	obj->contains_count = read_byte(f);

	switch (obj->movement_type) 
	{
	case MT_PHYSICS:

		read_vector(&obj->mtype.phys_info.velocity, f);
		read_vector(&obj->mtype.phys_info.thrust, f);

		obj->mtype.phys_info.mass = read_fix(f);
		obj->mtype.phys_info.drag = read_fix(f);
		obj->mtype.phys_info.brakes = read_fix(f);

		read_vector(&obj->mtype.phys_info.rotvel, f);
		read_vector(&obj->mtype.phys_info.rotthrust, f);

		obj->mtype.phys_info.turnroll = read_fixang(f);
		obj->mtype.phys_info.flags = read_short(f);

		break;

	case MT_SPINNING:

		read_vector(&obj->mtype.spin_rate, f);
		break;

	case MT_NONE:
		break;

	default:
		Int3();
	}

	switch (obj->control_type) 
	{
	case CT_AI:
	{
		int i;

		obj->ctype.ai_info.behavior = read_byte(f);

		for (i = 0; i < MAX_AI_FLAGS; i++)
			obj->ctype.ai_info.flags[i] = read_byte(f);

		obj->ctype.ai_info.hide_segment = read_short(f);
		obj->ctype.ai_info.hide_index = read_short(f);
		obj->ctype.ai_info.path_length = read_short(f);
		obj->ctype.ai_info.cur_path_index = read_short(f);

		if (version <= 25) {
			read_short(f);	//				obj->ctype.ai_info.follow_path_start_seg	= 
			read_short(f);	//				obj->ctype.ai_info.follow_path_end_seg		= 
		}

		break;
	}

	case CT_EXPLOSION:

		obj->ctype.expl_info.spawn_time = read_fix(f);
		obj->ctype.expl_info.delete_time = read_fix(f);
		obj->ctype.expl_info.delete_objnum = read_short(f);
		obj->ctype.expl_info.next_attach = obj->ctype.expl_info.prev_attach = obj->ctype.expl_info.attach_parent = -1;

		break;

	case CT_WEAPON:

		//do I really need to read these?  Are they even saved to disk?

		obj->ctype.laser_info.parent_type = read_short(f);
		obj->ctype.laser_info.parent_num = read_short(f);
		obj->ctype.laser_info.parent_signature = read_int(f);

		break;

	case CT_LIGHT:

		obj->ctype.light_info.intensity = read_fix(f);
		break;

	case CT_POWERUP:

		if (version >= 25)
			obj->ctype.powerup_info.count = read_int(f);
		else
			obj->ctype.powerup_info.count = 1;

		if (obj->id == POW_VULCAN_WEAPON)
			obj->ctype.powerup_info.count = VULCAN_WEAPON_AMMO_AMOUNT;

		if (obj->id == POW_GAUSS_WEAPON)
			obj->ctype.powerup_info.count = VULCAN_WEAPON_AMMO_AMOUNT;

		if (obj->id == POW_OMEGA_WEAPON)
			obj->ctype.powerup_info.count = MAX_OMEGA_CHARGE;

		break;


	case CT_NONE:
	case CT_FLYING:
	case CT_DEBRIS:
		break;

	case CT_SLEW:		//the player is generally saved as slew
		break;

	case CT_CNTRLCEN:
		break;

	case CT_MORPH:
	case CT_FLYTHROUGH:
	case CT_REPAIRCEN:
	default:
		Int3();
	}

	switch (obj->render_type) 
	{
	case RT_NONE:
		break;

	case RT_MORPH:
	case RT_POLYOBJ:
	{
		int i, tmo;

		obj->rtype.pobj_info.model_num = read_int(f);

		for (i = 0; i < MAX_SUBMODELS; i++)
			read_angvec(&obj->rtype.pobj_info.anim_angles[i], f);

		obj->rtype.pobj_info.subobj_flags = read_int(f);

		tmo = read_int(f);

#ifndef EDITOR
		obj->rtype.pobj_info.tmap_override = tmo;
#else
		if (tmo == -1)
			obj->rtype.pobj_info.tmap_override = -1;
		else 
		{
			int xlated_tmo = tmap_xlate_table[tmo];
			if (xlated_tmo < 0)
			{
				mprintf((0, "Couldn't find texture for demo object, model_num = %d\n", obj->rtype.pobj_info.model_num));
				Int3();
				xlated_tmo = 0;
			}
			obj->rtype.pobj_info.tmap_override = xlated_tmo;
		}
#endif

		obj->rtype.pobj_info.alt_textures = 0;

		break;
	}

	case RT_WEAPON_VCLIP:
	case RT_HOSTAGE:
	case RT_POWERUP:
	case RT_FIREBALL:

		obj->rtype.vclip_info.vclip_num = read_int(f);
		obj->rtype.vclip_info.frametime = read_fix(f);
		obj->rtype.vclip_info.framenum = read_byte(f);
		if (obj->rtype.vclip_info.vclip_num >= activeBMTable->vclips.size()) 
			obj->rtype.vclip_info.vclip_num = 0;

		break;

	case RT_LASER:
		break;

	default:
		Int3();

	}

}

#ifdef EDITOR

//writes one object to the given file
void write_object(object* obj, FILE* f)
{
	gs_write_byte(obj->type, f);
	gs_write_byte(obj->id, f);

	gs_write_byte(obj->control_type, f);
	gs_write_byte(obj->movement_type, f);
	gs_write_byte(obj->render_type, f);
	gs_write_byte(obj->flags, f);

	gs_write_short(obj->segnum, f);

	gr_write_vector(&obj->pos, f);
	gs_write_matrix(&obj->orient, f);

	gs_write_fix(obj->size, f);
	gs_write_fix(obj->shields, f);

	gr_write_vector(&obj->last_pos, f);

	gs_write_byte(obj->contains_type, f);
	gs_write_byte(obj->contains_id, f);
	gs_write_byte(obj->contains_count, f);

	switch (obj->movement_type)
	{
	case MT_PHYSICS:

		gr_write_vector(&obj->mtype.phys_info.velocity, f);
		gr_write_vector(&obj->mtype.phys_info.thrust, f);

		gs_write_fix(obj->mtype.phys_info.mass, f);
		gs_write_fix(obj->mtype.phys_info.drag, f);
		gs_write_fix(obj->mtype.phys_info.brakes, f);

		gr_write_vector(&obj->mtype.phys_info.rotvel, f);
		gr_write_vector(&obj->mtype.phys_info.rotthrust, f);

		gs_write_fixang(obj->mtype.phys_info.turnroll, f);
		gs_write_short(obj->mtype.phys_info.flags, f);

		break;

	case MT_SPINNING:

		gr_write_vector(&obj->mtype.spin_rate, f);
		break;

	case MT_NONE:
		break;

	default:
		Int3();
	}

	switch (obj->control_type) 
	{
	case CT_AI: 
	{
		int i;

		gs_write_byte(obj->ctype.ai_info.behavior, f);

		for (i = 0; i < MAX_AI_FLAGS; i++)
			gs_write_byte(obj->ctype.ai_info.flags[i], f);

		gs_write_short(obj->ctype.ai_info.hide_segment, f);
		gs_write_short(obj->ctype.ai_info.hide_index, f);
		gs_write_short(obj->ctype.ai_info.path_length, f);
		gs_write_short(obj->ctype.ai_info.cur_path_index, f);

		// -- unused! mk, 08/13/95 -- gs_write_short(obj->ctype.ai_info.follow_path_start_seg,f);
		// -- unused! mk, 08/13/95 -- gs_write_short(obj->ctype.ai_info.follow_path_end_seg,f);

		break;
	}

	case CT_EXPLOSION:

		gs_write_fix(obj->ctype.expl_info.spawn_time, f);
		gs_write_fix(obj->ctype.expl_info.delete_time, f);
		gs_write_short(obj->ctype.expl_info.delete_objnum, f);

		break;

	case CT_WEAPON:

		//do I really need to write these objects?

		gs_write_short(obj->ctype.laser_info.parent_type, f);
		gs_write_short(obj->ctype.laser_info.parent_num, f);
		gs_write_int(obj->ctype.laser_info.parent_signature, f);

		break;

	case CT_LIGHT:

		gs_write_fix(obj->ctype.light_info.intensity, f);
		break;

	case CT_POWERUP:

		gs_write_int(obj->ctype.powerup_info.count, f);
		break;

	case CT_NONE:
	case CT_FLYING:
	case CT_DEBRIS:
		break;

	case CT_SLEW:		//the player is generally saved as slew
		break;

	case CT_CNTRLCEN:
		break;			//control center object.

	case CT_MORPH:
	case CT_REPAIRCEN:
	case CT_FLYTHROUGH:
	default:
		Int3();

	}

	switch (obj->render_type) 
	{
	case RT_NONE:
		break;

	case RT_MORPH:
	case RT_POLYOBJ:
	{
		int i;

		gs_write_int(obj->rtype.pobj_info.model_num, f);

		for (i = 0; i < MAX_SUBMODELS; i++)
			gs_write_angvec(&obj->rtype.pobj_info.anim_angles[i], f);

		gs_write_int(obj->rtype.pobj_info.subobj_flags, f);

		gs_write_int(obj->rtype.pobj_info.tmap_override, f);

		break;
	}

	case RT_WEAPON_VCLIP:
	case RT_HOSTAGE:
	case RT_POWERUP:
	case RT_FIREBALL:

		gs_write_int(obj->rtype.vclip_info.vclip_num, f);
		gs_write_fix(obj->rtype.vclip_info.frametime, f);
		gs_write_byte(obj->rtype.vclip_info.framenum, f);

		break;

	case RT_LASER:
		break;

	default:
		Int3();
	}
}
#endif

typedef struct 
{
	int			robot_flags;		// Up to 32 different robots
	fix			hit_points;			// How hard it is to destroy this particular matcen
	fix			interval;			// Interval between materialogrifizations
	short			segnum;				// Segment this is attached to.
	short			fuelcen_num;		// Index in fuelcen array.
} old_matcen_info;

extern void remove_trigger_num(int trigger_num);

int8_t ConvertTrigger(short v29Flags) {

	mprintf((1, "Converting trigger %x\n", v29Flags));

	if (v29Flags & TRIGGER_CONTROL_DOORS)
		return TT_OPEN_DOOR;
	else if (v29Flags & TRIGGER_SHIELD_DAMAGE)
		return -1;// Int3();
	else if (v29Flags & TRIGGER_ENERGY_DRAIN)
		return -1;// Int3();
	else if (v29Flags & TRIGGER_EXIT)
		return TT_EXIT;
	else if (v29Flags & TRIGGER_MATCEN)
		return TT_MATCEN;
	else if (v29Flags & TRIGGER_ILLUSION_OFF)
		return TT_ILLUSION_OFF;
	else if (v29Flags & TRIGGER_SECRET_EXIT)
		return TT_SECRET_EXIT;
	else if (v29Flags & TRIGGER_ILLUSION_ON)
		return TT_ILLUSION_ON;
	else if (v29Flags & TRIGGER_UNLOCK_DOORS)
		return TT_UNLOCK_DOOR;
	else if (v29Flags & TRIGGER_OPEN_WALL)
		return TT_OPEN_WALL;
	else if (v29Flags & TRIGGER_CLOSE_WALL)
		return TT_CLOSE_WALL;
	else if (v29Flags & TRIGGER_ILLUSORY_WALL)
		return TT_ILLUSORY_WALL;
	else
		return -1;
}

extern std::vector<fix> Last_afterburner_time;
extern std::vector<int8_t> Lighting_objects;
extern std::vector<fix> object_light;
extern std::vector<int> object_sig;

// -----------------------------------------------------------------------------
// Load game 
// Loads all the relevant data for a level.
// If level != -1, it loads the filename with extension changed to .min
// Otherwise it loads the appropriate level mine.
// returns 0=everything ok, 1=old version, -1=error
int LoadGameDataD1(CFILE* LoadFile)
{

	mprintf((1, "\nLoading D1 game data\n"));

	int i, j;
	int start_offset;

	start_offset = cftell(LoadFile);

	//===================== READ FILE INFO ========================

	// Set default values
	game_fileinfo.level = -1;
	game_fileinfo.player_offset = -1;
	game_fileinfo.player_sizeof = sizeof(player);
	game_fileinfo.object_offset = -1;
	game_fileinfo.object_howmany = 0;
	game_fileinfo.object_sizeof = sizeof(object);
	game_fileinfo.walls_offset = -1;
	game_fileinfo.walls_howmany = 0;
	game_fileinfo.walls_sizeof = sizeof(wall);
	game_fileinfo.doors_offset = -1;
	game_fileinfo.doors_howmany = 0;
	game_fileinfo.doors_sizeof = sizeof(active_door);
	game_fileinfo.triggers_offset = -1;
	game_fileinfo.triggers_howmany = 0;
	game_fileinfo.triggers_sizeof = sizeof(trigger);
	game_fileinfo.control_offset = -1;
	game_fileinfo.control_howmany = 0;
	game_fileinfo.control_sizeof = sizeof(control_center_triggers);
	game_fileinfo.matcen_offset = -1;
	game_fileinfo.matcen_howmany = 0;
	game_fileinfo.matcen_sizeof = sizeof(matcen_info);

	// Read in game_top_fileinfo to get size of saved fileinfo.

	if (cfseek(LoadFile, start_offset, SEEK_SET))
		Error("Error seeking in gamesave.c");

	if (cfread(&game_top_fileinfo, sizeof(game_top_fileinfo), 1, LoadFile) != 1)
		Error("Error reading game_top_fileinfo in gamesave.c");

	// Check signature
	if (game_top_fileinfo.fileinfo_signature != 0x6705)
		return -1;

	// Check version number
	if (game_top_fileinfo.fileinfo_version < GAME_COMPATIBLE_VERSION)
		return -1;

	// Now, Read in the fileinfo
	if (cfseek(LoadFile, start_offset, SEEK_SET))
		Error("Error seeking to game_fileinfo in gamesave.c");

	game_fileinfo.fileinfo_signature = cfile_read_short(LoadFile);

	game_fileinfo.fileinfo_version = cfile_read_short(LoadFile);
	game_fileinfo.fileinfo_sizeof = cfile_read_int(LoadFile);
	for (i = 0; i < 15; i++)
		game_fileinfo.mine_filename[i] = cfile_read_byte(LoadFile);
	game_fileinfo.level = cfile_read_int(LoadFile);
	game_fileinfo.player_offset = cfile_read_int(LoadFile);				// Player info
	game_fileinfo.player_sizeof = cfile_read_int(LoadFile);
	game_fileinfo.object_offset = cfile_read_int(LoadFile);				// Object info
	game_fileinfo.object_howmany = cfile_read_int(LoadFile);
	game_fileinfo.object_sizeof = cfile_read_int(LoadFile);
	game_fileinfo.walls_offset = cfile_read_int(LoadFile);
	game_fileinfo.walls_howmany = cfile_read_int(LoadFile);
	game_fileinfo.walls_sizeof = cfile_read_int(LoadFile);
	game_fileinfo.doors_offset = cfile_read_int(LoadFile);
	game_fileinfo.doors_howmany = cfile_read_int(LoadFile);
	game_fileinfo.doors_sizeof = cfile_read_int(LoadFile);
	game_fileinfo.triggers_offset = cfile_read_int(LoadFile);
	game_fileinfo.triggers_howmany = cfile_read_int(LoadFile);
	game_fileinfo.triggers_sizeof = cfile_read_int(LoadFile);
	game_fileinfo.links_offset = cfile_read_int(LoadFile);
	game_fileinfo.links_howmany = cfile_read_int(LoadFile);
	game_fileinfo.links_sizeof = cfile_read_int(LoadFile);
	game_fileinfo.control_offset = cfile_read_int(LoadFile);
	game_fileinfo.control_howmany = cfile_read_int(LoadFile);
	game_fileinfo.control_sizeof = cfile_read_int(LoadFile);
	game_fileinfo.matcen_offset = cfile_read_int(LoadFile);
	game_fileinfo.matcen_howmany = cfile_read_int(LoadFile);
	game_fileinfo.matcen_sizeof = cfile_read_int(LoadFile);

	if (game_top_fileinfo.fileinfo_version >= 14) //load mine filename
	{
		cfile_get_string(&Current_level_name[0], LEVEL_NAME_LEN, LoadFile);
	}
	else
		Current_level_name[0] = 0;

	if (game_top_fileinfo.fileinfo_version >= 19) //load pof names
	{
		N_save_pof_names = (int)cfile_read_short(LoadFile);
		Save_pof_names.resize(N_save_pof_names);
		cfread(Save_pof_names.data(), FILENAME_LEN, N_save_pof_names, LoadFile);
	}

	//===================== READ PLAYER INFO ==========================
	Object_next_signature = 0;

	//===================== READ OBJECT INFO ==========================

	Gamesave_num_org_robots = 0;
	Gamesave_num_players = 0;

	if (game_fileinfo.object_offset > -1) 
	{
		if (cfseek(LoadFile, game_fileinfo.object_offset, SEEK_SET))
			Error("Error seeking to object_offset in gamesave.c");

		if (game_fileinfo.object_howmany > Objects.size()) {
			RelinkCache cache;
			ResizeObjectVectors(game_fileinfo.object_howmany, false);
			RelinkSpecialObjectPointers(cache);
		}

		for (i = 0; i < Objects.size(); i++) 
		{
			if (i < game_fileinfo.object_howmany) {
				read_object(&Objects[i], LoadFile, game_top_fileinfo.fileinfo_version);

				Objects[i].signature = Object_next_signature++;
				verify_object(&Objects[i]);
			} else
				Objects[i] = object();
		}
	}

	//===================== READ WALL INFO ============================
	if (game_fileinfo.walls_offset > -1)
	{
		if (!cfseek(LoadFile, game_fileinfo.walls_offset, SEEK_SET)) 
		{
			Walls.resize(game_fileinfo.walls_howmany);
			
			for (i = 0; i < game_fileinfo.walls_howmany; i++) 
			{
				if (game_top_fileinfo.fileinfo_version >= 20) 
					read_wall(&Walls[i], LoadFile->file);
				else if (game_top_fileinfo.fileinfo_version >= 17)
					read_v19_wall(&Walls[i], LoadFile);
				else 
					read_v16_wall(&Walls[i], LoadFile);
			}

			validate_walls();
		}
	}
	
	//===================== READ DOOR INFO ============================
	if (game_fileinfo.doors_offset > -1)
	{
		if (!cfseek(LoadFile, game_fileinfo.doors_offset, SEEK_SET)) 
		{
			mprintf((1, "load_game_data: active doors present\n"));

			ActiveDoors.resize(game_fileinfo.doors_howmany);

			for (i = 0; i < game_fileinfo.doors_howmany; i++) 
			{
				if (game_top_fileinfo.fileinfo_version >= 20) 
					read_active_door(&ActiveDoors[i], LoadFile->file);
				else 
					read_v19_active_door(&ActiveDoors[i], LoadFile);
			}
		}
	} else
		ActiveDoors.clear();

	//==================== READ TRIGGER INFO ==========================
	if (game_fileinfo.triggers_offset > -1)
	{
		if (!cfseek(LoadFile, game_fileinfo.triggers_offset, SEEK_SET)) 
		{
			//if (game_fileinfo.triggers_howmany > MAX_TRIGGERS)
			//	Error("Level contains more than MAX_TRIGGERS(%d) triggers.", MAX_TRIGGERS);

			Triggers.resize(game_fileinfo.triggers_howmany);

			for (i = 0; i < game_fileinfo.triggers_howmany; i++) 
			{
				int8_t type1 = cfile_read_byte(LoadFile);
				short flags1 = cfile_read_short(LoadFile);

				Triggers[i].type = MakeD1HTrigger(flags1);
				Triggers[i].flags = MakeD1HTriggerFlags(flags1);
				Triggers[i].value = cfile_read_int(LoadFile);
				Triggers[i].time = cfile_read_int(LoadFile);
				//Triggers[i].link_num = cfile_read_byte(LoadFile);
				cfile_read_byte(LoadFile);
				Triggers[i].num_links = cfile_read_short(LoadFile);
				for (j = 0; j < MAX_WALLS_PER_LINK; j++)
					Triggers[i].seg[j] = cfile_read_short(LoadFile);
				for (j = 0; j < MAX_WALLS_PER_LINK; j++)
					Triggers[i].side[j] = cfile_read_short(LoadFile);
			}
		}
	}

	//================ READ CONTROL CENTER TRIGGER INFO ===============
	if (game_fileinfo.control_offset > -1)
	{
		if (!cfseek(LoadFile, game_fileinfo.control_offset, SEEK_SET)) 
		{
			for (i = 0; i < game_fileinfo.control_howmany; i++)
				ControlCenterTriggers.num_links = read_short(LoadFile);
			for (j = 0; j < MAX_CONTROLCEN_LINKS; j++)
				ControlCenterTriggers.seg[j] = read_short(LoadFile);
			for (j = 0; j < MAX_CONTROLCEN_LINKS; j++)
				ControlCenterTriggers.side[j] = read_short(LoadFile);
		}
	}

	/*
	if (game_fileinfo.control_offset > -1)
	{
		if (!cfseek(LoadFile, game_fileinfo.control_offset, SEEK_SET)) 
		{
			for (i = 0; i < game_fileinfo.control_howmany; i++)
				ControlCenterTriggers.num_links = read_short(LoadFile);
			for (j = 0; j < MAX_CONTROLCEN_LINKS; j++)
				ControlCenterTriggers.seg[j] = read_short(LoadFile);
			for (j = 0; j < MAX_CONTROLCEN_LINKS; j++)
				ControlCenterTriggers.side[j] = read_short(LoadFile);
		}
	}
	*/

	//================ READ MATERIALOGRIFIZATIONATORS INFO ===============
	if (game_fileinfo.matcen_offset > -1)
	{
		int	j;

		if (!cfseek(LoadFile, game_fileinfo.matcen_offset, SEEK_SET)) 
		{
			RobotCenters.resize(game_fileinfo.matcen_howmany);

			// mprintf((0, "Reading %i materialization centers.\n", game_fileinfo.matcen_howmany));
			for (i = 0; i < game_fileinfo.matcen_howmany; i++) 
			{
				//Assert(sizeof(RobotCenters[i]) == game_fileinfo.matcen_sizeof);
				//if (cfread(&RobotCenters[i], game_fileinfo.matcen_sizeof, 1, LoadFile) != 1)
				//	Error("Error reading RobotCenters in gamesave.c", i);
				read_matcen(&RobotCenters[i], LoadFile->file);
				//	Set links in RobotCenters to Station array
				for (j = 0; j <= Highest_segment_index; j++)
					if (Segments[j].special == SEGMENT_IS_ROBOTMAKER)
						if (Segments[j].matcen_num == i)
							RobotCenters[i].fuelcen_num = Segments[j].value;

				// mprintf((0, "   %i: flags = %08x\n", i, RobotCenters[i].robot_flags));
			}
		}
	}


	//========================= UPDATE VARIABLES ======================

	reset_objects(game_fileinfo.object_howmany);

	for (i = 0; i < Objects.size(); i++) 
	{
		Objects[i].next = Objects[i].prev = -1;
		if (Objects[i].type != OBJ_NONE) 
		{
			int objsegnum = Objects[i].segnum;

			if (objsegnum > Highest_segment_index)		//bogus object
				Objects[i].type = OBJ_NONE;
			else
			{
				Objects[i].segnum = -1;			//avoid Assert()
				obj_link(i, objsegnum);
			}
		}
	}

	clear_transient_objects(1);		//1 means clear proximity bombs

	// Make sure non-transparent doors are set correctly.
	for (i = 0; i < Num_segments; i++)
		for (j = 0; j < MAX_SIDES_PER_SEGMENT; j++) 
		{
			side* sidep = &Segments[i].sides[j];
			if ((sidep->wall_num != -1) && (Walls[sidep->wall_num].clip_num != -1)) 
			{
				//mprintf((0, "Checking Wall %d\n", Segments[i].sides[j].wall_num));
				if (activeBMTable->wclips[Walls[sidep->wall_num].clip_num].flags & WCF_TMAP1) 
				{
					//mprintf((0, "Fixing non-transparent door.\n"));
					sidep->tmap_num = activeBMTable->wclips[Walls[sidep->wall_num].clip_num].frames[0];
					sidep->tmap_num2 = 0;
				}
			}
		}


	auto Num_walls = game_fileinfo.walls_howmany;
	//reset_walls();
	//Walls.resize(Num_walls);

	auto Num_open_doors = game_fileinfo.doors_howmany;
	auto Num_triggers = game_fileinfo.triggers_howmany;
	auto Num_robot_centers = game_fileinfo.matcen_howmany;

	//fix old wall structs
	if (game_top_fileinfo.fileinfo_version < 17) 
	{
		int segnum, sidenum, wallnum;

		for (segnum = 0; segnum <= Highest_segment_index; segnum++)
			for (sidenum = 0; sidenum < 6; sidenum++)
				if ((wallnum = Segments[segnum].sides[sidenum].wall_num) != -1) 
				{
					Walls[wallnum].segnum = segnum;
					Walls[wallnum].sidenum = sidenum;
				}
	}

#ifndef NDEBUG
	{
		int	sidenum;
		for (sidenum = 0; sidenum < 6; sidenum++) 
		{
			int	wallnum = Segments[Highest_segment_index].sides[sidenum].wall_num;
			if (wallnum != -1)
				if ((Walls[wallnum].segnum != Highest_segment_index) || (Walls[wallnum].sidenum != sidenum))
					Int3();	//	Error.  Bogus walls in this segment.
								// Consult Yuan or Mike.
		}
	}
#endif

	//create_local_segment_data();

	fix_object_segs();

#ifndef NDEBUG
	dump_mine_info();
#endif

	if (game_top_fileinfo.fileinfo_version < GAME_VERSION)
		return 1;		//means old version
	else
		return 0;
}

int LoadGameDataD2(CFILE* LoadFile)
{
	mprintf((1, "\nLoading D2 game data\n"));

	int i, j;
	int start_offset;

	start_offset = cftell(LoadFile);

	//===================== READ FILE INFO ========================

	// Set default values
	game_fileinfo.level = -1;
	game_fileinfo.player_offset = -1;
	game_fileinfo.player_sizeof = sizeof(player);
	game_fileinfo.object_offset = -1;
	game_fileinfo.object_howmany = 0;
	game_fileinfo.object_sizeof = sizeof(object);
	game_fileinfo.walls_offset = -1;
	game_fileinfo.walls_howmany = 0;
	game_fileinfo.walls_sizeof = sizeof(wall);
	game_fileinfo.doors_offset = -1;
	game_fileinfo.doors_howmany = 0;
	game_fileinfo.doors_sizeof = sizeof(active_door);
	game_fileinfo.triggers_offset = -1;
	game_fileinfo.triggers_howmany = 0;
	game_fileinfo.triggers_sizeof = sizeof(trigger);
	game_fileinfo.control_offset = -1;
	game_fileinfo.control_howmany = 0;
	game_fileinfo.control_sizeof = sizeof(control_center_triggers);
	game_fileinfo.matcen_offset = -1;
	game_fileinfo.matcen_howmany = 0;
	game_fileinfo.matcen_sizeof = sizeof(matcen_info);

	game_fileinfo.dl_indices_offset = -1;
	game_fileinfo.dl_indices_howmany = 0;
	game_fileinfo.dl_indices_sizeof = sizeof(dl_index);

	game_fileinfo.delta_light_offset = -1;
	game_fileinfo.delta_light_howmany = 0;
	game_fileinfo.delta_light_sizeof = sizeof(delta_light);

	// Read in game_top_fileinfo to get size of saved fileinfo.

	if (cfseek(LoadFile, start_offset, SEEK_SET))
		Error("Error seeking in gamesave.c");

	//	if (cfread( &game_top_fileinfo, sizeof(game_top_fileinfo), 1, LoadFile) != 1)
	//		Error( "Error reading game_top_fileinfo in gamesave.c" );

	game_top_fileinfo.fileinfo_signature = read_short(LoadFile);
	game_top_fileinfo.fileinfo_version = read_short(LoadFile);
	game_top_fileinfo.fileinfo_sizeof = read_int(LoadFile);

	// Check signature
	if (game_top_fileinfo.fileinfo_signature != 0x6705)
		return -1;

	// Check version number
	if (game_top_fileinfo.fileinfo_version < GAME_COMPATIBLE_VERSION)
		return -1;

	// Now, Read in the fileinfo
	if (cfseek(LoadFile, start_offset, SEEK_SET))
		Error("Error seeking to game_fileinfo in gamesave.c");

	//	if (cfread( &game_fileinfo, game_top_fileinfo.fileinfo_sizeof, 1, LoadFile )!=1)
	//		Error( "Error reading game_fileinfo in gamesave.c" );

	game_fileinfo.fileinfo_signature = read_short(LoadFile);
	game_fileinfo.fileinfo_version = read_short(LoadFile);
	game_fileinfo.fileinfo_sizeof = read_int(LoadFile);
	for (i = 0; i < 15; i++)
		game_fileinfo.mine_filename[i] = read_byte(LoadFile);
	game_fileinfo.level = read_int(LoadFile);
	game_fileinfo.player_offset = read_int(LoadFile);				// Player info
	game_fileinfo.player_sizeof = read_int(LoadFile);
	game_fileinfo.object_offset = read_int(LoadFile);				// Object info
	game_fileinfo.object_howmany = read_int(LoadFile);
	game_fileinfo.object_sizeof = read_int(LoadFile);
	game_fileinfo.walls_offset = read_int(LoadFile);
	game_fileinfo.walls_howmany = read_int(LoadFile);
	game_fileinfo.walls_sizeof = read_int(LoadFile);
	game_fileinfo.doors_offset = read_int(LoadFile);
	game_fileinfo.doors_howmany = read_int(LoadFile);
	game_fileinfo.doors_sizeof = read_int(LoadFile);
	game_fileinfo.triggers_offset = read_int(LoadFile);
	game_fileinfo.triggers_howmany = read_int(LoadFile);
	game_fileinfo.triggers_sizeof = read_int(LoadFile);
	game_fileinfo.links_offset = read_int(LoadFile);
	game_fileinfo.links_howmany = read_int(LoadFile);
	game_fileinfo.links_sizeof = read_int(LoadFile);
	game_fileinfo.control_offset = read_int(LoadFile);
	game_fileinfo.control_howmany = read_int(LoadFile);
	game_fileinfo.control_sizeof = read_int(LoadFile);
	game_fileinfo.matcen_offset = read_int(LoadFile);
	game_fileinfo.matcen_howmany = read_int(LoadFile);
	game_fileinfo.matcen_sizeof = read_int(LoadFile);

	if (game_top_fileinfo.fileinfo_version >= 29) 
	{
		game_fileinfo.dl_indices_offset = read_int(LoadFile);
		game_fileinfo.dl_indices_howmany = read_int(LoadFile);
		game_fileinfo.dl_indices_sizeof = read_int(LoadFile);

		game_fileinfo.delta_light_offset = read_int(LoadFile);
		game_fileinfo.delta_light_howmany = read_int(LoadFile);
		game_fileinfo.delta_light_sizeof = read_int(LoadFile);
	}

	if (game_top_fileinfo.fileinfo_version >= 14) 	//load mine filename
	{
		cfgets(Current_level_name, sizeof(Current_level_name), LoadFile);

		if (Current_level_name[strlen(Current_level_name) - 1] == '\n')
			Current_level_name[strlen(Current_level_name) - 1] = 0;
	}
	else
		Current_level_name[0] = 0;

	if (game_top_fileinfo.fileinfo_version >= 19) 	//load pof names
	{
		N_save_pof_names = read_short(LoadFile);
		Save_pof_names.resize(N_save_pof_names);
		cfread(Save_pof_names.data(), FILENAME_LEN, N_save_pof_names, LoadFile);
	}

	//===================== READ PLAYER INFO ==========================
	Object_next_signature = 0;

	//===================== READ OBJECT INFO ==========================

	Gamesave_num_org_robots = 0;
	Gamesave_num_players = 0;

	if (game_fileinfo.object_offset > -1)
	{
		if (cfseek(LoadFile, game_fileinfo.object_offset, SEEK_SET))
			Error("Error seeking to object_offset in gamesave.c");

		if (game_fileinfo.object_howmany > Objects.size()) {
			RelinkCache cache;
			ResizeObjectVectors(game_fileinfo.object_howmany, false);
			RelinkSpecialObjectPointers(cache);
		}

		for (i = 0; i < Objects.size(); i++)
		{
			if (i < game_fileinfo.object_howmany) {
				read_object(&Objects[i], LoadFile, game_top_fileinfo.fileinfo_version);

				Objects[i].signature = Object_next_signature++;
				verify_object(&Objects[i]);
			}
			else
				Objects[i] = object();
		}
	}

	//===================== READ WALL INFO ============================

	if (game_fileinfo.walls_offset > -1)
	{
		if (!cfseek(LoadFile, game_fileinfo.walls_offset, SEEK_SET))
		{
			Walls.resize(game_fileinfo.walls_howmany);

			for (i = 0; i < game_fileinfo.walls_howmany; i++) 
			{
				if (game_top_fileinfo.fileinfo_version >= 20)
				{
					Assert(24 == game_fileinfo.walls_sizeof);

					Walls[i].segnum = read_int(LoadFile);
					Walls[i].sidenum = read_int(LoadFile);
					Walls[i].hps = read_fix(LoadFile);
					Walls[i].linked_wall = read_int(LoadFile);
					Walls[i].type = read_byte(LoadFile);
					Walls[i].flags = read_byte(LoadFile);
					Walls[i].state = read_byte(LoadFile);
					Walls[i].trigger = read_byte(LoadFile);
					Walls[i].clip_num = read_byte(LoadFile);
					Walls[i].keys = read_byte(LoadFile);
					Walls[i].controlling_trigger = read_byte(LoadFile);
					Walls[i].cloak_value = read_byte(LoadFile);
				}
				else if (game_top_fileinfo.fileinfo_version >= 17) 
				{
					v19_wall w;

					Assert(21 == game_fileinfo.walls_sizeof);
					mprintf((1, "load_game_data: legacy v19 walls present\n"));
					read_v19_wall(&Walls[i], LoadFile);
				}
				else 
				{
					v16_wall w;

					Assert(9 == game_fileinfo.walls_sizeof);
					mprintf((1, "load_game_data: legacy v16 walls present\n"));
					read_v16_wall(&Walls[i], LoadFile);
				}
			}

			validate_walls();
		}
	}

	//===================== READ DOOR INFO ============================

	if (game_fileinfo.doors_offset > -1)
	{
		if (!cfseek(LoadFile, game_fileinfo.doors_offset, SEEK_SET)) 
		{
			mprintf((1, "load_game_data: active doors present\n"));

			ActiveDoors.resize(game_fileinfo.doors_howmany);

			for (i = 0; i < game_fileinfo.doors_howmany; i++)
			{
				if (game_top_fileinfo.fileinfo_version >= 20) 
				{
					Assert(16 == game_fileinfo.doors_sizeof);

					ActiveDoors[i].n_parts = read_int(LoadFile);
					ActiveDoors[i].front_wallnum[0] = read_short(LoadFile);
					ActiveDoors[i].front_wallnum[1] = read_short(LoadFile);
					ActiveDoors[i].back_wallnum[0] = read_short(LoadFile);
					ActiveDoors[i].back_wallnum[1] = read_short(LoadFile);
					ActiveDoors[i].time = read_fix(LoadFile);
				}
				else 
				{
					v19_door d;
					int p;

					Assert(20 == game_fileinfo.doors_sizeof);

					read_v19_active_door(&ActiveDoors[i], LoadFile);
				}
			}
		}
	} else
		ActiveDoors.clear();

	//==================== READ TRIGGER INFO ==========================

	if (game_fileinfo.triggers_offset > -1)
	{
		if (!cfseek(LoadFile, game_fileinfo.triggers_offset, SEEK_SET)) 
		{
			//if (game_fileinfo.triggers_howmany > MAX_TRIGGERS)
			//	Error("Level contains more than MAX_TRIGGERS(%d) triggers.", MAX_TRIGGERS);
			Triggers.resize(game_fileinfo.triggers_howmany);

			for (i = 0; i < game_fileinfo.triggers_howmany; i++)
				if (game_top_fileinfo.fileinfo_version < 31)
				{
					v30_trigger trig;
					int t, type;

					if (game_top_fileinfo.fileinfo_version < 30)
					{
						v29_trigger trig29;
						int t;

						mprintf((1, "load_game_data: loading v29 trigger\n"));

						if (cfread(&trig29, game_fileinfo.triggers_sizeof, 1, LoadFile) != 1)
							Error("Error reading Triggers[%d] in gamesave.c", i);

						trig.flags = trig29.flags;
						trig.num_links = trig29.num_links;
						trig.num_links = trig29.num_links;
						trig.value = trig29.value;
						trig.time = trig29.time;

						for (t = 0; t < trig.num_links; t++) 
						{
							trig.seg[t] = trig29.seg[t];
							trig.side[t] = trig29.side[t];
						}

					}
					else
					{
						mprintf((1, "load_game_data: loading v30 trigger\n"));
						if (cfread(&trig, game_fileinfo.triggers_sizeof, 1, LoadFile) != 1)
							Error("Error reading Triggers[%d] in gamesave.c", i);
					}

					//Assert(trig.flags & TRIGGER_ON);
					trig.flags &= ~TRIGGER_ON;

					Triggers[i].type = MakeD1HTrigger(trig.flags);
					Triggers[i].flags = MakeD1HTriggerFlags(trig.flags);
					Triggers[i].num_links = trig.num_links;
					Triggers[i].num_links = trig.num_links;
					Triggers[i].value = trig.value;
					Triggers[i].time = trig.time;

					for (t = 0; t < trig.num_links; t++) 
					{
						Triggers[i].seg[t] = trig.seg[t];
						Triggers[i].side[t] = trig.side[t];
					}
				}
				else 
				{
					Triggers[i].type = MakeD2HTrigger(read_byte(LoadFile));
					Triggers[i].flags = read_byte(LoadFile);
					Triggers[i].num_links = read_byte(LoadFile);
					read_byte(LoadFile);
					Triggers[i].value = read_fix(LoadFile);
					Triggers[i].time = read_fix(LoadFile);
					for (j = 0; j < MAX_WALLS_PER_LINK; j++)
						Triggers[i].seg[j] = read_short(LoadFile);
					for (j = 0; j < MAX_WALLS_PER_LINK; j++)
						Triggers[i].side[j] = read_short(LoadFile);
				}
		}
	}

	//================ READ CONTROL CENTER TRIGGER INFO ===============

	if (game_fileinfo.control_offset > -1)
	{
		if (!cfseek(LoadFile, game_fileinfo.control_offset, SEEK_SET)) 
		{
			for (i = 0; i < game_fileinfo.control_howmany; i++)
				ControlCenterTriggers.num_links = read_short(LoadFile);
			for (j = 0; j < MAX_CONTROLCEN_LINKS; j++)
				ControlCenterTriggers.seg[j] = read_short(LoadFile);
			for (j = 0; j < MAX_CONTROLCEN_LINKS; j++)
				ControlCenterTriggers.side[j] = read_short(LoadFile);
		}
	}


	//================ READ MATERIALOGRIFIZATIONATORS INFO ===============

	if (game_fileinfo.matcen_offset > -1)
	{
		int	j;

		if (!cfseek(LoadFile, game_fileinfo.matcen_offset, SEEK_SET)) 
		{
			RobotCenters.resize(game_fileinfo.matcen_howmany);

			// mprintf((0, "Reading %i materialization centers.\n", game_fileinfo.matcen_howmany));
			for (i = 0; i < game_fileinfo.matcen_howmany; i++) 
			{
				if (game_top_fileinfo.fileinfo_version < 27) 
				{
					old_matcen_info m;
					Assert(game_fileinfo.matcen_sizeof == 16);
					RobotCenters[i].robot_flags[0] = read_int(LoadFile);
					RobotCenters[i].robot_flags[1] = 0;
					RobotCenters[i].hit_points = read_fix(LoadFile);
					RobotCenters[i].interval = read_fix(LoadFile);
					RobotCenters[i].segnum = read_short(LoadFile);
					RobotCenters[i].fuelcen_num = read_short(LoadFile);
				}
				else 
				{
					Assert(game_fileinfo.matcen_sizeof == 20);
					RobotCenters[i].robot_flags[0] = read_int(LoadFile);
					RobotCenters[i].robot_flags[1] = read_int(LoadFile);
					RobotCenters[i].hit_points = read_fix(LoadFile);
					RobotCenters[i].interval = read_fix(LoadFile);
					RobotCenters[i].segnum = read_short(LoadFile);
					RobotCenters[i].fuelcen_num = read_short(LoadFile);
				}

				//	Set links in RobotCenters to Station array

				for (j = 0; j <= Highest_segment_index; j++)
					if (Segment2s[j].special == SEGMENT_IS_ROBOTMAKER)
						if (Segment2s[j].matcen_num == i)
							RobotCenters[i].fuelcen_num = Segment2s[j].value;

				// mprintf((0, "   %i: flags = %08x\n", i, RobotCenters[i].robot_flags));
			}
		}
	}


	//================ READ DL_INDICES INFO ===============

	//Num_static_lights = 0;

	if (game_top_fileinfo.fileinfo_version >= 29) {

		if (game_fileinfo.dl_indices_offset > -1)
		{
			int	i;

			if (!cfseek(LoadFile, game_fileinfo.dl_indices_offset, SEEK_SET))
			{
				
				Dl_indices.resize(game_fileinfo.dl_indices_howmany);

				for (i = 0; i < game_fileinfo.dl_indices_howmany; i++)
				{
					Dl_indices[i].segnum = read_short(LoadFile);
					Dl_indices[i].sidenum = read_byte(LoadFile);
					Dl_indices[i].count = read_byte(LoadFile);
					Dl_indices[i].index = read_short(LoadFile);
				}
			}
		}

	} else {
		mprintf((0, "Warning: Old mine version.  Not reading Dl_indices info.\n"));
	}

	//	Indicate that no light has been subtracted from any vertices.
	clear_light_subtracted();

	//================ READ DELTA LIGHT INFO ===============

	if (game_top_fileinfo.fileinfo_version >= 29) {

		if (game_fileinfo.delta_light_offset > -1) {
			int	i;

			if (!cfseek(LoadFile, game_fileinfo.delta_light_offset, SEEK_SET)) {

				Delta_lights.resize(game_fileinfo.delta_light_howmany);

				for (i = 0; i < game_fileinfo.delta_light_howmany; i++) {
					Delta_lights[i].segnum = read_short(LoadFile);
					Delta_lights[i].sidenum = read_byte(LoadFile);
					Delta_lights[i].dummy = read_byte(LoadFile);
					Delta_lights[i].vert_light[0] = read_byte(LoadFile);
					Delta_lights[i].vert_light[1] = read_byte(LoadFile);
					Delta_lights[i].vert_light[2] = read_byte(LoadFile);
					Delta_lights[i].vert_light[3] = read_byte(LoadFile);
				}
			}
		}

	} else {
		mprintf((0, "Warning: Old mine version.  Not reading delta light info.\n"));
	}

	//========================= UPDATE VARIABLES ======================

	reset_objects(game_fileinfo.object_howmany);

	for (i = 0; i < Objects.size(); i++) 
	{
		Objects[i].next = Objects[i].prev = -1;
		if (Objects[i].type != OBJ_NONE) 
		{
			int objsegnum = Objects[i].segnum;

			if (objsegnum > Highest_segment_index)		//bogus object
				Objects[i].type = OBJ_NONE;
			else {
				Objects[i].segnum = -1;			//avoid Assert()
				obj_link(i, objsegnum);
			}
		}
	}

	clear_transient_objects(1);		//1 means clear proximity bombs

	// Make sure non-transparent doors are set correctly.
	for (i = 0; i < Num_segments; i++)
		for (j = 0; j < MAX_SIDES_PER_SEGMENT; j++)
		{
			side* sidep = &Segments[i].sides[j];
			if ((sidep->wall_num != -1) && (Walls[sidep->wall_num].clip_num != -1)) 
			{
				//mprintf((0, "Checking Wall %d\n", Segments[i].sides[j].wall_num));
				if (activeBMTable->wclips[Walls[sidep->wall_num].clip_num].flags & WCF_TMAP1) 
				{
					//mprintf((0, "Fixing non-transparent door.\n"));
					sidep->tmap_num = activeBMTable->wclips[Walls[sidep->wall_num].clip_num].frames[0];
					sidep->tmap_num2 = 0;
				}
			}
		}


	auto Num_walls = game_fileinfo.walls_howmany;
	//reset_walls();
	//Walls.resize(Num_walls);

	auto Num_open_doors = game_fileinfo.doors_howmany;
	auto Num_triggers = game_fileinfo.triggers_howmany;
	Triggers.resize(Num_triggers);

	//go through all walls, killing references to invalid triggers
	for (i = 0; i < Num_walls; i++)
		if (Walls[i].trigger >= Num_triggers)
		{
			mprintf((0, "Removing reference to invalid trigger %d from wall %d\n", Walls[i].trigger, i));
			Walls[i].trigger = -1;	//kill trigger
		}

	//go through all triggers, killing unused ones
	for (i = 0; i < Num_triggers;) 
	{
		int w;

		//	Find which wall this trigger is connected to.
		for (w = 0; w < Num_walls; w++)
			if (Walls[w].trigger == i)
				break;

#ifdef EDITOR
		if (w == Num_walls) 
		{
			mprintf((0, "Removing unreferenced trigger %d\n", i));
			remove_trigger_num(i);
		}
		else
#endif
			i++;
	}

	//	MK, 10/17/95: Make walls point back at the triggers that control them.
	//	Go through all triggers, stuffing controlling_trigger field in Walls.
	{	
		int t;

		for (i = 0; i < Num_walls; i++)
			Walls[i].controlling_trigger = -1;

		for (t = 0; t < Num_triggers; t++) 
		{
			int	l;
			for (l = 0; l < Triggers[t].num_links; l++)
			{
				int	seg_num, side_num, wall_num;

				seg_num = Triggers[t].seg[l];
				side_num = Triggers[t].side[l];
				wall_num = Segments[seg_num].sides[side_num].wall_num;

				// -- if (Walls[wall_num].controlling_trigger != -1)
				// -- 	Int3();

				//check to see that if a trigger requires a wall that it has one,
				//and if it requires a matcen that it has one

				if (Triggers[t].type & HTT_MATCEN) 
				{
					if (Segment2s[seg_num].special != SEGMENT_IS_ROBOTMAKER)
						Int3();		//matcen trigger doesn't point to matcen
				}
				else if (!(Triggers[t].type & (HTT_LIGHT_OFF | HTT_LIGHT_ON))) {	//light triggers don't require walls
					if (wall_num == -1)
						Int3();	//	This is illegal.  This trigger requires a wall
					else
						Walls[wall_num].controlling_trigger = t;
				}
			}
		}
	}

	auto Num_robot_centers = game_fileinfo.matcen_howmany;

	//fix old wall structs
	if (game_top_fileinfo.fileinfo_version < 17) 
	{
		int segnum, sidenum, wallnum;

		for (segnum = 0; segnum <= Highest_segment_index; segnum++)
			for (sidenum = 0; sidenum < 6; sidenum++)
				if ((wallnum = Segments[segnum].sides[sidenum].wall_num) != -1) 
				{
					Walls[wallnum].segnum = segnum;
					Walls[wallnum].sidenum = sidenum;
				}
	}

#ifndef NDEBUG
	{
		int	sidenum;
		for (sidenum = 0; sidenum < 6; sidenum++)
		{
			int	wallnum = Segments[Highest_segment_index].sides[sidenum].wall_num;
			if (wallnum != -1)
				if ((Walls[wallnum].segnum != Highest_segment_index) || (Walls[wallnum].sidenum != sidenum))
					Int3();	//	Error.  Bogus walls in this segment.
								// Consult Yuan or Mike.
		}
	}
#endif

	//create_local_segment_data();

	fix_object_segs();

#ifndef NDEBUG
	dump_mine_info();
#endif

	if (game_top_fileinfo.fileinfo_version < GAME_VERSION && !(game_top_fileinfo.fileinfo_version == 25 && GAME_VERSION == 26))
		return 1;		//means old version
	else
		return 0;
}

int load_game_data(CFILE* LoadFile) {
	if (currentGame == G_DESCENT_1)
		return LoadGameDataD1(LoadFile);
	else if (currentGame == G_DESCENT_2)
		return LoadGameDataD2(LoadFile);
	else
		Int3();
}

int check_segment_connections(void);

extern void	set_ambient_sound_flags(void);

// -----------------------------------------------------------------------------
//loads from an already-open file
// returns 0=everything ok, 1=old version, -1=error
int load_mine_data(CFILE* LoadFile);
int load_mine_data_compiled(CFILE* LoadFile);

#define LEVEL_FILE_VERSION		8
//1 -> 2  add palette name
//2 -> 3  add control center explosion time
//3 -> 4  add reactor strength
//4 -> 5  killed hostage text stuff
//5 -> 6  added Secret_return_segment and Secret_return_orient
//6 -> 7	 added flickering lights
//7 -> 8  made version 8 to be not compatible with D2 1.0 & 1.1

#ifndef RELEASE
char* Level_being_loaded = NULL;
#endif

#ifdef COMPACT_SEGS
extern void ncache_flush();
#endif

#ifdef SANTA
extern int HoardEquipped();
#endif

extern	int	Slide_segs_computed;

int no_old_level_file_error = 0;

//loads a level (.LVL) file from disk
//returns 0 if success, else error code
int load_level(char* filename_passed)
{
#ifdef EDITOR
	int use_compiled_level = 1;
#endif
	CFILE* LoadFile;
	char filename[128];
	int sig, version, minedata_offset, gamedata_offset;
	int mine_err, game_err, i;

	Slide_segs_computed = 0;

	if (Game_mode & GM_NETWORK)
	{
#ifdef NETWORK
		for (i = 0; i < MAX_POWERUP_TYPES; i++)
		{
			MaxPowerupsAllowed[i] = 0;
			PowerupsInMine[i] = 0;
		}
#endif
	}

#ifdef COMPACT_SEGS
	ncache_flush();
#endif

#ifndef RELEASE
	Level_being_loaded = filename_passed;
#endif

	strcpy(filename, filename_passed);
	_strupr(filename);

#ifdef EDITOR
	//if we have the editor, try the LVL first, no matter what was passed.
	//if we don't have an LVL, try RDL  
	//if we don't have the editor, we just use what was passed

	change_filename_extension(filename, filename_passed, ".LVL");
	use_compiled_level = 0;

	if (!cfexist(filename))
	{
		change_filename_extension(filename, filename, ".RL2");
		use_compiled_level = 1;
	}
#endif

	LoadFile = cfopen(filename, "rb");

	if (!LoadFile)
	{
#ifdef EDITOR
		mprintf((0, "Can't open level file <%s>\n", filename));
		return 1;
#else
		Error("Can't open file <%s>\n", filename);
#endif
	}

	strcpy(Gamesave_current_filename, filename);

	//	#ifdef NEWDEMO
	//	if ( Newdemo_state == ND_STATE_RECORDING )
	//		newdemo_record_start_demo();
	//	#endif

	sig = read_int(LoadFile);
	version = read_int(LoadFile);
	minedata_offset = read_int(LoadFile);
	gamedata_offset = read_int(LoadFile);

	Assert(sig == 'PLVL');

	printf(" Level version %d\n", version);

	if (version >= 8) //read dummy data
	{			
#ifdef SANTA
		if (HoardEquipped())
		{
			read_int(LoadFile);
			read_short(LoadFile);
			read_byte(LoadFile);
		}
		else Error("This level requires the Vertigo Enhanced version of D2.");
#else
		Error("load_level: Sorry, ISB is bad at this...");
#endif
	}

	if (version < 5)
		read_int(LoadFile);		//was hostagetext_offset

	if (version > 1)
	{
		cfgets(Current_level_palette, sizeof(Current_level_palette), LoadFile);
		if (Current_level_palette[strlen(Current_level_palette) - 1] == '\n')
			Current_level_palette[strlen(Current_level_palette) - 1] = 0;
	}

	if (version >= 3)
		Base_control_center_explosion_time = read_int(LoadFile);
	else
		Base_control_center_explosion_time = DEFAULT_CONTROL_CENTER_EXPLOSION_TIME;

	if (version >= 4)
		Reactor_strength = read_int(LoadFile);
	else 
		Reactor_strength = -1;	//use old defaults

	if (version >= 7)
	{
		auto Num_flickering_lights = read_int(LoadFile);
		Flickering_lights.resize(Num_flickering_lights);
		Flickering_lights.shrink_to_fit();
		for (i = 0; i < Num_flickering_lights; i++)
		{
			Flickering_lights[i].segnum = read_short(LoadFile);
			Flickering_lights[i].sidenum = read_short(LoadFile);
			Flickering_lights[i].mask = read_int(LoadFile);
			Flickering_lights[i].timer = read_fix(LoadFile);
			Flickering_lights[i].delay = read_fix(LoadFile);
		}
	}
	else
		Flickering_lights.clear();

	if (currentGame == G_DESCENT_1)
		strcpy(Current_level_palette, "descent.256");
	else if (version <= 1 || Current_level_palette[0] == 0)
		strcpy(Current_level_palette, "groupa.256");

	printf("Current palette: %s\n", Current_level_palette);

	if (version < 6) 
	{
		Secret_return_segment = 0;
		Secret_return_orient.rvec.x = F1_0;	Secret_return_orient.rvec.y = 0;			Secret_return_orient.rvec.z = 0;
		Secret_return_orient.fvec.x = 0;	Secret_return_orient.fvec.y = F1_0;		Secret_return_orient.fvec.z = 0;
		Secret_return_orient.uvec.x = 0;	Secret_return_orient.uvec.y = 0;			Secret_return_orient.uvec.z = F1_0;
	}
	else 
	{
		Secret_return_segment = read_int(LoadFile);
		Secret_return_orient.rvec.x = read_int(LoadFile);
		Secret_return_orient.rvec.y = read_int(LoadFile);
		Secret_return_orient.rvec.z = read_int(LoadFile);
		Secret_return_orient.fvec.x = read_int(LoadFile);
		Secret_return_orient.fvec.y = read_int(LoadFile);
		Secret_return_orient.fvec.z = read_int(LoadFile);
		Secret_return_orient.uvec.x = read_int(LoadFile);
		Secret_return_orient.uvec.y = read_int(LoadFile);
		Secret_return_orient.uvec.z = read_int(LoadFile);
	}

	cfseek(LoadFile, minedata_offset, SEEK_SET);
#ifdef EDITOR
	if (!use_compiled_level) 
	{
		mine_err = load_mine_data(LoadFile);
		//	Compress all uv coordinates in mine, improves texmap precision. --MK, 02/19/96
		compress_uv_coordinates_all();
	}
	else
#endif
		//NOTE LINK TO ABOVE!!
		mine_err = load_mine_data_compiled(LoadFile);

	ResetLavaWalls();

	if (mine_err == -1) 
	{	//error!!
		cfclose(LoadFile);
		return 2;
	}

	printf("Preparing to load_game_data\n");

	cfseek(LoadFile, gamedata_offset, SEEK_SET);
	game_err = load_game_data(LoadFile);

	if (game_err == -1) 
	{	//error!!
		cfclose(LoadFile);
		return 3;
	}

	//======================== CLOSE FILE =============================

	cfclose(LoadFile);

	if (activePiggyTable->gameBitmaps.size() == 1) //only bogus initialized
		load_palette(Current_level_palette, 1, 1);

	if (CurrentDataVersion != DataVer::DEMO)
		set_ambient_sound_flags();

#ifdef EDITOR
	write_game_text_file(filename);
	if (Errors_in_mine)
	{
		if (is_real_level(filename))
		{
			char  ErrorMessage[200];

			snprintf(ErrorMessage, 200, "Warning: %i errors in %s!\n", Errors_in_mine, Level_being_loaded);
			stop_time();
			gr_palette_load(gr_palette);
			nm_messagebox(NULL, 1, "Continue", ErrorMessage);
			start_time();
		}
		else
			mprintf((1, "Error: %i errors in %s.\n", Errors_in_mine, Level_being_loaded));
	}
#endif

#ifdef EDITOR
	//If an old version, ask the use if he wants to save as new version
	if (!no_old_level_file_error && (Function_mode == FMODE_EDITOR) && (((LEVEL_FILE_VERSION > 3) && version < LEVEL_FILE_VERSION) || mine_err == 1 || game_err == 1))
	{
		char  ErrorMessage[200];

		snprintf(ErrorMessage, 200,
			"You just loaded a old version\n"
			"level.  Would you like to save\n"
			"it as a current version level?");

		stop_time();
		gr_palette_load(gr_palette);
		if (nm_messagebox(NULL, 2, "Don't Save", "Save", ErrorMessage) == 1)
			save_level(filename);
		start_time();
	}
#endif

#ifdef EDITOR
	if (Function_mode == FMODE_EDITOR)
		editor_status("Loaded NEW mine %s, \"%s\"", filename, Current_level_name);
#endif

#ifdef EDITOR
	if (check_segment_connections())
		nm_messagebox("ERROR", 1, "Ok",
			"Connectivity errors detected in\n"
			"mine.  See monochrome screen for\n"
			"details, and contact Matt or Mike.");
#endif

	return 0;
}

#ifdef EDITOR
int get_level_name()
{
	//NO_UI!!!	UI_WINDOW 				*NameWindow = NULL;
	//NO_UI!!!	UI_GADGET_INPUTBOX	*NameText;
	//NO_UI!!!	UI_GADGET_BUTTON 		*QuitButton;
	//NO_UI!!!
	//NO_UI!!!	// Open a window with a quit button
	//NO_UI!!!	NameWindow = ui_open_window( 20, 20, 300, 110, WIN_DIALOG );
	//NO_UI!!!	QuitButton = ui_add_gadget_button( NameWindow, 150-24, 60, 48, 40, "Done", NULL );
	//NO_UI!!!
	//NO_UI!!!	ui_wprintf_at( NameWindow, 10, 12,"Please enter a name for this mine:" );
	//NO_UI!!!	NameText = ui_add_gadget_inputbox( NameWindow, 10, 30, LEVEL_NAME_LEN, LEVEL_NAME_LEN, Current_level_name );
	//NO_UI!!!
	//NO_UI!!!	NameWindow->keyboard_focus_gadget = (UI_GADGET *)NameText;
	//NO_UI!!!	QuitButton->hotkey = KEY_ENTER;
	//NO_UI!!!
	//NO_UI!!!	ui_gadget_calc_keys(NameWindow);
	//NO_UI!!!
	//NO_UI!!!	while (!QuitButton->pressed && last_keypress!=KEY_ENTER) {
	//NO_UI!!!		ui_mega_process();
	//NO_UI!!!		ui_window_do_gadgets(NameWindow);
	//NO_UI!!!	}
	//NO_UI!!!
	//NO_UI!!!	strcpy( Current_level_name, NameText->text );
	//NO_UI!!!
	//NO_UI!!!	if ( NameWindow!=NULL )	{
	//NO_UI!!!		ui_close_window( NameWindow );
	//NO_UI!!!		NameWindow = NULL;
	//NO_UI!!!	}
	//NO_UI!!!

	newmenu_item m[2];

	m[0].type = NM_TYPE_TEXT; m[0].text = "Please enter a name for this mine:";
	m[1].type = NM_TYPE_INPUT; m[1].text = Current_level_name; m[1].text_len = LEVEL_NAME_LEN;

	newmenu_do(NULL, "Enter mine name", 2, m, NULL);

	return 0;
}
#endif


#ifdef EDITOR

int	Errors_in_mine;

// -----------------------------------------------------------------------------
void write_game_fileinfo(FILE* fp)
{
	file_write_short(fp, game_fileinfo.fileinfo_signature);
	file_write_short(fp, game_fileinfo.fileinfo_version);
	file_write_int(fp, game_fileinfo.fileinfo_sizeof);
	fwrite(game_fileinfo.mine_filename, sizeof(char), 15, fp);
	file_write_int(fp, game_fileinfo.level);
	file_write_int(fp, game_fileinfo.player_offset);
	file_write_int(fp, game_fileinfo.player_sizeof);
	file_write_int(fp, game_fileinfo.object_offset);
	file_write_int(fp, game_fileinfo.object_howmany);
	file_write_int(fp, game_fileinfo.object_sizeof);
	file_write_int(fp, game_fileinfo.walls_offset);
	file_write_int(fp, game_fileinfo.walls_howmany);
	file_write_int(fp, game_fileinfo.walls_sizeof);
	file_write_int(fp, game_fileinfo.doors_offset);
	file_write_int(fp, game_fileinfo.doors_howmany);
	file_write_int(fp, game_fileinfo.doors_sizeof);
	file_write_int(fp, game_fileinfo.triggers_offset);
	file_write_int(fp, game_fileinfo.triggers_howmany);
	file_write_int(fp, game_fileinfo.triggers_sizeof);
	file_write_int(fp, game_fileinfo.links_offset);
	file_write_int(fp, game_fileinfo.links_howmany);
	file_write_int(fp, game_fileinfo.links_sizeof);
	file_write_int(fp, game_fileinfo.control_offset);
	file_write_int(fp, game_fileinfo.control_howmany);
	file_write_int(fp, game_fileinfo.control_sizeof);
	file_write_int(fp, game_fileinfo.matcen_offset);
	file_write_int(fp, game_fileinfo.matcen_howmany);
	file_write_int(fp, game_fileinfo.matcen_sizeof);
	file_write_int(fp, game_fileinfo.dl_indices_offset);
	file_write_int(fp, game_fileinfo.dl_indices_howmany);
	file_write_int(fp, game_fileinfo.dl_indices_sizeof);
	file_write_int(fp, game_fileinfo.delta_light_offset);
	file_write_int(fp, game_fileinfo.delta_light_howmany);
	file_write_int(fp, game_fileinfo.delta_light_sizeof);
}

void write_dl_index(dl_index* index, FILE* fp)
{
	file_write_short(fp, index->segnum);
	file_write_byte(fp, index->sidenum);
	file_write_byte(fp, index->count);
	file_write_short(fp, index->index);
}

void write_delta_light(delta_light* light, FILE* fp)
{
	file_write_short(fp, light->segnum);
	file_write_byte(fp, light->sidenum);
	file_write_byte(fp, light->dummy);
	file_write_byte(fp, light->vert_light[0]);
	file_write_byte(fp, light->vert_light[1]);
	file_write_byte(fp, light->vert_light[2]);
	file_write_byte(fp, light->vert_light[3]);
}

// -----------------------------------------------------------------------------
int compute_num_delta_light_records(void)
{
	int	i;
	int	total = 0;

	for (i = 0; i < Num_static_lights; i++) 
	{
		total += Dl_indices[i].count;
	}

	return total;
}

// -----------------------------------------------------------------------------
// Save game
int save_game_data(FILE* SaveFile)
{
	int  player_offset, object_offset, walls_offset, doors_offset, triggers_offset, control_offset, matcen_offset; //, links_offset;
	int	dl_indices_offset, delta_light_offset;
	int start_offset, end_offset;
	int i;

	start_offset = ftell(SaveFile);

	//===================== SAVE FILE INFO ========================
	game_fileinfo.fileinfo_signature = 0x6705;
	game_fileinfo.fileinfo_version = GAME_VERSION;
	game_fileinfo.level = Current_level_num;
	game_fileinfo.fileinfo_sizeof = sizeof(game_fileinfo);
	game_fileinfo.player_offset = -1;
	game_fileinfo.player_sizeof = sizeof(player);
	game_fileinfo.object_offset = -1;
	game_fileinfo.object_howmany = Highest_object_index + 1;
	game_fileinfo.object_sizeof = sizeof(object);
	game_fileinfo.walls_offset = -1;
	game_fileinfo.walls_howmany = Num_walls;
	game_fileinfo.walls_sizeof = sizeof(wall);
	game_fileinfo.doors_offset = -1;
	game_fileinfo.doors_howmany = Num_open_doors;
	game_fileinfo.doors_sizeof = sizeof(active_door);
	game_fileinfo.triggers_offset = -1;
	game_fileinfo.triggers_howmany = Num_triggers;
	game_fileinfo.triggers_sizeof = sizeof(trigger);
	game_fileinfo.control_offset = -1;
	game_fileinfo.control_howmany = 1;
	game_fileinfo.control_sizeof = sizeof(control_center_triggers);
	game_fileinfo.matcen_offset = -1;
	game_fileinfo.matcen_howmany = Num_robot_centers;
	game_fileinfo.matcen_sizeof = sizeof(matcen_info);

	game_fileinfo.dl_indices_offset = -1;
	game_fileinfo.dl_indices_howmany = Num_static_lights;
	game_fileinfo.dl_indices_sizeof = sizeof(dl_index);

	game_fileinfo.delta_light_offset = -1;
	game_fileinfo.delta_light_howmany = compute_num_delta_light_records();
	game_fileinfo.delta_light_sizeof = sizeof(delta_light);

	// Write the fileinfo
	write_game_fileinfo(SaveFile);

	// Write the mine name
	fprintf(SaveFile, "%s\n", Current_level_name);

	file_write_short(SaveFile, activeBMTable->models.size());
	fwrite(Pof_names, activeBMTable->models.size(), sizeof(*Pof_names), SaveFile);

	//==================== SAVE PLAYER INFO ===========================

	player_offset = ftell(SaveFile);
	write_player_file(&Players[Player_num], SaveFile);

	//==================== SAVE OBJECT INFO ===========================

	object_offset = ftell(SaveFile);
	//fwrite( &Objects, sizeof(object), game_fileinfo.object_howmany, SaveFile );
	for (i = 0; i < game_fileinfo.object_howmany; i++)
		write_object(&Objects[i], SaveFile);

	//==================== SAVE WALL INFO =============================

	walls_offset = ftell(SaveFile);
	for (i = 0; i < game_fileinfo.walls_howmany; i++)
		write_wall(&Walls[i], SaveFile);

	//==================== SAVE DOOR INFO =============================

	doors_offset = ftell(SaveFile);
	for (i = 0; i < game_fileinfo.doors_howmany; i++)
		write_active_door(&ActiveDoors[i], SaveFile);

	//==================== SAVE TRIGGER INFO =============================

	triggers_offset = ftell(SaveFile);
	for (i = 0; i < game_fileinfo.triggers_howmany; i++)
		write_trigger(&Triggers[i], SaveFile);

	//================ SAVE CONTROL CENTER TRIGGER INFO ===============

	control_offset = ftell(SaveFile);
	write_reactor_triggers(&ControlCenterTriggers, SaveFile);

	//================ SAVE MATERIALIZATION CENTER TRIGGER INFO ===============

	matcen_offset = ftell(SaveFile);
	// mprintf((0, "Writing %i materialization centers\n", game_fileinfo.matcen_howmany));
	// { int i;
	// for (i=0; i<game_fileinfo.matcen_howmany; i++)
	// 	mprintf((0, "   %i: robot_flags = %08x\n", i, RobotCenters[i].robot_flags));
	// }
	for (i = 0; i < game_fileinfo.matcen_howmany; i++)
		write_matcen(&RobotCenters[i], SaveFile);

	//================ SAVE DELTA LIGHT INFO ===============
	dl_indices_offset = ftell(SaveFile);
	for (i = 0; i < game_fileinfo.dl_indices_howmany; i++)
		write_dl_index(&Dl_indices[i], SaveFile);

	delta_light_offset = ftell(SaveFile);
	fwrite(Delta_lights, sizeof(delta_light), game_fileinfo.delta_light_howmany, SaveFile);

	//============= REWRITE FILE INFO, TO SAVE OFFSETS ===============

	// Update the offset fields
	game_fileinfo.player_offset = player_offset;
	game_fileinfo.object_offset = object_offset;
	game_fileinfo.walls_offset = walls_offset;
	game_fileinfo.doors_offset = doors_offset;
	game_fileinfo.triggers_offset = triggers_offset;
	game_fileinfo.control_offset = control_offset;
	game_fileinfo.matcen_offset = matcen_offset;
	game_fileinfo.dl_indices_offset = dl_indices_offset;
	game_fileinfo.delta_light_offset = delta_light_offset;

	end_offset = ftell(SaveFile);

	// Write the fileinfo
	fseek(SaveFile, start_offset, SEEK_SET);  // Move to TOF
	write_game_fileinfo(SaveFile);

	// Go back to end of data
	fseek(SaveFile, end_offset, SEEK_SET);

	return 0;
}

int save_mine_data(FILE* SaveFile);

// -----------------------------------------------------------------------------
// Save game
int save_level_sub(char* filename, int compiled_version)
{
	FILE* SaveFile;
	char temp_filename[128];
	int sig = 'PLVL', version = LEVEL_FILE_VERSION;
	int minedata_offset = 0, gamedata_offset = 0;
	int i;

	if (!compiled_version) 
	{
		write_game_text_file(filename);

		if (Errors_in_mine)
		{
			if (is_real_level(filename))
			{
				char  ErrorMessage[200];

				snprintf(ErrorMessage, 200, "Warning: %i errors in this mine!\n", Errors_in_mine);
				stop_time();
				gr_palette_load(gr_palette);

				if (nm_messagebox(NULL, 2, "Cancel Save", "Save", ErrorMessage) != 1)
				{
					start_time();
					return 1;
				}
				start_time();
			}
			else
				mprintf((1, "Error: %i errors in this mine.  See the 'txm' file.\n", Errors_in_mine));
		}
		change_filename_extension(temp_filename, filename, ".LVL");
	}
	else
	{
		// macs are using the regular hog/rl2 files for shareware
#if defined(SHAREWARE) && !defined(MACINTOSH)
		change_filename_extension(temp_filename, filename, ".SL2");
#else		
		change_filename_extension(temp_filename, filename, ".RL2");
#endif
	}

	SaveFile = fopen(temp_filename, "wb");
	if (!SaveFile)
	{
		char ErrorMessage[256];

		char fname[20];
		_splitpath(temp_filename, NULL, NULL, fname, NULL);

		snprintf(ErrorMessage, 256, \
			"ERROR: Cannot write to '%s'.\nYou probably need to check out a locked\nversion of the file. You should save\nthis under a different filename, and then\ncheck out a locked copy by typing\n\'co -l %s.lvl'\nat the DOS prompt.\n"
			, temp_filename, fname);
		stop_time();
		gr_palette_load(gr_palette);
		nm_messagebox(NULL, 1, "Ok", ErrorMessage);
		start_time();
		return 1;
	}

	if (Current_level_name[0] == 0)
		strcpy(Current_level_name, "Untitled");

	clear_transient_objects(1);		//1 means clear proximity bombs

	compress_objects();		//after this, Highest_object_index == num objects

	//make sure player is in a segment
	if (update_object_seg(&Objects[Players[0].objnum]) == 0) 
	{
		if (ConsoleObject->segnum > Highest_segment_index)
			ConsoleObject->segnum = 0;
		compute_segment_center(&ConsoleObject->pos, &(Segments[ConsoleObject->segnum]));
	}

	fix_object_segs();

	//Write the header

	gs_write_int(sig, SaveFile);
	gs_write_int(version, SaveFile);

	//save placeholders
	gs_write_int(minedata_offset, SaveFile);
	gs_write_int(gamedata_offset, SaveFile);

	//Now write the damn data

	//write the version 8 data (to make file unreadable by 1.0 & 1.1)
	gs_write_int(GameTime, SaveFile);
	gs_write_short(FrameCount, SaveFile);
	gs_write_byte(FrameTime, SaveFile);

	// Write the palette file name
	fprintf(SaveFile, "%s\n", Current_level_palette);

	gs_write_int(Base_control_center_explosion_time, SaveFile);
	gs_write_int(Reactor_strength, SaveFile);

	gs_write_int(Num_flickering_lights, SaveFile);
	for (i = 0; i < Num_flickering_lights; i++)
	{
		gs_write_short(Flickering_lights[i].segnum, SaveFile);
		gs_write_short(Flickering_lights[i].sidenum, SaveFile);
		gs_write_int(Flickering_lights[i].mask, SaveFile);
		gs_write_fix(Flickering_lights[i].timer, SaveFile);
		gs_write_fix(Flickering_lights[i].delay, SaveFile);
	}

	gs_write_int(Secret_return_segment, SaveFile);
	gs_write_int(Secret_return_orient.rvec.x, SaveFile);
	gs_write_int(Secret_return_orient.rvec.y, SaveFile);
	gs_write_int(Secret_return_orient.rvec.z, SaveFile);
	gs_write_int(Secret_return_orient.fvec.x, SaveFile);
	gs_write_int(Secret_return_orient.fvec.y, SaveFile);
	gs_write_int(Secret_return_orient.fvec.z, SaveFile);
	gs_write_int(Secret_return_orient.uvec.x, SaveFile);
	gs_write_int(Secret_return_orient.uvec.y, SaveFile);
	gs_write_int(Secret_return_orient.uvec.z, SaveFile);

	minedata_offset = ftell(SaveFile);
	if (!compiled_version)
		save_mine_data(SaveFile);
	else
		save_mine_data_compiled(SaveFile);
	gamedata_offset = ftell(SaveFile);
	save_game_data(SaveFile);

	fseek(SaveFile, sizeof(sig) + sizeof(version), SEEK_SET);
	gs_write_int(minedata_offset, SaveFile);
	gs_write_int(gamedata_offset, SaveFile);

	//==================== CLOSE THE FILE =============================
	fclose(SaveFile);

	if (!compiled_version)
	{
		if (Function_mode == FMODE_EDITOR)
			editor_status("Saved mine %s, \"%s\"", filename, Current_level_name);
	}

	return 0;

}

extern void compress_uv_coordinates_all(void);

int save_level(char* filename)
{
	int r1;

	// Save normal version...
	r1 = save_level_sub(filename, 0);

	// Save compiled version...
	save_level_sub(filename, 1);

	return r1;
}

#endif	//EDITOR

#ifndef NDEBUG
void dump_mine_info(void)
{
	int	segnum, sidenum;
	fix	min_u, max_u, min_v, max_v, min_l, max_l, max_sl;

	min_u = F1_0 * 1000;
	min_v = min_u;
	min_l = min_u;

	max_u = -min_u;
	max_v = max_u;
	max_l = max_u;

	max_sl = 0;

	for (segnum = 0; segnum <= Highest_segment_index; segnum++) 
	{
		for (sidenum = 0; sidenum < MAX_SIDES_PER_SEGMENT; sidenum++) 
		{
			int	vertnum;
			side* sidep = &Segments[segnum].sides[sidenum];

			if (Segment2s[segnum].static_light > max_sl)
				max_sl = Segment2s[segnum].static_light;

			for (vertnum = 0; vertnum < 4; vertnum++) 
			{
				if (sidep->uvls[vertnum].u < min_u)
					min_u = sidep->uvls[vertnum].u;
				else if (sidep->uvls[vertnum].u > max_u)
					max_u = sidep->uvls[vertnum].u;

				if (sidep->uvls[vertnum].v < min_v)
					min_v = sidep->uvls[vertnum].v;
				else if (sidep->uvls[vertnum].v > max_v)
					max_v = sidep->uvls[vertnum].v;

				if (sidep->uvls[vertnum].l < min_l)
					min_l = sidep->uvls[vertnum].l;
				else if (sidep->uvls[vertnum].l > max_l)
					max_l = sidep->uvls[vertnum].l;
			}
		}
	}

	//	mprintf((0, "Smallest uvl = %7.3f %7.3f %7.3f.  Largest uvl = %7.3f %7.3f %7.3f\n", f2fl(min_u), f2fl(min_v), f2fl(min_l), f2fl(max_u), f2fl(max_v), f2fl(max_l)));
	//	mprintf((0, "Static light maximum = %7.3f\n", f2fl(max_sl)));
	//	mprintf((0, "Number of walls: %i\n", Num_walls));

}

#endif

#ifdef EDITOR

//read in every level in mission and save out compiled version 
void save_all_compiled_levels(void)
{
	do_load_save_levels(1);
}

//read in every level in mission
void load_all_levels(void)
{
	do_load_save_levels(0);
}

void do_load_save_levels(int save)
{
	int level_num;

	if (!SafetyCheck())
		return;

	no_old_level_file_error = 1;

	for (level_num = 1; level_num <= Last_level; level_num++)
	{
		load_level(Level_names[level_num - 1]);
		load_palette(Current_level_palette, 1, 1);		//don't change screen
		if (save)
			save_level_sub(Level_names[level_num - 1], 1);
	}

	for (level_num = -1; level_num >= Last_secret_level; level_num--) 
	{
		load_level(Secret_level_names[-level_num - 1]);
		load_palette(Current_level_palette, 1, 1);		//don't change screen
		if (save)
			save_level_sub(Secret_level_names[-level_num - 1], 1);
	}

	no_old_level_file_error = 0;
}

#endif

