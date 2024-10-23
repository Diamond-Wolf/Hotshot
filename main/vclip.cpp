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

#define	ONE_FRAME_TIME	0x3ffffffe	//Will eventually come from object.h

#include <stdlib.h>

#include "misc/rand.h"
#include "misc/error.h"

#include "vclip.h"
#include "inferno.h"
#include "laser.h"
#include "weapon.h"
#include "bm.h"

//draw an object which renders as a vclip
void draw_vclip_object(object* obj, fix timeleft, int lighted, int vclip_num)
{
	int nf, bitmapnum;

	nf = activeBMTable->vclips[vclip_num].num_frames;

	bitmapnum = (nf - f2i(fixdiv((nf - 1) * timeleft, activeBMTable->vclips[vclip_num].play_time))) - 1;

	if (bitmapnum >= activeBMTable->vclips[vclip_num].num_frames)
		bitmapnum = activeBMTable->vclips[vclip_num].num_frames - 1;

	if (bitmapnum >= 0)
	{
		if (activeBMTable->vclips[vclip_num].flags & VF_ROD)
			draw_object_tmap_rod(obj, activeBMTable->vclips[vclip_num].frames[bitmapnum], lighted);
		else
		{
			Assert(lighted == 0);		//blob cannot now be lighted

			draw_object_blob(obj, activeBMTable->vclips[vclip_num].frames[bitmapnum]);
		}
	}

}

void draw_weapon_vclip(object* obj)
{
	int	vclip_num;
	fix	modtime, play_time;

	//mprintf( 0, "[Drawing obj %d type %d fireball size %x]\n", obj-Objects.data(), Weapon_info[obj->id].weapon_vclip, obj->size );

	Assert(obj->type == OBJ_WEAPON);

	vclip_num = activeBMTable->weapons[obj->id].weapon_vclip;

	modtime = obj->lifeleft;
	play_time = activeBMTable->vclips[vclip_num].play_time;

	//	Special values for modtime were causing enormous slowdown for omega blobs.
	if (modtime == IMMORTAL_TIME)
		modtime = play_time;

	//	Should cause Omega blobs (which live for one frame) to not always be the same.
	if (modtime == ONE_FRAME_TIME)
		modtime = P_Rand();

	if (obj->id == PROXIMITY_ID) //make prox bombs spin out of sync
	{
		int objnum = obj - Objects.data();

		modtime += (modtime * (objnum & 7)) / 16;	//add variance to spin rate

		while (modtime > play_time)
			modtime -= play_time;

		if ((objnum & 1) ^ ((objnum >> 1) & 1))			//make some spin other way
			modtime = play_time - modtime;

	}
	else
	{
		while (modtime > play_time)
			modtime -= play_time;
	}

	draw_vclip_object(obj, modtime, 0, vclip_num);
}
