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

#include "misc/types.h"
#include "inferno.h"
#include "2d/gr.h"
#include "3d/3d.h"
#include "bm.h"
#include "mem/mem.h"
#include "platform/mono.h"
#include "misc/error.h"
#include "object.h"
#include "vclip.h"
#include "effects.h"
#include "polyobj.h"
#include "wall.h"
#include "textures.h"
#include "game.h"
#include "multi.h"
#include "iff/iff.h"
#include "cfile/cfile.h"
#include "powerup.h"
#include "sounds.h"
#include "piggy.h"
#include "aistruct.h"
#include "robot.h"
#include "weapon.h"
#include "gauges.h"
#include "player.h"
#include "endlevel.h"
#include "cntrlcen.h"
#include "misc/byteswap.h"
#include "laser.h"

//uint8_t Sounds[MAX_SOUNDS];
//uint8_t AltSounds[MAX_SOUNDS];

int Num_total_object_types;
int8_t	ObjType[MAX_OBJTYPE];
int8_t	ObjId[MAX_OBJTYPE];
fix	ObjStrength[MAX_OBJTYPE];

//for each model, a model number for dying & dead variants, or -1 if none
//int Dying_modelnums[MAX_POLYGON_MODELS];
//int Dead_modelnums[MAX_POLYGON_MODELS];

//the polygon model number to use for the marker
//int	Marker_model_num = -1;

//right now there's only one player ship, but we can have another by 
//adding an array and setting the pointer to the active ship.
//player_ship only_player_ship,*Player_ship=&only_player_ship;
player_ship* Player_ship;

//----------------- Miscellaneous bitmap pointers ---------------
//int					Num_cockpits = 0;
//bitmap_index		cockpit_bitmap[N_COCKPIT_BITMAPS];

//---------------- Variables for wall textures ------------------
//int 					Num_tmaps;
//tmap_info 			TmapInfo[MAX_TEXTURES];

//---------------- Variables for object textures ----------------

//int					First_multi_bitmap_num=-1;

//bitmap_index		ObjBitmaps[MAX_OBJ_BITMAPS];
//uint16_t			ObjBitmapPtrs[MAX_OBJ_BITMAPS];		// These point back into ObjBitmaps, since some are used twice.

bmtable d1Table;
bmtable d2Table;

//bmtable::bmtable() {
//	Init();
//}

#define READ_LIMIT(d1, d2) (currentGame == G_DESCENT_1 ? (d1) : (d2))

void bmtable::Init(bool reinit) {

	if (initialized && !reinit)
		return;

	initialized = true;

	cockpits.reserve(N_COCKPIT_BITMAPS);
	tmaps.reserve(MAX_TEXTURES);
	objectBitmaps.reserve(MAX_OBJ_BITMAPS);
	objectBitmapPointers.reserve(MAX_OBJ_BITMAPS);
	models.reserve(MAX_POLYGON_MODELS);
	textures.reserve(MAX_TEXTURES);
	sounds.reserve(MAX_SOUNDS);
	altSounds.reserve(MAX_SOUNDS);
	vclips.reserve(VCLIP_MAXNUM);
	eclips.reserve(MAX_EFFECTS);
	wclips.reserve(MAX_WALL_ANIMS);
	robots.reserve(MAX_ROBOT_TYPES);
	weapons.reserve(MAX_WEAPON_TYPES);
	powerups.reserve(MAX_POWERUP_TYPES);
	reactors.reserve(MAX_REACTORS);
	gauges.reserve(MAX_GAUGE_BMS);
	hiresGauges.reserve(MAX_GAUGE_BMS);
	dyingModels.reserve(MAX_POLYGON_MODELS);
	deadModels.reserve(MAX_POLYGON_MODELS);
	reactorGunPos.reserve(MAX_CONTROLCEN_GUNS);
	reactorGunDirs.reserve(MAX_CONTROLCEN_GUNS);

	piggy.Init();
}

void bmtable::SetActive() {
	activeBMTable = this;
	piggy.SetActive();

	Player_ship = &player;
}

extern void free_polygon_models();

bmtable::~bmtable() {
	activeBMTable = this;
	free_polygon_models();
}

void read_tmap_info(CFILE *fp, int inNumTexturesToRead, int inOffset) {

	int i;
	for (i = inOffset; i < (inNumTexturesToRead + inOffset); i++) {
		tmap_info tmap;
		if (currentGame == G_DESCENT_1) {
			cfread(&tmap.filename[0], 13, 1, fp);
			tmap.flags = cfile_read_byte(fp);
			tmap.lighting = cfile_read_fix(fp);
			tmap.damage = cfile_read_fix(fp);
			tmap.eclip_num = cfile_read_int(fp);
			tmap.destroyed = -1;
			tmap.slide_u = 0;
			tmap.slide_v = 0;
		} else {
			tmap.flags = cfile_read_byte(fp);
			tmap.pad[0] = cfile_read_byte(fp);
			tmap.pad[1] = cfile_read_byte(fp);
			tmap.pad[2] = cfile_read_byte(fp);
			tmap.lighting = cfile_read_fix(fp);
			tmap.damage = cfile_read_fix(fp);
			tmap.eclip_num = cfile_read_short(fp);
			tmap.destroyed = cfile_read_short(fp);
			tmap.slide_u = cfile_read_short(fp);
			tmap.slide_v = cfile_read_short(fp);
		}
		activeBMTable->tmaps.push_back(std::move(tmap));
	}
}

void read_vclip_info(CFILE *fp, int inNumVClipsToRead, int inOffset)
{
	int i, j;

	if (activeBMTable->vclips.size() < inOffset + inNumVClipsToRead)
		activeBMTable->vclips.resize(inOffset + inNumVClipsToRead);
	
	for (i = inOffset; i < (inNumVClipsToRead + inOffset); i++)
	{
		activeBMTable->vclips[i].play_time = cfile_read_fix(fp);
		activeBMTable->vclips[i].num_frames = cfile_read_int(fp);
		activeBMTable->vclips[i].frame_time = cfile_read_fix(fp);
		activeBMTable->vclips[i].flags = cfile_read_int(fp);
		activeBMTable->vclips[i].sound_num = cfile_read_short(fp);
		for (j = 0; j < VCLIP_MAX_FRAMES; j++)
			activeBMTable->vclips[i].frames[j].index = cfile_read_short(fp);
		activeBMTable->vclips[i].light_value = cfile_read_fix(fp);
	}

}

void read_effect_info(CFILE *fp, int inNumEffectsToRead, int inOffset)
{
	int i, j;

	if (activeBMTable->eclips.size() < inOffset + inNumEffectsToRead)
		activeBMTable->eclips.resize(inOffset + inNumEffectsToRead);

	for (i = inOffset; i < (inNumEffectsToRead + inOffset); i++)
	{
		activeBMTable->eclips[i].vc.play_time = cfile_read_fix(fp);
		activeBMTable->eclips[i].vc.num_frames = cfile_read_int(fp);
		activeBMTable->eclips[i].vc.frame_time = cfile_read_fix(fp);
		activeBMTable->eclips[i].vc.flags = cfile_read_int(fp);
		activeBMTable->eclips[i].vc.sound_num = cfile_read_short(fp);
		for (j = 0; j < VCLIP_MAX_FRAMES; j++) 
			activeBMTable->eclips[i].vc.frames[j].index = cfile_read_short(fp);
		activeBMTable->eclips[i].vc.light_value = cfile_read_fix(fp);
		activeBMTable->eclips[i].time_left = cfile_read_fix(fp);
		activeBMTable->eclips[i].frame_count = cfile_read_int(fp);
		activeBMTable->eclips[i].changing_wall_texture = cfile_read_short(fp);
		activeBMTable->eclips[i].changing_object_texture = cfile_read_short(fp);
		activeBMTable->eclips[i].flags = cfile_read_int(fp);
		activeBMTable->eclips[i].crit_clip = cfile_read_int(fp);
		activeBMTable->eclips[i].dest_bm_num = cfile_read_int(fp);
		activeBMTable->eclips[i].dest_vclip = cfile_read_int(fp);
		activeBMTable->eclips[i].dest_eclip = cfile_read_int(fp);
		activeBMTable->eclips[i].dest_size = cfile_read_fix(fp);
		activeBMTable->eclips[i].sound_num = cfile_read_int(fp);
		activeBMTable->eclips[i].segnum = cfile_read_int(fp);
		activeBMTable->eclips[i].sidenum = cfile_read_int(fp);
	}
}

