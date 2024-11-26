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
#include <algorithm>

#include "inferno.h"
#include "game.h"
#include "platform/mono.h"
#include "3d/3d.h"

#include "ai.h"
#include "object.h"
#include "render.h"
#include "misc/error.h"
#include "laser.h"
#include "fvi.h"
#include "polyobj.h"
#include "bm.h"
#include "weapon.h"
#include "physics.h"
#include "collide.h"
#include "player.h"
#include "wall.h"
#include "vclip.h"
#include "digi.h"
#include "fireball.h"
#include "morph.h"
#include "effects.h"
#include "platform/timer.h"
#include "sounds.h"
#include "cntrlcen.h"
#include "multibot.h"
#include "multi.h"
#include "network.h"
#include "gameseq.h"
#include "platform/key.h"
#include "powerup.h"
#include "gauges.h"
#include "stringtable.h"
#include "misc/rand.h"
#include "newcheat.h"

#include "ai_ifwd.h"

#ifdef EDITOR
#include "editor\editor.h"
#include "editor\kdefs.h"
#endif

#ifndef NDEBUG
#include "string.h"
#include <time.h>
#endif

extern object* create_morph_robot( segment *segp, vms_vector object_pos, int object_id);

// --------------------------------------------------------------------------------------------------------------------
//	Create a Buddy bot.
//	This automatically happens when you bring up the Buddy menu in a debug version.
//	It is available as a cheat in a non-debug (release) version.
void create_buddy_bot(void)
{
	int	buddy_id;
	vms_vector	object_pos;

	for (buddy_id=0; buddy_id<activeBMTable->robots.size(); buddy_id++)
		if (activeBMTable->robots[buddy_id].companion)
			break;

	if (buddy_id == activeBMTable->robots.size()) {
		mprintf((0, "Can't create Buddy.  No 'companion' bot found in activeBMTable->robots!\n"));
		return;
	}

	compute_segment_center(&object_pos, &Segments[ConsoleObject->segnum]);

	create_morph_robot( &Segments[ConsoleObject->segnum], object_pos, buddy_id);
}

int boss_fits_in_seg(object* boss_objp, int segnum); //[ISB] I really like how watcom c apparently didn't require you to have prototypes for functions. 

extern void init_buddy_for_level(void);

//--debug-- #ifndef NDEBUG
//--debug-- int	Total_turns=0;
//--debug-- int	Prevented_turns=0;
//--debug-- #endif

#define	AI_TURN_SCALE	1
#define	BABY_SPIDER_ID	14
#define	FIRE_AT_NEARBY_PLAYER_THRESHOLD	(F1_0*40)

extern void physics_turn_towards_vector(vms_vector goal_vector, object *obj, fix rate);
extern fix Seismic_tremor_magnitude;

extern int create_gated_robot( int segnum, int object_id, vms_vector* pos);

//	Overall_agitation affects:
//		Widens field of view.  Field of view is in range 0..1 (specified in bitmaps.tbl as N/360 degrees).
//			Overall_agitation/128 subtracted from field of view, making robots see wider.
//		Increases distance to which robot will search to create path to player by Overall_agitation/8 segments.
//		Decreases wait between fire times by Overall_agitation/64 seconds.

// ----------------------------------------------------------------------------------
void set_next_fire_time_d2(object *objp, ai_local *ailp, robot_info *robptr, int gun_num)
{
	//	For guys in snipe mode, they have a 50% shot of getting this shot in free.
	if ((gun_num != 0) || (robptr->weapon_type2 == -1))
		if ((objp->ctype.ai_info.behavior != AIB_SNIPE) || (P_Rand() > 16384))
			ailp->rapidfire_count++;

	if (((gun_num != 0) || (robptr->weapon_type2 == -1)) && (ailp->rapidfire_count < robptr->rapidfire_count[Difficulty_level])) {
		ailp->next_fire = std::min((fix)(F1_0/8), robptr->firing_wait[Difficulty_level]/2);
	} else {
		if ((robptr->weapon_type2 == -1) || (gun_num != 0)) {
			ailp->next_fire = robptr->firing_wait[Difficulty_level];
			if (ailp->rapidfire_count >= robptr->rapidfire_count[Difficulty_level])
				ailp->rapidfire_count = 0;
		} else
			ailp->next_fire2 = robptr->firing_wait2[Difficulty_level];
	}
}
 
