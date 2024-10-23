//#define PSX_BUILD_TOOLS

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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "platform/mono.h"
#include "main_shared/inferno.h"
#include "main_shared/segment.h"
#include "main_shared/textures.h"
#include "main_shared/wall.h"
#include "main_shared/object.h"
#include "main_shared/gamemine.h"
#include "misc/error.h"
#include "main_shared/gameseg.h"
#include "main_shared/game.h"
#include "main_shared/piggy.h"
#include "main_shared/texmerge.h"
#include "main_shared/polyobj.h"
#include "main_shared/vclip.h"
#include "main_shared/effects.h"
#include "main_shared/fireball.h"
#include "main_shared/weapon.h"
#include "2d/palette.h"
#include "platform/timer.h"
#include "stringtable.h"
#include "main_shared/cntrlcen.h"
#include "main_shared/gauges.h"
#include "main_shared/powerup.h"
#include "main_shared/fuelcen.h"
#include "main_shared/mission.h"


#ifdef WINDOWS
void paging_touch_vclip_w( vclip * vc )
{
	int i;

	for (i=0; i<vc->num_frames; i++ )	{
		if ( GameBitmaps[(vc->frames[i]).index].bm_flags & BM_FLAG_PAGED_OUT) 
			piggy_bitmap_page_in_w( vc->frames[i],1 );
	}
}
#endif


void paging_touch_vclip( vclip * vc )
{
	int i;

	for (i=0; i<vc->num_frames; i++ )	{
		PIGGY_PAGE_IN( vc->frames[i] );
	}
}


void paging_touch_wall_effects( int tmap_num )
{
	int i;

	for (i=0;i<activeBMTable->eclips.size();i++)	{
		if ( activeBMTable->eclips[i].changing_wall_texture == tmap_num )	{
			paging_touch_vclip( &activeBMTable->eclips[i].vc );

			if (activeBMTable->eclips[i].dest_bm_num > -1)
				PIGGY_PAGE_IN( activeBMTable->textures
[activeBMTable->eclips[i].dest_bm_num] );	//use this bitmap when monitor destroyed
			if ( activeBMTable->eclips[i].dest_vclip > -1 )
				paging_touch_vclip( &activeBMTable->vclips[activeBMTable->eclips[i].dest_vclip] );		  //what vclip to play when exploding

			if ( activeBMTable->eclips[i].dest_eclip > -1 )
				paging_touch_vclip( &activeBMTable->eclips[activeBMTable->eclips[i].dest_eclip].vc ); //what eclip to play when exploding

			if ( activeBMTable->eclips[i].crit_clip > -1 )
				paging_touch_vclip( &activeBMTable->eclips[activeBMTable->eclips[i].crit_clip].vc ); //what eclip to play when mine critical
		}

	}
}

void paging_touch_object_effects( int tmap_num )
{
	int i;

	for (i=0;i<activeBMTable->eclips.size();i++)	{
		if ( activeBMTable->eclips[i].changing_object_texture == tmap_num )	{
			paging_touch_vclip( &activeBMTable->eclips[i].vc );
		}
	}
}


void paging_touch_model( int modelnum )
{
	int i;
	polymodel *pm = &activeBMTable->models[modelnum];

	for (i=0;i<pm->n_textures;i++)	{
		PIGGY_PAGE_IN( activeBMTable->objectBitmaps[activeBMTable->objectBitmapPointers[pm->first_texture+i]] );
		paging_touch_object_effects( activeBMTable->objectBitmapPointers[pm->first_texture+i] );
		#ifdef PSX_BUILD_TOOLS
		// cmp added
		paging_touch_object_effects( pm->first_texture+i );
		#endif
	}
}