void read_wallanim_info(CFILE *fp, int inNumWallAnimsToRead, int inOffset)
{
	int i, j;

	if (activeBMTable->wclips.size() < inNumWallAnimsToRead + inOffset)
		activeBMTable->wclips.resize(inNumWallAnimsToRead + inOffset);
	
	for (i = inOffset; i < (inNumWallAnimsToRead + inOffset); i++)
	{
		activeBMTable->wclips[i].play_time = cfile_read_fix(fp);;
		activeBMTable->wclips[i].num_frames = cfile_read_short(fp);;
		for (j = 0; j < READ_LIMIT(MAX_CLIP_FRAMES_D1, MAX_CLIP_FRAMES_D2); j++)
			activeBMTable->wclips[i].frames[j] = cfile_read_short(fp);
		activeBMTable->wclips[i].open_sound = cfile_read_short(fp);
		activeBMTable->wclips[i].close_sound = cfile_read_short(fp);
		activeBMTable->wclips[i].flags = cfile_read_short(fp);
		cfread(activeBMTable->wclips[i].filename, 13, 1, fp);
		activeBMTable->wclips[i].pad = cfile_read_byte(fp);
	}		
}

void ReadRobotD1(CFILE* fp, int inNumRobotsToRead, int inOffset) {
	
	int i, j, k;
	
	for (i = inOffset; i < (inNumRobotsToRead + inOffset); i++)
	{
		activeBMTable->robots[i].model_num = cfile_read_int(fp);
		activeBMTable->robots[i].n_guns = cfile_read_int(fp);
		for (j = 0; j < MAX_GUNS; j++) 
		{
			activeBMTable->robots[i].gun_points[j].x = cfile_read_fix(fp);
			activeBMTable->robots[i].gun_points[j].y = cfile_read_fix(fp);
			activeBMTable->robots[i].gun_points[j].z = cfile_read_fix(fp);
		}
		for (j = 0; j < MAX_GUNS; j++)
			activeBMTable->robots[i].gun_submodels[j] = cfile_read_byte(fp);
		activeBMTable->robots[i].exp1_vclip_num = cfile_read_short(fp);
		activeBMTable->robots[i].exp1_sound_num = cfile_read_short(fp);
		activeBMTable->robots[i].exp2_vclip_num = cfile_read_short(fp);
		activeBMTable->robots[i].exp2_sound_num = cfile_read_short(fp);
		activeBMTable->robots[i].weapon_type = cfile_read_short(fp);
		activeBMTable->robots[i].contains_id = cfile_read_byte(fp);
		activeBMTable->robots[i].contains_count = cfile_read_byte(fp);
		activeBMTable->robots[i].contains_prob = cfile_read_byte(fp);
		activeBMTable->robots[i].contains_type = cfile_read_byte(fp);
		activeBMTable->robots[i].score_value = cfile_read_int(fp);
		activeBMTable->robots[i].lighting = cfile_read_fix(fp);
		activeBMTable->robots[i].strength = cfile_read_fix(fp);
		activeBMTable->robots[i].mass = cfile_read_fix(fp);
		activeBMTable->robots[i].drag = cfile_read_fix(fp);
		for (j = 0; j < NDL; j++)
			activeBMTable->robots[i].field_of_view[j] = cfile_read_fix(fp);
		for (j = 0; j < NDL; j++)
			activeBMTable->robots[i].firing_wait[j] = cfile_read_fix(fp);
		for (j = 0; j < NDL; j++)
			activeBMTable->robots[i].turn_time[j] = cfile_read_fix(fp);
		for (j = 0; j < NDL; j++)
			activeBMTable->robots[i].fire_power[j] = cfile_read_fix(fp);
		for (j = 0; j < NDL; j++)
			activeBMTable->robots[i].shield[j] = cfile_read_fix(fp);
		for (j = 0; j < NDL; j++)
			activeBMTable->robots[i].max_speed[j] = cfile_read_fix(fp);
		for (j = 0; j < NDL; j++)
			activeBMTable->robots[i].circle_distance[j] = cfile_read_fix(fp);
		for (j = 0; j < NDL; j++)
			activeBMTable->robots[i].rapidfire_count[j] = cfile_read_byte(fp);
		for (j = 0; j < NDL; j++)
			activeBMTable->robots[i].evade_speed[j] = cfile_read_byte(fp);
		activeBMTable->robots[i].cloak_type = cfile_read_byte(fp);
		activeBMTable->robots[i].attack_type = cfile_read_byte(fp);
		activeBMTable->robots[i].boss_flag = cfile_read_byte(fp);
		activeBMTable->robots[i].see_sound = cfile_read_byte(fp);
		activeBMTable->robots[i].attack_sound = cfile_read_byte(fp);
		activeBMTable->robots[i].claw_sound = cfile_read_byte(fp);

		for (j = 0; j < MAX_GUNS + 1; j++) 
		{
			for (k = 0; k < N_ANIM_STATES; k++) 
			{
				activeBMTable->robots[i].anim_states[j][k].n_joints = cfile_read_short(fp);
				activeBMTable->robots[i].anim_states[j][k].offset = cfile_read_short(fp);
			}
		}

		activeBMTable->robots[i].always_0xabcd = cfile_read_int(fp);

		if (i == 10) //gopher
			activeBMTable->robots[i].behavior = AIB_RUN_FROM;
		else
			activeBMTable->robots[i].behavior = AIB_NORMAL;

		if (activeBMTable->robots[i].boss_flag) {
			activeBMTable->robots[i].deathroll_sound = SOUND_BOSS_SHARE_DIE;
			activeBMTable->robots[i].see_sound = SOUND_BOSS_SHARE_SEE;
			activeBMTable->robots[i].attack_sound = SOUND_BOSS_SHARE_ATTACK;
		}

		activeBMTable->robots[i].companion = 0;
		activeBMTable->robots[i].thief = 0;
		activeBMTable->robots[i].kamikaze = 0;
		activeBMTable->robots[i].badass = 0;
		activeBMTable->robots[i].weapon_type2 = -1;
	}
}