extern int Player_exploded;

#define	FIRE_K	8		//	Controls average accuracy of robot firing.  Smaller numbers make firing worse.  Being power of 2 doesn't matter.

// ====================================================================================================================

#define	MIN_LEAD_SPEED		(F1_0*4)
#define	MAX_LEAD_DISTANCE	(F1_0*200)
#define	LEAD_RANGE			(F1_0/2)

// --------------------------------------------------------------------------------------------------------------------
//	Computes point at which projectile fired by robot can hit player given positions, player vel, elapsed time
fix compute_lead_component(fix player_pos, fix robot_pos, fix player_vel, fix elapsed_time)
{
	return fixdiv(player_pos - robot_pos, elapsed_time) + player_vel;
}

// --------------------------------------------------------------------------------------------------------------------
//	Lead the player, returning point to fire at in fire_point.
//	Rules:
//		Player not cloaked
//		Player must be moving at a speed >= MIN_LEAD_SPEED
//		Player not farther away than MAX_LEAD_DISTANCE
//		dot(vector_to_player, player_direction) must be in -LEAD_RANGE..LEAD_RANGE
//		if firing a matter weapon, less leading, based on skill level.
int lead_player(object *objp, vms_vector fire_point, vms_vector believed_player_pos, int gun_num, vms_vector* fire_vec)
{
	fix			dot, player_speed, dist_to_player, max_weapon_speed, projected_time;
	vms_vector	player_movement_dir, vec_to_player;
	int			weapon_type;
	weapon_info	*wptr;
	robot_info	*robptr;

	if (Players[Player_num].flags & PLAYER_FLAGS_CLOAKED)
		return 0;

	player_movement_dir = ConsoleObject->mtype.phys_info.velocity;
	player_speed = vm_vec_normalize_quick(&player_movement_dir);

	if (player_speed < MIN_LEAD_SPEED)
		return 0;

	vm_vec_sub(&vec_to_player, &believed_player_pos, &fire_point);
	dist_to_player = vm_vec_normalize_quick(&vec_to_player);
	if (dist_to_player > MAX_LEAD_DISTANCE)
		return 0;

	dot = vm_vec_dot(&vec_to_player, &player_movement_dir);

	if ((dot < -LEAD_RANGE) || (dot > LEAD_RANGE))
		return 0;

	//	Looks like it might be worth trying to lead the player.
	robptr = &activeBMTable->robots[objp->id];
	weapon_type = robptr->weapon_type;
	if (robptr->weapon_type2 != -1)
		if (gun_num == 0)
			weapon_type = robptr->weapon_type2;

	wptr = &activeBMTable->weapons[weapon_type];
	max_weapon_speed = wptr->speed[Difficulty_level];
	if (max_weapon_speed < F1_0)
		return 0;

	//	Matter weapons:
	//	At Rookie or Trainee, don't lead at all.
	//	At higher skill levels, don't lead as well.  Accomplish this by screwing up max_weapon_speed.
	if (wptr->matter)
		if (Difficulty_level <= 1)
			return 0;
		else
			max_weapon_speed *= (NDL-Difficulty_level);

	projected_time = fixdiv(dist_to_player, max_weapon_speed);

	fire_vec->x = compute_lead_component(believed_player_pos.x, fire_point.x, ConsoleObject->mtype.phys_info.velocity.x, projected_time);
	fire_vec->y = compute_lead_component(believed_player_pos.y, fire_point.y, ConsoleObject->mtype.phys_info.velocity.y, projected_time);
	fire_vec->z = compute_lead_component(believed_player_pos.z, fire_point.z, ConsoleObject->mtype.phys_info.velocity.z, projected_time);

	vm_vec_normalize_quick(fire_vec);

	Assert(vm_vec_dot(fire_vec, &objp->orient.fvec) < 3*F1_0/2);

	//	Make sure not firing at especially strange angle.  If so, try to correct.  If still bad, give up after one try.
	if (vm_vec_dot(fire_vec, &objp->orient.fvec) < F1_0/2) {
		vm_vec_add2(fire_vec, &vec_to_player);
		vm_vec_scale(fire_vec, F1_0/2);
		if (vm_vec_dot(fire_vec, &objp->orient.fvec) < F1_0/2) {
			return 0;
		}
	}

	return 1;
}
 
