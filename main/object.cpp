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

#include <string.h>	// for memset
#include <stdio.h>
#include <algorithm>

#include "misc/rand.h"

#include "inferno.h"
#include "game.h"
#include "2d/gr.h"
#include "stdlib.h"
#include "bm.h"
//#include "error.h"
#include "platform/mono.h"
#include "3d/3d.h"
#include "segment.h"
#include "texmap/texmap.h"
#include "platform/key.h"
#include "gameseg.h"
#include "gameseq.h"
#include "textures.h"

#include "object.h"
#include "physics.h"
#include "slew.h"		
#include "render.h"
#include "wall.h"
#include "vclip.h"
#include "polyobj.h"
#include "fireball.h"
#include "laser.h"
#include "misc/error.h"
//#include "pa_enabl.h"
#include "ai.h"
#include "hostage.h"
#include "morph.h"
#include "cntrlcen.h"
#include "powerup.h"
#include "fuelcen.h"
#include "endlevel.h"

#include "sounds.h"
#include "collide.h"

#include "lighting.h"
#include "newdemo.h"
#include "player.h"
#include "weapon.h"
#include "network.h"
#include "newmenu.h"
#include "gauges.h"
#include "arcade.h"
#include "multi.h"
#include "menu.h"
#include "misc/args.h"
#include "stringtable.h"
#include "piggy.h"
#include "switch.h"
#include "cfile/cfile.h"
#include "game.h"

#ifdef TACTILE
#include "tactile.h"
#endif

#ifdef EDITOR
#include "editor\editor.h"
#endif

#ifdef _3DFX
#include "3dfx_des.h"
#endif

/*
 *  Global variables
 */

extern std::vector<int8_t> WasRecorded;
extern std::vector<int8_t> ViewWasRecorded;

uint8_t CollisionResult[MAX_OBJECT_TYPES][MAX_OBJECT_TYPES];

object* ConsoleObject;					//the object that is the player

object* Viewer_save;

static std::vector<short> free_obj_list(MAX_OBJECTS + 1);

extern std::vector<fix> Last_afterburner_time;
extern std::vector<int8_t> Lighting_objects;
extern std::vector<fix> object_light;
extern std::vector<int> object_sig;

//Data for objects

// -- Object stuff

//info on the various types of objects
#ifndef NDEBUG
object	Object_minus_one;
#endif

std::vector<object> Objects(MAX_OBJECTS);
int num_objects = 0;
int Highest_object_index = 0;
int Highest_ever_object_index = 0;

// grs_bitmap *robot_bms[MAX_ROBOT_BITMAPS];	//all bitmaps for all robots

// int robot_bm_nums[MAX_ROBOT_TYPES];		//starting bitmap num for each robot
// int robot_n_bitmaps[MAX_ROBOT_TYPES];		//how many bitmaps for each robot

// char *robot_names[MAX_ROBOT_TYPES];		//name of each robot

//--unused-- int Num_robot_types=0;

int print_object_info = 0;
//@@int Object_viewer = 0;

//object * Slew_object = NULL;	// Object containing slew object info.

//--unused-- int Player_controller_type = 0;

window_rendered_data Window_rendered_data[MAX_RENDERED_WINDOWS];

#ifndef NDEBUG
char	Object_type_names[MAX_OBJECT_TYPES][9] = {
	"WALL    ",
	"FIREBALL",
	"ROBOT   ",
	"HOSTAGE ",
	"PLAYER  ",
	"WEAPON  ",
	"CAMERA  ",
	"POWERUP ",
	"DEBRIS  ",
	"CNTRLCEN",
	"FLARE   ",
	"CLUTTER ",
	"GHOST   ",
	"LIGHT   ",
	"COOP    ",
	"MARKER  ",
};
#endif

#ifndef RELEASE
//set viewer object to next object in array
void object_goto_next_viewer()
{
	int i, start_obj = 0;

	start_obj = Viewer - Objects.data();		//get viewer object number

	for (i = 0; i <= Highest_object_index; i++)
	{
		start_obj++;
		if (start_obj > Highest_object_index) start_obj = 0;

		if (Objects[start_obj].type != OBJ_NONE)
		{
			Viewer = &Objects[start_obj];
			return;
		}
	}
	Error("Couldn't find a viewer object!");
}

//set viewer object to next object in array
void object_goto_prev_viewer()
{
	int i, start_obj = 0;

	start_obj = Viewer - Objects.data();		//get viewer object number

	for (i = 0; i <= Highest_object_index; i++)
	{
		start_obj--;
		if (start_obj < 0) start_obj = Highest_object_index;

		if (Objects[start_obj].type != OBJ_NONE)
		{
			Viewer = &Objects[start_obj];
			return;
		}
	}
	Error("Couldn't find a viewer object!");
}
#endif

extern void verify_console_object();

void ResizeObjectVectors(int newSize, bool shrinkToFit) {

	int curSize = Objects.size();

	Objects.resize(newSize);
	free_obj_list.resize(newSize);
	free_obj_list.push_back(newSize); //for the extra slot, when obj count = list size
	for (int i = curSize; i < newSize; i++)
		free_obj_list[i] = i;
	Ai_local_info.resize(newSize);
	Last_afterburner_time.resize(newSize);
	Lighting_objects.resize(newSize);
	object_light.resize(newSize);
	object_sig.resize(newSize);
	WasRecorded.resize(newSize);
	ViewWasRecorded.resize(newSize);

#ifdef NETWORK
	local_to_remote.resize(newSize);
	object_owner.resize(newSize);
#endif

	if (shrinkToFit) {
		Objects.shrink_to_fit();
		free_obj_list.shrink_to_fit();
		Ai_local_info.shrink_to_fit();
		Last_afterburner_time.shrink_to_fit();
		Lighting_objects.shrink_to_fit();
		object_light.shrink_to_fit();
		object_sig.shrink_to_fit();
		WasRecorded.shrink_to_fit();
		ViewWasRecorded.shrink_to_fit();

#ifdef NETWORK
		local_to_remote.shrink_to_fit();
		object_owner.shrink_to_fit();
#endif
		
	}

}

void RelinkSpecialObjectPointers(const RelinkCache& cache) {

	extern object* Viewer_save;
	extern object* old_viewer;
	extern object* prev_obj;
	extern object* slew_obj;
	
	verify_console_object();

	if (cache.viewer < Objects.size())
		Viewer = &Objects[cache.viewer];

	if (cache.missile < Objects.size())
		Missile_viewer = &Objects[cache.missile];

	if (cache.save < Objects.size())
		Viewer_save = &Objects[cache.save];

	for (int i = 0; i < MAX_PLAYERS; i++) {
		if (cache.guideds[i] < Objects.size())
			Guided_missile[i] = &Objects[cache.guideds[i]];
	}

	if (cache.old < Objects.size())
		old_viewer = &Objects[cache.old];

	if (cache.prev < Objects.size())
		prev_obj = &Objects[cache.prev];

	if (cache.deadcam < Objects.size())
		Dead_player_camera = &Objects[cache.deadcam];

	if (cache.slew < Objects.size())
		slew_obj = &Objects[cache.slew];

}

object* obj_find_first_of_type(int type)
{
	int i;

	for (i = 0; i <= Highest_object_index; i++)
		if (Objects[i].type == type)
			return (&Objects[i]);
	return ((object*)NULL);
}

int obj_return_num_of_type(int type)
{
	int i, count = 0;

	for (i = 0; i <= Highest_object_index; i++)
		if (Objects[i].type == type)
			count++;
	return (count);
}
int obj_return_num_of_typeid(int type, int id)
{
	int i, count = 0;

	for (i = 0; i <= Highest_object_index; i++)
		if (Objects[i].type == type && Objects[i].id == id)
			count++;
	return (count);
}

int global_orientation = 0;

//draw an object that has one bitmap & doesn't rotate
void draw_object_blob(object* obj, bitmap_index bmi)
{
	int	orientation = 0;
	grs_bitmap* bm = &activePiggyTable->gameBitmaps[bmi.index];

	if (obj->type == OBJ_FIREBALL)
		orientation = (obj - Objects.data()) & 7;

	orientation = global_orientation; //[ISB] k

	PIGGY_PAGE_IN(bmi);

	if (bm->bm_w > bm->bm_h)
		g3_draw_bitmap(&obj->pos, obj->size, fixmuldiv(obj->size, bm->bm_h, bm->bm_w), bm, orientation);
	else
		g3_draw_bitmap(&obj->pos, fixmuldiv(obj->size, bm->bm_w, bm->bm_h), obj->size, bm, orientation);
}

//draw an object that is a texture-mapped rod
void draw_object_tmap_rod(object* obj, bitmap_index bitmapi, int lighted)
{
	grs_bitmap* bitmap = &activePiggyTable->gameBitmaps[bitmapi.index];
	fix light;

	vms_vector delta, top_v, bot_v;
	g3s_point top_p, bot_p;

	PIGGY_PAGE_IN(bitmapi);

	//bitmap->bm_handle = bitmapi.index;

	vm_vec_copy_scale(&delta, &obj->orient.uvec, obj->size);

	vm_vec_add(&top_v, &obj->pos, &delta);
	vm_vec_sub(&bot_v, &obj->pos, &delta);

	g3_rotate_point(&top_p, &top_v);
	g3_rotate_point(&bot_p, &bot_v);

	if (lighted)
		light = compute_object_light(obj, &top_p.p3_vec);
	else
		light = f1_0;

#ifdef _3DFX
	_3dfx_rendering_poly_obj = 1;
#endif

#ifdef PA_3DFX_VOODOO
	light = f1_0;
#endif

	g3_draw_rod_tmap(bitmap, &bot_p, obj->size, &top_p, obj->size, light);

#ifdef _3DFX
	_3dfx_rendering_poly_obj = 0;
#endif

}

int	Linear_tmap_polygon_objects = 1;

extern fix Max_thrust;

//used for robot engine glow
#define MAX_VELOCITY i2f(50)

//function that takes the same parms as draw_tmap, but renders as flat poly
//we need this to do the cloaked effect
extern void draw_tmap_flat(grs_bitmap* bm, int nv, g3s_point** vertlist);

//what darkening level to use when cloaked
#define CLOAKED_FADE_LEVEL		28

#define	CLOAK_FADEIN_DURATION_PLAYER	F2_0
#define	CLOAK_FADEOUT_DURATION_PLAYER	F2_0

#define	CLOAK_FADEIN_DURATION_ROBOT	F1_0
#define	CLOAK_FADEOUT_DURATION_ROBOT	F1_0