void ReadRobotD2(CFILE* fp, int inNumRobotsToRead, int inOffset) {
	
	int i, j, k;
	
	for (i = inOffset; i < (inNumRobotsToRead + inOffset); i++)
	{
		activeBMTable->robots[i].model_num = cfile_read_int(fp);
		for (j = 0; j < MAX_GUNS; j++)
			cfile_read_vector(&(activeBMTable->robots[i].gun_points[j]), fp);
		for (j = 0; j < MAX_GUNS; j++)
			activeBMTable->robots[i].gun_submodels[j] = cfile_read_byte(fp);

		activeBMTable->robots[i].exp1_vclip_num = cfile_read_short(fp);
		activeBMTable->robots[i].exp1_sound_num = cfile_read_short(fp);

		activeBMTable->robots[i].exp2_vclip_num = cfile_read_short(fp);
		activeBMTable->robots[i].exp2_sound_num = cfile_read_short(fp);

		activeBMTable->robots[i].weapon_type = cfile_read_byte(fp);
		activeBMTable->robots[i].weapon_type2 = cfile_read_byte(fp);
		activeBMTable->robots[i].n_guns = cfile_read_byte(fp);
		activeBMTable->robots[i].contains_id = cfile_read_byte(fp);

		activeBMTable->robots[i].contains_count = cfile_read_byte(fp);
		activeBMTable->robots[i].contains_prob = cfile_read_byte(fp);
		activeBMTable->robots[i].contains_type = cfile_read_byte(fp);
		activeBMTable->robots[i].kamikaze = cfile_read_byte(fp);

		activeBMTable->robots[i].score_value = cfile_read_short(fp);
		activeBMTable->robots[i].badass = cfile_read_byte(fp);
		activeBMTable->robots[i].energy_drain = cfile_read_byte(fp);
		
		activeBMTable->robots[i].lighting = cfile_read_fix(fp);
		activeBMTable->robots[i].strength = cfile_read_fix(fp);

		activeBMTable->robots[i].mass = cfile_read_fix(fp);
		activeBMTable->robots[i].drag = cfile_read_fix(fp);

		for (j = 0; j < NDL; j++)
			activeBMTable->robots[i].field_of_view[j] = cfile_read_fix(fp);
		for (j = 0; j < NDL; j++)
			activeBMTable->robots[i].firing_wait[j] = cfile_read_fix(fp);
		for (j = 0; j < NDL; j++)
			activeBMTable->robots[i].firing_wait2[j] = cfile_read_fix(fp);
		for (j = 0; j < NDL; j++)
			activeBMTable->robots[i].turn_time[j] = cfile_read_fix(fp);
		for (j = 0; j < NDL; j++)
			activeBMTable->robots[i].max_speed[j] = cfile_read_fix(fp);
		for (j = 0; j < NDL; j++)
			activeBMTable->robots[i].circle_distance[j] = cfile_read_fix(fp);
		for (j = 0; j < NDL; j++)
			cfread(&(activeBMTable->robots[i].rapidfire_count[j]), sizeof(int8_t), 1, fp);
		for (j = 0; j < NDL; j++)
			cfread(&(activeBMTable->robots[i].evade_speed[j]), sizeof(int8_t), 1, fp);
		activeBMTable->robots[i].cloak_type = cfile_read_byte(fp);
		activeBMTable->robots[i].attack_type = cfile_read_byte(fp);

		activeBMTable->robots[i].see_sound = cfile_read_byte(fp);
		activeBMTable->robots[i].attack_sound = cfile_read_byte(fp);
		activeBMTable->robots[i].claw_sound = cfile_read_byte(fp);
		activeBMTable->robots[i].taunt_sound = cfile_read_byte(fp);

		activeBMTable->robots[i].boss_flag = cfile_read_byte(fp);
		activeBMTable->robots[i].companion = cfile_read_byte(fp);
		activeBMTable->robots[i].smart_blobs = cfile_read_byte(fp);
		activeBMTable->robots[i].energy_blobs = cfile_read_byte(fp);

		activeBMTable->robots[i].thief = cfile_read_byte(fp);
		activeBMTable->robots[i].pursuit = cfile_read_byte(fp);
		activeBMTable->robots[i].lightcast = cfile_read_byte(fp);
		activeBMTable->robots[i].death_roll = cfile_read_byte(fp);

		activeBMTable->robots[i].flags = cfile_read_byte(fp);
		activeBMTable->robots[i].pad[0] = cfile_read_byte(fp);
		activeBMTable->robots[i].pad[1] = cfile_read_byte(fp);
		activeBMTable->robots[i].pad[2] = cfile_read_byte(fp);

		activeBMTable->robots[i].deathroll_sound = cfile_read_byte(fp);
		activeBMTable->robots[i].glow = cfile_read_byte(fp);
		activeBMTable->robots[i].behavior = cfile_read_byte(fp);
		activeBMTable->robots[i].aim = cfile_read_byte(fp);

		for (j = 0; j < MAX_GUNS + 1; j++) 
		{
			for (k = 0; k < N_ANIM_STATES; k++) 
			{
				activeBMTable->robots[i].anim_states[j][k].n_joints = cfile_read_short(fp);
				activeBMTable->robots[i].anim_states[j][k].offset = cfile_read_short(fp);
			}
		}

		activeBMTable->robots[i].always_0xabcd = cfile_read_int(fp);
	}
}

void read_robot_info(CFILE *fp, int inNumRobotsToRead, int inOffset) {

	if (activeBMTable->robots.size() < inNumRobotsToRead + inOffset)
		activeBMTable->robots.resize(inNumRobotsToRead + inOffset);

	if (currentGame == G_DESCENT_1)
		ReadRobotD1(fp, inNumRobotsToRead, inOffset);
	else
		ReadRobotD2(fp, inNumRobotsToRead, inOffset);
}

void read_robot_joint_info(CFILE *fp, int inNumRobotJointsToRead, int inOffset)
{
	int i;

	if (activeBMTable->robotJoints.size() < inNumRobotJointsToRead + inOffset)
		activeBMTable->robotJoints.resize(inNumRobotJointsToRead + inOffset);

	for (i = inOffset; i < (inNumRobotJointsToRead + inOffset); i++)
	{
		activeBMTable->robotJoints[i].jointnum = cfile_read_short(fp);
		cfile_read_angvec(&(activeBMTable->robotJoints[i].angles), fp);
	}
}

void ReadWeaponD1(CFILE *fp, int inNumWeaponsToRead, int inOffset)
{
	int i, j;

	for (i = inOffset; i < (inNumWeaponsToRead + inOffset); i++)
	{
		activeBMTable->weapons[i].render_type = cfile_read_byte(fp);
		activeBMTable->weapons[i].model_num = (int8_t)cfile_read_byte(fp); //For unified weapon struct, bash to signed first so that it converts to short correctly
		activeBMTable->weapons[i].model_num_inner = (int8_t)cfile_read_byte(fp);
		activeBMTable->weapons[i].persistent = cfile_read_byte(fp);
		activeBMTable->weapons[i].flash_vclip = cfile_read_byte(fp);
		activeBMTable->weapons[i].flash_sound = cfile_read_short(fp);
		activeBMTable->weapons[i].robot_hit_vclip = cfile_read_byte(fp);
		activeBMTable->weapons[i].robot_hit_sound = cfile_read_short(fp);
		activeBMTable->weapons[i].wall_hit_vclip = cfile_read_byte(fp);
		activeBMTable->weapons[i].wall_hit_sound = cfile_read_short(fp);
		activeBMTable->weapons[i].fire_count = cfile_read_byte(fp);
		activeBMTable->weapons[i].ammo_usage = cfile_read_byte(fp);
		activeBMTable->weapons[i].weapon_vclip = cfile_read_byte(fp);
		activeBMTable->weapons[i].destroyable = cfile_read_byte(fp);
		activeBMTable->weapons[i].matter = cfile_read_byte(fp);
		activeBMTable->weapons[i].bounce = cfile_read_byte(fp);
		activeBMTable->weapons[i].homing_flag = cfile_read_byte(fp);
		activeBMTable->weapons[i].dum1 = cfile_read_byte(fp);
		activeBMTable->weapons[i].dum2 = cfile_read_byte(fp);
		activeBMTable->weapons[i].dum3 = cfile_read_byte(fp);
		activeBMTable->weapons[i].energy_usage = cfile_read_fix(fp);
		activeBMTable->weapons[i].fire_wait = cfile_read_fix(fp);
		activeBMTable->weapons[i].bitmap.index = cfile_read_short(fp);	// bitmap_index = short
		activeBMTable->weapons[i].blob_size = cfile_read_fix(fp);
		activeBMTable->weapons[i].flash_size = cfile_read_fix(fp);
		activeBMTable->weapons[i].impact_size = cfile_read_fix(fp);
		for (j = 0; j < NDL; j++)
			activeBMTable->weapons[i].strength[j] = cfile_read_fix(fp);
		for (j = 0; j < NDL; j++)
			activeBMTable->weapons[i].speed[j] = cfile_read_fix(fp);
		activeBMTable->weapons[i].mass = cfile_read_fix(fp);
		activeBMTable->weapons[i].drag = cfile_read_fix(fp);
		activeBMTable->weapons[i].thrust = cfile_read_fix(fp);

		activeBMTable->weapons[i].po_len_to_width_ratio = cfile_read_fix(fp);
		activeBMTable->weapons[i].light = cfile_read_fix(fp);
		activeBMTable->weapons[i].lifetime = cfile_read_fix(fp);
		activeBMTable->weapons[i].damage_radius = cfile_read_fix(fp);
		activeBMTable->weapons[i].picture.index = cfile_read_short(fp);		// bitmap_index is a short

		activeBMTable->weapons[i].children = -1;
		if (i == SMART_ID)
			activeBMTable->weapons[i].children = PLAYER_SMART_HOMING_ID;

	}
}

