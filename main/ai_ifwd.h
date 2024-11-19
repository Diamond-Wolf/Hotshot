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

#include "object.h"
#include "robot.h"

#ifndef NDEBUG
extern int	Do_ai_flag;
extern int	Cvv_test;
extern int	Gun_point_hack;
extern fix	Prev_boss_shields;
#endif

extern int      Robot_sound_volume;
extern std::vector<int8_t>	New_awareness;

extern void move_around_player(object *objp, vms_vector *vec_to_player, int fast_flag);
extern void set_next_fire_time(object *objp, ai_local *ailp, robot_info *robptr, int gun_num);
extern void ai_fire_laser_at_player(object* obj, vms_vector* fire_point, int gun_num, vms_vector *believed_player_pos);
extern void move_away_from_player(object *objp, vms_vector *vec_to_player, int attack_type);
extern void ai_move_relative_to_player(object *objp, ai_local *ailp, fix dist_to_player, vms_vector *vec_to_player, fix circle_distance, int evade_only, int player_visibility);
extern void move_object_to_legal_spot(object *objp);
extern void teleport_boss(object* objp);
extern void do_boss_dying_frame(size_t objnum);
extern void do_boss_stuff(object* objp, int player_visibility);
extern void set_player_awareness_all();

inline int8_t Ai_transition_table[AI_MAX_EVENT][AI_MAX_STATE][AI_MAX_STATE] = {
	{
		//	Event = AIE_FIRE, a nearby object fired
		//	none			rest			srch			lock			flin			fire			reco				// CURRENT is rows, GOAL is columns
		{	AIS_ERR_,	AIS_LOCK,	AIS_LOCK,	AIS_LOCK,	AIS_FLIN,	AIS_FIRE,	AIS_RECO},		//	none
		{	AIS_ERR_,	AIS_LOCK,	AIS_LOCK,	AIS_LOCK,	AIS_FLIN,	AIS_FIRE,	AIS_RECO},		//	rest
		{	AIS_ERR_,	AIS_LOCK,	AIS_LOCK,	AIS_LOCK,	AIS_FLIN,	AIS_FIRE,	AIS_RECO},		//	search
		{	AIS_ERR_,	AIS_LOCK,	AIS_LOCK,	AIS_LOCK,	AIS_FLIN,	AIS_FIRE,	AIS_RECO},		//	lock
		{	AIS_ERR_,	AIS_REST,	AIS_LOCK,	AIS_LOCK,	AIS_LOCK,	AIS_FIRE,	AIS_RECO},		//	flinch
		{	AIS_ERR_,	AIS_FIRE,	AIS_FIRE,	AIS_FIRE,	AIS_FLIN,	AIS_FIRE,	AIS_RECO},		//	fire
		{	AIS_ERR_,	AIS_LOCK,	AIS_LOCK,	AIS_LOCK,	AIS_FLIN,	AIS_FIRE,	AIS_FIRE}		//	recoil
		},

	//	Event = AIE_HITT, a nearby object was hit (or a wall was hit)
	{
	{	AIS_ERR_,	AIS_LOCK,	AIS_LOCK,	AIS_LOCK,	AIS_FLIN,	AIS_FIRE,	AIS_RECO},
	{	AIS_ERR_,	AIS_LOCK,	AIS_LOCK,	AIS_LOCK,	AIS_FLIN,	AIS_FIRE,	AIS_RECO},
	{	AIS_ERR_,	AIS_LOCK,	AIS_LOCK,	AIS_LOCK,	AIS_FLIN,	AIS_FIRE,	AIS_RECO},
	{	AIS_ERR_,	AIS_LOCK,	AIS_LOCK,	AIS_LOCK,	AIS_FLIN,	AIS_FIRE,	AIS_RECO},
	{	AIS_ERR_,	AIS_LOCK,	AIS_LOCK,	AIS_LOCK,	AIS_LOCK,	AIS_FLIN,	AIS_FLIN},
	{	AIS_ERR_,	AIS_REST,	AIS_LOCK,	AIS_LOCK,	AIS_LOCK,	AIS_FIRE,	AIS_RECO},
	{	AIS_ERR_,	AIS_LOCK,	AIS_LOCK,	AIS_LOCK,	AIS_FLIN,	AIS_FIRE,	AIS_FIRE}
	},

	//	Event = AIE_COLL, player collided with robot
	{
	{	AIS_ERR_,	AIS_LOCK,	AIS_LOCK,	AIS_LOCK,	AIS_FLIN,	AIS_FIRE,	AIS_RECO},
	{	AIS_ERR_,	AIS_LOCK,	AIS_LOCK,	AIS_LOCK,	AIS_FLIN,	AIS_FIRE,	AIS_RECO},
	{	AIS_ERR_,	AIS_LOCK,	AIS_LOCK,	AIS_LOCK,	AIS_FLIN,	AIS_FIRE,	AIS_RECO},
	{	AIS_ERR_,	AIS_LOCK,	AIS_LOCK,	AIS_LOCK,	AIS_FLIN,	AIS_FIRE,	AIS_RECO},
	{	AIS_ERR_,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN,	AIS_LOCK,	AIS_FLIN,	AIS_FLIN},
	{	AIS_ERR_,	AIS_REST,	AIS_LOCK,	AIS_LOCK,	AIS_LOCK,	AIS_FIRE,	AIS_RECO},
	{	AIS_ERR_,	AIS_LOCK,	AIS_LOCK,	AIS_LOCK,	AIS_FLIN,	AIS_FIRE,	AIS_FIRE}
	},

	//	Event = AIE_HURT, player hurt robot (by firing at and hitting it)
	//	Note, this doesn't necessarily mean the robot JUST got hit, only that that is the most recent thing that happened.
	{
	{	AIS_ERR_,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN},
	{	AIS_ERR_,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN},
	{	AIS_ERR_,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN},
	{	AIS_ERR_,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN},
	{	AIS_ERR_,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN},
	{	AIS_ERR_,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN},
	{	AIS_ERR_,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN,	AIS_FLIN}
	}
};