//do special cloaked render
void draw_cloaked_object(object * obj, fix light, fix * glow, fix cloak_start_time, fix cloak_end_time)
{
	fix cloak_delta_time, total_cloaked_time;
	fix light_scale;
	int cloak_value;
	int fading = 0;		//if true, fading, else cloaking
	fix	Cloak_fadein_duration;
	fix	Cloak_fadeout_duration;


	total_cloaked_time = cloak_end_time - cloak_start_time;

	switch (obj->type) 
	{
	case OBJ_PLAYER:
		Cloak_fadein_duration = CLOAK_FADEIN_DURATION_PLAYER;
		Cloak_fadeout_duration = CLOAK_FADEOUT_DURATION_PLAYER;
		break;
	case OBJ_ROBOT:
		Cloak_fadein_duration = CLOAK_FADEIN_DURATION_ROBOT;
		Cloak_fadeout_duration = CLOAK_FADEOUT_DURATION_ROBOT;
		break;
	default:
		Int3();		//	Contact Mike: Unexpected object type in draw_cloaked_object.
	}

	cloak_delta_time = GameTime - cloak_start_time;

	if (cloak_delta_time < Cloak_fadein_duration / 2) 
	{
		light_scale = fixdiv(Cloak_fadein_duration / 2 - cloak_delta_time, Cloak_fadein_duration / 2);
		fading = 1;
	}
	else if (cloak_delta_time < Cloak_fadein_duration)
	{
		cloak_value = f2i(fixdiv(cloak_delta_time - Cloak_fadein_duration / 2, Cloak_fadein_duration / 2) * CLOAKED_FADE_LEVEL);
	}
	else if (GameTime < cloak_end_time - Cloak_fadeout_duration) 
	{
		static int cloak_delta = 0, cloak_dir = 1;
		static fix cloak_timer = 0;

		//note, if more than one cloaked object is visible at once, the
		//pulse rate will change!

		cloak_timer -= FrameTime;
		while (cloak_timer < 0) 
		{
			cloak_timer += Cloak_fadeout_duration / 12;

			cloak_delta += cloak_dir;

			if (cloak_delta == 0 || cloak_delta == 4)
				cloak_dir = -cloak_dir;
		}

		cloak_value = CLOAKED_FADE_LEVEL - cloak_delta;

	}
	else if (GameTime < cloak_end_time - Cloak_fadeout_duration / 2)
	{
		cloak_value = f2i(fixdiv(total_cloaked_time - Cloak_fadeout_duration / 2 - cloak_delta_time, Cloak_fadeout_duration / 2) * CLOAKED_FADE_LEVEL);
	}
	else 
	{
		light_scale = fixdiv(Cloak_fadeout_duration / 2 - (total_cloaked_time - cloak_delta_time), Cloak_fadeout_duration / 2);
		fading = 1;
	}

	if (fading) 
	{
		fix new_light, save_glow;
		bitmap_index* alt_textures = NULL;

#ifdef NETWORK
		if (obj->rtype.pobj_info.alt_textures > 0)
			alt_textures = multi_player_textures[obj->rtype.pobj_info.alt_textures - 1];
#endif

		new_light = fixmul(light, light_scale);
		save_glow = glow[0];
		glow[0] = fixmul(glow[0], light_scale);
		draw_polygon_model(obj->pos, obj->orient, &obj->rtype.pobj_info.anim_angles[0], obj->rtype.pobj_info.model_num, obj->rtype.pobj_info.subobj_flags, new_light, glow, alt_textures);
		glow[0] = save_glow;
	}
	else 
	{
		Gr_scanline_darkening_level = cloak_value;
		gr_setcolor(BM_XRGB(0, 0, 0));	//set to black (matters for s3)
		g3_set_special_render(draw_tmap_flat, NULL, NULL);		//use special flat drawer
		draw_polygon_model(obj->pos, obj->orient, &obj->rtype.pobj_info.anim_angles[0], obj->rtype.pobj_info.model_num, obj->rtype.pobj_info.subobj_flags, light, glow, NULL);
		g3_set_special_render(NULL, NULL, NULL);
		Gr_scanline_darkening_level = GR_FADE_LEVELS;
	}

}

//draw an object which renders as a polygon model
void draw_polygon_object(object* obj)
{
	fix light;
	int	imsave;
	fix engine_glow_value[2];		//element 0 is for engine glow, 1 for headlight

	light = compute_object_light(obj, NULL);

	//	If option set for bright players in netgame, brighten them!
#ifdef NETWORK
	if (Game_mode & GM_MULTI)
		if (Netgame.BrightPlayers)
			light = F1_0;
#endif

	//make robots brighter according to robot glow field
	if (obj->type == OBJ_ROBOT)
		light += (activeBMTable->robots[obj->id].glow << 12);		//convert 4:4 to 16:16

	if (obj->type == OBJ_WEAPON)
		if (obj->id == FLARE_ID)
			light += F1_0 * 2;

	if (obj->type == OBJ_MARKER)
		light += F1_0 * 2;


	imsave = Interpolation_method;
	if (Linear_tmap_polygon_objects)
		Interpolation_method = 1;

	//set engine glow value
	engine_glow_value[0] = f1_0 / 5;
	if (obj->movement_type == MT_PHYSICS) 
	{
		if (obj->mtype.phys_info.flags & PF_USES_THRUST && obj->type == OBJ_PLAYER && obj->id == Player_num) 
		{
			fix thrust_mag = vm_vec_mag_quick(&obj->mtype.phys_info.thrust);
			engine_glow_value[0] += (fixdiv(thrust_mag, Player_ship->max_thrust) * 4) / 5;
		}
		else 
		{
			fix speed = vm_vec_mag_quick(&obj->mtype.phys_info.velocity);
			engine_glow_value[0] += (fixdiv(speed, MAX_VELOCITY) * 3) / 5;
		}
	}

	//set value for player headlight
	if (obj->type == OBJ_PLAYER) 
	{
		if (Players[obj->id].flags & PLAYER_FLAGS_HEADLIGHT && !Endlevel_sequence)
			if (Players[obj->id].flags & PLAYER_FLAGS_HEADLIGHT_ON)
				engine_glow_value[1] = -2;		//draw white!
			else
				engine_glow_value[1] = -1;		//draw normal color (grey)
		else
			engine_glow_value[1] = -3;			//don't draw
	}

	if (obj->rtype.pobj_info.tmap_override != -1) 
	{
		polymodel* pm = &activeBMTable->models[obj->rtype.pobj_info.model_num];
		bitmap_index bm_ptrs[12];

		int i;

		Assert(pm->n_textures <= 12);

		for (i = 0; i < 12; i++)		//fill whole array, in case simple model needs more
			bm_ptrs[i] = activeBMTable->textures
[obj->rtype.pobj_info.tmap_override];

		draw_polygon_model(obj->pos, obj->orient, &obj->rtype.pobj_info.anim_angles[0], obj->rtype.pobj_info.model_num, obj->rtype.pobj_info.subobj_flags, light, engine_glow_value, bm_ptrs);
	}
	else
	{
		if (obj->type == OBJ_PLAYER && (Players[obj->id].flags & PLAYER_FLAGS_CLOAKED))
			draw_cloaked_object(obj, light, engine_glow_value, Players[obj->id].cloak_time, Players[obj->id].cloak_time + CLOAK_TIME_MAX);
		else if ((obj->type == OBJ_ROBOT) && (obj->ctype.ai_info.CLOAKED)) 
		{
			if (activeBMTable->robots[obj->id].boss_flag)
				draw_cloaked_object(obj, light, engine_glow_value, Boss_cloak_start_time, Boss_cloak_end_time);
			else
				draw_cloaked_object(obj, light, engine_glow_value, GameTime - F1_0 * 10, GameTime + F1_0 * 10);
		}
		else 
		{
			bitmap_index* alt_textures = NULL;

#ifdef NETWORK
			if (obj->rtype.pobj_info.alt_textures > 0)
				alt_textures = multi_player_textures[obj->rtype.pobj_info.alt_textures - 1];
#endif

			//	Snipers get bright when they fire.
			if (Ai_local_info[obj - Objects.data()].next_fire < F1_0 / 8) 
			{
				if (obj->ctype.ai_info.behavior == AIB_SNIPE)
					light = 2 * light + F1_0;
		}

			draw_polygon_model(obj->pos, obj->orient, &obj->rtype.pobj_info.anim_angles[0], obj->rtype.pobj_info.model_num, obj->rtype.pobj_info.subobj_flags, light, engine_glow_value, alt_textures);
			if (obj->type == OBJ_WEAPON && (activeBMTable->weapons[obj->id].model_num_inner > -1)) 
			{
				fix dist_to_eye = vm_vec_dist_quick(&Viewer->pos, &obj->pos);
				if (dist_to_eye < Simple_model_threshhold_scale * F1_0 * 2)
					draw_polygon_model(obj->pos, obj->orient, &obj->rtype.pobj_info.anim_angles[0], activeBMTable->weapons[obj->id].model_num_inner, obj->rtype.pobj_info.subobj_flags, light, engine_glow_value, alt_textures);
			}
	}
}
	Interpolation_method = imsave;
}

int	Player_fired_laser_this_frame = -1;



// -----------------------------------------------------------------------------
//this routine checks to see if an robot rendered near the middle of
//the screen, and if so and the player had fired, "warns" the robot
void set_robot_location_info(object* objp)
{
	if (Player_fired_laser_this_frame != -1) 
	{
		g3s_point temp;

		g3_rotate_point(&temp, &objp->pos);

		if (temp.p3_codes & CC_BEHIND)		//robot behind the screen
			return;

		//the code below to check for object near the center of the screen
		//completely ignores z, which may not be good

		if ((abs(temp.p3_x) < F1_0 * 4) && (abs(temp.p3_y) < F1_0 * 4))
		{
			objp->ctype.ai_info.danger_laser_num = Player_fired_laser_this_frame;
			objp->ctype.ai_info.danger_laser_signature = Objects[Player_fired_laser_this_frame].signature;
		}
	}
}

//	------------------------------------------------------------------------------------------------------------------
void create_small_fireball_on_object(object* objp, fix size_scale, int sound_flag)
{
	fix			size;
	vms_vector	pos, rand_vec;
	int			segnum;

	size_t objnum = objp - Objects.data();

	pos = objp->pos;
	make_random_vector(&rand_vec);

	vm_vec_scale(&rand_vec, objp->size / 2);

	vm_vec_add2(&pos, &rand_vec);

	size = fixmul(size_scale, F1_0 / 2 + P_Rand() * 4 / 2);

	segnum = find_point_seg(pos, objp->segnum);
	if (segnum != -1) 
	{
		object* expl_obj;
		expl_obj = object_create_explosion(segnum, pos, size, VCLIP_SMALL_EXPLOSION);
		if (!expl_obj)
			return;
		objp = &Objects[objnum];
		obj_attach(objp, expl_obj);
		if (P_Rand() < 8192) 
		{
			fix	vol = F1_0 / 2;
			if (objp->type == OBJ_ROBOT)
				vol *= 2;
			else if (sound_flag)
				digi_link_sound_to_object(SOUND_EXPLODING_WALL, objp - Objects.data(), 0, vol);
		}
	}
}

//	------------------------------------------------------------------------------------------------------------------
void create_vclip_on_object(object* objp, fix size_scale, int vclip_num)
{
	fix			size;
	vms_vector	pos, rand_vec;
	int			segnum;

	pos = objp->pos;
	make_random_vector(&rand_vec);

	vm_vec_scale(&rand_vec, objp->size / 2);

	vm_vec_add2(&pos, &rand_vec);

	size = fixmul(size_scale, F1_0 + P_Rand() * 4);

	segnum = find_point_seg(pos, objp->segnum);
	if (segnum != -1) {
		object* expl_obj;
		expl_obj = object_create_explosion(segnum, pos, size, vclip_num);
		if (!expl_obj)
			return;

		expl_obj->movement_type = MT_PHYSICS;
		expl_obj->mtype.phys_info.velocity.x = objp->mtype.phys_info.velocity.x / 2;
		expl_obj->mtype.phys_info.velocity.y = objp->mtype.phys_info.velocity.y / 2;
		expl_obj->mtype.phys_info.velocity.z = objp->mtype.phys_info.velocity.z / 2;
	}
}

// -----------------------------------------------------------------------------
//	Render an object.  Calls one of several routines based on type
void render_object(object* obj)
{
	int	mld_save;

	if (obj == Viewer) return;

	if (obj->type == OBJ_NONE)
	{
#ifndef NDEBUG
		mprintf((1, "ERROR!!!! Bogus obj %d in seg %d is rendering!\n", obj - Objects.data(), obj->segnum));
		Int3();
#endif
		return;
	}

	mld_save = Max_linear_depth;
	Max_linear_depth = Max_linear_depth_objects;

	switch (obj->render_type)
	{

	case RT_NONE:	break;		//doesn't render, like the player

	case RT_POLYOBJ:
		draw_polygon_object(obj);

		//"warn" robot if being shot at
		if (obj->type == OBJ_ROBOT)
			set_robot_location_info(obj);

		break;

	case RT_MORPH:	draw_morph_object(obj); break;
	case RT_FIREBALL: draw_fireball(obj); break;
	case RT_WEAPON_VCLIP: draw_weapon_vclip(obj); break;
	case RT_HOSTAGE: draw_hostage(obj); break;
	case RT_POWERUP: draw_powerup(obj); break;
	case RT_LASER: Laser_render(obj); break;
	default: Error("Unknown render_type <%d>", obj->render_type);
	}

#ifdef NEWDEMO
	if (obj->render_type != RT_NONE)
		if (Newdemo_state == ND_STATE_RECORDING) 
		{
			if (!WasRecorded[obj - Objects.data()])
			{
				newdemo_record_render_object(obj);
				WasRecorded[obj - Objects.data()] = 1;
			}
		}
#endif

	Max_linear_depth = mld_save;

}

void check_and_fix_matrix(vms_matrix* m);

#define vm_angvec_zero(v) (v)->p=(v)->b=(v)->h=0

