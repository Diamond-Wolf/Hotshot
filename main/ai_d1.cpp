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

#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include "main/inferno.h"
#include "main/game.h"
#include "platform/mono.h"
#include "3d/3d.h"
#include "cfile/cfile.h"
#include "main/object.h"
#include "main/render.h"
#include "misc/error.h"
#include "main/ai.h"
#include "main/laser.h"
#include "main/fvi.h"
#include "main/polyobj.h"
#include "main/bm.h"
#include "main/weapon.h"
#include "main/physics.h"
#include "main/collide.h"
#include "main/fuelcen.h"
#include "main/player.h"
#include "main/wall.h"
#include "main/vclip.h"
#include "main/digi.h"
#include "main/fireball.h"
#include "main/morph.h"
#include "main/effects.h"
#include "platform/timer.h"
#include "main/sounds.h"
#include "main/cntrlcen.h"
#include "main/multibot.h"
#include "main/multi.h"
#include "main/network.h"
#include "main/gameseq.h"
#include "platform/key.h"
#include "main/powerup.h"
#include "main/gauges.h"
#include "stringtable.h"
#include "misc/rand.h"
#include "newcheat.h"

#include "ai_ifwd.h"

#ifdef EDITOR
#include "editor\editor.h"
#endif

#ifndef NDEBUG
#include "string.h"
#include <time.h>
#endif

#define	JOHN_CHEATS_SIZE_1	6
#define	JOHN_CHEATS_SIZE_2	6
#define	JOHN_CHEATS_SIZE_3	6

uint8_t	john_cheats_1[JOHN_CHEATS_SIZE_1] = { KEY_P ^ 0x00 ^ 0x34,
															KEY_O ^ 0x10 ^ 0x34,
															KEY_B ^ 0x20 ^ 0x34,
															KEY_O ^ 0x30 ^ 0x34,
															KEY_Y ^ 0x40 ^ 0x34,
															KEY_S ^ 0x50 ^ 0x34 };

#define	PARALLAX	0		//	If !0, then special debugging info for Parallax eyes only enabled.

#define MIN_D 0x100

int	john_cheats_index_1;		//	POBOYS		detonate reactor
#define	ANIM_RATE		(F1_0/16)
#define	DELTA_ANG_SCALE	16

int	john_cheats_index_2;		//	PORGYS		high speed weapon firing

// int	No_ai_flag=0;

#define	OVERALL_AGITATION_MAX	100

#define		MAX_AI_CLOAK_INFO	8	//	Must be a power of 2!

#define	BOSS_CLOAK_DURATION	(F1_0*7)
#define	BOSS_DEATH_DURATION	(F1_0*6)
#define	BOSS_DEATH_SOUND_DURATION	0x2ae14		//	2.68 seconds

//	Amount of time since the current robot was last processed for things such as movement.
//	It is not valid to use FrameTime because robots do not get moved every frame.
//fix	AI_proc_time;

//	---------- John: End of variables which must be saved as part of gamesave. ----------

int	john_cheats_index_4;		//	PLETCHnnn	paint robots

#ifndef SHAREWARE
//	0	mech
//	1	green claw
//	2	spider
//	3	josh
//	4	violet
//	5	cloak vulcan
//	6	cloak mech
//	7	brain
//	8	onearm
//	9	plasma
//	10	toaster
//	11	bird
//	12	missile bird
//	13	polyhedron
//	14	baby spider
//	15	mini boss
//	16	super mech
//	17	shareware boss
//	18	cloak-green	; note, gating in this guy benefits player, cloak objects
//	19	vulcan
//	20	toad
//	21	4-claw
//	22	quad-laser
// 23 super boss

// int8_t	Super_boss_gate_list[] = {0, 1, 2, 9, 11, 16, 18, 19, 21, 22, 0, 9, 9, 16, 16, 18, 19, 19, 22, 22};
int8_t	Super_boss_gate_list[] = { 0, 1, 8, 9, 10, 11, 12, 15, 16, 18, 19, 20, 22, 0, 8, 11, 19, 20, 8, 20, 8 };
#define	MAX_GATE_INDEX	( sizeof(Super_boss_gate_list) / sizeof(Super_boss_gate_list[0]) )
#endif

int	Ugly_robot_cheat, Ugly_robot_texture;
extern	int8_t	Enable_john_cheat_1, Enable_john_cheat_2, Enable_john_cheat_3, Enable_john_cheat_4;

uint8_t	john_cheats_3[2 * JOHN_CHEATS_SIZE_3 + 1] = { KEY_Y ^ 0x67,
																KEY_E ^ 0x66,
																KEY_C ^ 0x65,
																KEY_A ^ 0x64,
																KEY_N ^ 0x63,
																KEY_U ^ 0x62,
																KEY_L ^ 0x61 };

//	These globals are set by a call to find_vector_intersection, which is a slow routine,
//	so we don't want to call it again (for this object) unless we have to.

#define	AIS_MAX	8
#define	AIE_MAX	4

#ifndef NDEBUG
int	Ai_animation_test = 0;
#endif

// Current state indicates where the robot current is, or has just done.
//	Transition table between states for an AI object.
//	 First dimension is trigger event.
//	Second dimension is current state.
//	 Third dimension is goal state.
//	Result is new goal state.
//	ERR_ means something impossible has happened.

uint8_t	john_cheats_2[2 * JOHN_CHEATS_SIZE_2] = { KEY_P ^ 0x00 ^ 0x43, 0x66,
																KEY_O ^ 0x10 ^ 0x43, 0x11,
																KEY_R ^ 0x20 ^ 0x43, 0x8,
																KEY_G ^ 0x30 ^ 0x43, 0x2,
																KEY_Y ^ 0x40 ^ 0x43, 0x0,
																KEY_S ^ 0x50 ^ 0x43 };

void john_cheat_func_1(int key)
{
	if (!Cheats_enabled)
		return;

	if (key == (john_cheats_1[john_cheats_index_1] ^ (john_cheats_index_1 << 4) ^ 0x34)) {
		john_cheats_index_1++;
		if (john_cheats_index_1 == JOHN_CHEATS_SIZE_1) {
			do_controlcen_destroyed_stuff(NULL);
			john_cheats_index_1 = 0;
			digi_play_sample(SOUND_CHEATER, F1_0);
		}
	}
	else
		john_cheats_index_1 = 0;
}

void john_cheat_func_2(int key)
{
	if (!Cheats_enabled)
		return;

	if (key == (john_cheats_2[2 * john_cheats_index_2] ^ (john_cheats_index_2 << 4) ^ 0x43)) {
		john_cheats_index_2++;
		if (john_cheats_index_2 == JOHN_CHEATS_SIZE_2) {
			cheatValues[CI_RAPID_FIRE] = 0xBADA55;
			do_megawow_powerup(200);
			john_cheats_index_2 = 0;
			digi_play_sample(SOUND_CHEATER, F1_0);
		}
	}
	else
		john_cheats_index_2 = 0;
}

//--debug-- #ifndef NDEBUG
//--debug-- int	Total_turns=0;
//--debug-- int	Prevented_turns=0;
//--debug-- #endif

#define	AI_TURN_SCALE	1
#define	BABY_SPIDER_ID	14

extern void physics_turn_towards_vector(vms_vector * goal_vector, object * obj, fix rate);

// --------------------------------------------------------------------------------------------------------------------
void ai_turn_randomly(vms_vector* vec_to_player, object* obj, fix rate, int previous_visibility)
{
	vms_vector	curvec;

	//	Random turning looks too stupid, so 1/4 of time, cheat.
	if (previous_visibility)
		if (P_Rand() > 0x7400) {
			ai_turn_towards_vector(vec_to_player, obj, rate);
			return;
		}
	//--debug-- 	if (P_Rand() > 0x6000)
	//--debug-- 		Prevented_turns++;

	curvec = obj->mtype.phys_info.rotvel;

	curvec.y += F1_0 / 64;

	curvec.x += curvec.y / 6;
	curvec.y += curvec.z / 4;
	curvec.z += curvec.x / 10;

	if (abs(curvec.x) > F1_0 / 8) curvec.x /= 4;
	if (abs(curvec.y) > F1_0 / 8) curvec.y /= 4;
	if (abs(curvec.z) > F1_0 / 8) curvec.z /= 4;

	obj->mtype.phys_info.rotvel = curvec;

}