void ReadWeaponD2(CFILE *fp, int inNumWeaponsToRead, int inOffset)
{
	int i, j;
	
	for (i = inOffset; i < (inNumWeaponsToRead + inOffset); i++)
	{
		activeBMTable->weapons[i].render_type = cfile_read_byte(fp);
		activeBMTable->weapons[i].persistent = cfile_read_byte(fp);
		activeBMTable->weapons[i].model_num = cfile_read_short(fp);
		activeBMTable->weapons[i].model_num_inner = cfile_read_short(fp);

		activeBMTable->weapons[i].flash_vclip = cfile_read_byte(fp);
		activeBMTable->weapons[i].robot_hit_vclip = cfile_read_byte(fp);
		activeBMTable->weapons[i].flash_sound = cfile_read_short(fp);		

		activeBMTable->weapons[i].wall_hit_vclip = cfile_read_byte(fp);
		activeBMTable->weapons[i].fire_count = cfile_read_byte(fp);
		activeBMTable->weapons[i].robot_hit_sound = cfile_read_short(fp);
		
		activeBMTable->weapons[i].ammo_usage = cfile_read_byte(fp);
		activeBMTable->weapons[i].weapon_vclip = cfile_read_byte(fp);
		activeBMTable->weapons[i].wall_hit_sound = cfile_read_short(fp);		

		activeBMTable->weapons[i].destroyable = cfile_read_byte(fp);
		activeBMTable->weapons[i].matter = cfile_read_byte(fp);
		activeBMTable->weapons[i].bounce = cfile_read_byte(fp);
		activeBMTable->weapons[i].homing_flag = cfile_read_byte(fp);

		activeBMTable->weapons[i].speedvar = cfile_read_byte(fp);
		activeBMTable->weapons[i].flags = cfile_read_byte(fp);
		activeBMTable->weapons[i].flash = cfile_read_byte(fp);
		activeBMTable->weapons[i].afterburner_size = cfile_read_byte(fp);
		
		if (CurrentDataVersion == DataVer::DEMO)
			activeBMTable->weapons[i].children = -1;
		else
			activeBMTable->weapons[i].children = cfile_read_byte(fp);

		activeBMTable->weapons[i].energy_usage = cfile_read_fix(fp);
		activeBMTable->weapons[i].fire_wait = cfile_read_fix(fp);
		
		if (CurrentDataVersion == DataVer::DEMO)
			activeBMTable->weapons[i].multi_damage_scale = F1_0;
		else
			activeBMTable->weapons[i].multi_damage_scale = cfile_read_fix(fp);
		
		activeBMTable->weapons[i].bitmap.index = cfile_read_short(fp);	// bitmap_index = short

		activeBMTable->weapons[i].blob_size = cfile_read_fix(fp);
		activeBMTable->weapons[i].flash_size = cfile_read_fix(fp);
		activeBMTable->weapons[i].impact_size = cfile_read_fix(fp);
		for (j = 0; j < NDL; j++)
			activeBMTable->weapons[i].strength[j] = cfile_read_fix(fp);
		for (j = 0; j < NDL; j++)
			activeBMTable->weapons[i].speed[j] = cfile_read_fix(fp);
		activeBMTable->weapons[i].mass = cfile_read_fix(fp);
		activeBMTable->weapons[i].drag = cfile_read_fix(fp);
		activeBMTable->weapons[i].thrust = cfile_read_fix(fp);
		activeBMTable->weapons[i].po_len_to_width_ratio = cfile_read_fix(fp);
		activeBMTable->weapons[i].light = cfile_read_fix(fp);
		activeBMTable->weapons[i].lifetime = cfile_read_fix(fp);
		activeBMTable->weapons[i].damage_radius = cfile_read_fix(fp);
		activeBMTable->weapons[i].picture.index = cfile_read_short(fp);		// bitmap_index is a short
		if (CurrentDataVersion == DataVer::DEMO)
			activeBMTable->weapons[i].hires_picture.index = 0;
		else
			activeBMTable->weapons[i].hires_picture.index = cfile_read_short(fp);		// bitmap_index is a short
	}
}

void read_weapon_info(CFILE* fp, int inNumWeaponsToRead, int inOffset) {

	if (activeBMTable->weapons.size() < inNumWeaponsToRead + inOffset)
		activeBMTable->weapons.resize(inNumWeaponsToRead + inOffset);

	if (currentGame == G_DESCENT_1)
		ReadWeaponD1(fp, inNumWeaponsToRead, inOffset);
	else
		ReadWeaponD2(fp, inNumWeaponsToRead, inOffset);

}

void read_powerup_info(CFILE *fp, int inNumPowerupsToRead, int inOffset)
{
	int i;

	if (activeBMTable->powerups.size() < inNumPowerupsToRead + inOffset)
		activeBMTable->powerups.resize(inNumPowerupsToRead + inOffset);
	
	for (i = inOffset; i < (inNumPowerupsToRead + inOffset); i++)
	{
		activeBMTable->powerups[i].vclip_num = cfile_read_int(fp);
		activeBMTable->powerups[i].hit_sound = cfile_read_int(fp);
		activeBMTable->powerups[i].size = cfile_read_fix(fp);
		activeBMTable->powerups[i].light = cfile_read_fix(fp);
	}
}