void reset_player_object()
{
	int i;

	//Init physics

	vm_vec_zero(&ConsoleObject->mtype.phys_info.velocity);
	vm_vec_zero(&ConsoleObject->mtype.phys_info.thrust);
	vm_vec_zero(&ConsoleObject->mtype.phys_info.rotvel);
	vm_vec_zero(&ConsoleObject->mtype.phys_info.rotthrust);
	ConsoleObject->mtype.phys_info.brakes = ConsoleObject->mtype.phys_info.turnroll = 0;
	ConsoleObject->mtype.phys_info.mass = Player_ship->mass;
	ConsoleObject->mtype.phys_info.drag = Player_ship->drag;
	ConsoleObject->mtype.phys_info.flags |= PF_TURNROLL | PF_LEVELLING | PF_WIGGLE | PF_USES_THRUST;

	//Init render info

	ConsoleObject->render_type = RT_POLYOBJ;
	ConsoleObject->rtype.pobj_info.model_num = Player_ship->model_num;		//what model is this?
	ConsoleObject->rtype.pobj_info.subobj_flags = 0;		//zero the flags
	ConsoleObject->rtype.pobj_info.tmap_override = -1;		//no tmap override!

	for (i = 0; i < MAX_SUBMODELS; i++)
		vm_angvec_zero(&ConsoleObject->rtype.pobj_info.anim_angles[i]);

	// Clear misc

	ConsoleObject->flags = 0;
}

//make object0 the player, setting all relevant fields
void init_player_object()
{
	ConsoleObject->type = OBJ_PLAYER;
	ConsoleObject->id = 0;					//no sub-types for player
	ConsoleObject->signature = 0;			//player has zero, others start at 1
	ConsoleObject->size = activeBMTable->models[Player_ship->model_num].rad;
	ConsoleObject->control_type = CT_SLEW;			//default is player slewing
	ConsoleObject->movement_type = MT_PHYSICS;		//change this sometime
	ConsoleObject->lifeleft = IMMORTAL_TIME;
	ConsoleObject->attached_obj = -1;

	reset_player_object();
}

//sets up the free list & init player & whatever else
void init_objects()
{
	int i;

	collide_init();

	mprintf((0, "Object vector size: %d\n", Objects.size()));

	for (i = 0; i < Objects.size(); i++) 
	{
		Objects[i].type = OBJ_NONE;
		Objects[i].segnum = -1;
	}

	if (free_obj_list.size() != Objects.size()) {
		free_obj_list.resize(Objects.size());
		free_obj_list.push_back(Objects.size());
	}

	for (i = 0; i < free_obj_list.size(); i++)
		free_obj_list[i] = i;

	for (i = 0; i < Segments.size(); i++)
		Segments[i].objects = -1;

	ConsoleObject = Viewer = &Objects[0];

	init_player_object();
	obj_link(ConsoleObject - Objects.data(), 0);	//put in the world in segment 0

	num_objects = 1;						//just the player
	Highest_object_index = 0;
}

//after calling init_object(), the network code has grabbed specific
//object slots without allocating them.  Go though the objects & build
//the free list, then set the apporpriate globals
void special_reset_objects(void)
{
	int i;

	num_objects = Objects.size();

	Highest_object_index = 0;
	Assert(Objects[0].type != OBJ_NONE);		//0 should be used

	if (free_obj_list.size() != Objects.size()) {
		free_obj_list.resize(Objects.size());
		free_obj_list.push_back(Objects.size());
	}

	for (i = Objects.size(); i--;)
		if (Objects[i].type == OBJ_NONE)
			free_obj_list[--num_objects] = i;
		else
			if (i > Highest_object_index)
				Highest_object_index = i;
}

#ifndef NDEBUG
int is_object_in_seg(int segnum, int objn)
{
	int objnum, count = 0;

	for (objnum = Segments[segnum].objects; objnum != -1; objnum = Objects[objnum].next)
	{
		if (count > Objects.size()) 
		{
			Int3();
			return count;
		}
		if (objnum == objn) count++;
	}
	return count;
}

int search_all_segments_for_object(int objnum)
{
	int i;
	int count = 0;

	for (i = 0; i <= Highest_segment_index; i++) 
	{
		count += is_object_in_seg(i, objnum);
	}
	return count;
}

void johns_obj_unlink(int segnum, int objnum)
{
	object* obj = &Objects[objnum];
	segment* seg = &Segments[segnum];

	Assert(objnum != -1);

	if (obj->prev == -1)
		seg->objects = obj->next;
	else
		Objects[obj->prev].next = obj->next;

	if (obj->next != -1) Objects[obj->next].prev = obj->prev;
}

void remove_incorrect_objects()
{
	int segnum, objnum, count;

	for (segnum = 0; segnum <= Highest_segment_index; segnum++) 
	{
		count = 0;
		for (objnum = Segments[segnum].objects; objnum != -1; objnum = Objects[objnum].next) 
		{
			count++;
#ifndef NDEBUG
			if (count > Objects.size()) 
			{
				mprintf((1, "Object list in segment %d is circular.\n", segnum));
				Int3();
			}
#endif
			if (Objects[objnum].segnum != segnum) 
			{
#ifndef NDEBUG
				mprintf((0, "Removing object %d from segment %d.\n", objnum, segnum));
				Int3();
#endif
				johns_obj_unlink(segnum, objnum);
			}
		}
	}
}

void remove_all_objects_but(int segnum, int objnum)
{
	int i;

	for (i = 0; i <= Highest_segment_index; i++) 
	{
		if (segnum != i) 
		{
			if (is_object_in_seg(i, objnum)) 
			{
				johns_obj_unlink(i, objnum);
			}
		}
	}
}

int check_duplicate_objects()
{
	int i, count = 0;

	for (i = 0; i <= Highest_object_index; i++)
	{
		if (Objects[i].type != OBJ_NONE) 
		{
			count = search_all_segments_for_object(i);
			if (count > 1) 
			{
#ifndef NDEBUG
				mprintf((1, "Object %d is in %d segments!\n", i, count));
				Int3();
#endif
				remove_all_objects_but(Objects[i].segnum, i);
				return count;
			}
		}
	}
	return count;
}

void list_seg_objects(int segnum)
{
	int objnum, count = 0;

	for (objnum = Segments[segnum].objects; objnum != -1; objnum = Objects[objnum].next) 
	{
		count++;
		if (count > Objects.size())
		{
			Int3();
			return;
		}
	}
	return;
}
#endif

//link the object into the list for its segment
void obj_link(int objnum, int segnum)
{
	object* obj = &Objects[objnum];

	Assert(objnum != -1);

	Assert(obj->segnum == -1);

	Assert(segnum >= 0 && segnum <= Highest_segment_index);

	obj->segnum = segnum;

	obj->next = Segments[segnum].objects;
	obj->prev = -1;

	Segments[segnum].objects = objnum;

	if (obj->next != -1) Objects[obj->next].prev = objnum;

	//list_seg_objects( segnum );
	//check_duplicate_objects();

	Assert(Objects[0].next != 0);
	if (Objects[0].next == 0)
		Objects[0].next = -1;

	Assert(Objects[0].prev != 0);
	if (Objects[0].prev == 0)
		Objects[0].prev = -1;
}

void obj_unlink(int objnum)
{
	object* obj = &Objects[objnum];
	segment* seg = &Segments[obj->segnum];

	Assert(objnum != -1);

	if (obj->prev == -1)
		seg->objects = obj->next;
	else
		Objects[obj->prev].next = obj->next;

	if (obj->next != -1) Objects[obj->next].prev = obj->prev;

	obj->segnum = -1;

	Assert(Objects[0].next != 0);
	Assert(Objects[0].prev != 0);
}

int Object_next_signature = 1;	//player gets 0, others start at 1

int Debris_object_count = 0;

int	Unused_object_slots;

int free_object_slots(int num_used);
void obj_detach_one(object* sub);

//----------------------------------------------------------------------------------

int allocCheck = 0;
int extraObjNum = -1;
bool objectGCReady;

//After this many object creations, run GC on the object list.
#define OBJECT_GC_RATE 1000

void doObjectGC() {

	allocCheck = 0;
	objectGCReady = false;

	if (Objects.size() <= MAX_OBJECTS) {
		mprintf((0, "Skipping object GC. Not enough objects.\n"));
		return;
	}

	int gcCap = num_objects + MAX_OBJECTS;

	if (Objects.size() > gcCap) {

		compress_objects();

		RelinkCache cache;

		mprintf((0, "Performing object GC.\nOld capacity: %d, ", Objects.capacity()));
		ResizeObjectVectors(gcCap, true);
		mprintf((0, "New capacity: %d\n", Objects.capacity()));

		RelinkSpecialObjectPointers(cache);

	} else
		mprintf((0, "Did not perform object GC.\nCapacity / size / cap (num):\n%d / %d / %d (%d)\n", Objects.capacity(), Objects.size(), gcCap, num_objects)); 
		
}

void objectPushAll() {
	RelinkCache cache;

	Objects.push_back(object());
	free_obj_list.push_back(free_obj_list.size());
	Ai_local_info.push_back(ai_local());
	Last_afterburner_time.push_back(0);
	Lighting_objects.push_back(0);
	object_light.push_back(0);
	object_sig.push_back(0);
	WasRecorded.push_back(0);
	ViewWasRecorded.push_back(0);

#ifdef NETWORK
	local_to_remote.push_back(-1);
	object_owner.push_back(-1);
#endif

	num_objects++;

	RelinkSpecialObjectPointers(cache);
}

//returns the number of a free object, updating Highest_object_index.
//Generally, obj_create() should be called to get an object, since it
//fills in important fields and does the linking.
//returns -1 if no free objects
int obj_allocate(void)
{

	int objnum;
	bool createAtEnd = false;

	/*if (num_objects >= Objects.size() - 2) 
	{
		int	num_freed;

		num_freed = free_object_slots(Objects.size() - 10);
		mprintf((0, " *** Freed %i objects in frame %i\n", num_freed, FrameCount));
	}*/

	if (num_objects > Objects.size()) {
		mprintf((1, "More objects than are in the vector!? (num %d, size %d)\nBashing to size\n", num_objects, Objects.size()));
		num_objects = Objects.size();
	}

	if (num_objects >= Objects.size() - 1) 
	{
#ifndef NDEBUG
		mprintf((1, "Hit object vector size!\n"));
#endif
		objnum = Objects.size();
		createAtEnd = true;

		objectPushAll();
	}

	if (!createAtEnd)
		objnum = free_obj_list[num_objects++];

	if (objnum > Highest_object_index)
	{
		Highest_object_index = objnum;
		if (Highest_object_index > Highest_ever_object_index)
			Highest_ever_object_index = Highest_object_index;
	}

	if (allocCheck++ >= OBJECT_GC_RATE) {
		objectGCReady = true;
	}

	{
		int	i;
		Unused_object_slots = 0;
		for (i = 0; i <= Highest_object_index; i++)
			if (Objects[i].type == OBJ_NONE)
				Unused_object_slots++;
	}
	return objnum;
}

//frees up an object.  Generally, obj_delete() should be called to get
//rid of an object.  This function deallocates the object entry after
//the object has been unlinked
void obj_free(int objnum)
{
	if (free_obj_list.size() != Objects.size()) {
		free_obj_list.resize(Objects.size());
		free_obj_list.push_back(Objects.size());
	}

	free_obj_list[--num_objects] = objnum;
	Assert(num_objects >= 0);

	if (objnum == Highest_object_index)
		while (Objects[--Highest_object_index].type == OBJ_NONE);
}