// --------------------------------------------------------------------------------------------------------------------
//	Note: Parameter vec_to_player is only passed now because guns which aren't on the forward vector from the
//	center of the robot will not fire right at the player.  We need to aim the guns at the player.  Barring that, we cheat.
//	When this routine is complete, the parameter vec_to_player should not be necessary.
void ai_fire_laser_at_player_d2(object *obj, vms_vector fire_point, int gun_num, vms_vector believed_player_pos)
{
	size_t			objnum = obj-Objects.data();
	ai_local		*ailp = &Ai_local_info[objnum];
	robot_info	*robptr = &activeBMTable->robots[obj->id];
	vms_vector	fire_vec;
	vms_vector	bpp_diff;
	int			weapon_type;
	fix			aim, dot;
	int			count;

	Assert(robptr->attack_type == 0);	//	We should never be coming here for the green guy, as he has no laser!

	//	If this robot is only awake because a camera woke it up, don't fire.
	if (obj->ctype.ai_info.SUB_FLAGS & SUB_FLAGS_CAMERA_AWAKE)
		return;

	if (cheatValues[CI_NO_FIRING_D1])
		return;

	if (obj->control_type == CT_MORPH)
		return;

	//	If player is exploded, stop firing.
	if (Player_exploded)
		return;

	if (obj->ctype.ai_info.dying_start_time)
		return;		//	No firing while in death roll.

	//	Don't let the boss fire while in death roll.  Sorry, this is the easiest way to do this.
	//	If you try to key the boss off obj->ctype.ai_info.dying_start_time, it will hose the endlevel stuff.
	if (Boss_dying_start_time & activeBMTable->robots[obj->id].boss_flag)
		return;

	//	If player is cloaked, maybe don't fire based on how long cloaked and randomness.
	if (Players[Player_num].flags & PLAYER_FLAGS_CLOAKED) {
		fix	cloak_time = Ai_cloak_info[objnum % MAX_AI_CLOAK_INFO].last_time;

		if (GameTime - cloak_time > CLOAK_TIME_MAX/4)
			if (P_Rand() > fixdiv(GameTime - cloak_time, CLOAK_TIME_MAX)/2) {
				set_next_fire_time(obj, ailp, robptr, gun_num);
				return;
			}
	}

	//	Handle problem of a robot firing through a wall because its gun tip is on the other
	//	side of the wall than the robot's center.  For speed reasons, we normally only compute
	//	the vector from the gun point to the player.  But we need to know whether the gun point
	//	is separated from the robot's center by a wall.  If so, don't fire!
	if (obj->ctype.ai_info.SUB_FLAGS & SUB_FLAGS_GUNSEG) {
		//	Well, the gun point is in a different segment than the robot's center.
		//	This is almost always ok, but it is not ok if something solid is in between.
		int	conn_side;
		int	gun_segnum = find_point_seg(fire_point, obj->segnum);

		//	See if these segments are connected, which should almost always be the case.
		conn_side = find_connect_side(&Segments[gun_segnum], &Segments[obj->segnum]);
		if (conn_side != -1) {
			//	They are connected via conn_side in segment obj->segnum.
			//	See if they are unobstructed.
			if (!(WALL_IS_DOORWAY(&Segments[obj->segnum], conn_side) & WID_FLY_FLAG)) {
				//	Can't fly through, so don't let this bot fire through!
				return;
			}
		} else {
			//	Well, they are not directly connected, so use find_vector_intersection to see if they are unobstructed.
			fvi_query	fq;
			fvi_info		hit_data;
			int			fate;

			fq.startseg				= obj->segnum;
			fq.p0						= &obj->pos;
			fq.p1						= &fire_point;
			fq.rad					= 0;
			fq.thisobjnum			= objnum;
			fq.ignore_obj_list	= NULL;
			fq.flags					= FQ_TRANSWALL;

			fate = find_vector_intersection(&fq, &hit_data);
			if (fate != HIT_NONE) {
				Int3();		//	This bot's gun is poking through a wall, so don't fire.
				move_towards_segment_center(obj);		//	And decrease chances it will happen again.
				return;
			}
		}
	}

	// -- mprintf((0, "Firing from gun #%i at time = %7.3f\n", gun_num, f2fl(GameTime)));

	//	Set position to fire at based on difficulty level and robot's aiming ability
	aim = FIRE_K*F1_0 - (FIRE_K-1)*(robptr->aim << 8);	//	F1_0 in bitmaps.tbl = same as used to be.  Worst is 50% more error.

	//	Robots aim more poorly during seismic disturbance.
	if (Seismic_tremor_magnitude) {
		fix	temp;

		temp = F1_0 - abs(Seismic_tremor_magnitude);
		if (temp < F1_0/2)
			temp = F1_0/2;

		aim = fixmul(aim, temp);
	}

	//	Lead the player half the time.
	//	Note that when leading the player, aim is perfect.  This is probably acceptable since leading is so hacked in.
	//	Problem is all robots will lead equally badly.
	if (P_Rand() < 16384) {
		if (lead_player(obj, fire_point, believed_player_pos, gun_num, &fire_vec))		//	Stuff direction to fire at in fire_point.
			goto player_led;
	}

	dot = 0;
	count = 0;			//	Don't want to sit in this loop forever...
	while ((count < 4) && (dot < F1_0/4)) {
		bpp_diff.x = believed_player_pos.x + fixmul((P_Rand()-16384) * (NDL-Difficulty_level-1) * 4, aim);
		bpp_diff.y = believed_player_pos.y + fixmul((P_Rand()-16384) * (NDL-Difficulty_level-1) * 4, aim);
		bpp_diff.z = believed_player_pos.z + fixmul((P_Rand()-16384) * (NDL-Difficulty_level-1) * 4, aim);

		vm_vec_normalized_dir_quick(&fire_vec, &bpp_diff, &fire_point);
		dot = vm_vec_dot(&obj->orient.fvec, &fire_vec);
		count++;
	}
player_led: ;

	weapon_type = robptr->weapon_type;
	if (robptr->weapon_type2 != -1)
		if (gun_num == 0)
			weapon_type = robptr->weapon_type2;

	Laser_create_new_easy( fire_vec, fire_point, objnum, weapon_type, 1);
	obj = &Objects[objnum];

#ifdef NETWORK
	if (Game_mode & GM_MULTI) 
	{
		ai_multi_send_robot_position(objnum, -1);
		multi_send_robot_fire(objnum, obj->ctype.ai_info.CURRENT_GUN, &fire_vec);
	}
#endif

	create_awareness_event(obj, PA_NEARBY_ROBOT_FIRED);

	set_next_fire_time(obj, ailp, robptr, gun_num);

}