void read_polygon_models(CFILE *fp, int inNumPolygonModelsToRead, int inOffset)
{
	int i, j;

	if (activeBMTable->models.size() < inNumPolygonModelsToRead + inOffset)
		activeBMTable->models.resize(inNumPolygonModelsToRead + inOffset);

	for (i = inOffset; i < (inNumPolygonModelsToRead + inOffset); i++)
	{
		activeBMTable->models[i].n_models = cfile_read_int(fp);
		activeBMTable->models[i].model_data_size = cfile_read_int(fp);
		/*Polygon_models[i].model_data = (uint8_t *)*/cfile_read_int(fp);
		activeBMTable->models[i].model_data = NULL; //shut up compiler warning, data isn't useful anyways
		for (j = 0; j < MAX_SUBMODELS; j++)
			activeBMTable->models[i].submodel_ptrs[j] = cfile_read_int(fp);
		for (j = 0; j < MAX_SUBMODELS; j++)
			cfile_read_vector(&(activeBMTable->models[i].submodel_offsets[j]), fp);
		for (j = 0; j < MAX_SUBMODELS; j++)
			cfile_read_vector(&(activeBMTable->models[i].submodel_norms[j]), fp);
		for (j = 0; j < MAX_SUBMODELS; j++)
			cfile_read_vector(&(activeBMTable->models[i].submodel_pnts[j]), fp);
		for (j = 0; j < MAX_SUBMODELS; j++)
			activeBMTable->models[i].submodel_rads[j] = cfile_read_fix(fp);
		for (j = 0; j < MAX_SUBMODELS; j++)
			activeBMTable->models[i].submodel_parents[j] = cfile_read_byte(fp);
		for (j = 0; j < MAX_SUBMODELS; j++)
			cfile_read_vector(&(activeBMTable->models[i].submodel_mins[j]), fp);
		for (j = 0; j < MAX_SUBMODELS; j++)
			cfile_read_vector(&(activeBMTable->models[i].submodel_maxs[j]), fp);
		cfile_read_vector(&(activeBMTable->models[i].mins), fp);
		cfile_read_vector(&(activeBMTable->models[i].maxs), fp);
		activeBMTable->models[i].rad = cfile_read_fix(fp);		
		activeBMTable->models[i].n_textures = cfile_read_byte(fp);
		activeBMTable->models[i].first_texture = cfile_read_short(fp);
		activeBMTable->models[i].simpler_model = cfile_read_byte(fp);
	}
}

void read_player_ship(CFILE *fp)
{
	int i;

	activeBMTable->player.model_num = cfile_read_int(fp);
	activeBMTable->player.expl_vclip_num = cfile_read_int(fp);
	activeBMTable->player.mass = cfile_read_fix(fp);
	activeBMTable->player.drag = cfile_read_fix(fp);
	activeBMTable->player.max_thrust = cfile_read_fix(fp);
	activeBMTable->player.reverse_thrust = cfile_read_fix(fp);
	activeBMTable->player.brakes = cfile_read_fix(fp);
	activeBMTable->player.wiggle = cfile_read_fix(fp);
	activeBMTable->player.max_rotthrust = cfile_read_fix(fp);
	for (i = 0; i < N_PLAYER_GUNS; i++)
		cfile_read_vector(&(activeBMTable->player.gun_points[i]), fp);
}

void read_reactor_info(CFILE *fp, int inNumReactorsToRead, int inOffset)
{
	int i, j;
	
	if (activeBMTable->reactors.size() < inNumReactorsToRead + inOffset)
		activeBMTable->reactors.resize(inNumReactorsToRead + inOffset);

	for (i = inOffset; i < (inNumReactorsToRead + inOffset); i++)
	{
		activeBMTable->reactors[i].model_num = cfile_read_int(fp);
		activeBMTable->reactors[i].n_guns = cfile_read_int(fp);
		for (j = 0; j < MAX_CONTROLCEN_GUNS; j++)
			cfile_read_vector(&(activeBMTable->reactors[i].gun_points[j]), fp);
		for (j = 0; j < MAX_CONTROLCEN_GUNS; j++)
			cfile_read_vector(&(activeBMTable->reactors[i].gun_dirs[j]), fp);
	}
}

#ifdef SHAREWARE
//extern int exit_modelnum,destroyed_exit_modelnum, Num_bitmap_files;
int N_ObjBitmaps, extra_bitmap_num;

bitmap_index exitmodel_bm_load_sub( char * filename )
{
	bitmap_index bitmap_num;
	grs_bitmap * new;
	uint8_t newpal[256*3];
	int i, iff_error;		//reference parm to avoid warning message

	bitmap_num.index = 0;

	MALLOC( new, grs_bitmap, 1 );
	iff_error = iff_read_bitmap(filename,new,BM_LINEAR,newpal);
	new->bm_handle=0;
	if (iff_error != IFF_NO_ERROR)		{
		Error("Error loading exit model bitmap <%s> - IFF error: %s",filename,iff_errormsg(iff_error));
	}
	
	if ( iff_has_transparency )
		gr_remap_bitmap_good( new, newpal, iff_transparent_color, 254 );
	else
		gr_remap_bitmap_good( new, newpal, -1, 254 );

	new->avg_color = 0;	//compute_average_pixel(new);

	bitmap_num.index = extra_bitmap_num;

	GameBitmaps[extra_bitmap_num++] = *new;
	
	mem_free( new );
	return bitmap_num;
}

grs_bitmap *load_exit_model_bitmap(char *name)
{
	Assert(N_ObjBitmaps < MAX_OBJ_BITMAPS);

	{
		activeBMTable->objectBitmaps[N_ObjBitmaps] = exitmodel_bm_load_sub(name);
		if (GameBitmaps[activeBMTable->objectBitmaps[N_ObjBitmaps].index].bm_w!=64 || GameBitmaps[activeBMTable->objectBitmaps[N_ObjBitmaps].index].bm_h!=64)
			Error("Bitmap <%s> is not 64x64",name);
		activeBMTable->objectBitmapPointers[N_ObjBitmaps] = N_ObjBitmaps;
		N_ObjBitmaps++;
		Assert(N_ObjBitmaps < MAX_OBJ_BITMAPS);
		return &GameBitmaps[activeBMTable->objectBitmaps[N_ObjBitmaps-1].index];
	}
}