//-----------------------------------------------------------------------------
//	Scan the object list, freeing down to num_used objects
//	Returns number of slots freed.
int free_object_slots(int num_used)
{
	int	i, olind;
	std::vector<int> obj_list(Objects.size());
	int	num_already_free, num_to_free, original_num_to_free;

	olind = 0;
	num_already_free = Objects.size() - Highest_object_index - 1;

	if (Objects.size() - num_already_free < num_used)
		return 0;

	for (i = 0; i <= Highest_object_index; i++) 
	{
		if (Objects[i].flags & OF_SHOULD_BE_DEAD) 
		{
			num_already_free++;
			if (Objects.size() - num_already_free < num_used)
				return num_already_free;
		}
		else
			switch (Objects[i].type) 
			{
			case OBJ_NONE:
				num_already_free++;
				if (Objects.size() - num_already_free < num_used)
					return 0;
				break;
			case OBJ_WALL:
			case OBJ_FLARE:
				Int3();		//	This is curious.  What is an object that is a wall?
				break;
			case OBJ_FIREBALL:
			case OBJ_WEAPON:
			case OBJ_DEBRIS:
				obj_list[olind++] = i;
				break;
			case OBJ_ROBOT:
			case OBJ_HOSTAGE:
			case OBJ_PLAYER:
			case OBJ_CNTRLCEN:
			case OBJ_CLUTTER:
			case OBJ_GHOST:
			case OBJ_LIGHT:
			case OBJ_CAMERA:
			case OBJ_POWERUP:
				break;
			}

	}

	num_to_free = Objects.size() - num_used - num_already_free;
	original_num_to_free = num_to_free;

	if (num_to_free > olind)
	{
		mprintf((1, "Warning: Asked to free %i objects, but can only free %i.\n", num_to_free, olind));
		num_to_free = olind;
	}

	
	for (i = 0; i < num_to_free; i++)
		if (Objects[obj_list[i]].type == OBJ_DEBRIS)
		{
			num_to_free--;
			mprintf((0, "Freeing   DEBRIS object %3i\n", obj_list[i]));
			Objects[obj_list[i]].flags |= OF_SHOULD_BE_DEAD;
		}

	if (!num_to_free)
		return original_num_to_free;

	for (i = 0; i < num_to_free; i++)
		if (Objects[obj_list[i]].type == OBJ_FIREBALL && Objects[obj_list[i]].ctype.expl_info.delete_objnum == -1) 
		{
			num_to_free--;
			mprintf((0, "Freeing FIREBALL object %3i\n", obj_list[i]));
			Objects[obj_list[i]].flags |= OF_SHOULD_BE_DEAD;
		}

	if (!num_to_free)
		return original_num_to_free;

	/* //With theoretically infinite objects, should never need to actually delete any important ones

	for (i = 0; i < num_to_free; i++)
		if ((Objects[obj_list[i]].type == OBJ_WEAPON) && (Objects[obj_list[i]].id == FLARE_ID)) 
		{
			num_to_free--;
			Objects[obj_list[i]].flags |= OF_SHOULD_BE_DEAD;
		}

	if (!num_to_free)
		return original_num_to_free;

	for (i = 0; i < num_to_free; i++)
		if ((Objects[obj_list[i]].type == OBJ_WEAPON) && (Objects[obj_list[i]].id != FLARE_ID)) 
		{
			num_to_free--;
			mprintf((0, "Freeing   WEAPON object %3i\n", obj_list[i]));
			Objects[obj_list[i]].flags |= OF_SHOULD_BE_DEAD;
		}
	*/

	return original_num_to_free - num_to_free;

}

//-----------------------------------------------------------------------------
//initialize a new object.  adds to the list for the given segment
//note that segnum is really just a suggestion, since this routine actually
//searches for the correct segment
//returns the object number
int obj_create(uint8_t type, uint8_t id, int segnum, vms_vector pos,
	vms_matrix* orient_in, fix size, uint8_t ctype, uint8_t mtype, uint8_t rtype)
{
	int objnum;
	object* obj;

	vms_matrix orient = orient_in ? *orient_in : vmd_identity_matrix;

	Assert(segnum <= Highest_segment_index);
	Assert(segnum >= 0);
	Assert(ctype <= CT_CNTRLCEN);

	//if (type == OBJ_DEBRIS && Debris_object_count >= Max_debris_objects)
	//	return -1;

	if (get_seg_masks(pos, segnum, 0).centermask != 0)
		if ((segnum = find_point_seg(pos, segnum)) == -1) {
#ifndef NDEBUG
			mprintf((0, "Bad segnum in obj_create (type=%d)\n", type));
#endif
			return -1;		//don't create this object
		}

	// Find next free object
	objnum = obj_allocate();

	if (objnum == -1)		//no free objects
		return -1;

	obj = &Objects[objnum];

	Assert(obj->type == OBJ_NONE);		//make sure unused 
	Assert(obj->segnum == -1);

	// Zero out object structure to keep weird bugs from happening
	// in uninitialized fields.
	memset(obj, 0, sizeof(object));

	obj->signature = Object_next_signature++;
	obj->type = type;
	obj->id = id;
	obj->last_pos = pos;
	obj->pos = pos;
	obj->size = size;
	obj->flags = 0;
	//@@if (orient != NULL) 
	//@@	obj->orient 			= *orient;

	obj->orient = orient;

	obj->control_type = ctype;
	obj->movement_type = mtype;
	obj->render_type = rtype;
	obj->contains_type = -1;

	obj->lifeleft = IMMORTAL_TIME;		//assume immortal
	obj->attached_obj = -1;

	if (obj->control_type == CT_POWERUP)
		obj->ctype.powerup_info.count = 1;

	// Init physics info for this object
	if (obj->movement_type == MT_PHYSICS) 
	{
		vm_vec_zero(&obj->mtype.phys_info.velocity);
		vm_vec_zero(&obj->mtype.phys_info.thrust);
		vm_vec_zero(&obj->mtype.phys_info.rotvel);
		vm_vec_zero(&obj->mtype.phys_info.rotthrust);

		obj->mtype.phys_info.mass = 0;
		obj->mtype.phys_info.drag = 0;
		obj->mtype.phys_info.brakes = 0;
		obj->mtype.phys_info.turnroll = 0;
		obj->mtype.phys_info.flags = 0;
	}

	if (obj->render_type == RT_POLYOBJ)
		obj->rtype.pobj_info.tmap_override = -1;

	obj->shields = 20 * F1_0;

	segnum = find_point_seg(pos, segnum);		//find correct segment

	//Assert(segnum != -1);
	if (segnum == -1) {
		obj->type = OBJ_NONE;		//unused!
		obj->signature = -1;
		obj->segnum = -1;				// zero it!

		obj_free(objnum);

		mprintf((1, "New object outside of mine!\n"));

		return -1;
	}

	obj->segnum = -1;					//set to zero by memset, above
	obj_link(objnum, segnum);

	//	Set (or not) persistent bit in phys_info.
	if (obj->type == OBJ_WEAPON) 
	{
		Assert(obj->control_type == CT_WEAPON);
		obj->mtype.phys_info.flags |= (activeBMTable->weapons[obj->id].persistent * PF_PERSISTENT);
		obj->ctype.laser_info.creation_time = GameTime;
		obj->ctype.laser_info.last_hitobj = -1;
		obj->ctype.laser_info.multiplier = F1_0;
	}

	if (obj->control_type == CT_POWERUP)
		obj->ctype.powerup_info.creation_time = GameTime;

	if (obj->control_type == CT_EXPLOSION)
		obj->ctype.expl_info.next_attach = obj->ctype.expl_info.prev_attach = obj->ctype.expl_info.attach_parent = -1;

#ifndef NDEBUG
	if (print_object_info)
		mprintf((0, "Created object %d of type %d\n", objnum, obj->type));
#endif

	if (obj->type == OBJ_DEBRIS)
		Debris_object_count++;

	return objnum;
}

#ifdef EDITOR
//create a copy of an object. returns new object number
int obj_create_copy(int objnum, vms_vector new_pos, int newsegnum)
{
	int newobjnum;
	object* obj;

	// Find next free object
	newobjnum = obj_allocate();

	if (newobjnum == -1)
		return -1;

	obj = &Objects[newobjnum];

	*obj = Objects[objnum];

	obj->pos = obj->last_pos = *new_pos;

	obj->next = obj->prev = obj->segnum = -1;

	obj_link(newobjnum, newsegnum);

	obj->signature = Object_next_signature++;

	//we probably should initialize sub-structures here

	return newobjnum;

}
#endif

extern void newdemo_record_guided_end();

void obj_detach_all(object* parent);

//remove object from the world
void obj_delete(int objnum)
{
	int pnum;
	object* obj = &Objects[objnum];

	Assert(objnum != -1);
	Assert(objnum != 0);
	Assert(obj->type != OBJ_NONE);
	Assert(obj != ConsoleObject);

	if (obj->type == OBJ_WEAPON && obj->id == GUIDEDMISS_ID) 
	{
		pnum = Objects[obj->ctype.laser_info.parent_num].id;
		mprintf((0, "Deleting a guided missile! Player %d\n\n", pnum));

		if (pnum != Player_num) 
		{
			mprintf((0, "deleting missile that belongs to %d (%s)!\n", pnum, Players[pnum].callsign));
			Guided_missile[pnum] = NULL;
		}
		else if (Newdemo_state == ND_STATE_RECORDING)
			newdemo_record_guided_end();

	}

	if (obj == Viewer)		//deleting the viewer?
		Viewer = ConsoleObject;						//..make the player the viewer

	if (obj->flags & OF_ATTACHED)		//detach this from object
		obj_detach_one(obj);

	if (obj->attached_obj != -1)		//detach all objects from this
		obj_detach_all(obj);

#if !defined(NDEBUG) && !defined(NMONO)
	if (print_object_info) mprintf((0, "Deleting object %d of type %d\n", objnum, Objects[objnum].type));
#endif

	if (obj->type == OBJ_DEBRIS)
		Debris_object_count--;

	obj_unlink(objnum);

	Assert(Objects[0].next != 0);

	obj->type = OBJ_NONE;		//unused!
	obj->signature = -1;
	obj->segnum = -1;				// zero it!

	obj_free(objnum);
}

#define	DEATH_SEQUENCE_LENGTH			(F1_0*5)
#define	DEATH_SEQUENCE_EXPLODE_TIME	(F1_0*2)

int		Player_is_dead = 0;			//	If !0, then player is dead, but game continues so he can watch.
object* Dead_player_camera = NULL;	//	Object index of object watching deader.
fix		Player_time_of_death;		//	Time at which player died.
int		Player_flags_save;
int		Player_exploded = 0;
int		Death_sequence_aborted = 0;
int		Player_eggs_dropped = 0;
fix		Camera_to_player_dist_goal = F1_0 * 4;

uint8_t		Control_type_save, Render_type_save;
extern	int Cockpit_mode_save;	//set while in letterbox or rear view, or -1

//	------------------------------------------------------------------------------------------------------------------
void dead_player_end(void)
{
	if (!Player_is_dead)
		return;

	if (Newdemo_state == ND_STATE_RECORDING)
		newdemo_record_restore_cockpit();

	Player_is_dead = 0;
	Player_exploded = 0;
	obj_delete(Dead_player_camera - Objects.data());
	Dead_player_camera = NULL;
	select_cockpit(Cockpit_mode_save);
	Cockpit_mode_save = -1;
	Viewer = Viewer_save;
	ConsoleObject->type = OBJ_PLAYER;
	ConsoleObject->flags = Player_flags_save;

	Assert((Control_type_save == CT_FLYING) || (Control_type_save == CT_SLEW));

	ConsoleObject->control_type = Control_type_save;
	ConsoleObject->render_type = Render_type_save;
	Players[Player_num].flags &= ~PLAYER_FLAGS_INVULNERABLE;
	Player_eggs_dropped = 0;

}

//	------------------------------------------------------------------------------------------------------------------
//	Camera is less than size of player away from 
void set_camera_pos(vms_vector* camera_pos, object* objp)
{
	int	count = 0;
	fix	camera_player_dist;
	fix	far_scale;

	camera_player_dist = vm_vec_dist_quick(camera_pos, &objp->pos);

	if (camera_player_dist < Camera_to_player_dist_goal) //2*objp->size) {
	{ 
		//	Camera is too close to player object, so move it away.
		vms_vector	player_camera_vec;
		fvi_query	fq;
		fvi_info		hit_data;
		vms_vector	local_p1;

		vm_vec_sub(&player_camera_vec, camera_pos, &objp->pos);
		if ((player_camera_vec.x == 0) && (player_camera_vec.y == 0) && (player_camera_vec.z == 0))
			player_camera_vec.x += F1_0 / 16;

		hit_data.hit_type = HIT_WALL;
		far_scale = F1_0;

		while ((hit_data.hit_type != HIT_NONE) && (count++ < 6)) {
			vms_vector	closer_p1;
			vm_vec_normalize_quick(&player_camera_vec);
			vm_vec_scale(&player_camera_vec, Camera_to_player_dist_goal);

			fq.p0 = &objp->pos;
			vm_vec_add(&closer_p1, &objp->pos, &player_camera_vec);		//	This is the actual point we want to put the camera at.
			vm_vec_scale(&player_camera_vec, far_scale);						//	...but find a point 50% further away...
			vm_vec_add(&local_p1, &objp->pos, &player_camera_vec);		//	...so we won't have to do as many cuts.

			fq.p1 = &local_p1;
			fq.startseg = objp->segnum;
			fq.rad = 0;
			fq.thisobjnum = objp - Objects.data();
			fq.ignore_obj_list = NULL;
			fq.flags = 0;
			find_vector_intersection(&fq, &hit_data);

			if (hit_data.hit_type == HIT_NONE) {
				*camera_pos = closer_p1;
			}
			else 
			{
				make_random_vector(&player_camera_vec);
				far_scale = 3 * F1_0 / 2;
			}
		}
	}
}