// --------------------------------------------------------------------------------------------------------------------
//	If a hiding robot gets bumped or hit, he decides to find another hiding place.
void do_ai_robot_hit_d2(object *objp, int type)
{
	if (objp->control_type == CT_AI) {
		if ((type == PA_WEAPON_ROBOT_COLLISION) || (type == PA_PLAYER_COLLISION))
			switch (objp->ctype.ai_info.behavior) {
				case AIB_STILL:
				{
					int	r;

					//	Attack robots (eg, green guy) shouldn't have behavior = still.
					Assert(activeBMTable->robots[objp->id].attack_type == 0);

					r = P_Rand();
					//	1/8 time, charge player, 1/4 time create path, rest of time, do nothing
					if (r < 4096) {
						// -- mprintf((0, "Still guy switching to Station, creating path to player."));
						create_path_to_player(objp, 10, 1);
						objp->ctype.ai_info.behavior = AIB_STATION;
						objp->ctype.ai_info.hide_segment = objp->segnum;
						Ai_local_info[objp-Objects.data()].mode = AIM_CHASE_OBJECT;
					} else if (r < 4096+8192) {
						// -- mprintf((0, "Still guy creating n segment path."));
						create_n_segment_path(objp, P_Rand()/8192 + 2, -1);
						Ai_local_info[objp-Objects.data()].mode = AIM_FOLLOW_PATH;
					}
					break;
				}
			}
	}

}

