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

#include <stdio.h>

#include <vector>

#include "2d/gr.h"
#include "piggy.h"
#include "polyobj.h"
#include "vclip.h"
#include "effects.h"
#include "wall.h"
#include "weapon.h"
#include "powerup.h"
#include "player.h"
#include "cntrlcen.h"

#define SANTA

#define MAX_TEXTURES_D1 800
#define MAX_TEXTURES_D2 1200

#define MAX_TEXTURES		MAX_TEXTURES_D2

#define BM_MAX_ARGS			10

//tmapinfo flags
#define TMI_VOLATILE		1		//this material blows up when hit
#define TMI_WATER			2		//this material is water
#define TMI_FORCE_FIELD	4		//this is force field - flares don't stick
#define TMI_GOAL_BLUE	8		//this is used to remap the blue goal
#define TMI_GOAL_RED		16		//this is used to remap the red goal
#define TMI_GOAL_HOARD	32		//this is used to remap the goals

typedef struct
{
	uint8_t		flags;				//values defined above
	uint8_t		pad[3];				//keep alignment
	fix		lighting;			//how much light this casts
	fix		damage;				//how much damage being against this does (for lava)
	short		eclip_num;			//the eclip that changes this, or -1
	short		destroyed;			//bitmap to show when destroyed, or -1
	short		slide_u,slide_v;	//slide rates of texture, stored in 8:8 fix
	char		filename[13];		//used by editor to remap textures
} tmap_info;

extern int Num_object_types;

#define N_COCKPIT_BITMAPS_D1 4
#define N_COCKPIT_BITMAPS_D2 6

#define N_COCKPIT_BITMAPS N_COCKPIT_BITMAPS_D2

#ifdef EDITOR
extern int TmapList[MAX_TEXTURES];
void bm_write_all(FILE* fp); //[ISB] for piggy.cpp
#endif

//for each model, a model number for dying & dead variants, or -1 if none

//the model number of the marker object
//extern int Marker_model_num;

// Initializes the palette, bitmap system...
int bm_init();
void bm_close();

// Initializes the Texture[] array of bmd_bitmap structures.
void init_textures();

#define OL_ROBOT 				1
#define OL_HOSTAGE 			2
#define OL_POWERUP 			3
#define OL_CONTROL_CENTER	4
#define OL_PLAYER				5
#define OL_CLUTTER			6		//some sort of misc object
#define OL_EXIT				7		//the exit model for external scenes
#define OL_WEAPON				8		//a weapon that can be placed

#define MAX_OBJTYPE_D1 100
#define MAX_OBJTYPE_D2 140

#define	MAX_OBJTYPE			MAX_OBJTYPE_D2

extern int Num_total_object_types;		//	Total number of object types, including robots, hostages, powerups, control centers, faces
extern int8_t	ObjType[MAX_OBJTYPE];		// Type of an object, such as Robot, eg if ObjType[11] == OL_ROBOT, then object #11 is a robot
extern int8_t	ObjId[MAX_OBJTYPE];			// ID of a robot, within its class, eg if ObjType[11] == 3, then object #11 is the third robot
extern fix	ObjStrength[MAX_OBJTYPE];	// initial strength of each object

#define MAX_OBJ_BITMAPS_D1 210
#define MAX_OBJ_BITMAPS_D2 600

#define MAX_OBJ_BITMAPS MAX_OBJ_BITMAPS_D2

struct bmtable {

	std::vector<bitmap_index> cockpits;
	std::vector<tmap_info> tmaps;
	std::vector<bitmap_index> objectBitmaps;
	std::vector<uint16_t> objectBitmapPointers;

	std::vector<bitmap_index> textures;
	std::vector<polymodel> models;
	std::vector<uint8_t> sounds;
	std::vector<uint8_t> altSounds;

	std::vector<vclip> vclips;
	std::vector<eclip> eclips;
	std::vector<wclip> wclips;

	std::vector<robot_info> robots;
	std::vector<jointpos> robotJoints;

	std::vector<weapon_info> weapons;
	std::vector<powerup_type_info> powerups;

	std::vector<reactor> reactors;

	std::vector<bitmap_index> gauges;
	std::vector<bitmap_index> hiresGauges;

	std::vector<int> dyingModels;
	std::vector<int> deadModels;

	std::vector<vms_vector> reactorGunPos;
	std::vector<vms_vector> reactorGunDirs;

	player_ship player;

	int exitModel;
	int destroyedExitModel;
	int markerModel;

	int firstMultiBitmapNum;

	uint8_t game;

	piggytable piggy;

	bmtable();
	void Init();
	~bmtable();
};

/*extern bmtable d1Table;
extern bmtable d2Table;*/

extern bmtable* activeBMTable;

// Initializes all bitmaps from BITMAPS.TBL file.
int bm_init_use_tbl(void);

//[ISB] Functions for writing data structures, for the editor.
void write_tmap_info(FILE* fp);
void write_vclip_info(FILE* fp);
void write_effect_info(FILE* fp);
void write_wallanim_info(FILE* fp);
void write_robot_info(FILE* fp);
void write_robot_joints_info(FILE* fp);
void write_weapon_info(FILE* fp);
void write_powerup_info(FILE* fp);
void write_polygon_models(FILE* fp);
void write_player_ship(FILE* fp);