void paging_touch_weapon( int weapon_type )
{
	// Page in the robot's weapons.
	
	if ( (weapon_type < 0) || (weapon_type > activeBMTable->weapons.size()) ) return;

	if ( activeBMTable->weapons[weapon_type].picture.index )	{
		PIGGY_PAGE_IN( activeBMTable->weapons[weapon_type].picture );
	}		
	
	if ( activeBMTable->weapons[weapon_type].flash_vclip > -1 )
		paging_touch_vclip(&activeBMTable->vclips[activeBMTable->weapons[weapon_type].flash_vclip]);
	if ( activeBMTable->weapons[weapon_type].wall_hit_vclip > -1 )
		paging_touch_vclip(&activeBMTable->vclips[activeBMTable->weapons[weapon_type].wall_hit_vclip]);
	if ( activeBMTable->weapons[weapon_type].damage_radius )	{
		// Robot_hit_vclips are actually badass_vclips
		if ( activeBMTable->weapons[weapon_type].robot_hit_vclip > -1 )
			paging_touch_vclip(&activeBMTable->vclips[activeBMTable->weapons[weapon_type].robot_hit_vclip]);
	}

	switch( activeBMTable->weapons[weapon_type].render_type )	{
	case WEAPON_RENDER_VCLIP:
		if ( activeBMTable->weapons[weapon_type].weapon_vclip > -1 )
			paging_touch_vclip( &activeBMTable->vclips[activeBMTable->weapons[weapon_type].weapon_vclip] );
		break;
	case WEAPON_RENDER_NONE:
		break;
	case WEAPON_RENDER_POLYMODEL:
		paging_touch_model( activeBMTable->weapons[weapon_type].model_num );
		break;
	case WEAPON_RENDER_BLOB:
		PIGGY_PAGE_IN( activeBMTable->weapons[weapon_type].bitmap );
		break;
	}
}



int8_t super_boss_gate_type_list[13] = {0, 1, 8, 9, 10, 11, 12, 15, 16, 18, 19, 20, 22 };

void paging_touch_robot( int robot_index )
{
	int i;

	//mprintf((0, "Robot %d loading...", robot_index));

	// Page in robot_index
	paging_touch_model(activeBMTable->robots[robot_index].model_num);
	if ( activeBMTable->robots[robot_index].exp1_vclip_num>-1 )
		paging_touch_vclip(&activeBMTable->vclips[activeBMTable->robots[robot_index].exp1_vclip_num]);
	if ( activeBMTable->robots[robot_index].exp2_vclip_num>-1 )
		paging_touch_vclip(&activeBMTable->vclips[activeBMTable->robots[robot_index].exp2_vclip_num]);

	// Page in his weapons
	paging_touch_weapon( activeBMTable->robots[robot_index].weapon_type );

	// A super-boss can gate in robots...
	if ( activeBMTable->robots[robot_index].boss_flag==2 )	{
		for (i=0; i<13; i++ )
			paging_touch_robot(super_boss_gate_type_list[i]);

		paging_touch_vclip( &activeBMTable->vclips[VCLIP_MORPHING_ROBOT] );
	}
}


void paging_touch_object( object * obj )
{
	int v;

	switch (obj->render_type) {

		case RT_NONE:	break;		//doesn't render, like the player

		case RT_POLYOBJ:
			if ( obj->rtype.pobj_info.tmap_override != -1 )
				PIGGY_PAGE_IN(activeBMTable->textures
					[obj->rtype.pobj_info.tmap_override]);
			else
				paging_touch_model(obj->rtype.pobj_info.model_num);
			break;

		case RT_POWERUP: 
			if ( obj->rtype.vclip_info.vclip_num > -1 ) {
		//@@	#ifdef WINDOWS
		//@@		paging_touch_vclip_w(&Vclip[obj->rtype.vclip_info.vclip_num]);
		//@@	#else
				paging_touch_vclip(&activeBMTable->vclips[obj->rtype.vclip_info.vclip_num]);
		//@@	#endif
			}
			break;

		case RT_MORPH:	break;

		case RT_FIREBALL: break;

		case RT_WEAPON_VCLIP: break;

		case RT_HOSTAGE: 
			paging_touch_vclip(&activeBMTable->vclips[obj->rtype.vclip_info.vclip_num]);
			break;

		case RT_LASER: break;
 	}

	switch (obj->type) {	
	case OBJ_PLAYER:	
		v = get_explosion_vclip(obj, 0);
		if ( v > -1 )
			paging_touch_vclip(&activeBMTable->vclips[v]);
		break;
	case OBJ_ROBOT:
		paging_touch_robot( obj->id );
		break;
	case OBJ_CNTRLCEN:
		paging_touch_weapon( CONTROLCEN_WEAPON_NUM );
		if (activeBMTable->deadModels[obj->rtype.pobj_info.model_num] != -1)	{
			paging_touch_model(activeBMTable->deadModels[obj->rtype.pobj_info.model_num]);
		}
		break;
	}
}

	