extern	int	Buddy_objnum;

#define	MAX_SPEW_BOT		3

//Add some rows for D1 bosses until AI merge
int	Spew_bots[NUM_D2_BOSSES + 2][MAX_SPEW_BOT] = {
	{-1, -1, -1},
	{8, 9, 11},
	{38, 40, -1},
	{37, -1, -1},
	{43, 57, -1},
	{26, 27, 58},
	{59, 58, 54},
	{60, 61, 54},

	{69, 29, 24},
	{72, 60, 73} 
};

int	Max_spew_bots[NUM_D2_BOSSES + 2] = {0, 3, 2, 1, 2, 3, 3, 3,  3, 3};

//	----------------------------------------------------------------------------------------------------------
//	objp points at a boss.  He was presumably just hit and he's supposed to create a bot at the hit location *pos.
int boss_spew_robot(object *objp, vms_vector pos)
{
	int		objnum, segnum;
	int		boss_index;

	size_t bobjnum = objp - Objects.data();

	int8_t bossFlag = activeBMTable->robots[objp->id].boss_flag;

	if (bossFlag < BOSS_D2)
		boss_index = bossFlag - 1;
	else
		boss_index = bossFlag - BOSS_D2 + 2;

	Assert((boss_index >= 0) && (boss_index < NUM_D2_BOSSES + 2));

	segnum = find_point_seg(pos, objp->segnum);
	if (segnum == -1)
	{
		mprintf((0, "Tried to spew a bot outside the mine!  Aborting!\n"));
		return -1;
	}	

	objnum = create_gated_robot( segnum, Spew_bots[boss_index][(Max_spew_bots[boss_index] * P_Rand()) >> 15], &pos);
 
	//	Make spewed robot come tumbling out as if blasted by a flash missile.
	if (objnum != -1) 
	{
		objp = &Objects[bobjnum];
		object	*newobjp = &Objects[objnum];
		int		force_val;

		force_val = F1_0/FrameTime;

		if (force_val)
		{
			newobjp->ctype.ai_info.SKIP_AI_COUNT += force_val;
			newobjp->mtype.phys_info.rotthrust.x = ((P_Rand() - 16384) * force_val)/16;
			newobjp->mtype.phys_info.rotthrust.y = ((P_Rand() - 16384) * force_val)/16;
			newobjp->mtype.phys_info.rotthrust.z = ((P_Rand() - 16384) * force_val)/16;
			newobjp->mtype.phys_info.flags |= PF_USES_THRUST;

			//	Now, give a big initial velocity to get moving away from boss.
			vm_vec_sub(&newobjp->mtype.phys_info.velocity, &pos, &objp->pos);
			vm_vec_normalize_quick(&newobjp->mtype.phys_info.velocity);
			vm_vec_scale(&newobjp->mtype.phys_info.velocity, F1_0*128);
		}
	}

	return objnum;
}