extern void drop_player_eggs(size_t pobjnum);
extern int get_explosion_vclip(object* obj, int stage);
extern void multi_cap_objects();
extern int Proximity_dropped, Smartmines_dropped;

//	------------------------------------------------------------------------------------------------------------------
void AdjustMineSpawn()
{
	if (!(Game_mode & GM_NETWORK))
		return;  // No need for this function in any other mode

	if (!(Game_mode & GM_HOARD))
		Players[Player_num].secondary_ammo[PROXIMITY_INDEX] += Proximity_dropped;
	Players[Player_num].secondary_ammo[SMART_MINE_INDEX] += Smartmines_dropped;
	Proximity_dropped = 0;
	Smartmines_dropped = 0;
}

void dead_player_frame(void)
{
	fix	time_dead;
	vms_vector	fvec;

	if (Player_is_dead) 
	{
		time_dead = GameTime - Player_time_of_death;

		//	If unable to create camera at time of death, create now.
		if (Dead_player_camera == Viewer_save) 
		{
			int		objnum;
			object* player = &Objects[Players[Player_num].objnum];

			objnum = obj_create(OBJ_CAMERA, 0, player->segnum, player->pos, &player->orient, 0, CT_NONE, MT_NONE, RT_NONE);

			mprintf((0, "Creating new dead player camera.\n"));
			if (objnum != -1)
				Viewer = Dead_player_camera = &Objects[objnum];
			else {
				mprintf((1, "Can't create dead player camera.\n"));
				Int3();
			}
		}

		ConsoleObject->mtype.phys_info.rotvel.x = std::max(0, (int)(DEATH_SEQUENCE_EXPLODE_TIME - time_dead)) / 4;
		ConsoleObject->mtype.phys_info.rotvel.y = std::max(0, (int)(DEATH_SEQUENCE_EXPLODE_TIME - time_dead)) / 2;
		ConsoleObject->mtype.phys_info.rotvel.z = std::max(0, (int)(DEATH_SEQUENCE_EXPLODE_TIME - time_dead)) / 3;

		Camera_to_player_dist_goal = std::min((int)(time_dead * 8), F1_0 * 20) + ConsoleObject->size;

		set_camera_pos(&Dead_player_camera->pos, ConsoleObject);

		//		if (time_dead < DEATH_SEQUENCE_EXPLODE_TIME+F1_0*2) {
		vm_vec_sub(&fvec, &ConsoleObject->pos, &Dead_player_camera->pos);
		vm_vector_2_matrix(&Dead_player_camera->orient, &fvec, NULL, NULL);
		//		} else {
		//			Dead_player_camera->movement_type = MT_PHYSICS;
		//			Dead_player_camera->mtype.phys_info.rotvel.y = F1_0/8;
		//		}

		if (time_dead > DEATH_SEQUENCE_EXPLODE_TIME) 
		{
			if (!Player_exploded) 
			{

				if (Players[Player_num].hostages_on_board > 1)
					HUD_init_message(TXT_SHIP_DESTROYED_2, Players[Player_num].hostages_on_board);
				else if (Players[Player_num].hostages_on_board == 1)
					HUD_init_message(TXT_SHIP_DESTROYED_1);
				else
					HUD_init_message(TXT_SHIP_DESTROYED_0);

#ifdef TACTILE 
				if (TactileStick)
				{
					ClearForces();
				}
#endif
				Player_exploded = 1;
				if (!Arcade_mode)
				{
					if (Game_mode & GM_NETWORK)
					{
#ifdef NETWORK
						AdjustMineSpawn();
						multi_cap_objects();
#endif
					}

					drop_player_eggs(ConsoleObject - Objects.data());
					Player_eggs_dropped = 1;
#ifdef NETWORK
					if (Game_mode & GM_MULTI)
					{
						//multi_send_position(Players[Player_num].objnum);
						multi_send_player_explode(MULTI_PLAYER_EXPLODE);
					}
#endif
				}

				explode_badass_player(ConsoleObject);

				//is this next line needed, given the badass call above?
				explode_object(ConsoleObject, 0);
				ConsoleObject->flags &= ~OF_SHOULD_BE_DEAD;		//don't really kill player
				ConsoleObject->render_type = RT_NONE;				//..just make him disappear
				ConsoleObject->type = OBJ_GHOST;						//..and kill intersections
				Players[Player_num].flags &= ~PLAYER_FLAGS_HEADLIGHT_ON;
					}
				}
		else {
			if (P_Rand() < FrameTime * 4) {
#ifdef NETWORK
				if (Game_mode & GM_MULTI)
					multi_send_create_explosion(Player_num);
#endif
				create_small_fireball_on_object(ConsoleObject, F1_0, 1);
			}
		}


		if (!Arcade_mode) 
		{
			if (Death_sequence_aborted)  //time_dead > DEATH_SEQUENCE_LENGTH) {
			{
				if (!Player_eggs_dropped) 
				{
					if (Game_mode & GM_NETWORK)
					{
#ifdef NETWORK
						AdjustMineSpawn();
						multi_cap_objects();
#endif
					}

					drop_player_eggs(ConsoleObject - Objects.data());
					Player_eggs_dropped = 1;
#ifdef NETWORK
					if (Game_mode & GM_MULTI)
					{
						//multi_send_position(Players[Player_num].objnum);
						multi_send_player_explode(MULTI_PLAYER_EXPLODE);
					}
#endif
				}

				DoPlayerDead();		//kill_player();
					}
				}
		else {
			if (Death_sequence_aborted || (time_dead > DEATH_SEQUENCE_LENGTH)) {
				DoPlayerDead();		//kill_player();
			}
		}
	}
}




int Killed_in_frame = -1;
short Killed_objnum = -1;
extern char Multi_killed_yourself;

//	------------------------------------------------------------------------------------------------------------------
void start_player_death_sequence(object* player)
{
	int	objnum;

	Assert(player == ConsoleObject);
	if ((Player_is_dead != 0) || (Dead_player_camera != NULL))
		return;

	//Assert(Player_is_dead == 0);
	//Assert(Dead_player_camera == NULL);

	reset_rear_view();

	if (!(Game_mode & GM_MULTI))
		HUD_clear_messages();

	Killed_in_frame = FrameCount;
	Killed_objnum = player - Objects.data();
	Death_sequence_aborted = 0;

#ifdef NETWORK
	if (Game_mode & GM_MULTI)
	{
		multi_send_kill(Players[Player_num].objnum);

		//		If Hoard, increase number of orbs by 1
		//    Only if you haven't killed yourself
		//		This prevents cheating

		if (Game_mode & GM_HOARD)
			if (Players[Player_num].secondary_ammo[PROXIMITY_INDEX] < 12)
				if (!Multi_killed_yourself)
					Players[Player_num].secondary_ammo[PROXIMITY_INDEX]++;

	}
#endif

	PaletteRedAdd = 40;
	Player_is_dead = 1;
#ifdef TACTILE
	if (TactileStick)
		Buffeting(70);
#endif

	//Players[Player_num].flags &= ~(PLAYER_FLAGS_AFTERBURNER);

	vm_vec_zero(&player->mtype.phys_info.rotthrust);
	vm_vec_zero(&player->mtype.phys_info.thrust);

	Player_time_of_death = GameTime;

	size_t tempID = player - Objects.data();
	objnum = obj_create(OBJ_CAMERA, 0, player->segnum, player->pos, &player->orient, 0, CT_NONE, MT_NONE, RT_NONE);
	player = &Objects[tempID];

	Viewer_save = Viewer;
	if (objnum != -1)
		Viewer = Dead_player_camera = &Objects[objnum];
	else {
		mprintf((1, "Can't create dead player camera.\n"));
		Int3();
		Dead_player_camera = Viewer;
	}

	if (Cockpit_mode_save == -1)		//if not already saved
		Cockpit_mode_save = Cockpit_mode;
	select_cockpit(CM_LETTERBOX);
	if (Newdemo_state == ND_STATE_RECORDING)
		newdemo_record_letterbox();

	Player_flags_save = player->flags;
	Control_type_save = player->control_type;
	Render_type_save = player->render_type;

	player->flags &= ~OF_SHOULD_BE_DEAD;
	//	Players[Player_num].flags |= PLAYER_FLAGS_INVULNERABLE;
	player->control_type = CT_NONE;
	player->shields = F1_0 * 1000;

	PALETTE_FLASH_SET(0, 0, 0);
	}

//	------------------------------------------------------------------------------------------------------------------
void obj_delete_all_that_should_be_dead()
{
	int i;
	object* objp;
	int		local_dead_player_object = -1;

	// Move all objects
	//objp = Objects;

	for (i = 0; i <= Highest_object_index; i++) {
		objp = &Objects[i];
		if ((objp->type != OBJ_NONE) && (objp->flags & OF_SHOULD_BE_DEAD)) {
			Assert(!(objp->type == OBJ_FIREBALL && objp->ctype.expl_info.delete_time != -1));
			if (objp->type == OBJ_PLAYER) {
				if (objp->id == Player_num) {
					if (local_dead_player_object == -1) {
						start_player_death_sequence(objp);
						local_dead_player_object = objp - Objects.data();
					}
					else
						Int3();	//	Contact Mike: Illegal, killed player twice in this frame!
									// Ok to continue, won't start death sequence again!
					// kill_player();
				}
			}
			else {
				obj_delete(i);
			}
		}
		//objp++;
	}
}

//when an object has moved into a new segment, this function unlinks it
//from its old segment, and links it into the new segment
void obj_relink(int objnum, int newsegnum)
{

	Assert((objnum >= 0) && (objnum <= Highest_object_index));
	Assert((newsegnum <= Highest_segment_index) && (newsegnum >= 0));

	obj_unlink(objnum);

	obj_link(objnum, newsegnum);

#ifndef NDEBUG
	if (get_seg_masks(Objects[objnum].pos, Objects[objnum].segnum, 0).centermask != 0)
		mprintf((1, "obj_relink violates seg masks.\n"));
#endif
}

//process a continuously-spinning object
void spin_object(object* obj)
{
	vms_angvec rotangs;
	vms_matrix rotmat, new_pm;

	Assert(obj->movement_type == MT_SPINNING);

	rotangs.p = fixmul(obj->mtype.spin_rate.x, FrameTime);
	rotangs.h = fixmul(obj->mtype.spin_rate.y, FrameTime);
	rotangs.b = fixmul(obj->mtype.spin_rate.z, FrameTime);

	vm_angles_2_matrix(&rotmat, &rotangs);

	vm_matrix_x_matrix(&new_pm, &obj->orient, &rotmat);
	obj->orient = new_pm;

	check_and_fix_matrix(&obj->orient);
}

int Drop_afterburner_blob_flag;		//ugly hack
extern void multi_send_drop_blobs(char);
extern void fuelcen_check_for_goal(segment*);

//see if wall is volatile, and if so, cause damage to player  
//returns true if player is in lava
int check_volatile_wall(object* obj, int segnum, int sidenum, vms_vector hitpt);

//	Time at which this object last created afterburner blobs.

std::vector<fix> Last_afterburner_time(MAX_OBJECTS);