//	Overall_agitation affects:
//		Widens field of view.  Field of view is in range 0..1 (specified in bitmaps.tbl as N/360 degrees).
//			Overall_agitation/128 subtracted from field of view, making robots see wider.
//		Increases distance to which robot will search to create path to player by Overall_agitation/8 segments.
//		Decreases wait between fire times by Overall_agitation/64 seconds.

void john_cheat_func_4(int key)
{
	if (!Cheats_enabled)
		return;

	switch (john_cheats_index_4) {
	case 3:
		if (key == KEY_T)
			john_cheats_index_4++;
		else
			john_cheats_index_4 = 0;
		break;

	case 1:
		if (key == KEY_L)
			john_cheats_index_4++;
		else
			john_cheats_index_4 = 0;
		break;

	case 2:
		if (key == KEY_E)
			john_cheats_index_4++;
		else
			john_cheats_index_4 = 0;
		break;

	case 0:
		if (key == KEY_P)
			john_cheats_index_4++;
		break;


	case 4:
		if (key == KEY_C)
			john_cheats_index_4++;
		else
			john_cheats_index_4 = 0;
		break;

	case 5:
		if (key == KEY_H)
			john_cheats_index_4++;
		else
			john_cheats_index_4 = 0;
		break;

	case 6:
		Ugly_robot_texture = 0;
	case 7:
	case 8:
		if ((key >= KEY_1) && (key <= KEY_0)) {
			john_cheats_index_4++;
			Ugly_robot_texture *= 10;
			if (key != KEY_0)
				Ugly_robot_texture += key - 1;
			if (john_cheats_index_4 == 9) {
				if (Ugly_robot_texture == 999) {
					Ugly_robot_cheat = 0;
					HUD_init_message(TXT_ROBOT_PAINTING_OFF);
				}
				else {
					HUD_init_message(TXT_ROBOT_PAINTING_ON, Ugly_robot_texture);
					Ugly_robot_cheat = 0xBADA55;
				}
				mprintf((0, "Paint value = %i\n", Ugly_robot_texture));
				john_cheats_index_4 = 0;
			}
		}
		else
			john_cheats_index_4 = 0;

		break;
	default:
		john_cheats_index_4 = 0;
	}
}

// ----------------------------------------------------------------------------------
void set_next_fire_time_d1(object* objp, ai_local* ailp, robot_info* robptr, int gun_num)
{
	ailp->rapidfire_count++;

	if (ailp->rapidfire_count < robptr->rapidfire_count[Difficulty_level]) {
		ailp->next_fire = std::min((fix)(F1_0 / 8), robptr->firing_wait[Difficulty_level] / 2);
	}
	else {
		ailp->rapidfire_count = 0;
		ailp->next_fire = robptr->firing_wait[Difficulty_level];
	}
}
 
extern int Player_exploded;

// --------------------------------------------------------------------------------------------------------------------
//	Note: Parameter vec_to_player is only passed now because guns which aren't on the forward vector from the
//	center of the robot will not fire right at the player.  We need to aim the guns at the player.  Barring that, we cheat.
//	When this routine is complete, the parameter vec_to_player should not be necessary.
void ai_fire_laser_at_player_d1(object* obj, vms_vector* fire_point, int gun_num, vms_vector *believed_player_pos)
{
	int			objnum = obj - Objects.data();
	ai_local* ailp = &Ai_local_info[objnum];
	robot_info* robptr = &activeBMTable->robots[obj->id];
	vms_vector	fire_vec;
	vms_vector	bpp_diff;

	if (cheatValues[CI_NO_FIRING_D1])
		return;

#ifndef NDEBUG
	//	We should never be coming here for the green guy, as he has no laser!
	if (robptr->attack_type == 1)
		Int3();	// Contact Mike: This is impossible.
#endif

	if (obj->control_type == CT_MORPH)
		return;

	//	If player is exploded, stop firing.
	if (Player_exploded)
		return;

	//	If player is cloaked, maybe don't fire based on how long cloaked and randomness.
	if (Players[Player_num].flags & PLAYER_FLAGS_CLOAKED) {
		fix	cloak_time = Ai_cloak_info[objnum % MAX_AI_CLOAK_INFO].last_time;

		if (GameTime - cloak_time > CLOAK_TIME_MAX / 4)
			if (P_Rand() > fixdiv(GameTime - cloak_time, CLOAK_TIME_MAX) / 2) {
				set_next_fire_time(nullptr, ailp, robptr, 0);
				return;
			}
	}

		//	Set position to fire at based on difficulty level.
	bpp_diff.x = Believed_player_pos.x + (P_Rand() - 16384) * (NDL - Difficulty_level - 1) * 4;
	bpp_diff.y = Believed_player_pos.y + (P_Rand() - 16384) * (NDL - Difficulty_level - 1) * 4;
	bpp_diff.z = Believed_player_pos.z + (P_Rand() - 16384) * (NDL - Difficulty_level - 1) * 4;

	//	Half the time fire at the player, half the time lead the player.
	if (P_Rand() > 16384) {

		vm_vec_normalized_dir_quick(&fire_vec, &bpp_diff, fire_point);

	}
	else {
		vms_vector	player_direction_vector;

		vm_vec_sub(&player_direction_vector, &bpp_diff, &bpp_diff);

		// If player is not moving, fire right at him!
		//	Note: If the robot fires in the direction of its forward vector, this is bad because the weapon does not
		//	come out from the center of the robot; it comes out from the side.  So it is common for the weapon to miss
		//	its target.  Ideally, we want to point the guns at the player.  For now, just fire right at the player.
		if ((abs(player_direction_vector.x < 0x10000)) && (abs(player_direction_vector.y < 0x10000)) && (abs(player_direction_vector.z < 0x10000))) {

			vm_vec_normalized_dir_quick(&fire_vec, &bpp_diff, fire_point);

			// Player is moving.  Determine where the player will be at the end of the next frame if he doesn't change his
			//	behavior.  Fire at exactly that point.  This isn't exactly what you want because it will probably take the laser
			//	a different amount of time to get there, since it will probably be a different distance from the player.
			//	So, that's why we write games, instead of guiding missiles...
		}
		else {
			vm_vec_sub(&fire_vec, &bpp_diff, fire_point);
			vm_vec_scale(&fire_vec, fixmul(activeBMTable->weapons[activeBMTable->robots[obj->id].weapon_type].speed[Difficulty_level], FrameTime));

			vm_vec_add2(&fire_vec, &player_direction_vector);
			vm_vec_normalize_quick(&fire_vec);

		}
	}

	//#ifndef NDEBUG
	//	if (robptr->boss_flag)
	//		mprintf((0, "Boss (%i) fires!\n", obj-Objects));
	//#endif

	Laser_create_new_easy(&fire_vec, fire_point, obj - Objects.data(), robptr->weapon_type, 1);
	obj = &Objects[objnum];

#ifndef SHAREWARE
#ifdef NETWORK
	if (Game_mode & GM_MULTI)
	{
		ai_multi_send_robot_position(objnum, -1);
		multi_send_robot_fire(objnum, obj->ctype.ai_info.CURRENT_GUN, &fire_vec);
	}
#endif
#endif

	create_awareness_event(obj, PA_NEARBY_ROBOT_FIRED);

	set_next_fire_time(nullptr, ailp, robptr, 0);

	//	If the boss fired, allow him to teleport very soon (right after firing, cool!), pending other factors.
	if (robptr->boss_flag)
		Last_teleport_time -= Boss_teleport_interval / 2;
}

// --------------------------------------------------------------------------------------------------------------------
//	If a hiding robot gets bumped or hit, he decides to find another hiding place.
void do_ai_robot_hit_d1(object* objp, int type)
{
	if (objp->control_type == CT_AI) {
		if ((type == PA_WEAPON_ROBOT_COLLISION) || (type == PA_PLAYER_COLLISION))
			switch (objp->ctype.ai_info.behavior) {
			case AIM_HIDE:
				objp->ctype.ai_info.SUBMODE = AISM_GOHIDE;
				break;
			case AIM_STILL:
				Ai_local_info[objp - Objects.data()].mode = AIM_CHASE_OBJECT;
				break;
			}
	}

}

//--//	-----------------------------------------------------------------------------------------------------------
//--//	Return true if object *objp is allowed to open door at wall_num
//--int door_openable_by_robot(object *objp, int wall_num)
//--{
//--	if (objp->id == ROBOT_BRAIN)
//--		if (Walls[wall_num].keys == KEY_NONE)
//--			return 1;
//--
//--	return 0;
//--}