void load_exit_models()
{
	CFILE *exit_hamfile;
	int i, j;
	uint8_t pal[768];
	int start_num;

	start_num = N_ObjBitmaps;
	extra_bitmap_num = Num_bitmap_files;
	load_exit_model_bitmap("steel1.bbm");
	load_exit_model_bitmap("rbot061.bbm");
	load_exit_model_bitmap("rbot062.bbm");

	load_exit_model_bitmap("steel1.bbm");
	load_exit_model_bitmap("rbot061.bbm");
	load_exit_model_bitmap("rbot063.bbm");

	exit_hamfile = cfopen(":Data:exit.ham","rb");

	//exit_modelnum = N_polygon_models++;
	//destroyed_exit_modelnum = N_polygon_models++;

	activeBMTable->exitModel = activeBMTable->models.size();
	activeBMTable->destroyedExitModel = activeBMTable->exitModel + 1;

	activeBMTable->models.resize(activeBMTable->models.size() + 2);

	/*activeBMTable->models.push_back(polymodel());
	exit_modelnum = models.size() - 1;
	activeBMTable->models.push_back(polymodel());
	destroyed_exit_modelnum = models.size() - 1;*/

	#ifndef MACINTOSH
	cfread( &activeBMTable->models[activeBMTable->exitModel], sizeof(polymodel), 1, exit_hamfile );
	cfread( &activeBMTable->models[activeBMTable->destroyedExitModel], sizeof(polymodel), 1, exit_hamfile );
	#else
	for (i = activeBMTable->exitModel; i <= activeBMTable->destroyedExitModel; i++) {
		activeBMTable->models[i].n_models = cfile_read_int(exit_hamfile);
		activeBMTable->models[i].model_data_size = cfile_read_int(exit_hamfile);
		activeBMTable->models[i].model_data = (uint8_t *)read_int_swap(exit_hamfile);
		for (j = 0; j < MAX_SUBMODELS; j++)
			activeBMTable->models[i].submodel_ptrs[j] = cfile_read_int(exit_hamfile);
		for (j = 0; j < MAX_SUBMODELS; j++)
			cfile_read_vector(&(activeBMTable->models[i].submodel_offsets), exit_hamfile);
		for (j = 0; j < MAX_SUBMODELS; j++)
			cfile_read_vector(&(activeBMTable->models[i].submodel_norms), exit_hamfile);
		for (j = 0; j < MAX_SUBMODELS; j++)
			cfile_read_vector(&(activeBMTable->models[i].submodel_pnts), exit_hamfile);
		for (j = 0; j < MAX_SUBMODELS; j++)
			activeBMTable->models[i].submodel_rads[j] = cfile_read_fix(exit_hamfile);
		for (j = 0; j < MAX_SUBMODELS; j++)
			activeBMTable->models[i].submodel_parents[j] = cfile_read_byte(exit_hamfile);
		for (j = 0; j < MAX_SUBMODELS; j++)
			cfile_read_vector(&(activeBMTable->models[i].submodel_mins), exit_hamfile);
		for (j = 0; j < MAX_SUBMODELS; j++)
			cfile_read_vector(&(activeBMTable->models[i].submodel_maxs), exit_hamfile);
		cfile_read_vector(&(activeBMTable->models[i].mins), exit_hamfile);
		cfile_read_vector(&(activeBMTable->models[i].maxs), exit_hamfile);
		activeBMTable->models[i].rad = cfile_read_fix(exit_hamfile);		
		activeBMTable->models[i].n_textures = cfile_read_byte(exit_hamfile);
		activeBMTable->models[i].first_texture = cfile_read_short(exit_hamfile);
		activeBMTable->models[i].simpler_model = cfile_read_byte(exit_hamfile);
	}
	activeBMTable->models[activeBMTable->exitModel].first_texture = start_num;
	activeBMTable->models[activeBMTable->destroyedExitModel].first_texture = start_num+3;
	#endif

	activeBMTable->models[activeBMTable->exitModel].model_data = mem_malloc(activeBMTable->models[activeBMTable->exitModel].model_data_size);
	Assert( activeBMTable->models[activeBMTable->exitModel].model_data != NULL );
	cfread( activeBMTable->models[activeBMTable->exitModel].model_data, sizeof(uint8_t), activeBMTable->models[activeBMTable->exitModel].model_data_size, exit_hamfile );
	#ifdef MACINTOSH
	swap_polygon_model_data(activeBMTable->models[activeBMTable->exitModel].model_data);
	#endif
	g3_init_polygon_model(activeBMTable->models[activeBMTable->exitModel].model_data);

	activeBMTable->models[activeBMTable->destroyedExitModel].model_data = mem_malloc(activeBMTable->models[activeBMTable->destroyedExitModel].model_data_size);
	Assert( activeBMTable->models[activeBMTable->destroyedExitModel].model_data != NULL );
	cfread( activeBMTable->models[activeBMTable->destroyedExitModel].model_data, sizeof(uint8_t), activeBMTable->models[activeBMTable->destroyedExitModel].model_data_size, exit_hamfile );
	#ifdef MACINTOSH
	swap_polygon_model_data(activeBMTable->models[activeBMTable->destroyedExitModel].model_data);
	#endif
	g3_init_polygon_model(activeBMTable->models[activeBMTable->destroyedExitModel].model_data);

	cfclose(exit_hamfile);

}
#endif		// SHAREWARE

//-----------------------------------------------------------------
// Read data from piggy.
// This is called when the editor is OUT.  
// If editor is in, bm_init_use_table() is called.
int bm_init() {
	
	if (shouldAutoClearBMTable) { //Destroy unused table to clear memory
		if (activeBMTable == &d1Table)
			d2Table = bmtable(); 
		else if (activeBMTable == &d2Table)
			d1Table = bmtable();
		else
			Int3(); //Got a rogue table active
	}

	activeBMTable->Init(false);

	//init_polygon_models();
	if (!piggy_init())				// This calls bm_read_all
		Error("Cannot open pig and/or ham file");

	piggy_read_sounds();

	//if (currentGame == G_DESCENT_1 || CurrentDataVersion == DataVer::DEMO)
	init_endlevel();		//this is in bm_init_use_tbl(), so I gues it goes here

	return 0;
}