// --------------------------------------------------------------------------------------------------------------------
//	Call this each time the player starts a new ship.
void init_ai_for_ship(void)
{
	int	i;

	for (i=0; i<MAX_AI_CLOAK_INFO; i++)
	{
		Ai_cloak_info[i].last_time = GameTime;
		Ai_cloak_info[i].last_segment = ConsoleObject->segnum;
		Ai_cloak_info[i].last_position = ConsoleObject->pos;
	}
}

//	----------------------------------------------------------------------
//	General purpose robot-dies-with-death-roll-and-groan code.
//	Return true if object just died.
//	scale: F1_0*4 for boss, much smaller for much smaller guys
int do_robot_dying_frame(object* objp, fix start_time, fix roll_duration, int *dying_sound_playing, int death_sound, fix expl_scale, fix sound_scale)
{
	fix	roll_val, temp;
	fix	sound_duration;

	if (!roll_duration)
		roll_duration = F1_0/4;

	roll_val = fixdiv(GameTime - start_time, roll_duration);

	fix_sincos(fixmul(roll_val, roll_val), &temp, &objp->mtype.phys_info.rotvel.x);
	fix_sincos(roll_val, &temp, &objp->mtype.phys_info.rotvel.y);
	fix_sincos(roll_val-F1_0/8, &temp, &objp->mtype.phys_info.rotvel.z);

	objp->mtype.phys_info.rotvel.x = (GameTime - start_time)/9;
	objp->mtype.phys_info.rotvel.y = (GameTime - start_time)/5;
	objp->mtype.phys_info.rotvel.z = (GameTime - start_time)/7;

	if (digi_sample_rate)
	{
		int soundno = digi_xlat_sound(death_sound);
		if (soundno >= 0 && soundno != 255)
			sound_duration = fixdiv(activePiggyTable->gameSounds[soundno].length, digi_sample_rate);
		else
			sound_duration = F1_0;
	}
	else
		sound_duration = F1_0;

	if (start_time + roll_duration - sound_duration < GameTime) 
	{
		if (!*dying_sound_playing) 
		{
			mprintf((0, "Starting death sound!\n"));
			*dying_sound_playing = 1;
			digi_link_sound_to_object2( death_sound, objp-Objects.data(), 0, sound_scale, sound_scale*256 );	//	F1_0*512 means play twice as loud
		} else if (P_Rand() < FrameTime*16)
			create_small_fireball_on_object(objp, (F1_0 + P_Rand()) * (16 * expl_scale/F1_0)/8, 0);
	} else if (P_Rand() < FrameTime*8)
		create_small_fireball_on_object(objp, (F1_0/2 + P_Rand()) * (16 * expl_scale/F1_0)/8, 1);

	if (start_time + roll_duration < GameTime)
		return 1;
	else
		return 0;
}

//	----------------------------------------------------------------------
void start_robot_death_sequence(object *objp)
{
	objp->ctype.ai_info.dying_start_time = GameTime;
	objp->ctype.ai_info.dying_sound_playing = 0;
	objp->ctype.ai_info.SKIP_AI_COUNT = 0;

}

//	----------------------------------------------------------------------
void do_boss_dying_frame_d2(size_t objnum)
{
	int	rval;

	rval = do_robot_dying_frame(&Objects[objnum], Boss_dying_start_time, BOSS_DEATH_DURATION, &Boss_dying_sound_playing, activeBMTable->robots[Objects[objnum].id].deathroll_sound, F1_0*4, F1_0*4);
	
	if (rval) {
		do_controlcen_destroyed_stuff(NULL);
		explode_object(&Objects[objnum], F1_0/4);
		digi_link_sound_to_object2(SOUND_BADASS_EXPLOSION, objnum, 0, F2_0, F1_0*512);
	}
}

extern void recreate_thief(object *objp);