//--unused-- //	-----------------------------------------------------------------------------------------------------------
//--unused-- //	For all doors this guy can open in his segment, open them.
//--unused-- void ai_open_doors_in_segment(object *objp)
//--unused-- {
//--unused-- 	int	i;
//--unused-- 	int	segnum = objp->segnum;
//--unused-- 
//--unused-- 	for (i=0; i<MAX_SIDES_PER_SEGMENT; i++)
//--unused-- 		if (Segments[segnum].sides[i].wall_num != -1) {
//--unused-- 			int	wall_num = Segments[segnum].sides[i].wall_num;
//--unused-- 			if ((Walls[wall_num].type == WALL_DOOR) && (Walls[wall_num].keys == KEY_NONE) && (Walls[wall_num].state == WALL_DOOR_CLOSED))
//--unused-- 				if (door_openable_by_robot(objp, wall_num)) {
//--unused-- 					mprintf((0, "Trying to open door at segment %i, side %i\n", segnum, i));
//--unused-- 					wall_open_door(&Segments[segnum], i);
//--unused-- 				}
//--unused-- 		}
//--unused-- }

// --------------------------------------------------------------------------------------------------------------------
//	Return true if a special object (player or control center) is in this segment.
int special_object_in_seg(int segnum)
{
	int	objnum;

	objnum = Segments[segnum].objects;

	while (objnum != -1) {
		if ((Objects[objnum].type == OBJ_PLAYER) || (Objects[objnum].type == OBJ_CNTRLCEN)) {
			mprintf((0, "Special object of type %i in segment %i\n", Objects[objnum].type, segnum));
			return 1;
		}
		else
			objnum = Objects[objnum].next;
	}

	return 0;
}

// --------------------------------------------------------------------------------------------------------------------
//	Randomly select a segment attached to *segp, reachable by flying.
int get_random_child(int segnum)
{
	int	sidenum;
	segment* segp = &Segments[segnum];

	sidenum = (P_Rand() * 6) >> 15;

	while (!(WALL_IS_DOORWAY(segp, sidenum) & WID_FLY_FLAG))
		sidenum = (P_Rand() * 6) >> 15;

	segnum = segp->children[sidenum];

	return segnum;
}

//	----------------------------------------------------------------------
void do_boss_dying_frame_d1(size_t objnum)
{

	object* objp = &Objects[objnum];

	fix	boss_roll_val, temp;

	boss_roll_val = fixdiv(GameTime - Boss_dying_start_time, BOSS_DEATH_DURATION);

	fix_sincos(fixmul(boss_roll_val, boss_roll_val), &temp, &objp->mtype.phys_info.rotvel.x);
	fix_sincos(boss_roll_val, &temp, &objp->mtype.phys_info.rotvel.y);
	fix_sincos(boss_roll_val - F1_0 / 8, &temp, &objp->mtype.phys_info.rotvel.z);

	objp->mtype.phys_info.rotvel.x = (GameTime - Boss_dying_start_time) / 9;
	objp->mtype.phys_info.rotvel.y = (GameTime - Boss_dying_start_time) / 5;
	objp->mtype.phys_info.rotvel.z = (GameTime - Boss_dying_start_time) / 7;

	if (Boss_dying_start_time + BOSS_DEATH_DURATION - BOSS_DEATH_SOUND_DURATION < GameTime) {
		if (!Boss_dying_sound_playing) {
			mprintf((0, "Starting boss death sound!\n"));
			Boss_dying_sound_playing = 1;
			digi_link_sound_to_object2(SOUND_BOSS_SHARE_DIE, objp - Objects.data(), 0, F1_0 * 4, F1_0 * 1024);	//	F1_0*512 means play twice as loud
		}
		else if (P_Rand() < FrameTime * 16) {
			create_small_fireball_on_object(objp, (F1_0 + P_Rand()) * 8, 0);
			objp = &Objects[objnum];
		}
	}
	else if (P_Rand() < FrameTime * 8) {
		create_small_fireball_on_object(objp, (F1_0 / 2 + P_Rand()) * 8, 1);
		objp = &Objects[objnum];
	}

	if (Boss_dying_start_time + BOSS_DEATH_DURATION < GameTime) {
		do_controlcen_destroyed_stuff(NULL);
		explode_object(objp, F1_0 / 4);
		digi_link_sound_to_object2(SOUND_BADASS_EXPLOSION, objnum, 0, F2_0, F1_0 * 512);
	}
}

extern void teleport_boss(object* objp);

// --------------------------------------------------------------------------------------------------------------------
//	Do special stuff for a boss.
void do_boss_stuff_d1(object* objp)
{
	//  New code, fixes stupid bug which meant boss never gated in robots if > 32767 seconds played.
	if (Last_teleport_time > GameTime)
		Last_teleport_time = GameTime;

	if (Last_gate_time > GameTime)
		Last_gate_time = GameTime;

#ifndef NDEBUG
	if (objp->shields != Prev_boss_shields) {
		mprintf((0, "Boss shields = %7.3f, object %i\n", f2fl(objp->shields), objp - Objects.data()));
		Prev_boss_shields = objp->shields;
	}
#endif

	if (!Boss_dying) {
		if (objp->ctype.ai_info.CLOAKED == 1) {
			if ((GameTime - Boss_cloak_start_time > BOSS_CLOAK_DURATION / 3) && (Boss_cloak_end_time - GameTime > BOSS_CLOAK_DURATION / 3) && (GameTime - Last_teleport_time > Boss_teleport_interval)) {
				if (ai_multiplayer_awareness(objp, 98))
					teleport_boss(objp);
			}
			else if (Boss_hit_this_frame) {
				Boss_hit_this_frame = 0;
				Last_teleport_time -= Boss_teleport_interval / 4;
			}

			if (GameTime > Boss_cloak_end_time)
				objp->ctype.ai_info.CLOAKED = 0;
		}
		else {
			if ((GameTime - Boss_cloak_end_time > Boss_cloak_interval) || Boss_hit_this_frame) {
				if (ai_multiplayer_awareness(objp, 95))
				{
					Boss_hit_this_frame = 0;
					Boss_cloak_start_time = GameTime;
					Boss_cloak_end_time = GameTime + Boss_cloak_duration;
					objp->ctype.ai_info.CLOAKED = 1;
#ifndef SHAREWARE
#ifdef NETWORK
					if (Game_mode & GM_MULTI)
						multi_send_boss_actions(objp - Objects.data(), 2, 0, 0);
#endif
#endif
				}
			}
		}
	}
	else
		do_boss_dying_frame(objp - Objects.data());

}

#define	BOSS_TO_PLAYER_GATE_DISTANCE	(F1_0*150)

#ifndef SHAREWARE

// --------------------------------------------------------------------------------------------------------------------
//	Do special stuff for a boss.
void do_super_boss_stuff(object* objp, fix dist_to_player, int player_visibility)
{
	static int eclip_state = 0;
	do_boss_stuff(objp, 0);

	size_t objnum = objp - Objects.data();

	// Only master player can cause gating to occur.
#ifdef NETWORK
	if ((Game_mode & GM_MULTI) && !network_i_am_master())
		return;
#endif

	if ((dist_to_player < BOSS_TO_PLAYER_GATE_DISTANCE) || player_visibility || (Game_mode & GM_MULTI)) {
		if (GameTime - Last_gate_time > Gate_interval / 2) {
			restart_effect(BOSS_ECLIP_NUM);
#ifndef SHAREWARE
#ifdef NETWORK
			if (eclip_state == 0) {
				multi_send_boss_actions(objp - Objects.data(), 4, 0, 0);
				eclip_state = 1;
			}
#endif
#endif
		}
		else {
			stop_effect(BOSS_ECLIP_NUM);
#ifndef SHAREWARE
#ifdef NETWORK
			if (eclip_state == 1) {
				multi_send_boss_actions(objnum, 5, 0, 0);
				eclip_state = 0;
			}
#endif
#endif
		}

		if (GameTime - Last_gate_time > Gate_interval)
			if (ai_multiplayer_awareness(objp, 99)) {
				int	rtval;
				int	randtype = (P_Rand() * MAX_GATE_INDEX) >> 15;

				Assert(randtype < MAX_GATE_INDEX);
				randtype = Super_boss_gate_list[randtype];
				Assert(randtype < activeBMTable->robots.size());

				rtval = gate_in_robot(randtype, -1);
#ifndef SHAREWARE
#ifdef NETWORK
				if (rtval && (Game_mode & GM_MULTI))
				{
					multi_send_boss_actions(objnum, 3, randtype, Net_create_objnums[0]);
					map_objnum_local_to_local(Net_create_objnums[0]);
				}
#endif
#endif
			}
	}
}