//--------------------------------------------------------------------
//move an object for the current frame
void object_move_one(int objnum)
{
#ifndef DEMO_ONLY

	object* obj = &Objects[objnum];

	int	previous_segment = obj->segnum;

	obj->last_pos = obj->pos;			// Save the current position

	if ((obj->type == OBJ_PLAYER) && (Player_num == obj->id)) 
	{
		fix fuel;

		if (Game_mode & GM_CAPTURE)
			fuelcen_check_for_goal(&Segments[obj->segnum]);
#ifdef NETWORK
		if (Game_mode & GM_HOARD)
			fuelcen_check_for_hoard_goal(&Segments[obj->segnum]);
#endif

		fuel = fuelcen_give_fuel(&Segments[obj->segnum], INITIAL_ENERGY - Players[Player_num].energy);
		if (fuel > 0) {
			Players[Player_num].energy += fuel;

		}
	}

	if (obj->lifeleft != IMMORTAL_TIME) //if not immortal...
	{	
		//	Ok, this is a big hack by MK.
		//	If you want an object to last for exactly one frame, then give it a lifeleft of ONE_FRAME_TIME.
		if (obj->lifeleft != ONE_FRAME_TIME)
			obj->lifeleft -= FrameTime;		//...inevitable countdown towards death
	}

	Drop_afterburner_blob_flag = 0;

	switch (obj->control_type) 
	{

	case CT_NONE: break;
	case CT_FLYING:
#if !defined(NDEBUG) && !defined(NMONO)
		if (print_object_info > 1) mprintf((0, "Moving player object #%d\n", obj - Objects.data()));
#endif
		read_flying_controls(obj);
		break;
	case CT_REPAIRCEN: Int3();	// -- hey! these are no longer supported!! -- do_repair_sequence(obj); break;
	case CT_POWERUP: do_powerup_frame(obj); break;
	case CT_MORPH:			//morph implies AI
		do_morph_frame(obj);
		//NOTE: FALLS INTO AI HERE!!!!
	case CT_AI:
		//NOTE LINK TO CT_MORPH ABOVE!!!
		if (Game_suspended & SUSP_ROBOTS) return;
#if !defined(NDEBUG) && !defined(NMONO)
		if (print_object_info > 1) mprintf((0, "AI: Moving robot object #%d\n", obj - Objects.data()));
#endif
		do_ai_frame(objnum);
		break;

	case CT_WEAPON:		Laser_do_weapon_sequence(obj); break;
	case CT_EXPLOSION:	do_explosion_sequence(obj); break;

#ifndef RELEASE
	case CT_SLEW:
		if (keyd_pressed[KEY_PAD5]) slew_stop();//slew_stop(obj);
		if (keyd_pressed[KEY_NUMLOCK]) 
		{
			//slew_reset_orient(obj);
			slew_reset_orient();
		}
		slew_frame(0);		// Does velocity addition for us.
		break;
#endif


		//		case CT_FLYTHROUGH:
		//			do_flythrough(obj,0);			// HACK:do_flythrough should operate on an object!!!!
		//			//check_object_seg(obj);
		//			return;	// DON'T DO THE REST OF OBJECT STUFF SINCE THIS IS A SPECIAL CASE!!!
		//			break;

	case CT_DEBRIS: do_debris_frame(obj); break;
	case CT_LIGHT: break;		//doesn't do anything
	case CT_REMOTE: break;		//movement is handled in com_process_input
	case CT_CNTRLCEN: do_controlcen_frame(obj); break;
	default:
		Error("Unknown control type %d in object %i, sig/type/id = %i/%i/%i", obj->control_type, obj - Objects.data(), obj->signature, obj->type, obj->id);
		break;

	}

	obj = &Objects[objnum]; //reset in case any object creations caused a reallocation

	if (obj->lifeleft < 0) {		// We died of old age
		obj->flags |= OF_SHOULD_BE_DEAD;
		if (obj->type == OBJ_WEAPON) {
			if (activeBMTable->weapons[obj->id].damage_radius)
				explode_badass_weapon(obj, obj->pos);
			create_smart_children(&Objects[objnum], NUM_SMART_CHILDREN); //just grab object inline in case explosion caused reallocation
			obj = &Objects[objnum]; //smart children call might have caused yet another reallocation
		}
		else if (obj->type == OBJ_ROBOT) {	//make robots explode
			explode_object(obj, 0);	
			obj = &Objects[objnum]; 
		}
	}

	if (obj->type == OBJ_NONE || obj->flags & OF_SHOULD_BE_DEAD)
		return;			//object has been deleted

	switch (obj->movement_type) {

	case MT_NONE:			
		break;	//this doesn't move

	case MT_PHYSICS:		
		do_physics_sim(objnum);	
		obj = &Objects[objnum]; 
		break;	//move by physics

	case MT_SPINNING:		
		spin_object(obj); 
		break;

	}

	//	If player and moved to another segment, see if hit any triggers.
	// also check in player under a lavafall
	if (obj->type == OBJ_PLAYER && obj->movement_type == MT_PHYSICS) {

		if (previous_segment != obj->segnum) {
			int	connect_side, i;
			int	old_level = Current_level_num;
			for (i = 0; i < n_phys_segs - 1; i++) {
				connect_side = find_connect_side(&Segments[phys_seglist[i + 1]], &Segments[phys_seglist[i]]);
				if (connect_side != -1)
					check_trigger(&Segments[phys_seglist[i]], connect_side, obj - Objects.data(), 0);
#ifndef NDEBUG
				else {	// segments are not directly connected, so do binary subdivision until you find connected segments.
					mprintf((1, "UNCONNECTED SEGMENTS %d,%d\n", phys_seglist[i + 1], phys_seglist[i]));
					// -- Unnecessary, MK, 09/04/95 -- Int3();
				}
#endif

				//maybe we've gone on to the next level.  if so, bail!
				if (Current_level_num != old_level)
					return;
			}
		}

		{
			int sidemask, under_lavafall = 0;
			static int lavafall_hiss_playing[MAX_PLAYERS] = { 0 };

			sidemask = get_seg_masks(obj->pos, obj->segnum, obj->size).sidemask;
			if (sidemask) {
				int sidenum, bit, wall_num;

				for (sidenum = 0, bit = 1; sidenum < 6; bit <<= 1, sidenum++)
					if ((sidemask & bit) && ((wall_num = Segments[obj->segnum].sides[sidenum].wall_num) != -1) && Walls[wall_num].type == WALL_ILLUSION) {
						int type;
						if ((type = check_volatile_wall(obj, obj->segnum, sidenum, obj->pos)) != 0) {
							int sound = (type == 1) ? SOUND_LAVAFALL_HISS : SOUND_SHIP_IN_WATERFALL;
							under_lavafall = 1;
							if (!lavafall_hiss_playing[obj->id]) {
								digi_link_sound_to_object3(sound, obj - Objects.data(), 1, F1_0, i2f(256), -1, -1);
								lavafall_hiss_playing[obj->id] = 1;
							}
						}
					}
			}

			if (!under_lavafall && lavafall_hiss_playing[obj->id]) {
				digi_kill_sound_linked_to_object(obj - Objects.data());
				lavafall_hiss_playing[obj->id] = 0;
			}
		}
	}

	//see if guided missile has flown through exit trigger
	if (obj == Guided_missile[Player_num] && obj->signature == Guided_missile_sig[Player_num]) {
		if (previous_segment != obj->segnum) {
			int	connect_side;
			connect_side = find_connect_side(&Segments[obj->segnum], &Segments[previous_segment]);
			if (connect_side != -1) {
				int wall_num, trigger_num;
				wall_num = Segments[previous_segment].sides[connect_side].wall_num;
				if (wall_num != -1) {
					trigger_num = Walls[wall_num].trigger;
					if (trigger_num != -1)
						if (Triggers[trigger_num].type & HTT_EXIT)
							Guided_missile[Player_num]->lifeleft = 0;
				}
			}
		}
	}

	if (Drop_afterburner_blob_flag) {
		Assert(obj == ConsoleObject);
		drop_afterburner_blobs(objnum, 2, i2f(5) / 2, -1);	//	-1 means use default lifetime
		obj = &Objects[objnum]; 
#ifdef NETWORK
		if (Game_mode & GM_MULTI)
			multi_send_drop_blobs(Player_num);
#endif
		Drop_afterburner_blob_flag = 0;
	}

	if ((obj->type == OBJ_WEAPON) && (activeBMTable->weapons[obj->id].afterburner_size)) {
		fix	vel = vm_vec_mag_quick(&obj->mtype.phys_info.velocity);
		fix	delay, lifetime;

		if (vel > F1_0 * 200)
			delay = F1_0 / 16;
		else if (vel > F1_0 * 40)
			delay = fixdiv(F1_0 * 13, vel);
		else
			delay = F1_0 / 4;

		lifetime = (delay * 3) / 2;
		if (!(Game_mode & GM_MULTI)) {
			delay /= 2;
			lifetime *= 2;
		}

		if ((Last_afterburner_time[objnum] + delay < GameTime) || (Last_afterburner_time[objnum] > GameTime)) {
			drop_afterburner_blobs(objnum, 1, i2f(activeBMTable->weapons[obj->id].afterburner_size) / 16, lifetime);
			obj = &Objects[objnum]; 
			Last_afterburner_time[objnum] = GameTime;
		}
	}

#else
	obj++;		//kill warning
#endif		//DEMO_ONLY
}

int	Max_used_objects = MAX_OBJECTS - 20;

//--------------------------------------------------------------------
//move all objects for the current frame
void object_move_all()
{
	int i;
	object* objp;

	Max_used_objects = Objects.size() - 20;

	// -- mprintf((0, "Frame %i: %i/%i objects used.\n", FrameCount, num_objects, MAX_OBJECTS));

	//	check_duplicate_objects();
	//	remove_incorrect_objects();

	//if (Highest_object_index > Max_used_objects)
	//	free_object_slots(Max_used_objects);		//	Free all possible object slots.

	obj_delete_all_that_should_be_dead();

	if (Auto_leveling_on)
		ConsoleObject->mtype.phys_info.flags |= PF_LEVELLING;
	else
		ConsoleObject->mtype.phys_info.flags &= ~PF_LEVELLING;

	// Move all objects
	//objp = Objects;

#ifndef DEMO_ONLY
	for (i = 0; i <= Highest_object_index; i++) {
		objp = &Objects[i];
		if ((objp->type != OBJ_NONE) && (!(objp->flags & OF_SHOULD_BE_DEAD))) {
			object_move_one(i);
		}
		//objp++;
	}
#else
	i = 0;	//kill warning
#endif

//	check_duplicate_objects();
//	remove_incorrect_objects();

}