void bm_read_all(CFILE* fp) {

	int i, t;

	auto NumTextures = cfile_read_int(fp);
	for (i = 0; i < READ_LIMIT(MAX_TEXTURES_D1, NumTextures); i++)
		activeBMTable->textures.push_back(BITMAP_INDEX(cfile_read_short(fp)));
	read_tmap_info(fp, READ_LIMIT(MAX_TEXTURES_D1, NumTextures), 0);

	uint8_t sounds[MAX_SOUNDS];
	uint8_t altSounds[MAX_SOUNDS];

	t = READ_LIMIT(MAX_SOUNDS_D1, cfile_read_int(fp));

	cfread(sounds, sizeof(uint8_t), t, fp);
	cfread(altSounds, sizeof(uint8_t), t, fp);

	activeBMTable->sounds.assign(sounds, sounds + t);
	activeBMTable->altSounds.assign(altSounds, altSounds + t);

	auto Num_vclips = cfile_read_int(fp);
	read_vclip_info(fp, READ_LIMIT(VCLIP_MAXNUM_D1, Num_vclips), 0);

	auto Num_effects = cfile_read_int(fp);
	read_effect_info(fp, READ_LIMIT(MAX_EFFECTS_D1, Num_effects), 0);

	auto Num_wall_anims = cfile_read_int(fp);
	read_wallanim_info(fp, READ_LIMIT(MAX_WALL_ANIMS_D1, Num_wall_anims), 0);

	auto N_robot_types = cfile_read_int(fp);
	read_robot_info(fp, READ_LIMIT(MAX_ROBOT_TYPES_D1, N_robot_types), 0);

	auto N_robot_joints = cfile_read_int(fp);
	read_robot_joint_info(fp, READ_LIMIT(MAX_ROBOT_JOINTS_D1, N_robot_joints), 0);

	auto N_weapon_types = cfile_read_int(fp);
	read_weapon_info(fp, READ_LIMIT(MAX_WEAPON_TYPES_D1, N_weapon_types), 0);

	auto N_powerup_types = cfile_read_int(fp);
	read_powerup_info(fp, READ_LIMIT(MAX_POWERUP_TYPES_D1, N_powerup_types), 0);

	auto N_polygon_models = cfile_read_int(fp);
	read_polygon_models(fp, N_polygon_models, 0);

	for (i = 0; i < N_polygon_models; i++)
	{
		activeBMTable->models[i].model_data = (uint8_t*)mem_malloc(activeBMTable->models[i].model_data_size);
		Assert(activeBMTable->models[i].model_data != NULL);
		cfread(activeBMTable->models[i].model_data, sizeof(uint8_t), activeBMTable->models[i].model_data_size, fp);
#ifdef MACINTOSH
		swap_polygon_model_data(activeBMTable->models[i].model_data);
#endif
		g3_init_polygon_model(activeBMTable->models[i].model_data);
	}

	//cfread( Dying_modelnums, sizeof(int), N_polygon_models, fp );
	//cfread( Dead_modelnums, sizeof(int), N_polygon_models, fp );

	if (currentGame == G_DESCENT_1) {

		for (i = 0; i < MAX_GAUGE_BMS_D1; i++) {
			bitmap_index bi {(uint16_t)cfile_read_short(fp)};
			activeBMTable->gauges.push_back(bi);
			activeBMTable->hiresGauges.push_back(bi);
		}

		for (i = 0; i < MAX_POLYGON_MODELS_D1; i++)
			activeBMTable->dyingModels.push_back(cfile_read_int(fp));
		for (i = 0; i < MAX_POLYGON_MODELS_D1; i++)
			activeBMTable->deadModels.push_back(cfile_read_int(fp));

		for (i = 0; i < MAX_OBJ_BITMAPS_D1; i++)
			activeBMTable->objectBitmaps.push_back(BITMAP_INDEX(cfile_read_short(fp)));
		for (i = 0; i < MAX_OBJ_BITMAPS_D1; i++)
			activeBMTable->objectBitmapPointers.push_back((uint16_t)cfile_read_short(fp));

	} else {

		for (i = 0; i < N_polygon_models; i++)
			activeBMTable->dyingModels.push_back(cfile_read_int(fp));

		for (i = 0; i < N_polygon_models; i++)
			activeBMTable->deadModels.push_back(cfile_read_int(fp));

		t = cfile_read_int(fp);
		//if (t > MAX_GAUGE_BMS)
		//	Error("Too many gauges present in hamfile. Got %d, max %d.", t, MAX_GAUGE_BMS);
		for (i = 0; i < t; i++)
		{
			activeBMTable->gauges.push_back(BITMAP_INDEX(cfile_read_short(fp)));
		}
		for (i = 0; i < t; i++)
		{
			activeBMTable->hiresGauges.push_back(BITMAP_INDEX(cfile_read_short(fp)));
		}

		t = cfile_read_int(fp);

		//if (t > MAX_OBJ_BITMAPS)
		//	Error("Too many obj bitmaps present in hamfile. Got %d, max %d.", t, MAX_OBJ_BITMAPS);

	#ifdef SHAREWARE
		N_ObjBitmaps = t;
	#endif
		for (i = 0; i < t; i++)
		{
			activeBMTable->objectBitmaps.push_back(BITMAP_INDEX(cfile_read_short(fp)));
		}
		for (i = 0; i < t; i++)
		{
			activeBMTable->objectBitmapPointers.push_back((uint16_t)cfile_read_short(fp));
		}

	}

	read_player_ship(fp);

	auto Num_cockpits = cfile_read_int(fp);
	
	//if (Num_cockpits > N_COCKPIT_BITMAPS)
	//	Error("Too many cockpits present in hamfile. Got %d, max %d.", Num_cockpits, N_COCKPIT_BITMAPS);
	for (i = 0; i < READ_LIMIT(N_COCKPIT_BITMAPS_D1, Num_cockpits); i++) {
		activeBMTable->cockpits.push_back(BITMAP_INDEX(cfile_read_short(fp)));
	}

	printf("\n%d cockpits\n", Num_cockpits);

	if (currentGame == G_DESCENT_1) {

		cfread(sounds, sizeof(uint8_t), MAX_SOUNDS_D1, fp);
		cfread(altSounds, sizeof(uint8_t), MAX_SOUNDS_D1, fp);

		activeBMTable->sounds.assign(sounds, sounds + MAX_SOUNDS_D1);
		activeBMTable->altSounds.assign(altSounds, altSounds + MAX_SOUNDS_D1);

		Num_total_object_types = cfile_read_int(fp); 

		cfread(ObjType, sizeof(int8_t), MAX_OBJTYPE_D1, fp); //Is this actually used anywhere?
		cfread(ObjId, sizeof(int8_t), MAX_OBJTYPE_D1, fp);
		for (i = 0; i < MAX_OBJTYPE_D1; i++)
			ObjStrength[i] = cfile_read_fix(fp);

		if (activeBMTable->reactors.size() < 1) {
			activeBMTable->reactors.resize(1);
		}

		for (i=0; i<Num_total_object_types; i++)
			if (ObjType[i] == OL_CONTROL_CENTER)
				activeBMTable->reactors[0].model_num = ObjId[i];

		activeBMTable->firstMultiBitmapNum = cfile_read_int(fp);
		auto N_controlcen_guns = cfile_read_int(fp);
		activeBMTable->reactors[0].n_guns = N_controlcen_guns;

		activeBMTable->reactorGunPos.resize(MAX_CONTROLCEN_GUNS_D1);
		activeBMTable->reactorGunDirs.resize(MAX_CONTROLCEN_GUNS_D1);
		
		for (i = 0; i < MAX_CONTROLCEN_GUNS_D1; i++) 
		{
			activeBMTable->reactorGunPos[i].x = cfile_read_fix(fp);
			activeBMTable->reactorGunPos[i].y = cfile_read_fix(fp);
			activeBMTable->reactorGunPos[i].z = cfile_read_fix(fp);
			activeBMTable->reactors[0].gun_points[i] = activeBMTable->reactorGunPos[i];
		}
		for (i = 0; i < MAX_CONTROLCEN_GUNS_D1; i++) 
		{
			activeBMTable->reactorGunDirs[i].x = cfile_read_fix(fp);
			activeBMTable->reactorGunDirs[i].y = cfile_read_fix(fp);
			activeBMTable->reactorGunDirs[i].z = cfile_read_fix(fp);
			activeBMTable->reactors[0].gun_dirs[i] = activeBMTable->reactorGunDirs[i];
		}


		activeBMTable->exitModel = cfile_read_int(fp);
		activeBMTable->destroyedExitModel = cfile_read_int(fp);

		//[ISB] I'm normally not huge about changing game logic, but this isn't user facing most of the time, so lets do it anyways
		//This code will make the editor work better when run without parsing bitmaps.tbl. 
#ifdef EDITOR
		N_hostage_types++;
		Hostage_vclip_num[0] = 33;
		//Construct TMAP list
		for (i = 0; i < NumTextures; i++)
		{
			if (i >= activeBMTable->wclips[0].frames[0]) //Don't include doors
			{
				break;
			}
			TmapList[i] = i;
			Num_tmaps++;
		}
#endif

	} else {

		activeBMTable->firstMultiBitmapNum = cfile_read_int(fp);

		auto Num_reactors = cfile_read_int(fp);
		read_reactor_info(fp, Num_reactors, 0);

		activeBMTable->markerModel = cfile_read_int(fp);

		if (CurrentDataVersion == DataVer::DEMO)
		{
			activeBMTable->exitModel = cfile_read_int(fp);
			activeBMTable->destroyedExitModel = cfile_read_int(fp);
		}

	}
}

//these values are the number of each item in the release of d2
//extra items added after the release get written in an additional hamfile
#define N_D2_ROBOT_TYPES		66
#define N_D2_ROBOT_JOINTS		1145
#define N_D2_POLYGON_MODELS		166
#define N_D2_OBJBITMAPS			422
#define N_D2_OBJBITMAPPTRS		502
#define N_D2_WEAPON_TYPES		62