#endif

// --------------------------------------------------------------------------------------------------------------------
void do_ai_frame_d1(size_t objnum)
{
	object* obj = &Objects[objnum];
	ai_static* aip = &obj->ctype.ai_info;
	ai_local* ailp = &Ai_local_info[objnum];
	fix			dist_to_player;
	vms_vector	vec_to_player;
	fix			dot;
	robot_info* robptr;
	int			player_visibility = -1;
	int			obj_ref;
	int			object_animates;
	int			new_goal_state;
	int			visibility_and_vec_computed = 0;
	int			previous_visibility;
	vms_vector	gun_point;
	vms_vector	vis_vec_pos;

	if (aip->SKIP_AI_COUNT) {
		aip->SKIP_AI_COUNT--;
		return;
	}

	//	Kind of a hack.  If a robot is flinching, but it is time for it to fire, unflinch it.
	//	Else, you can turn a big nasty robot into a wimp by firing flares at it.
	//	This also allows the player to see the cool flinch effect for mechs without unbalancing the game.
	if ((aip->GOAL_STATE == AIS_FLIN) && (ailp->next_fire < 0)) {
		aip->GOAL_STATE = AIS_FIRE;
	}

#ifndef NDEBUG
	if ((aip->behavior == AIB_RUN_FROM) && (ailp->mode != AIM_RUN_FROM_OBJECT))
		//Int3();	//	This is peculiar.  Behavior is run from, but mode is not.  Contact Mike.

	mprintf_animation_info((obj));

	if (Ai_animation_test) {
		if (aip->GOAL_STATE == aip->CURRENT_STATE) {
			aip->GOAL_STATE++;
			if (aip->GOAL_STATE > AIS_RECO)
				aip->GOAL_STATE = AIS_REST;
		}

		mprintf((0, "Frame %4i, current = %i, goal = %i\n", FrameCount, aip->CURRENT_STATE, aip->GOAL_STATE));
		object_animates = do_silly_animation(obj);
		if (object_animates)
			ai_frame_animation(obj);
		return;
	}

	if (!Do_ai_flag)
		return;

	if (Break_on_object != -1)
		if (objnum == Break_on_object)
			Int3();	//	Contact Mike: This is a debug break
#endif

	Believed_player_pos = Ai_cloak_info[objnum & (MAX_AI_CLOAK_INFO - 1)].last_position;

	// mprintf((0, "Object %i: behavior = %02x, mode = %i, awareness = %i, time = %7.3f\n", obj-Objects, aip->behavior, ailp->mode, ailp->player_awareness_type, f2fl(ailp->player_awareness_time)));
	// mprintf((0, "Object %i: behavior = %02x, mode = %i, awareness = %i, cur=%i, goal=%i\n", obj-Objects, aip->behavior, ailp->mode, ailp->player_awareness_type, aip->CURRENT_STATE, aip->GOAL_STATE));

//	Assert((aip->behavior >= MIN_BEHAVIOR) && (aip->behavior <= MAX_BEHAVIOR));
	if (!((aip->behavior >= MIN_BEHAVIOR) && (aip->behavior <= MAX_BEHAVIOR))) {
		// mprintf((0, "Object %i behavior is %i, setting to AIB_NORMAL, fix in editor!\n", objnum, aip->behavior));
		aip->behavior = AIB_NORMAL;
	}

	Assert(obj->segnum != -1);
	Assert(obj->id < activeBMTable->robots.size());

	robptr = &activeBMTable->robots[obj->id];
	Assert(robptr->always_0xabcd == 0xabcd);
	obj_ref = objnum ^ FrameCount;
	// -- if (ailp->wait_time > -F1_0*8)
	// -- 	ailp->wait_time -= FrameTime;
	if (ailp->next_fire > -F1_0 * 8)
		ailp->next_fire -= FrameTime;
	if (ailp->time_since_processed < F1_0 * 256)
		ailp->time_since_processed += FrameTime;
	previous_visibility = ailp->previous_visibility;	//	Must get this before we toast the master copy!

	//	Deal with cloaking for robots which are cloaked except just before firing.
	if (robptr->cloak_type == RI_CLOAKED_EXCEPT_FIRING)
		if (ailp->next_fire < F1_0 / 2)
			aip->CLOAKED = 1;
		else
			aip->CLOAKED = 0;

	if (cheatValues[CI_INFIGHTING]) {
		vis_vec_pos = obj->pos;
		compute_vis_and_vec(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);
		if (player_visibility) {
			int	ii, min_obj = -1;
			fix	min_dist = F1_0 * 200, cur_dist;

			for (ii = 0; ii <= Highest_object_index; ii++)
				if ((Objects[ii].type == OBJ_ROBOT) && (ii != objnum)) {
					cur_dist = vm_vec_dist_quick(&obj->pos, &Objects[ii].pos);

					if (cur_dist < F1_0 * 100)
						if (object_to_object_visibility(obj, &Objects[ii], FQ_TRANSWALL))
							if (cur_dist < min_dist) {
								min_obj = ii;
								min_dist = cur_dist;
							}
				}
			if (min_obj != -1) {
				Believed_player_pos = Objects[min_obj].pos;
				Believed_player_seg = Objects[min_obj].segnum;
				vm_vec_normalized_dir_quick(&vec_to_player, &Believed_player_pos, &obj->pos);
			}
		}
	}

	if (!(Players[Player_num].flags & PLAYER_FLAGS_CLOAKED))
		Believed_player_pos = ConsoleObject->pos;

	dist_to_player = vm_vec_dist_quick(&Believed_player_pos, &obj->pos);

	//--!-- 	mprintf((0, "%2i: %s, [vel = %5.1f %5.1f %5.1f] spd = %4.1f dtp=%5.1f ", objnum, mode_text[ailp->mode], f2fl(obj->mtype.phys_info.velocity.x), f2fl(obj->mtype.phys_info.velocity.y), f2fl(obj->mtype.phys_info.velocity.z), f2fl(vm_vec_mag(&obj->mtype.phys_info.velocity)), f2fl(dist_to_player)));
	//--!-- 	if (ailp->mode == AIM_FOLLOW_PATH) {
	//--!-- 		mprintf((0, "gseg = %i\n", Point_segs[aip->hide_index+aip->cur_path_index].segnum));
	//--!-- 	} else
	//--!-- 		mprintf((0, "\n"));

		//	If this robot can fire, compute visibility from gun position.
		//	Don't want to compute visibility twice, as it is expensive.  (So is call to calc_gun_point).
	if ((ailp->next_fire <= 0) && (dist_to_player < F1_0 * 200) && (robptr->n_guns) && !(robptr->attack_type)) {
		calc_gun_point(&gun_point, obj, aip->CURRENT_GUN);
		vis_vec_pos = gun_point;
		// mprintf((0, "Visibility = %i, computed from gun #%i\n", player_visibility, aip->CURRENT_GUN));
	}
	else {
		vis_vec_pos = obj->pos;
		vm_vec_zero(&gun_point);
		// mprintf((0, "Visibility = %i, computed from center.\n", player_visibility));
	}

	//	- -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -
	//	Occasionally make non-still robots make a path to the player.  Based on agitation and distance from player.
	if ((aip->behavior != AIB_RUN_FROM) && (aip->behavior != AIB_STILL) && !(Game_mode & GM_MULTI))
		if (Overall_agitation > 70) {
			if ((dist_to_player < F1_0 * 200) && (P_Rand() < FrameTime / 4)) {
				if (P_Rand() * (Overall_agitation - 40) > F1_0 * 5) {
					// -- mprintf((0, "(1) Object #%i going from still to path in frame %i.\n", objnum, FrameCount));
					create_path_to_player(obj, 4 + Overall_agitation / 8 + Difficulty_level, 1);
					// -- show_path_and_other(obj);
					return;
				}
			}
		}

	//	- -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -
	//	If retry count not 0, then add it into consecutive_retries.
	//	If it is 0, cut down consecutive_retries.
	//	This is largely a hack to speed up physics and deal with stupid AI.  This is low level
	//	communication between systems of a sort that should not be done.
	if ((ailp->retry_count) && !(Game_mode & GM_MULTI)) {
		ailp->consecutive_retries += ailp->retry_count;
		ailp->retry_count = 0;
		if (ailp->consecutive_retries > 3) {
			switch (ailp->mode) {
			case AIM_CHASE_OBJECT:
				// -- mprintf((0, "(2) Object #%i, retries while chasing, creating path to player in frame %i\n", objnum, FrameCount));
				create_path_to_player(obj, 4 + Overall_agitation / 8 + Difficulty_level, 1);
				break;
			case AIM_STILL:
				if (!((aip->behavior == AIB_STILL) || (aip->behavior == AIB_STATION)))	//	Behavior is still, so don't follow path.
					attempt_to_resume_path(obj);
				break;
			case AIM_FOLLOW_PATH:
				// mprintf((0, "Object %i following path got %i retries in frame %i\n", obj-Objects, ailp->consecutive_retries, FrameCount));
				if (Game_mode & GM_MULTI)
					ailp->mode = AIM_STILL;
				else
					attempt_to_resume_path(obj);
				break;
			case AIM_RUN_FROM_OBJECT:
				move_towards_segment_center(obj);
				obj->mtype.phys_info.velocity.x = 0;
				obj->mtype.phys_info.velocity.y = 0;
				obj->mtype.phys_info.velocity.z = 0;
				create_n_segment_path(obj, 5, -1);
				ailp->mode = AIM_RUN_FROM_OBJECT;
				break;
			case AIM_HIDE:
				move_towards_segment_center(obj);
				obj->mtype.phys_info.velocity.x = 0;
				obj->mtype.phys_info.velocity.y = 0;
				obj->mtype.phys_info.velocity.z = 0;
				// -- mprintf((0, "Hiding, yet creating path to player.\n"));
				if (Overall_agitation > (50 - Difficulty_level * 4))
					create_path_to_player(obj, 4 + Overall_agitation / 8, 1);
				else {
					create_n_segment_path(obj, 5, -1);
				}
				break;
			case AIM_OPEN_DOOR:
				create_n_segment_path_to_door(obj, 5, -1);
				break;
#ifndef NDEBUG
			case AIM_FOLLOW_PATH_2:
				Int3();	//	Should never happen!
				break;
#endif
			}
			ailp->consecutive_retries = 0;
		}
	}
	else
		ailp->consecutive_retries /= 2;

	//	- -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -
	//	If in materialization center, exit
	if (!(Game_mode & GM_MULTI) && (Segments[obj->segnum].special == SEGMENT_IS_ROBOTMAKER)) {
		ai_follow_path(obj, 1, 0, nullptr);		// 1 = player is visible, which might be a lie, but it works.
		return;
	}

	//	- -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -
	//	Decrease player awareness due to the passage of time.



	//	- -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -
	//	Decrease player awareness due to the passage of time.
	if (ailp->player_awareness_type) {
		if (ailp->player_awareness_time > 0) {
			ailp->player_awareness_time -= FrameTime;
			if (ailp->player_awareness_time <= 0) {
				ailp->player_awareness_time = F1_0 * 2;	//new: 11/05/94
				ailp->player_awareness_type--;	//new: 11/05/94
			}
		}
		else {
			ailp->player_awareness_type--;
			ailp->player_awareness_time = F1_0 * 2;
			// aip->GOAL_STATE = AIS_REST;
		}
	}
	else
		aip->GOAL_STATE = AIS_REST;							//new: 12/13/94


	if (Player_is_dead && (ailp->player_awareness_type == 0))
		if ((dist_to_player < F1_0 * 200) && (P_Rand() < FrameTime / 8)) {
			if ((aip->behavior != AIB_STILL) && (aip->behavior != AIB_RUN_FROM)) {
				if (!ai_multiplayer_awareness(obj, 30))
					return;
				ai_multi_send_robot_position(objnum, -1);

				if (!((ailp->mode == AIM_FOLLOW_PATH) && (aip->cur_path_index < aip->path_length - 1)))
					if (dist_to_player < F1_0 * 30)
						create_n_segment_path(obj, 5, 1);
					else
						create_path_to_player(obj, 20, 1);
			}
		}

	//	Make sure that if this guy got hit or bumped, then he's chasing player.
	if ((ailp->player_awareness_type == PA_WEAPON_ROBOT_COLLISION) || (ailp->player_awareness_type >= PA_PLAYER_COLLISION)) {
		if ((aip->behavior != AIB_STILL) && (aip->behavior != AIB_FOLLOW_PATH) && (aip->behavior != AIB_RUN_FROM) && (obj->id != ROBOT_BRAIN))
			ailp->mode = AIM_CHASE_OBJECT;
	}

	//	- -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -
	if ((aip->GOAL_STATE == AIS_FLIN) && (aip->CURRENT_STATE == AIS_FLIN))
		aip->GOAL_STATE = AIS_LOCK;

	//	- -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -
	//	Note: Should only do these two function calls for objects which animate
	if ((dist_to_player < F1_0 * 100)) { // && !(Game_mode & GM_MULTI)) {
		object_animates = do_silly_animation(obj);
		if (object_animates)
			ai_frame_animation(obj);
		//mprintf((0, "Object %i: goal=%i, current=%i\n", obj-Objects, obj->ctype.ai_info.GOAL_STATE, obj->ctype.ai_info.CURRENT_STATE));
	}
	else {
		//	If Object is supposed to animate, but we don't let it animate due to distance, then
		//	we must change its state, else it will never update.
		aip->CURRENT_STATE = aip->GOAL_STATE;
		object_animates = 0;		//	If we're not doing the animation, then should pretend it doesn't animate.
	}

	//Processed_this_frame++;
	//if (FrameCount != LastFrameCount) {
	//	LastFrameCount = FrameCount;
	//	mprintf((0, "Processed in frame %i = %i robots\n", FrameCount-1, Processed_this_frame));
	//	Processed_this_frame = 0;
	//}

	switch (activeBMTable->robots[obj->id].boss_flag) {
	case 0:
		break;
	case 1:
		if (aip->GOAL_STATE == AIS_FLIN)
			aip->GOAL_STATE = AIS_FIRE;
		if (aip->CURRENT_STATE == AIS_FLIN)
			aip->CURRENT_STATE = AIS_FIRE;
		dist_to_player /= 4;

		do_boss_stuff(obj, 0);
		obj = &Objects[objnum];
		dist_to_player *= 4;
		break;
#ifndef SHAREWARE
	case 2:
		if (aip->GOAL_STATE == AIS_FLIN)
			aip->GOAL_STATE = AIS_FIRE;
		if (aip->CURRENT_STATE == AIS_FLIN)
			aip->CURRENT_STATE = AIS_FIRE;
		compute_vis_and_vec(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);

		{	int pv = player_visibility;
		fix	dtp = dist_to_player / 4;

		//	If player cloaked, visibility is screwed up and superboss will gate in robots when not supposed to.
		if (Players[Player_num].flags & PLAYER_FLAGS_CLOAKED) {
			pv = 0;
			dtp = vm_vec_dist_quick(&ConsoleObject->pos, &obj->pos) / 4;
		}

		do_super_boss_stuff(obj, dtp, pv);
		obj = &Objects[objnum];
		}
		break;
#endif
	default:
		Int3();	//	Bogus boss flag value.
	}

	//	- -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -
	//	Time-slice, don't process all the time, purely an efficiency hack.
	//	Guys whose behavior is station and are not at their hide segment get processed anyway.
	if (ailp->player_awareness_type < PA_WEAPON_ROBOT_COLLISION - 1) { // If robot got hit, he gets to attack player always!
#ifndef NDEBUG
		if (Break_on_object != objnum) {	//	don't time slice if we're interested in this object.
#endif
			if ((dist_to_player > F1_0 * 250) && (ailp->time_since_processed <= F1_0 * 2))
				return;
			else if (!((aip->behavior == AIB_STATION) && (ailp->mode == AIM_FOLLOW_PATH) && (aip->hide_segment != obj->segnum))) {
				if ((dist_to_player > F1_0 * 150) && (ailp->time_since_processed <= F1_0))
					return;
				else if ((dist_to_player > F1_0 * 100) && (ailp->time_since_processed <= F1_0 / 2))
					return;
			}
#ifndef NDEBUG
		}
#endif
	}

	// -- 	if ((aip->behavior == AIB_STATION) && (ailp->mode == AIM_FOLLOW_PATH) && (aip->hide_segment != obj->segnum))
	// -- 		mprintf((0, "[%i] ", obj-Objects));

		//	Reset time since processed, but skew objects so not everything processed synchronously, else
		//	we get fast frames with the occasional very slow frame.
		// AI_proc_time = ailp->time_since_processed;
	ailp->time_since_processed = -((objnum & 0x03) * FrameTime) / 2;

	//	- -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -
	//	Perform special ability
	switch (obj->id) {
	case ROBOT_BRAIN:
		//	Robots function nicely if behavior is Station.  This means they won't move until they
		//	can see the player, at which time they will start wandering about opening doors.
		if (ConsoleObject->segnum == obj->segnum) {
			if (!ai_multiplayer_awareness(obj, 97))
				return;
			compute_vis_and_vec(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);
			move_away_from_player(obj, &vec_to_player, 0);
			ai_multi_send_robot_position(objnum, -1);
		}
		else if (ailp->mode != AIM_STILL) {
			int	r;

			r = openable_doors_in_segment(obj->segnum);
			if (r != -1) {
				ailp->mode = AIM_OPEN_DOOR;
				aip->GOALSIDE = r;
			}
			else if (ailp->mode != AIM_FOLLOW_PATH) {
				if (!ai_multiplayer_awareness(obj, 50))
					return;
				create_n_segment_path_to_door(obj, 8 + Difficulty_level, -1);		//	third parameter is avoid_seg, -1 means avoid nothing.
				ai_multi_send_robot_position(objnum, -1);
			}
		}
		else {
			compute_vis_and_vec(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);
			if (player_visibility) {
				if (!ai_multiplayer_awareness(obj, 50))
					return;
				create_n_segment_path_to_door(obj, 8 + Difficulty_level, -1);		//	third parameter is avoid_seg, -1 means avoid nothing.
				ai_multi_send_robot_position(objnum, -1);
			}
		}
		break;
	default:
		break;
	}

	//	- -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -
	switch (ailp->mode) {
	case AIM_CHASE_OBJECT: {		// chasing player, sort of, chase if far, back off if close, circle in between
		fix	circle_distance;

		circle_distance = robptr->circle_distance[Difficulty_level] + ConsoleObject->size;
		//	Green guy doesn't get his circle distance boosted, else he might never attack.
		if (robptr->attack_type != 1)
			circle_distance += (objnum & 0xf) * F1_0 / 2;

		compute_vis_and_vec(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);

		//	@mk, 12/27/94, structure here was strange.  Would do both clauses of what are now this if/then/else.  Used to be if/then, if/then.
		if ((player_visibility < 2) && (previous_visibility == 2)) { // this is redundant: mk, 01/15/95: && (ailp->mode == AIM_CHASE_OBJECT)) {
			// mprintf((0, "I used to be able to see the player!\n"));
			if (!ai_multiplayer_awareness(obj, 53)) {
				if (maybe_ai_do_actual_firing_stuff(obj, aip))
					ai_do_actual_firing_stuff(obj, aip, ailp, robptr, &vec_to_player, dist_to_player, &gun_point, player_visibility, object_animates, aip->CURRENT_GUN);
				return;
			}
			// -- mprintf((0, "(3) Object #%i going from chase to player path in frame %i.\n", objnum, FrameCount));
			create_path_to_player(obj, 8, 1);
			// -- show_path_and_other(obj);
			ai_multi_send_robot_position(objnum, -1);
		}
		else if ((player_visibility == 0) && (dist_to_player > F1_0 * 80) && (!(Game_mode & GM_MULTI))) {
			//	If pretty far from the player, player cannot be seen (obstructed) and in chase mode, switch to follow path mode.
			//	This has one desirable benefit of avoiding physics retries.
			if (aip->behavior == AIB_STATION) {
				ailp->goal_segment = aip->hide_segment;
				// -- mprintf((0, "(1) Object #%i going from chase to STATION in frame %i.\n", objnum, FrameCount));
				create_path_to_station(obj, 15);
				// -- show_path_and_other(obj);
			}
			else
				create_n_segment_path(obj, 5, -1);
			break;
		}

		if ((aip->CURRENT_STATE == AIS_REST) && (aip->GOAL_STATE == AIS_REST)) {
			if (player_visibility) {
				if (P_Rand() < FrameTime * player_visibility) {
					if (dist_to_player / 256 < P_Rand() * player_visibility) {
						// mprintf((0, "Object %i searching for player.\n", obj-Objects));
						aip->GOAL_STATE = AIS_SRCH;
						aip->CURRENT_STATE = AIS_SRCH;
					}
				}
			}
		}

		if (GameTime - ailp->time_player_seen > CHASE_TIME_LENGTH) {

			if (Game_mode & GM_MULTI)
				if (!player_visibility && (dist_to_player > F1_0 * 70)) {
					ailp->mode = AIM_STILL;
					return;
				}

			if (!ai_multiplayer_awareness(obj, 64)) {
				if (maybe_ai_do_actual_firing_stuff(obj, aip))
					ai_do_actual_firing_stuff(obj, aip, ailp, robptr, &vec_to_player, dist_to_player, &gun_point, player_visibility, object_animates, aip->CURRENT_GUN);
				return;
			}
			// -- mprintf((0, "(4) Object #%i going from chase to player path in frame %i.\n", objnum, FrameCount));
			create_path_to_player(obj, 10, 1);
			// -- show_path_and_other(obj);
			ai_multi_send_robot_position(objnum, -1);
		}
		else if ((aip->CURRENT_STATE != AIS_REST) && (aip->GOAL_STATE != AIS_REST)) {
			if (!ai_multiplayer_awareness(obj, 70)) {
				if (maybe_ai_do_actual_firing_stuff(obj, aip))
					ai_do_actual_firing_stuff(obj, aip, ailp, robptr, &vec_to_player, dist_to_player, &gun_point, player_visibility, object_animates, aip->CURRENT_GUN);
				return;
			}
			ai_move_relative_to_player(obj, ailp, dist_to_player, &vec_to_player, circle_distance, 0, 0);

			if ((obj_ref & 1) && ((aip->GOAL_STATE == AIS_SRCH) || (aip->GOAL_STATE == AIS_LOCK))) {
				if (player_visibility) // == 2)
					ai_turn_towards_vector(&vec_to_player, obj, robptr->turn_time[Difficulty_level]);
				else
					ai_turn_randomly(&vec_to_player, obj, robptr->turn_time[Difficulty_level], previous_visibility);
			}

			if (ai_evaded) {
				ai_multi_send_robot_position(objnum, 1);
				ai_evaded = 0;
			}
			else
				ai_multi_send_robot_position(objnum, -1);

			do_firing_stuff(obj, player_visibility, &vec_to_player);
			obj = &Objects[objnum];
		}
		break;
	}

	case AIM_RUN_FROM_OBJECT:
		compute_vis_and_vec(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);

		if (player_visibility) {
			if (ailp->player_awareness_type == 0)
				ailp->player_awareness_type = PA_WEAPON_ROBOT_COLLISION;

		}

		//	If in multiplayer, only do if player visible.  If not multiplayer, do always.
		if (!(Game_mode & GM_MULTI) || player_visibility)
			if (ai_multiplayer_awareness(obj, 75)) {
				ai_follow_path(obj, player_visibility, 0, nullptr);
				ai_multi_send_robot_position(objnum, -1);
			}

		if (aip->GOAL_STATE != AIS_FLIN)
			aip->GOAL_STATE = AIS_LOCK;
		else if (aip->CURRENT_STATE == AIS_FLIN)
			aip->GOAL_STATE = AIS_LOCK;

		//	Bad to let run_from robot fire at player because it will cause a war in which it turns towards the
		//	player to fire and then towards its goal to move.
		// do_firing_stuff(obj, player_visibility, &vec_to_player);
		//	Instead, do this:
		//	(Note, only drop if player is visible.  This prevents the bombs from being a giveaway, and
		//	also ensures that the robot is moving while it is dropping.  Also means fewer will be dropped.)
		if ((ailp->next_fire <= 0) && (player_visibility)) {
			vms_vector	fire_vec, fire_pos;

			if (!ai_multiplayer_awareness(obj, 75))
				return;

			fire_vec = obj->orient.fvec;
			vm_vec_negate(&fire_vec);
			vm_vec_add(&fire_pos, &obj->pos, &fire_vec);

			Laser_create_new_easy(&fire_vec, &fire_pos, objnum, PROXIMITY_ID, 1);
			obj = &Objects[objnum];
			ailp->next_fire = F1_0 * 5;		//	Drop a proximity bomb every 5 seconds.

#ifdef NETWORK
			if (Game_mode & GM_MULTI)
			{
				ai_multi_send_robot_position(objnum, -1);
				multi_send_robot_fire(objnum, -1, &fire_vec);
			}
#endif	
		}
		break;

	case AIM_FOLLOW_PATH: {
		int	anger_level = 65;

		if (aip->behavior == AIB_STATION)
			if (Point_segs[aip->hide_index + aip->path_length - 1].segnum == aip->hide_segment) {
				anger_level = 64;
				// mprintf((0, "Object %i, station, lowering anger to 64.\n"));
			}

		compute_vis_and_vec(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);

		//[ISB MODEM]unused modem game logic 
		/*if (Game_mode & (GM_MODEM | GM_SERIAL))
			if (!player_visibility && (dist_to_player > F1_0 * 70)) {
				ailp->mode = AIM_STILL;
				return;
			}*/

		if (!ai_multiplayer_awareness(obj, anger_level)) {
			if (maybe_ai_do_actual_firing_stuff(obj, aip)) {
				compute_vis_and_vec(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);
				ai_do_actual_firing_stuff(obj, aip, ailp, robptr, &vec_to_player, dist_to_player, &gun_point, player_visibility, object_animates, aip->CURRENT_GUN);
			}
			return;
		}

		ai_follow_path(obj, player_visibility, 0, nullptr);

		if (aip->GOAL_STATE != AIS_FLIN)
			aip->GOAL_STATE = AIS_LOCK;
		else if (aip->CURRENT_STATE == AIS_FLIN)
			aip->GOAL_STATE = AIS_LOCK;

		if ((aip->behavior != AIB_FOLLOW_PATH) && (aip->behavior != AIB_RUN_FROM)) {
			do_firing_stuff(obj, player_visibility, &vec_to_player);
			obj = &Objects[objnum];
		}

		if ((player_visibility == 2) && (aip->behavior != AIB_FOLLOW_PATH) && (aip->behavior != AIB_RUN_FROM) && (obj->id != ROBOT_BRAIN)) {
			if (robptr->attack_type == 0)
				ailp->mode = AIM_CHASE_OBJECT;
		}
		else if ((player_visibility == 0) && (aip->behavior == AIB_NORMAL) && (ailp->mode == AIM_FOLLOW_PATH) && (aip->behavior != AIB_RUN_FROM)) {
			ailp->mode = AIM_STILL;
			aip->hide_index = -1;
			aip->path_length = 0;
		}

		ai_multi_send_robot_position(objnum, -1);

		break;
	}

	case AIM_HIDE:
		if (!ai_multiplayer_awareness(obj, 71)) {
			if (maybe_ai_do_actual_firing_stuff(obj, aip)) {
				compute_vis_and_vec(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);
				ai_do_actual_firing_stuff(obj, aip, ailp, robptr, &vec_to_player, dist_to_player, &gun_point, player_visibility, object_animates, aip->CURRENT_GUN);
			}
			return;
		}

		compute_vis_and_vec(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);

		ai_follow_path(obj, player_visibility, 0, nullptr);

		if (aip->GOAL_STATE != AIS_FLIN)
			aip->GOAL_STATE = AIS_LOCK;
		else if (aip->CURRENT_STATE == AIS_FLIN)
			aip->GOAL_STATE = AIS_LOCK;

		ai_multi_send_robot_position(objnum, -1);
		break;

	case AIM_STILL:
		if ((dist_to_player < F1_0 * 120 + Difficulty_level * F1_0 * 20) || (ailp->player_awareness_type >= PA_WEAPON_ROBOT_COLLISION - 1)) {
			compute_vis_and_vec(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);

			//	turn towards vector if visible this time or last time, or rand
			// new!
			if ((player_visibility) || (previous_visibility) || ((P_Rand() > 0x4000) && !(Game_mode & GM_MULTI))) {
				if (!ai_multiplayer_awareness(obj, 71)) {
					if (maybe_ai_do_actual_firing_stuff(obj, aip))
						ai_do_actual_firing_stuff(obj, aip, ailp, robptr, &vec_to_player, dist_to_player, &gun_point, player_visibility, object_animates, aip->CURRENT_GUN);
					return;
				}
				ai_turn_towards_vector(&vec_to_player, obj, robptr->turn_time[Difficulty_level]);
				ai_multi_send_robot_position(objnum, -1);
			}

			do_firing_stuff(obj, player_visibility, &vec_to_player);
			obj = &Objects[objnum];
			//	This is debugging code!  Remove it!  It's to make the green guy attack without doing other kinds of movement.
			if (player_visibility) {		//	Change, MK, 01/03/94 for Multiplayer reasons.  If robots can't see you (even with eyes on back of head), then don't do evasion.
				if (robptr->attack_type == 1) {
					aip->behavior = AIB_NORMAL;
					if (!ai_multiplayer_awareness(obj, 80)) {
						if (maybe_ai_do_actual_firing_stuff(obj, aip))
							ai_do_actual_firing_stuff(obj, aip, ailp, robptr, &vec_to_player, dist_to_player, &gun_point, player_visibility, object_animates, aip->CURRENT_GUN);
						return;
					}
					ai_move_relative_to_player(obj, ailp, dist_to_player, &vec_to_player, 0, 0, 0);
					if (ai_evaded) {
						ai_multi_send_robot_position(objnum, 1);
						ai_evaded = 0;
					}
					else
						ai_multi_send_robot_position(objnum, -1);
				}
				else {
					//	Robots in hover mode are allowed to evade at half normal speed.
					if (!ai_multiplayer_awareness(obj, 81)) {
						if (maybe_ai_do_actual_firing_stuff(obj, aip))
							ai_do_actual_firing_stuff(obj, aip, ailp, robptr, &vec_to_player, dist_to_player, &gun_point, player_visibility, object_animates, aip->CURRENT_GUN);
						return;
					}
					ai_move_relative_to_player(obj, ailp, dist_to_player, &vec_to_player, 0, 1, 0);
					if (ai_evaded) {
						ai_multi_send_robot_position(objnum, -1);
						ai_evaded = 0;
					}
					else
						ai_multi_send_robot_position(objnum, -1);
				}
			}
			else if ((obj->segnum != aip->hide_segment) && (dist_to_player > F1_0 * 80) && (!(Game_mode & GM_MULTI))) {
				//	If pretty far from the player, player cannot be seen (obstructed) and in chase mode, switch to follow path mode.
				//	This has one desirable benefit of avoiding physics retries.
				if (aip->behavior == AIB_STATION) {
					ailp->goal_segment = aip->hide_segment;
					// -- mprintf((0, "(2) Object #%i going from STILL to STATION in frame %i.\n", objnum, FrameCount));
					create_path_to_station(obj, 15);
					// -- show_path_and_other(obj);
				}
				break;
			}
		}

		break;
	case AIM_OPEN_DOOR: {		// trying to open a door.
		vms_vector	center_point, goal_vector;
		Assert(obj->id == ROBOT_BRAIN);		//	Make sure this guy is allowed to be in this mode.

		if (!ai_multiplayer_awareness(obj, 62))
			return;
		compute_center_point_on_side(&center_point, &Segments[obj->segnum], aip->GOALSIDE);
		vm_vec_sub(&goal_vector, &center_point, &obj->pos);
		vm_vec_normalize_quick(&goal_vector);
		ai_turn_towards_vector(&goal_vector, obj, robptr->turn_time[Difficulty_level]);
		move_towards_vector(obj, &goal_vector, true);
		ai_multi_send_robot_position(objnum, -1);

		break;
	}

	default:
		mprintf((0, "Unknown mode = %i in robot %i, behavior = %i\n", ailp->mode, objnum, aip->behavior));
		ailp->mode = AIM_CHASE_OBJECT;
		break;
	}		// end:	switch (ailp->mode) {

	//	- -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -
	//	If the robot can see you, increase his awareness of you.
	//	This prevents the problem of a robot looking right at you but doing nothing.
	// Assert(player_visibility != -1);	//	Means it didn't get initialized!
	compute_vis_and_vec(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);
	if (player_visibility == 2)
		if (ailp->player_awareness_type == 0)
			ailp->player_awareness_type = PA_PLAYER_COLLISION;

	//	- -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -
	if (!object_animates) {
		aip->CURRENT_STATE = aip->GOAL_STATE;
		// mprintf((0, "Setting current to goal (%i) because object doesn't animate.\n", aip->GOAL_STATE));
	}

	Assert(ailp->player_awareness_type <= AIE_MAX);
	Assert(aip->CURRENT_STATE < AIS_MAX);
	Assert(aip->GOAL_STATE < AIS_MAX);

	//	- -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -
	if (ailp->player_awareness_type) {
		new_goal_state = Ai_transition_table[ailp->player_awareness_type - 1][aip->CURRENT_STATE][aip->GOAL_STATE];
		if (ailp->player_awareness_type == PA_WEAPON_ROBOT_COLLISION) {
			//	Decrease awareness, else this robot will flinch every frame.
			ailp->player_awareness_type--;
			ailp->player_awareness_time = F1_0 * 3;
		}

		if (new_goal_state == AIS_ERR_)
			new_goal_state = AIS_REST;

		if (aip->CURRENT_STATE == AIS_NONE)
			aip->CURRENT_STATE = AIS_REST;

		aip->GOAL_STATE = new_goal_state;

	}

	//	- -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -
	//	If new state = fire, then set all gun states to fire.
	if ((aip->GOAL_STATE == AIS_FIRE)) {
		int	i, num_guns;
		num_guns = activeBMTable->robots[obj->id].n_guns;
		for (i = 0; i < num_guns; i++)
			ailp->goal_state[i] = AIS_FIRE;
	}

	//	- -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -  - -
	//	Hack by mk on 01/04/94, if a guy hasn't animated to the firing state, but his next_fire says ok to fire, bash him there
	if ((ailp->next_fire < 0) && (aip->GOAL_STATE == AIS_FIRE))
		aip->CURRENT_STATE = AIS_FIRE;

	if ((aip->GOAL_STATE != AIS_FLIN) && (obj->id != ROBOT_BRAIN)) {
		switch (aip->CURRENT_STATE) {
		case	AIS_NONE:
			compute_vis_and_vec(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);

			dot = vm_vec_dot(&obj->orient.fvec, &vec_to_player);
			if (dot >= F1_0 / 2)
				if (aip->GOAL_STATE == AIS_REST)
					aip->GOAL_STATE = AIS_SRCH;
			//mprintf((0, "State = none, goal = %i.\n", aip->GOAL_STATE));
			break;
		case	AIS_REST:
			if (aip->GOAL_STATE == AIS_REST) {
				compute_vis_and_vec(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);
				if ((ailp->next_fire <= 0) && (player_visibility)) {
					// mprintf((0, "Setting goal state to fire from rest.\n"));
					aip->GOAL_STATE = AIS_FIRE;
				}
				//mprintf((0, "State = rest, goal = %i.\n", aip->GOAL_STATE));
			}
			break;
		case	AIS_SRCH:
			if (!ai_multiplayer_awareness(obj, 60))
				return;

			compute_vis_and_vec(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);

			if (player_visibility) {
				ai_turn_towards_vector(&vec_to_player, obj, robptr->turn_time[Difficulty_level]);
				ai_multi_send_robot_position(objnum, -1);
			}
			else if (!(Game_mode & GM_MULTI))
				ai_turn_randomly(&vec_to_player, obj, robptr->turn_time[Difficulty_level], previous_visibility);
			break;
		case	AIS_LOCK:
			compute_vis_and_vec(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);

			if (!(Game_mode & GM_MULTI) || (player_visibility)) {
				if (!ai_multiplayer_awareness(obj, 68))
					return;

				if (player_visibility) {
					ai_turn_towards_vector(&vec_to_player, obj, robptr->turn_time[Difficulty_level]);
					ai_multi_send_robot_position(objnum, -1);
				}
				else if (!(Game_mode & GM_MULTI))
					ai_turn_randomly(&vec_to_player, obj, robptr->turn_time[Difficulty_level], previous_visibility);
			}
			break;
		case	AIS_FIRE:
			compute_vis_and_vec(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);

			if (player_visibility) {
				if (!ai_multiplayer_awareness(obj, (ROBOT_FIRE_AGITATION - 1)))
				{
					if (Game_mode & GM_MULTI) {
						ai_do_actual_firing_stuff(obj, aip, ailp, robptr, &vec_to_player, dist_to_player, &gun_point, player_visibility, object_animates, aip->CURRENT_GUN);
						return;
					}
				}
				ai_turn_towards_vector(&vec_to_player, obj, robptr->turn_time[Difficulty_level]);
				ai_multi_send_robot_position(objnum, -1);
			}
			else if (!(Game_mode & GM_MULTI)) {
				ai_turn_randomly(&vec_to_player, obj, robptr->turn_time[Difficulty_level], previous_visibility);
			}

			//	Fire at player, if appropriate.
			ai_do_actual_firing_stuff(obj, aip, ailp, robptr, &vec_to_player, dist_to_player, &gun_point, player_visibility, object_animates, aip->CURRENT_GUN);
			obj = &Objects[objnum];

			break;
		case	AIS_RECO:
			if (!(obj_ref & 3)) {
				compute_vis_and_vec(obj, &vis_vec_pos, ailp, &vec_to_player, &player_visibility, robptr, &visibility_and_vec_computed);
				if (player_visibility) {
					if (!ai_multiplayer_awareness(obj, 69))
						return;
					ai_turn_towards_vector(&vec_to_player, obj, robptr->turn_time[Difficulty_level]);
					ai_multi_send_robot_position(objnum, -1);
				}
				else if (!(Game_mode & GM_MULTI)) {
					ai_turn_randomly(&vec_to_player, obj, robptr->turn_time[Difficulty_level], previous_visibility);
				}
			}
			break;
		case	AIS_FLIN:
			// mprintf((0, "State = flinch, goal = %i.\n", aip->GOAL_STATE));
			break;
		default:
			mprintf((1, "Unknown mode for AI object #%i\n", objnum));
			aip->GOAL_STATE = AIS_REST;
			aip->CURRENT_STATE = AIS_REST;
			break;
		}
	} // end of: if (aip->GOAL_STATE != AIS_FLIN) {

	// Switch to next gun for next fire.
	if (player_visibility == 0) {
		aip->CURRENT_GUN++;
		if (aip->CURRENT_GUN >= activeBMTable->robots[obj->id].n_guns)
			aip->CURRENT_GUN = 0;
	}

}