//make object array non-sparse
void compress_objects(void)
{
	extern object* Missile_viewer;
	extern object* Viewer_save;
	extern object* Guided_missile[MAX_PLAYERS];
	extern object* old_viewer;
	extern object* prev_obj;
	extern object* slew_obj;

	int start_i;	//,last_i;

#ifdef NETWORK
	std::vector<std::pair<uint32_t, uint32_t>> netRemaps;
	if (Game_mode && GM_MULTI) 
		netRemaps.reserve(Objects.size());
#endif

	//	Note: It's proper to do < (rather than <=) Highest_object_index here because we
	//	are just removing gaps, and the last object can't be a gap.
	for (start_i = 0; start_i < Highest_object_index; start_i++) {
		if (Objects[start_i].type == OBJ_NONE) {

			std::vector<object**> specialRelinks;
			int	segnum_copy;

			if (&Objects[Highest_object_index] == Viewer)
				specialRelinks.push_back(&Viewer);
			if (&Objects[Highest_object_index] == Missile_viewer)
				specialRelinks.push_back(&Missile_viewer);
			if (&Objects[Highest_object_index] == Viewer_save)
				specialRelinks.push_back(&Viewer_save);
			if (&Objects[Highest_object_index] == Dead_player_camera)
				specialRelinks.push_back(&Dead_player_camera);
			if (&Objects[Highest_object_index] == old_viewer)
				specialRelinks.push_back(&old_viewer);
			if (&Objects[Highest_object_index] == prev_obj)
				specialRelinks.push_back(&prev_obj);
			if (&Objects[Highest_object_index] == slew_obj)
				specialRelinks.push_back(&slew_obj);
			for (int i = 0; i < MAX_PLAYERS; i++) {
				if (&Objects[Highest_object_index] == Guided_missile[i])
					specialRelinks.push_back(&Guided_missile[i]);
			}
			for (auto& morph : morph_objects) {
				if (morph.obj == &Objects[Highest_object_index])
					specialRelinks.push_back(&morph.obj);
			}

			//Adjust objnum-as-pointer values embedded within objects
			for (int i = 0; i < Highest_object_index; i++) {

				if (Objects[i].next == Highest_object_index)
					Objects[i].next = start_i;

				if (Objects[i].prev == Highest_object_index)
					Objects[i].prev = start_i;

				if (Objects[i].attached_obj == Highest_object_index)
					Objects[i].attached_obj = start_i;

				if (Objects[i].control_type == CT_WEAPON) {

					if (Objects[i].ctype.laser_info.parent_num == Highest_object_index)
						Objects[i].ctype.laser_info.parent_num = start_i;

				} else if (Objects[i].control_type == CT_EXPLOSION || Objects[i].control_type == CT_DEBRIS) {

					if (Objects[i].ctype.expl_info.next_attach == Highest_object_index)
						Objects[i].ctype.expl_info.next_attach = start_i;

					if (Objects[i].ctype.expl_info.prev_attach == Highest_object_index)
						Objects[i].ctype.expl_info.prev_attach = start_i;

					if (Objects[i].ctype.expl_info.attach_parent == Highest_object_index)
						Objects[i].ctype.expl_info.attach_parent = start_i;

					if (Objects[i].ctype.expl_info.delete_objnum == Highest_object_index)
						Objects[i].ctype.expl_info.delete_objnum = start_i;
					
				} else if (Objects[i].control_type == CT_AI) {

					if (Objects[i].ctype.ai_info.danger_laser_num == Highest_object_index)
						Objects[i].ctype.ai_info.danger_laser_num = start_i;

				}

			}

			segnum_copy = Objects[Highest_object_index].segnum;

			obj_unlink(Highest_object_index);

			Objects[start_i] = Objects[Highest_object_index];
			Ai_local_info[start_i] = Ai_local_info[Highest_object_index];
			Last_afterburner_time[start_i] = Last_afterburner_time[Highest_object_index];
			Lighting_objects[start_i] = Lighting_objects[Highest_object_index];
			object_light[start_i] = object_light[Highest_object_index];
			object_sig[start_i] = object_sig[Highest_object_index];
			WasRecorded[start_i] = WasRecorded[Highest_object_index];
			ViewWasRecorded[start_i] = ViewWasRecorded[Highest_object_index];

#ifdef NETWORK
			if (Game_mode & GM_MULTI) {
				object_owner[start_i] = object_owner[Highest_object_index];

				uint32_t remoteLow = local_to_remote[start_i];
				uint16_t remoteHigh = local_to_remote[Highest_object_index];

				local_to_remote[start_i] = remoteHigh;

				remote_to_local[Player_num][remoteLow] = remote_to_local[Player_num][remoteHigh];
				remote_to_local[Player_num][remoteHigh] = -1;
				netRemaps.push_back(std::pair(remoteLow, remoteHigh));
			}
#endif

#ifdef EDITOR
			if (Cur_object_index == Highest_object_index)
				Cur_object_index = start_i;
#endif

			Objects[Highest_object_index].type = OBJ_NONE;

			obj_link(start_i, segnum_copy);

			for (auto& addr : specialRelinks)
				*addr = &Objects[start_i];

			while (Objects[--Highest_object_index].type == OBJ_NONE) 
				{ /*Do nothing*/ }

			//last_i = find_last_obj(last_i);

		}
	}

#ifdef NETWORK
	if (Game_mode & GM_MULTI) {
		int maxIndex;

		for (maxIndex = remote_to_local[Player_num].size() - 1; maxIndex == -1 && maxIndex >= 0; maxIndex--) 
			{}
		
		remote_to_local[Player_num].resize(maxIndex + 1);

		multi_send_remote_remap(remote_to_local[Player_num].size(), netRemaps);
	}
#endif

	reset_objects(num_objects, true);
	verify_console_object();

}

//called after load.  Takes number of objects,  and objects should be 
//compressed.  resets free list, marks unused objects as unused
void reset_objects(int n_objs, bool inLevel)
{
	int i;

	num_objects = n_objs;

	Assert(num_objects > 0);

	if (free_obj_list.size() != Objects.size()) {
		free_obj_list.resize(Objects.size());
		free_obj_list.push_back(Objects.size());
	}

	for (i = num_objects; i < Objects.size(); i++) {
		free_obj_list[i] = i;
		Objects[i].type = OBJ_NONE;
		Objects[i].segnum = -1;
	}

	Highest_object_index = num_objects - 1;

	if (!inLevel)
		Debris_object_count = 0;
}

//Tries to find a segment for an object, using find_point_seg()
int find_object_seg(object* obj)
{
	return find_point_seg(obj->pos, obj->segnum);
}


//If an object is in a segment, set its segnum field and make sure it's
//properly linked.  If not in any segment, returns 0, else 1.
//callers should generally use find_vector_intersection()  
int update_object_seg(object* obj)
{
	int newseg;

	newseg = find_object_seg(obj);

	if (newseg == -1)
		return 0;

	if (newseg != obj->segnum)
		obj_relink(obj - Objects.data(), newseg);

	return 1;
}


//go through all objects and make sure they have the correct segment numbers
void fix_object_segs()
{
	int i;

	for (i = 0; i <= Highest_object_index; i++)
		if (Objects[i].type != OBJ_NONE)
			if (update_object_seg(&Objects[i]) == 0) 
			{
				mprintf((1, "Cannot find segment for object %d in fix_object_segs()\n"));
				Int3();
				compute_segment_center(&Objects[i].pos, &Segments[Objects[i].segnum]);
			}
}

//delete objects, such as weapons & explosions, that shouldn't stay between levels
//	Changed by MK on 10/15/94, don't remove proximity bombs.
//if clear_all is set, clear even proximity bombs
void clear_transient_objects(int clear_all)
{
	int objnum;
	object* obj;

	for (objnum = 0, obj = &Objects[0]; objnum <= Highest_object_index; objnum++, obj++)
		if (((obj->type == OBJ_WEAPON) && !(activeBMTable->weapons[obj->id].flags & WIF_PLACABLE) && (clear_all || ((obj->id != PROXIMITY_ID) && (obj->id != SUPERPROX_ID)))) ||
			obj->type == OBJ_FIREBALL ||
			obj->type == OBJ_DEBRIS ||
			obj->type == OBJ_DEBRIS ||
			(obj->type != OBJ_NONE && obj->flags & OF_EXPLODING)) {

#ifndef NDEBUG
			if (Objects[objnum].lifeleft > i2f(2))
				mprintf((0, "Note: Clearing object %d (type=%d, id=%d) with lifeleft=%x\n", objnum, Objects[objnum].type, Objects[objnum].id, Objects[objnum].lifeleft));
#endif
			obj_delete(objnum);
		}
#ifndef NDEBUG
		else if (Objects[objnum].type != OBJ_NONE && Objects[objnum].lifeleft < i2f(2))
			mprintf((0, "Note: NOT clearing object %d (type=%d, id=%d) with lifeleft=%x\n", objnum, Objects[objnum].type, Objects[objnum].id, Objects[objnum].lifeleft));
#endif
}

//attaches an object, such as a fireball, to another object, such as a robot
void obj_attach(object* parent, object* sub)
{
	Assert(sub->type == OBJ_FIREBALL);
	Assert(sub->control_type == CT_EXPLOSION);

	Assert(sub->ctype.expl_info.next_attach == -1);
	Assert(sub->ctype.expl_info.prev_attach == -1);

	Assert(parent->attached_obj == -1 || Objects[parent->attached_obj].ctype.expl_info.prev_attach == -1);

	sub->ctype.expl_info.next_attach = parent->attached_obj;

	if (sub->ctype.expl_info.next_attach != -1)
		Objects[sub->ctype.expl_info.next_attach].ctype.expl_info.prev_attach = sub - Objects.data();

	parent->attached_obj = sub - Objects.data();

	sub->ctype.expl_info.attach_parent = parent - Objects.data();
	sub->flags |= OF_ATTACHED;

	Assert(sub->ctype.expl_info.next_attach != sub - Objects.data());
	Assert(sub->ctype.expl_info.prev_attach != sub - Objects.data());
}

//dettaches one object
void obj_detach_one(object* sub)
{
	Assert(sub->flags & OF_ATTACHED);
	Assert(sub->ctype.expl_info.attach_parent != -1);

	if ((Objects[sub->ctype.expl_info.attach_parent].type == OBJ_NONE) || (Objects[sub->ctype.expl_info.attach_parent].attached_obj == -1))
	{
		sub->flags &= ~OF_ATTACHED;
		return;
	}

	if (sub->ctype.expl_info.next_attach != -1) {
		Assert(Objects[sub->ctype.expl_info.next_attach].ctype.expl_info.prev_attach = sub - Objects.data());
		Objects[sub->ctype.expl_info.next_attach].ctype.expl_info.prev_attach = sub->ctype.expl_info.prev_attach;
	}

	if (sub->ctype.expl_info.prev_attach != -1) {
		Assert(Objects[sub->ctype.expl_info.prev_attach].ctype.expl_info.next_attach = sub - Objects.data());
		Objects[sub->ctype.expl_info.prev_attach].ctype.expl_info.next_attach = sub->ctype.expl_info.next_attach;
	}
	else {
		Assert(Objects[sub->ctype.expl_info.attach_parent].attached_obj = sub - Objects.data());
		Objects[sub->ctype.expl_info.attach_parent].attached_obj = sub->ctype.expl_info.next_attach;
	}

	sub->ctype.expl_info.next_attach = sub->ctype.expl_info.prev_attach = -1;
	sub->flags &= ~OF_ATTACHED;

}

//dettaches all objects from this object
void obj_detach_all(object* parent)
{
	while (parent->attached_obj != -1)
		obj_detach_one(&Objects[parent->attached_obj]);
}

//creates a marker object in the world.  returns the object number
int drop_marker_object(vms_vector pos, int segnum, vms_matrix orient, int marker_num)
{
	int objnum;

	Assert(activeBMTable->markerModel != -1);

	objnum = obj_create(OBJ_MARKER, marker_num, segnum, pos, &orient, activeBMTable->models[activeBMTable->markerModel].rad, CT_NONE, MT_NONE, RT_POLYOBJ);

	if (objnum >= 0) {
		object* obj = &Objects[objnum];

		obj->rtype.pobj_info.model_num = activeBMTable->markerModel;

		vm_vec_copy_scale(&obj->mtype.spin_rate, &obj->orient.uvec, F1_0 / 2);

		//	MK, 10/16/95: Using lifeleft to make it flash, thus able to trim lightlevel from all objects.
		obj->lifeleft = IMMORTAL_TIME - 1;
	}

	return objnum;
}

extern int Ai_last_missile_camera;

//	*viewer is a viewer, probably a missile.
//	wake up all robots that were rendered last frame subject to some constraints.
void wake_up_rendered_objects(object* viewer, int window_num)
{
	int	i;

	//	Make sure that we are processing current data.
	if (FrameCount != Window_rendered_data[window_num].frame) 
	{
		mprintf((1, "Warning: Called wake_up_rendered_objects with a bogus window.\n"));
		return;
	}

	Ai_last_missile_camera = viewer - Objects.data();

	for (i = 0; i < Window_rendered_data[window_num].num_objects; i++) 
	{
		int	objnum;
		object* objp;
		int	fcval = FrameCount & 3;

		objnum = Window_rendered_data[window_num].rendered_objects[i];
		if ((objnum & 3) == fcval)
		{
			objp = &Objects[objnum];

			if (objp->type == OBJ_ROBOT) 
			{
				if (vm_vec_dist_quick(&viewer->pos, &objp->pos) < F1_0 * 100) {
					ai_local* ailp = &Ai_local_info[objnum];
					if (ailp->player_awareness_type == 0) {
						objp->ctype.ai_info.SUB_FLAGS |= SUB_FLAGS_CAMERA_AWAKE;
						ailp->player_awareness_type = PA_WEAPON_ROBOT_COLLISION;
						ailp->player_awareness_time = F1_0 * 3;
						ailp->previous_visibility = 2;
					}
				}
			}
		}
	}
}

static void read_vector(vms_vector* v, FILE* file)
{
	//v->x = read_fix(file);
	//v->y = read_fix(file);
	//v->z = read_fix(file);
	v->x = file_read_int(file);
	v->y = file_read_int(file);
	v->z = file_read_int(file);
}

static void read_matrix(vms_matrix* m, FILE* file)
{
	read_vector(&m->rvec, file);
	read_vector(&m->uvec, file);
	read_vector(&m->fvec, file);
}

static void read_angvec(vms_angvec* v, FILE* file)
{
	v->p = file_read_short(file);
	v->b = file_read_short(file);
	v->h = file_read_short(file);
}

static void write_vector(vms_vector* v, FILE* file)
{
	//v->x = read_fix(file);
	//v->y = read_fix(file);
	//v->z = read_fix(file);
	file_write_int(file, v->x);
	file_write_int(file, v->y);
	file_write_int(file, v->z);
}