//	----------------------------------------------------------------------
int do_any_robot_dying_frame(object *objp)
{

	if (objp->ctype.ai_info.dying_start_time) {
		size_t robjnum = objp - Objects.data();
		int	rval, death_roll;

		death_roll = activeBMTable->robots[objp->id].death_roll;
		rval = do_robot_dying_frame(objp, objp->ctype.ai_info.dying_start_time, std::min(death_roll/2+1,6)*F1_0, &objp->ctype.ai_info.dying_sound_playing, activeBMTable->robots[objp->id].deathroll_sound, death_roll*F1_0/8, death_roll*F1_0/2);

		if (rval) {
			explode_object(&Objects[robjnum], F1_0/4);
			digi_link_sound_to_object2(SOUND_BADASS_EXPLOSION, robjnum, 0, F2_0, F1_0*512);
			if (Current_level_num < 0) { 
				objp = &Objects[robjnum];
				if (activeBMTable->robots[objp->id].thief)
					recreate_thief(objp);
			}
		}

		return 1;
	}

	return 0;
}

// --------------------------------------------------------------------------------------------------------------------
//	Do special stuff for a boss.
void do_boss_stuff_d2(object *objp, int player_visibility)
{
	int	boss_id, boss_index;

	boss_id = activeBMTable->robots[objp->id].boss_flag;

	//Assert((boss_id >= BOSS_D2) && (boss_id < BOSS_D2 + NUM_D2_BOSSES));

	//mprintf((0, "\nBoss ID: %d", boss_id));

	if (boss_id >= BOSS_D2)
		boss_index = boss_id - BOSS_D2 + 2;
	else
		boss_index = boss_id - 1;

	Assert(boss_index < NUM_D2_BOSSES + 2);

#ifndef NDEBUG
	if (objp->shields != Prev_boss_shields)
	{
		mprintf((0, "Boss shields = %7.3f, object %i\n", f2fl(objp->shields), objp-Objects.data()));
		Prev_boss_shields = objp->shields;
	}
#endif

	//	New code, fixes stupid bug which meant boss never gated in robots if > 32767 seconds played.
	if (Last_teleport_time > GameTime)
		Last_teleport_time = GameTime;

	if (Last_gate_time > GameTime)
		Last_gate_time = GameTime;

	//	@mk, 10/13/95:  Reason:
	//		Level 4 boss behind locked door.  But he's allowed to teleport out of there.  So he
	//		teleports out of there right away, and blasts player right after first door.
	if (!player_visibility && (GameTime - Boss_hit_time > F1_0*2))
		return;

	if (!Boss_dying && Boss_teleports[boss_index]) {
		if (objp->ctype.ai_info.CLOAKED == 1) {
			Boss_hit_time = GameTime;	//	Keep the cloak:teleport process going.
			if ((GameTime - Boss_cloak_start_time > BOSS_CLOAK_DURATION/3) && (Boss_cloak_end_time - GameTime > BOSS_CLOAK_DURATION/3) && (GameTime - Last_teleport_time > Boss_teleport_interval)) {
				if (ai_multiplayer_awareness(objp, 98))
					teleport_boss(objp);
			} else if (GameTime - Boss_hit_time > F1_0*2) {
				Last_teleport_time -= Boss_teleport_interval/4;
			}

			if (GameTime > Boss_cloak_end_time || GameTime < Boss_cloak_start_time)
				objp->ctype.ai_info.CLOAKED = 0;
		} else if ((GameTime - Boss_cloak_end_time > Boss_cloak_interval) || (GameTime - Boss_cloak_end_time < -Boss_cloak_duration)) {
			if (ai_multiplayer_awareness(objp, 95)) {
				Boss_cloak_start_time = GameTime;
				Boss_cloak_end_time = GameTime+Boss_cloak_duration;
				objp->ctype.ai_info.CLOAKED = 1;
#ifdef NETWORK
				if (Game_mode & GM_MULTI)
					multi_send_boss_actions(objp-Objects.data(), 2, 0, 0);
#endif
			}
		}
	}

}