int ai_save_state_d1(FILE* fp)
{
	int i;
	
	file_write_int(fp, Ai_initialized);
	file_write_int(fp, Overall_agitation);
	file_write_int(fp, Highest_object_index + 1);
	for (i = 0; i <= Highest_object_index; i++)
		P_WriteAILocals(&Ai_local_info[i], fp);
	for (i = 0; i < MAX_POINT_SEGS; i++)
		P_WriteSegPoint(&Point_segs[i], fp);
	for (i = 0; i < MAX_AI_CLOAK_INFO; i++)
		P_WriteCloakInfo(&Ai_cloak_info[i], fp);

	file_write_int(fp, Boss_cloak_start_time);
	file_write_int(fp, Boss_cloak_end_time);
	file_write_int(fp, Last_teleport_time);
	file_write_int(fp, Boss_teleport_interval);
	file_write_int(fp, Boss_cloak_interval);
	file_write_int(fp, Boss_cloak_duration);
	file_write_int(fp, Last_gate_time);
	file_write_int(fp, Gate_interval);
	file_write_int(fp, Boss_dying_start_time);
	file_write_int(fp, Boss_dying);
	file_write_int(fp, Boss_dying_sound_playing);
	file_write_int(fp, Boss_hit_this_frame);
	file_write_int(fp, Boss_been_hit);

	return 1;
}