void paging_touch_side( segment * segp, int sidenum )
{
	int tmap1, tmap2;

	if (!(WALL_IS_DOORWAY(segp,sidenum) & WID_RENDER_FLAG))
		return;

	tmap1 = segp->sides[sidenum].tmap_num;
	if (tmap1 >= activeBMTable->textures.size())
		return;

	paging_touch_wall_effects(tmap1);
	tmap2 = segp->sides[sidenum].tmap_num2;
	if (tmap2 != 0)	{
		texmerge_get_cached_bitmap( tmap1, tmap2 );
		paging_touch_wall_effects( tmap2 & 0x3FFF );
	} else	{
		PIGGY_PAGE_IN( activeBMTable->textures[tmap1] );
	}

	// PSX STUFF
	#ifdef PSX_BUILD_TOOLS
		// If there is water on the level, then force the water splash into memory
		if(!(activeBMTable->tmaps[tmap1].flags & TMI_VOLATILE) && (activeBMTable->tmaps[tmap1].flags & TMI_WATER)) {
			bitmap_index Splash;
			Splash.index = 1098;
			PIGGY_PAGE_IN(Splash);
			Splash.index = 1099;
			PIGGY_PAGE_IN(Splash);
			Splash.index = 1100;
			PIGGY_PAGE_IN(Splash);
			Splash.index = 1101;
			PIGGY_PAGE_IN(Splash);
			Splash.index = 1102;
			PIGGY_PAGE_IN(Splash);
		}
	#endif

}

void paging_touch_robot_maker( segment * segp )
{
	segment2	*seg2p = &Segment2s[segp-Segments];

	if ( seg2p->special == SEGMENT_IS_ROBOTMAKER )	{
		paging_touch_vclip(&activeBMTable->vclips[VCLIP_MORPHING_ROBOT]);
		if (RobotCenters[seg2p->matcen_num].robot_flags != 0) {
			int	i;
			uint32_t flags;
			int	robot_index;

			for (i=0;i<2;i++) {
				robot_index = i*32;
				flags = RobotCenters[seg2p->matcen_num].robot_flags[i];
				while (flags) {
					if (flags & 1)	{
						// Page in robot_index
						paging_touch_robot( robot_index );
					}
					flags >>= 1;
					robot_index++;
				}
			}
		}
	}
}


void paging_touch_segment(segment * segp)
{
	int sn;
	int objnum;
	segment2	*seg2p = &Segment2s[segp-Segments];

	if ( seg2p->special == SEGMENT_IS_ROBOTMAKER )	
		paging_touch_robot_maker(segp);

//	paging_draw_orb();
	for (sn=0;sn<MAX_SIDES_PER_SEGMENT;sn++)
	{
//		paging_draw_orb();
		paging_touch_side( segp, sn );
	}

	for (objnum=segp->objects;objnum!=-1;objnum=Objects[objnum].next)	
	{
//		paging_draw_orb();
		paging_touch_object( &Objects[objnum] );
	}

}



void paging_touch_walls()
{
	int i,j;
	wclip *anim;

	for (i=0;i<Num_walls;i++)
	{
//		paging_draw_orb();
		if ( Walls[i].clip_num > -1 )	
		{
			anim = &activeBMTable->wclips[Walls[i].clip_num];
			for (j=0; j < anim->num_frames; j++ )	
			{
				PIGGY_PAGE_IN( activeBMTable->textures
[anim->frames[j]] );
			}
		}
	}
}