static void write_matrix(vms_matrix* m, FILE* file)
{
	write_vector(&m->rvec, file);
	write_vector(&m->uvec, file);
	write_vector(&m->fvec, file);
}

static void write_angvec(vms_angvec* v, FILE* file)
{
	file_write_short(file, v->p);
	file_write_short(file, v->b);
	file_write_short(file, v->h);
}

//Read an object from disk. These unions are going to be the end of me.....
void read_obj_instance(object* obj, FILE* f)
{
	//Pad shorter structures by this much
	int bytesLeft;
	obj->signature = file_read_int(f);
	obj->type = file_read_byte(f);
	obj->id = file_read_byte(f);

	obj->next = file_read_short(f); obj->prev = file_read_short(f);

	obj->control_type = file_read_byte(f);
	obj->movement_type = file_read_byte(f);
	obj->render_type = file_read_byte(f);
	obj->flags = file_read_byte(f);

	obj->segnum = file_read_short(f);
	obj->attached_obj = file_read_short(f);

	read_vector(&obj->pos, f);
	read_matrix(&obj->orient, f);

	obj->size = file_read_int(f);
	obj->shields = file_read_int(f);

	read_vector(&obj->last_pos, f);

	obj->contains_type = file_read_byte(f);
	obj->contains_id = file_read_byte(f);
	obj->contains_count = file_read_byte(f);
	obj->matcen_creator = file_read_byte(f);

	obj->lifeleft = file_read_int(f);

	bytesLeft = 64;

	switch (obj->movement_type)
	{
	case MT_PHYSICS:
		read_vector(&obj->mtype.phys_info.velocity, f);
		read_vector(&obj->mtype.phys_info.thrust, f);
		obj->mtype.phys_info.mass = file_read_int(f);
		obj->mtype.phys_info.drag = file_read_int(f);
		obj->mtype.phys_info.brakes = file_read_int(f);
		read_vector(&obj->mtype.phys_info.rotvel, f);
		read_vector(&obj->mtype.phys_info.rotthrust, f);
		obj->mtype.phys_info.turnroll = file_read_short(f);
		obj->mtype.phys_info.flags = file_read_short(f);
		bytesLeft = 0;
		break;
	case MT_SPINNING:
		read_vector(&obj->mtype.spin_rate, f);
		bytesLeft -= 12;
		break;
	}
	fseek(f, bytesLeft, SEEK_CUR);

	bytesLeft = 34;
	switch (obj->control_type)
	{

	case CT_AI:
	{
		//30 //[ISB] gee, this sturcture is very aligned.
		int i;
		obj->ctype.ai_info.behavior = file_read_byte(f); //1

		for (i = 0; i < MAX_AI_FLAGS; i++)
			obj->ctype.ai_info.flags[i] = file_read_byte(f); //12

		obj->ctype.ai_info.hide_segment = file_read_short(f); //14
		obj->ctype.ai_info.hide_index = file_read_short(f); //16
		obj->ctype.ai_info.path_length = file_read_short(f); //18
		obj->ctype.ai_info.cur_path_index = file_read_byte(f); //19
		obj->ctype.ai_info.dying_sound_playing = file_read_byte(f); //20

		obj->ctype.ai_info.danger_laser_num = file_read_short(f); //22
		obj->ctype.ai_info.danger_laser_signature = file_read_int(f); //26
		obj->ctype.ai_info.dying_start_time = file_read_int(f); //30

		obj->ctype.ai_info.follow_path_start_seg = file_read_short(f); // [DW] D1 needs these
		obj->ctype.ai_info.follow_path_end_seg = file_read_short(f);

		bytesLeft = 0;
		break;
	}
	case CT_EXPLOSION:
	case CT_DEBRIS:
		//16
		obj->ctype.expl_info.spawn_time = file_read_int(f);
		obj->ctype.expl_info.delete_time = file_read_int(f);
		obj->ctype.expl_info.delete_objnum = file_read_short(f);
		obj->ctype.expl_info.attach_parent = file_read_short(f);
		obj->ctype.expl_info.next_attach = file_read_short(f);
		obj->ctype.expl_info.prev_attach = file_read_short(f);
		bytesLeft -= 16;
		break;
	case CT_WEAPON:
		//20
		obj->ctype.laser_info.parent_type = file_read_short(f);
		obj->ctype.laser_info.parent_num = file_read_short(f);
		obj->ctype.laser_info.parent_signature = file_read_int(f);
		obj->ctype.laser_info.creation_time = file_read_int(f);
		obj->ctype.laser_info.last_hitobj = file_read_short(f);
		obj->ctype.laser_info.track_goal = file_read_short(f);
		obj->ctype.laser_info.multiplier = file_read_int(f);
		bytesLeft -= 20;
		break;
	case CT_LIGHT:
		//4
		obj->ctype.light_info.intensity = file_read_int(f);
		bytesLeft -= 4;
		break;
	case CT_POWERUP:
		//4
		obj->ctype.powerup_info.count = file_read_int(f);
		obj->ctype.powerup_info.creation_time = file_read_int(f);
		obj->ctype.powerup_info.flags = file_read_int(f);
		bytesLeft -= 12;
		break;
	}
	fseek(f, bytesLeft, SEEK_CUR);

	bytesLeft = 76;
	switch (obj->render_type)
	{
	case RT_NONE:
		break;
	case RT_MORPH:
	case RT_POLYOBJ:
	{
		//76
		int i;
		obj->rtype.pobj_info.model_num = file_read_int(f);
		for (i = 0; i < MAX_SUBMODELS; i++)
			read_angvec(&obj->rtype.pobj_info.anim_angles[i], f);
		obj->rtype.pobj_info.subobj_flags = file_read_int(f);
		obj->rtype.pobj_info.tmap_override = file_read_int(f);
		obj->rtype.pobj_info.alt_textures = file_read_int(f);
		bytesLeft = 0;
		break;
	}
	case RT_WEAPON_VCLIP:
	case RT_HOSTAGE:
	case RT_POWERUP:
	case RT_FIREBALL:
		//9
		obj->rtype.vclip_info.vclip_num = file_read_int(f);
		obj->rtype.vclip_info.frametime = file_read_int(f);
		obj->rtype.vclip_info.framenum = file_read_byte(f);
		bytesLeft -= 9;
		break;
	case RT_LASER:
		break;
	}
	fseek(f, bytesLeft, SEEK_CUR);
}

void write_obj_instance(object* obj, FILE* f)
{
	//Pad shorter structures by this much
	int bytesLeft;
	uint8_t hack[76];
	memset(&hack[0], 0, 76 * sizeof(uint8_t));
	file_write_int(f, obj->signature);
	file_write_byte(f, obj->type);
	file_write_byte(f, obj->id);

	file_write_short(f, obj->next);
	file_write_short(f, obj->prev);

	file_write_byte(f, obj->control_type);
	file_write_byte(f, obj->movement_type);
	file_write_byte(f, obj->render_type);
	file_write_byte(f, obj->flags);

	file_write_short(f, obj->segnum);
	file_write_short(f, obj->attached_obj);

	write_vector(&obj->pos, f);
	write_matrix(&obj->orient, f);

	file_write_int(f, obj->size);
	file_write_int(f, obj->shields);

	write_vector(&obj->last_pos, f);

	file_write_byte(f, obj->contains_type);
	file_write_byte(f, obj->contains_id);
	file_write_byte(f, obj->contains_count);
	file_write_byte(f, obj->matcen_creator);

	file_write_int(f, obj->lifeleft);

	bytesLeft = 64;

	switch (obj->movement_type)
	{
	case MT_PHYSICS:
		write_vector(&obj->mtype.phys_info.velocity, f);
		write_vector(&obj->mtype.phys_info.thrust, f);
		file_write_int(f, obj->mtype.phys_info.mass);
		file_write_int(f, obj->mtype.phys_info.drag);
		file_write_int(f, obj->mtype.phys_info.brakes);
		write_vector(&obj->mtype.phys_info.rotvel, f);
		write_vector(&obj->mtype.phys_info.rotthrust, f);
		file_write_short(f, obj->mtype.phys_info.turnroll);
		file_write_short(f, obj->mtype.phys_info.flags);
		bytesLeft = 0;
		break;
	case MT_SPINNING:
		write_vector(&obj->mtype.spin_rate, f);
		bytesLeft -= 12;
		break;
	}
	//fseek(f, bytesLeft, SEEK_CUR);
	fwrite(&hack[0], 1, bytesLeft, f);

	bytesLeft = 34;
	switch (obj->control_type)
	{

	case CT_AI:
	{
		//30 very aligned structure
		int i;
		file_write_byte(f, obj->ctype.ai_info.behavior); //1

		for (i = 0; i < MAX_AI_FLAGS; i++)
			file_write_byte(f, obj->ctype.ai_info.flags[i]); //12

		file_write_short(f, obj->ctype.ai_info.hide_segment); //14
		file_write_short(f, obj->ctype.ai_info.hide_index); //16
		file_write_short(f, obj->ctype.ai_info.path_length); //18
		file_write_byte(f, obj->ctype.ai_info.cur_path_index); //19
		file_write_byte(f, obj->ctype.ai_info.dying_sound_playing); //20

		file_write_short(f, obj->ctype.ai_info.danger_laser_num); //22
		file_write_int(f, obj->ctype.ai_info.danger_laser_signature); //26
		file_write_int(f, obj->ctype.ai_info.dying_start_time); //30

		file_write_short(f, obj->ctype.ai_info.follow_path_start_seg); // [DW] D1 needs these
		file_write_short(f, obj->ctype.ai_info.follow_path_end_seg);
		
		bytesLeft = 0;
		break;
	}
	case CT_EXPLOSION:
	case CT_DEBRIS:
		//16
		file_write_int(f, obj->ctype.expl_info.spawn_time);
		file_write_int(f, obj->ctype.expl_info.delete_time);
		file_write_short(f, obj->ctype.expl_info.delete_objnum);
		file_write_short(f, obj->ctype.expl_info.attach_parent);
		file_write_short(f, obj->ctype.expl_info.next_attach);
		file_write_short(f, obj->ctype.expl_info.prev_attach);
		bytesLeft -= 16;
		break;
	case CT_WEAPON:
		//20
		file_write_short(f, obj->ctype.laser_info.parent_type);
		file_write_short(f, obj->ctype.laser_info.parent_num);
		file_write_int(f, obj->ctype.laser_info.parent_signature);
		file_write_int(f, obj->ctype.laser_info.creation_time);
		file_write_short(f, obj->ctype.laser_info.last_hitobj);
		file_write_short(f, obj->ctype.laser_info.track_goal);
		file_write_int(f, obj->ctype.laser_info.multiplier);
		bytesLeft -= 20;
		break;
	case CT_LIGHT:
		//4
		file_write_int(f, obj->ctype.light_info.intensity);
		bytesLeft -= 4;
		break;
	case CT_POWERUP:
		//4
		file_write_int(f, obj->ctype.powerup_info.count);
		file_write_int(f, obj->ctype.powerup_info.creation_time);
		file_write_int(f, obj->ctype.powerup_info.flags);
		bytesLeft -= 12;
		break;
	}
	//fseek(f, bytesLeft, SEEK_CUR);
	fwrite(&hack[0], 1, bytesLeft, f);

	bytesLeft = 76;
	switch (obj->render_type)
	{
	case RT_NONE:
		break;
	case RT_MORPH:
	case RT_POLYOBJ:
	{
		//76
		int i;
		file_write_int(f, obj->rtype.pobj_info.model_num);
		for (i = 0; i < MAX_SUBMODELS; i++)
			write_angvec(&obj->rtype.pobj_info.anim_angles[i], f);
		file_write_int(f, obj->rtype.pobj_info.subobj_flags);
		file_write_int(f, obj->rtype.pobj_info.tmap_override);
		file_write_int(f, obj->rtype.pobj_info.alt_textures);
		bytesLeft = 0;
		break;
	}
	case RT_WEAPON_VCLIP:
	case RT_HOSTAGE:
	case RT_POWERUP:
	case RT_FIREBALL:
		//9
		file_write_int(f, obj->rtype.vclip_info.vclip_num);
		file_write_int(f, obj->rtype.vclip_info.frametime);
		file_write_byte(f, obj->rtype.vclip_info.framenum);
		bytesLeft -= 9;
		break;
	case RT_LASER:
		break;
	}
	//fseek(f, bytesLeft, SEEK_CUR);
	fwrite(&hack[0], 1, bytesLeft, f);
}