int ai_restore_state_d1(FILE* fp)
{
	int i;

	Ai_initialized = file_read_int(fp);
	Overall_agitation = file_read_int(fp);
	int numObjects = file_read_int(fp);
	//Assert(Objects.size() == numObjects);
	for (i = 0; i < numObjects; i++)
		P_ReadAILocals(&Ai_local_info[i], fp);
	for (i = 0; i < MAX_POINT_SEGS; i++)
		P_ReadSegPoint(&Point_segs[i], fp);
	for (i = 0; i < MAX_AI_CLOAK_INFO; i++)
		P_ReadCloakInfo(&Ai_cloak_info[i], fp);

	Boss_cloak_start_time = file_read_int(fp);
	Boss_cloak_end_time = file_read_int(fp);
	Last_teleport_time = file_read_int(fp);
	Boss_teleport_interval = file_read_int(fp);
	Boss_cloak_interval = file_read_int(fp);
	Boss_cloak_duration = file_read_int(fp);
	Last_gate_time = file_read_int(fp);
	Gate_interval = file_read_int(fp);
	Boss_dying_start_time = file_read_int(fp);
	Boss_dying = file_read_int(fp);
	Boss_dying_sound_playing = file_read_int(fp);
	Boss_hit_this_frame = file_read_int(fp);
	Boss_been_hit = file_read_int(fp);

	ai_reset_all_paths();

	return 1;
}