void paging_touch_all()
{
	int black_screen;
	int s;
	
	stop_time();

	black_screen = gr_palette_faded_out;

	if ( gr_palette_faded_out )	{
		gr_clear_canvas( BM_XRGB(0,0,0) );
		gr_palette_load( gr_palette );
	}
	
//@@	show_boxed_message(TXT_LOADING);

	mprintf(( 0, "Loading all textures in mine..." ));
	for (s=0; s<=Highest_segment_index; s++)	{
		paging_touch_segment( &Segments[s] );
	}	
	
	paging_touch_walls();

	for ( s=0; s < activeBMTable->powerups.size(); s++ )	{
		if ( activeBMTable->powerups[s].vclip_num > -1 )	
			paging_touch_vclip(&activeBMTable->vclips[activeBMTable->powerups[s].vclip_num]);
	}

	for ( s=0; s<activeBMTable->weapons.size(); s++ )	{
		paging_touch_weapon(s);
	}

	for ( s=0; s < activeBMTable->powerups.size(); s++ )	{
		if ( activeBMTable->powerups[s].vclip_num > -1 )	
			paging_touch_vclip(&activeBMTable->vclips[activeBMTable->powerups[s].vclip_num]);
	}


	for (s=0; s < activeBMTable->gauges.size(); s++ )	{
		if ( activeBMTable->gauges[s].index )	{
			PIGGY_PAGE_IN( activeBMTable->gauges[s] );
		}
	}
	paging_touch_vclip( &activeBMTable->vclips[VCLIP_PLAYER_APPEARANCE] );
	paging_touch_vclip( &activeBMTable->vclips[VCLIP_POWERUP_DISAPPEARANCE] );


#ifdef PSX_BUILD_TOOLS

	//PSX STUFF 
	paging_touch_walls();
	for(s=0; s<=Highest_object_index; s++) {
		paging_touch_object(&Objects[s]);
	}


	{
		char * p;
		extern int Current_level_num;
		//extern ushort GameBitmapXlat[MAX_BITMAP_FILES];
		short Used[MAX_BITMAP_FILES];
		FILE * fp;
		char fname[128];
		int i;

		if (Current_level_num<0)                //secret level
			strcpy( fname, Secret_level_names[-Current_level_num-1] );
		else                                    //normal level
			strcpy( fname, Level_names[Current_level_num-1] );
		p = strchr( fname, '.' );
		if ( p ) *p = 0;
		strcat( fname, ".pag" );

		fp = fopen( fname, "wt" );
		for (i=0; i<MAX_BITMAP_FILES;i++ )      {
			Used[i] = 0;
		}
		for (i=0; i<MAX_BITMAP_FILES;i++ )      {
			Used[activePiggyTable->gameBitmapXlat[i]]++;
		}

		//cmp added so that .damage bitmaps are included for paged-in lights of the current level
		for (i=0; i<MAX_TEXTURES;i++) {
			if(activeBMTable->textures
[i].index > 0 && activeBMTable->textures
[i].index < MAX_BITMAP_FILES && 
				Used[activeBMTable->textures
[i].index] > 0 &&
				activeBMTable->tmaps[i].destroyed > 0 && activeBMTable->tmaps[i].destroyed < MAX_BITMAP_FILES) {
				Used[activeBMTable->textures
[activeBMTable->tmaps[i].destroyed].index] += 1;
				mprintf((0, "HERE %d ", activeBMTable->textures
[activeBMTable->tmaps[i].destroyed].index));

				PIGGY_PAGE_IN(activeBMTable->textures
[activeBMTable->tmaps[i].destroyed]);

			}
		}

		//	Force cockpit to be paged in.
		{
			bitmap_index bonk;
			bonk.index = 109;
			PIGGY_PAGE_IN(bonk);
		}

		// Force in the frames for markers
		{
			bitmap_index bonk2;
			bonk2.index = 2014;
			PIGGY_PAGE_IN(bonk2);
			bonk2.index = 2015;
			PIGGY_PAGE_IN(bonk2);
			bonk2.index = 2016;
			PIGGY_PAGE_IN(bonk2);
			bonk2.index = 2017;
			PIGGY_PAGE_IN(bonk2);
			bonk2.index = 2018;
			PIGGY_PAGE_IN(bonk2);
		}

		for (i=0; i<MAX_BITMAP_FILES;i++ )      {
			int paged_in = 1;
			// cmp debug
			//piggy_get_bitmap_name(i,fname);

			if (GameBitmaps[i].bm_flags & BM_FLAG_PAGED_OUT ) 
				paged_in = 0;

//                      if (GameBitmapXlat[i]!=i)
//                              paged_in = 0;

			if ( !Used[i] )
				paged_in = 0;
			if ( (i==47) || (i==48) )               // Mark red mplayer ship textures as paged in.
				paged_in = 1;
	
			if ( !paged_in )
				fprintf( fp, "0,\t// Bitmap %d (%s)\n", i, "test\0"); // cmp debug fname );
			else
				fprintf( fp, "1,\t// Bitmap %d (%s)\n", i, "test\0"); // cmp debug fname );
		}

		fclose(fp);
	}
#endif

	mprintf(( 0, "done\n" ));

//@@	clear_boxed_message();

	if ( black_screen )	{
		gr_palette_clear();
		gr_clear_canvas( BM_XRGB(0,0,0) );
	}
	start_time();
	reset_cockpit();		//force cockpit redraw next time

}

