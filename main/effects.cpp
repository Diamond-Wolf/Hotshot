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
#include <stdarg.h>
#include <string.h>

#include "2d/gr.h"
#include "effects.h"
#include "platform/mono.h"
#include "mem/mem.h"
#include "misc/error.h"
#include "vclip.h"
#include "cntrlcen.h"
#include "fuelcen.h"
#include "bm.h"
#include "game.h"
#include "inferno.h"
#include "textures.h"

void init_special_effects() 
{
	int i;

	for (i=0;i< activeBMTable->eclips.size();i++)
		activeBMTable->eclips[i].time_left = activeBMTable->eclips[i].vc.frame_time;
}

void reset_special_effects()
{
	int i;

	for (i=0;i< activeBMTable->eclips.size();i++)
	{
		activeBMTable->eclips[i].segnum = -1;					//clear any active one-shots
		activeBMTable->eclips[i].flags &= ~(EF_STOPPED|EF_ONE_SHOT);		//restart any stopped effects

		//reset bitmap, which could have been changed by a crit_clip
		if (activeBMTable->eclips[i].changing_wall_texture != -1)
			activeBMTable->textures[activeBMTable->eclips[i].changing_wall_texture] = activeBMTable->eclips[i].vc.frames[activeBMTable->eclips[i].frame_count];

		if (activeBMTable->eclips[i].changing_object_texture != -1)
			activeBMTable->objectBitmaps[activeBMTable->eclips[i].changing_object_texture] = activeBMTable->eclips[i].vc.frames[activeBMTable->eclips[i].frame_count];

	}
}

void do_special_effects()
{
	int i;
	eclip *ec;

	//for (i=0,ec=Effects;i<Num_effects;i++,ec++) 
	for (auto& ec : activeBMTable->eclips)
	{

		if ((ec.changing_wall_texture == -1) && (ec.changing_object_texture==-1) )
			continue;

		if (ec.flags & EF_STOPPED)
			continue;

		ec.time_left -= FrameTime;

		while (ec.time_left < 0) 
		{

			ec.time_left += ec.vc.frame_time;
			
			ec.frame_count++;
			if (ec.frame_count >= ec.vc.num_frames) 
			{
				if (ec.flags & EF_ONE_SHOT)
				{
					Assert(ec.segnum!=-1);
					Assert(ec.sidenum>=0 && ec.sidenum<6);
					Assert(ec.dest_bm_num!=0 && Segments[ec.segnum].sides[ec.sidenum].tmap_num2!=0);
					Segments[ec.segnum].sides[ec.sidenum].tmap_num2 = ec.dest_bm_num | (Segments[ec.segnum].sides[ec.sidenum].tmap_num2&0xc000);		//replace with destoyed
					ec.flags &= ~EF_ONE_SHOT;
					ec.segnum = -1;		//done with this
				}

				ec.frame_count = 0;
			}
		}

		if (ec.flags & EF_CRITICAL)
			continue;

		if (ec.crit_clip!=-1 && Control_center_destroyed) 
		{
			int n = ec.crit_clip;

			//*ec->bm_ptr = &GameBitmaps[Effects[n].vc.frames[Effects[n].frame_count].index];
			if (ec.changing_wall_texture != -1)
				activeBMTable->textures[ec.changing_wall_texture] = activeBMTable->eclips[n].vc.frames[activeBMTable->eclips[n].frame_count];

			if (ec.changing_object_texture != -1)
				activeBMTable->objectBitmaps[ec.changing_object_texture] = activeBMTable->eclips[n].vc.frames[activeBMTable->eclips[n].frame_count];

		}
		else	
		{
			// *ec->bm_ptr = &GameBitmaps[ec->vc.frames[ec->frame_count].index];
			if (ec.changing_wall_texture != -1)
				activeBMTable->textures[ec.changing_wall_texture] = ec.vc.frames[ec.frame_count];
	
			if (ec.changing_object_texture != -1)
				activeBMTable->objectBitmaps[ec.changing_object_texture] = ec.vc.frames[ec.frame_count];
		}

	}
}

void restore_effect_bitmap_icons()
{
	int i;

	for (i=0;i< activeBMTable->eclips.size();i++)
		if (! (activeBMTable->eclips[i].flags & EF_CRITICAL))
		{
			if (activeBMTable->eclips[i].changing_wall_texture != -1)
				activeBMTable->textures[activeBMTable->eclips[i].changing_wall_texture] = activeBMTable->eclips[i].vc.frames[0];
	
			if (activeBMTable->eclips[i].changing_object_texture != -1)
				activeBMTable->objectBitmaps[activeBMTable->eclips[i].changing_object_texture] = activeBMTable->eclips[i].vc.frames[0];
		}
			//if (Effects[i].bm_ptr != -1)
			//	*Effects[i].bm_ptr = &GameBitmaps[Effects[i].vc.frames[0].index];
}

//stop an effect from animating.  Show first frame.
void stop_effect(int effect_num)
{
	eclip *ec = &activeBMTable->eclips[effect_num];
	
	//Assert(ec->bm_ptr != -1);

	ec->flags |= EF_STOPPED;

	ec->frame_count = 0;
	//*ec->bm_ptr = &GameBitmaps[ec->vc.frames[0].index];

	if (ec->changing_wall_texture != -1)
		activeBMTable->textures[ec->changing_wall_texture] = ec->vc.frames[0];
	
	if (ec->changing_object_texture != -1)
		activeBMTable->objectBitmaps[ec->changing_object_texture] = ec->vc.frames[0];

}

//restart a stopped effect
void restart_effect(int effect_num)
{
	activeBMTable->eclips[effect_num].flags &= ~EF_STOPPED;

	//Assert(Effects[effect_num].bm_ptr != -1);
}