//type==1 means 1.1, type==2 means 1.2 (with weaons)
void bm_read_extra_robots(char *fname,int type)
{
	CFILE *fp;
	int t,i;
	int version;
	
	#ifdef MACINTOSH
		ulong varSave = 0;
	#endif

	fp = cfopen(fname,"rb");

	if (type == 2) {
		int sig;

		sig = cfile_read_int(fp);
		if (sig != 'XHAM')
			return;
		version = cfile_read_int(fp);
	}
	else
		version = 0;

	//read extra weapons

	t = cfile_read_int(fp);
	//N_weapon_types = N_D2_WEAPON_TYPES+t;
	//if (N_weapon_types >= MAX_WEAPON_TYPES)
	//	Error("Too many weapons (%d) in <%s>.  Max is %d.",t,fname,MAX_WEAPON_TYPES-N_D2_WEAPON_TYPES);
	read_weapon_info(fp, t, N_D2_WEAPON_TYPES);
	
	//now read robot info

	t = cfile_read_int(fp);
	//N_robot_types = N_D2_ROBOT_TYPES+t;
	//if (N_robot_types >= MAX_ROBOT_TYPES)
	//	Error("Too many robots (%d) in <%s>.  Max is %d.",t,fname,MAX_ROBOT_TYPES-N_D2_ROBOT_TYPES);
	read_robot_info(fp, t, N_D2_ROBOT_TYPES);
	
	t = cfile_read_int(fp);
	//if (activeBMTable->robotJoints.size() < N_D2_ROBOT_JOINTS + t)
	//	activeBMTable->robotJoints.resize(N_D2_ROBOT_JOINTS + t);
	//N_robot_joints = N_D2_ROBOT_JOINTS+t;
	//if (N_robot_joints >= MAX_ROBOT_JOINTS)
	//	Error("Too many robot joints (%d) in <%s>.  Max is %d.",t,fname,MAX_ROBOT_JOINTS-N_D2_ROBOT_JOINTS);
	read_robot_joint_info(fp, t, N_D2_ROBOT_JOINTS);
	
	t = cfile_read_int(fp);
	auto N_polygon_models = N_D2_POLYGON_MODELS+t;
	//if (N_polygon_models >= MAX_POLYGON_MODELS)
	//	Error("Too many polygon models (%d) in <%s>.  Max is %d.",t,fname,MAX_POLYGON_MODELS-N_D2_POLYGON_MODELS);
	read_polygon_models(fp, t, N_D2_POLYGON_MODELS);
	
	for (i=N_D2_POLYGON_MODELS; i<N_polygon_models; i++ )
	{
		activeBMTable->models[i].model_data = (uint8_t*)mem_malloc(activeBMTable->models[i].model_data_size);
		Assert( activeBMTable->models[i].model_data != NULL );
		cfread( activeBMTable->models[i].model_data, sizeof(uint8_t), activeBMTable->models[i].model_data_size, fp );
		
		g3_init_polygon_model(activeBMTable->models[i].model_data);
	}

	//cfread( &Dying_modelnums[N_D2_POLYGON_MODELS], sizeof(int), t, fp );
	//cfread( &Dead_modelnums[N_D2_POLYGON_MODELS], sizeof(int), t, fp );

	if (activeBMTable->dyingModels.size() < N_polygon_models) {
		activeBMTable->dyingModels.resize(N_polygon_models);
		activeBMTable->deadModels.resize(N_polygon_models);
	}

	for (i = N_D2_POLYGON_MODELS; i < N_polygon_models; i++)
	{
		activeBMTable->dyingModels[i] = cfile_read_int(fp);//SWAPINT(Dying_modelnums[i]);
	}
	for (i = N_D2_POLYGON_MODELS; i < N_polygon_models; i++)
	{
		activeBMTable->deadModels[i] = cfile_read_int(fp); //SWAPINT(Dead_modelnums[i]);
	}

	t = cfile_read_int(fp);
	//if (N_D2_OBJBITMAPS+t >= MAX_OBJ_BITMAPS)
	//	Error("Too many object bitmaps (%d) in <%s>.  Max is %d.",t,fname,MAX_OBJ_BITMAPS-N_D2_OBJBITMAPS);
	//cfread( &ObjBitmaps[N_D2_OBJBITMAPS], sizeof(bitmap_index), t, fp );
	for (i = N_D2_OBJBITMAPS; i < (N_D2_OBJBITMAPS + t); i++)
	{
		activeBMTable->objectBitmaps[i].index = cfile_read_short(fp);//SWAPSHORT(activeBMTable->objectBitmaps[i].index);
	}

	t = cfile_read_int(fp);
	//if (N_D2_OBJBITMAPPTRS+t >= MAX_OBJ_BITMAPS)
	//	Error("Too many object bitmap pointers (%d) in <%s>.  Max is %d.",t,fname,MAX_OBJ_BITMAPS-N_D2_OBJBITMAPPTRS);
	//cfread( &ObjBitmapPtrs[N_D2_OBJBITMAPPTRS], sizeof(uint16_t), t, fp );
	for (i = N_D2_OBJBITMAPPTRS; i < (N_D2_OBJBITMAPPTRS + t); i++)
	{
		activeBMTable->objectBitmapPointers[i] = cfile_read_short(fp);//SWAPSHORT(ObjBitmapPtrs[i]);
	}

	cfclose(fp);
}

extern void change_filename_extension( char *dest, const char *src, const char *new_ext );

int Robot_replacements_loaded = 0;

void load_robot_replacements(char *level_name)
{
	CFILE *fp;
	int t,i,j;
	char ifile_name[FILENAME_LEN];

	change_filename_extension(ifile_name, level_name, ".HXM" );
	
	fp = cfopen(ifile_name,"rb");

	if (!fp)		//no robot replacement file
		return;

	t = cfile_read_int(fp);			//read id "HXM!"
	if (t!= 0x21584d48) 
		Error("ID of HXM! file incorrect");

	t = cfile_read_int(fp);			//read version
	if (t<1)
		Error("HXM! version too old (%d)",t); 

	t = cfile_read_int(fp);			//read number of robots
	for (j=0;j<t;j++) {
		i = cfile_read_int(fp);		//read robot number
	   if (i<0 || i>=activeBMTable->robots.size())
			Error("Robots number (%d) out of range in (%s).  Range = [0..%d].",i,level_name,activeBMTable->robots.size()-1);
		read_robot_info(fp, 1, i);
	}

	t = cfile_read_int(fp);			//read number of joints
	for (j=0;j<t;j++) {
		i = cfile_read_int(fp);		//read joint number
		if (i<0 || i>=activeBMTable->robotJoints.size())
			Error("Robots joint (%d) out of range in (%s).  Range = [0..%d].",i,level_name,activeBMTable->robotJoints.size()-1);
		read_robot_joint_info(fp, 1, i);
	}

	t = cfile_read_int(fp);			//read number of polygon models
	for (j=0;j<t;j++)
	{
		i = cfile_read_int(fp);		//read model number
		if (i<0 || i>=activeBMTable->models.size())
			Error("Polygon model (%d) out of range in (%s).  Range = [0..%d].",i,level_name,activeBMTable->models.size()-1);

		//[ISB] I'm going to hurt someone
		//Free the old model data before loading a bogus pointer over it
		mem_free(activeBMTable->models[i].model_data);
	
		read_polygon_models(fp, 1, i);
	
		activeBMTable->models[i].model_data = (uint8_t*)mem_malloc(activeBMTable->models[i].model_data_size);
		Assert( activeBMTable->models[i].model_data != NULL );

		cfread( activeBMTable->models[i].model_data, sizeof(uint8_t), activeBMTable->models[i].model_data_size, fp );
		g3_init_polygon_model(activeBMTable->models[i].model_data);

		activeBMTable->dyingModels[i] = cfile_read_int(fp);
		activeBMTable->deadModels[i] = cfile_read_int(fp);
	}

	t = cfile_read_int(fp);			//read number of objbitmaps
	if (t > activeBMTable->objectBitmaps.size()) 
		activeBMTable->objectBitmaps.resize(t);
	
	for (j=0;j<t;j++) 
	{
		i = cfile_read_int(fp);		//read objbitmap number
		if (i<0 || i>=MAX_OBJ_BITMAPS)
			Error("Object bitmap number (%d) out of range in (%s).  Range = [0..%d].",i,level_name,MAX_OBJ_BITMAPS-1);
		activeBMTable->objectBitmaps[i].index = cfile_read_short(fp);
	}

	t = cfile_read_int(fp);			//read number of objbitmapptrs
	if (t > activeBMTable->objectBitmapPointers.size()) 
		activeBMTable->objectBitmapPointers.resize(t);
	
	for (j=0;j<t;j++) 
	{
		i = cfile_read_int(fp);		//read objbitmapptr number
		if (i<0 || i>=MAX_OBJ_BITMAPS)
			Error("Object bitmap pointer (%d) out of range in (%s).  Range = [0..%d].",i,level_name,MAX_OBJ_BITMAPS-1);
		activeBMTable->objectBitmapPointers[i] = cfile_read_short(fp);
	}

	cfclose(fp);
}
