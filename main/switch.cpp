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

#include "gameseq.h"
#include "newmenu.h"
#include "game.h"
#include "switch.h"
#include "inferno.h"
#include "segment.h"
#include "misc/error.h"
#include "gameseg.h"
#include "platform/mono.h"
#include "wall.h"
#include "texmap/texmap.h"
#include "fuelcen.h"
#include "cntrlcen.h"
#include "newdemo.h"
#include "player.h"
#include "endlevel.h"
#include "gauges.h"
#include "multi.h"
#include "network.h"
#include "2d/palette.h"
#include "robot.h"
#include "bm.h"

#ifdef EDITOR
#include "editor\editor.h"
#endif

std::vector<trigger> Triggers(MAX_TRIGGERS);

#ifdef EDITOR
fix trigger_time_count=F1_0;

//-----------------------------------------------------------------
// Initializes all the switches.
void trigger_init()
{
	int i;

	Num_triggers = 0;

	for (i=0;i<MAX_TRIGGERS;i++)
		{
		Triggers[i].type = 0;
		Triggers[i].flags = 0;
		Triggers[i].num_links = 0;
		Triggers[i].value = 0;
		Triggers[i].time = -1;
		}
}
#endif

//-----------------------------------------------------------------
// Executes a link, attached to a trigger.
// Toggles all walls linked to the switch.
// Opens doors, Blasts blast walls, turns off illusions.
void do_link(int8_t trigger_num)
{
	int i;

	mprintf((0, "Door link!\n"));

	if (trigger_num != -1) 
	{
		for (i=0;i<Triggers[trigger_num].num_links;i++) 
		{
			wall_toggle(&Segments[Triggers[trigger_num].seg[i]], Triggers[trigger_num].side[i]); 
			mprintf((0," trigger_num %d : seg %d, side %d\n", 
				trigger_num, Triggers[trigger_num].seg[i], Triggers[trigger_num].side[i]));
  		}
  	}
}

//close a door
void do_close_door(int8_t trigger_num)
{
	int i;

	mprintf((0, "Door close!\n"));

	if (trigger_num != -1) 
	{
		for (i=0;i<Triggers[trigger_num].num_links;i++)
			wall_close_door(&Segments[Triggers[trigger_num].seg[i]], Triggers[trigger_num].side[i]); 
  	}
}

//turns lighting on.  returns true if lights were actually turned on. (they
//would not be if they had previously been shot out).
int do_light_on(int8_t trigger_num)
{
	int i,ret=0;

	mprintf((0, "Lighting on!\n"));

	if (trigger_num != -1) 
	{
		for (i=0;i<Triggers[trigger_num].num_links;i++) 
		{
			int segnum,sidenum;
			segnum = Triggers[trigger_num].seg[i];
			sidenum = Triggers[trigger_num].side[i];

			//check if tmap2 casts light before turning the light on.  This
			//is to keep us from turning on blown-out lights
			if (activeBMTable->tmaps[Segments[segnum].sides[sidenum].tmap_num2 & 0x3fff].lighting)
			{
				ret |= add_light(segnum, sidenum); 		//any light sets flag
				enable_flicker(segnum, sidenum);
			}
		}
	}

	return ret;
}

//turns lighting off.  returns true if lights were actually turned off. (they
//would not be if they had previously been shot out).
int do_light_off(int8_t trigger_num)
{
	int i,ret=0;

	mprintf((0, "Lighting off!\n"));

	if (trigger_num != -1)
	{
		for (i=0;i<Triggers[trigger_num].num_links;i++) 
		{
			int segnum,sidenum;
			segnum = Triggers[trigger_num].seg[i];
			sidenum = Triggers[trigger_num].side[i];

			//check if tmap2 casts light before turning the light off.  This
			//is to keep us from turning off blown-out lights
			if (activeBMTable->tmaps[Segments[segnum].sides[sidenum].tmap_num2 & 0x3fff].lighting) 
			{
				ret |= subtract_light(segnum, sidenum); 	//any light sets flag
				disable_flicker(segnum, sidenum);
			}
  		}
  	}

	return ret;
}

// Unlocks all doors linked to the switch.
void do_unlock_doors(int8_t trigger_num)
{
	int i;

	mprintf((0, "Door unlock!\n"));

	if (trigger_num != -1) 
	{
		for (i=0;i<Triggers[trigger_num].num_links;i++) 
		{
			Walls[Segments[Triggers[trigger_num].seg[i]].sides[Triggers[trigger_num].side[i]].wall_num].flags &= ~WALL_DOOR_LOCKED;
			Walls[Segments[Triggers[trigger_num].seg[i]].sides[Triggers[trigger_num].side[i]].wall_num].keys = KEY_NONE;
  		}
  	}
}

// Return trigger number if door is controlled by a wall switch, else return -1.
int door_is_wall_switched(int wall_num)
{
	int i, t;

	for (t=0; t<Triggers.size(); t++) 
	{
		for (i=0; i<Triggers[t].num_links; i++) 
		{
			if (Segments[Triggers[t].seg[i]].sides[Triggers[t].side[i]].wall_num == wall_num) {
				mprintf((0, "Wall #%i is keyed to trigger #%i, link #%i\n", wall_num, t, i));
				return t;
			}
	  	}
	}

	return -1;
}

void flag_wall_switched_doors(void)
{
	int	i;

	for (i=0; i<Walls.size(); i++)
	{
		if (door_is_wall_switched(i))
			Walls[i].flags |= WALL_WALL_SWITCH;
	}

}

// Locks all doors linked to the switch.
void do_lock_doors(int8_t trigger_num)
{
	int i;

	mprintf((0, "Door lock!\n"));

	if (trigger_num != -1)
	{
		for (i=0;i<Triggers[trigger_num].num_links;i++)
		{
			Walls[Segments[Triggers[trigger_num].seg[i]].sides[Triggers[trigger_num].side[i]].wall_num].flags |= WALL_DOOR_LOCKED;
  		}
  	}
}

// Changes walls pointed to by a trigger. returns true if any walls changed
int do_change_walls(int8_t trigger_num)
{
	int i,ret=0;

	mprintf((0, "Wall remove!\n"));

	if (trigger_num != -1) 
	{
		for (i=0;i<Triggers[trigger_num].num_links;i++) 
		{
			segment *segp,*csegp;
			short side,cside;
			int new_wall_type;

			segp = &Segments[Triggers[trigger_num].seg[i]];
			side = Triggers[trigger_num].side[i];

			csegp = &Segments[segp->children[side]];
			cside = find_connect_side(segp, csegp);
			Assert(cside != -1);

			//segp->sides[side].wall_num = -1;
			//csegp->sides[cside].wall_num = -1;

			auto type = Triggers[trigger_num].type;

			if (type & HTT_OPEN_WALL)
				new_wall_type = WALL_OPEN;
			else if (new_wall_type & HTT_ILLUSORY_WALL)
				new_wall_type = WALL_ILLUSION;
			else if (new_wall_type & HTT_CLOSE_WALL)
				new_wall_type = WALL_CLOSED;

			if (Walls[segp->sides[side].wall_num].type == new_wall_type && Walls[csegp->sides[cside].wall_num].type == new_wall_type)
				continue;		//already in correct state, so skip

			ret = 1;

			////////////////////////////////////
			
			if (type & HTT_OPEN_WALL) {
				mprintf((0, "Open wall\n"));

				if ((activeBMTable->tmaps[segp->sides[side].tmap_num].flags & TMI_FORCE_FIELD))
				{
					vms_vector pos;
					compute_center_point_on_side(&pos, segp, side);
					digi_link_sound_to_pos(SOUND_FORCEFIELD_OFF, segp - Segments.data(), side, pos, 0, F1_0);
					if (segp->sides[side].wall_num >= 0) {
						Walls[segp->sides[side].wall_num].type = new_wall_type;
						digi_kill_sound_linked_to_segment(segp - Segments.data(), side, SOUND_FORCEFIELD_HUM);
					}
					if (csegp->sides[cside].wall_num >= 0) {
						Walls[csegp->sides[cside].wall_num].type = new_wall_type;
						digi_kill_sound_linked_to_segment(csegp - Segments.data(), cside, SOUND_FORCEFIELD_HUM);
					}
				}
				else
					start_wall_cloak(segp, side);

				ret = 1;

			} else if (type & HTT_ILLUSORY_WALL) {
				mprintf((0,"Illusory wall\n"));
				
				if (segp->sides[side].wall_num >= 0)
					Walls[segp->sides[side].wall_num].type = new_wall_type;
				if (csegp->sides[side].wall_num >= 0)
					Walls[csegp->sides[cside].wall_num].type = new_wall_type;

			} else if (type & HTT_CLOSE_WALL) {
				mprintf((0, "Close wall\n"));

				if ((activeBMTable->tmaps[segp->sides[side].tmap_num].flags & TMI_FORCE_FIELD))
				{
					vms_vector pos;
					compute_center_point_on_side(&pos, segp, side);
					digi_link_sound_to_pos(SOUND_FORCEFIELD_HUM, segp - Segments.data(), side, pos, 1, F1_0 / 2);
					if (segp->sides[side].wall_num >= 0)
						Walls[segp->sides[side].wall_num].type = new_wall_type;
					if (csegp->sides[side].wall_num >= 0)
						Walls[csegp->sides[cside].wall_num].type = new_wall_type;
				}
				else
					start_wall_decloak(segp, side);

			}

			kill_stuck_objects(segp->sides[side].wall_num);
			kill_stuck_objects(csegp->sides[cside].wall_num);

  		}
  	}

	return ret;
}

void print_trigger_message (int pnum,int trig,int shot,const char *message)
 {
	char *pl;		//points to 's' or nothing for plural word

   if (pnum!=Player_num)
		return;

	pl = const_cast<char*>((Triggers[trig].num_links>1)?"s":"");
  
    if (!(Triggers[trig].flags & TF_NO_MESSAGE) && shot)
     HUD_init_message (message,pl);
 }
 

void do_matcen(int8_t trigger_num)
{
	int i;

	mprintf((0, "Matcen link!\n"));

	if (trigger_num != -1)
	{
		for (i=0;i<Triggers[trigger_num].num_links;i++) 
		{
			trigger_matcen(Triggers[trigger_num].seg[i] ); 
			mprintf((0," trigger_num %d : seg %d\n", 
				trigger_num, Triggers[trigger_num].seg[i]));
  		}
  	}
}

	
void do_il_on(int8_t trigger_num)
{
	int i;

	mprintf((0, "Illusion ON\n"));

	if (trigger_num != -1) 
	{
		for (i=0;i<Triggers[trigger_num].num_links;i++)
		{
			wall_illusion_on(&Segments[Triggers[trigger_num].seg[i]], Triggers[trigger_num].side[i]); 
			mprintf((0," trigger_num %d : seg %d, side %d\n", 
				trigger_num, Triggers[trigger_num].seg[i], Triggers[trigger_num].side[i]));
  		}
  	}
}

void do_il_off(int8_t trigger_num)
{
	int i;
	
	mprintf((0, "Illusion OFF\n"));

	if (trigger_num != -1)
	{
		for (i=0;i<Triggers[trigger_num].num_links;i++)
		{
			vms_vector	cp;
			segment		*seg = &Segments[Triggers[trigger_num].seg[i]];
			int			side = Triggers[trigger_num].side[i];

			wall_illusion_off(seg, side);

			mprintf((0," trigger_num %d : seg %d, side %d\n", 
				trigger_num, Triggers[trigger_num].seg[i], Triggers[trigger_num].side[i]));

			compute_center_point_on_side(&cp, seg, side );
			digi_link_sound_to_pos( SOUND_WALL_REMOVED, seg-Segments.data(), side, cp, 0, F1_0 );

  		}
  	}
}

extern void EnterSecretLevel(void);
extern void ExitSecretLevel(void);
extern int p_secret_level_destroyed(void);

int wall_is_forcefield(trigger *trig)
{
	int i;

	for (i=0;i<trig->num_links;i++)
		if ((activeBMTable->tmaps[Segments[trig->seg[i]].sides[trig->side[i]].tmap_num].flags & TMI_FORCE_FIELD))
			break;

	return (i<trig->num_links);
}

bool ExecSecretLevel(int pnum) {
	int	truth;

	if (CurrentDataVersion == DataVer::DEMO)
	{
		HUD_init_message("Secret Level Teleporter disabled in Descent 2 Demo");
		digi_play_sample(SOUND_BAD_SELECTION, F1_0);
		return false;
	}

	if (pnum != Player_num)
		return false;

	if ((Players[Player_num].shields < 0) || Player_is_dead)
		return false;

	if (Game_mode & GM_MULTI)
	{
		HUD_init_message("Secret Level Teleporter disabled in multiplayer!");
		digi_play_sample(SOUND_BAD_SELECTION, F1_0);
		return false;
	}

	truth = p_secret_level_destroyed();

	if (Newdemo_state == ND_STATE_RECORDING)			// record whether we're really going to the secret level
		newdemo_record_secret_exit_blown(truth);

	if ((Newdemo_state != ND_STATE_PLAYBACK) && truth)
	{
		HUD_init_message("Secret Level destroyed.  Exit disabled.");
		digi_play_sample(SOUND_BAD_SELECTION, F1_0);
		return false;
	}

	if (Newdemo_state == ND_STATE_RECORDING)		// stop demo recording
		Newdemo_state = ND_STATE_PAUSED;

	digi_stop_all();		//kill the sounds

	if (currentGame != G_DESCENT_1)
		digi_play_sample(SOUND_SECRET_EXIT, F1_0);
	mprintf((0, "Exiting to secret level\n"));

	gr_palette_fade_out(gr_palette, 32, 0);
	EnterSecretLevel();
	Control_center_destroyed = 0;
	
	return true;
}

int check_trigger_sub(int trigger_num, int pnum,int shot)
{
	trigger *trig = &Triggers[trigger_num];

	mprintf ((0,"trignum=%d type=%d shot=%d\n",trigger_num,trig->type,shot));

	if (trig->flags & TF_DISABLED)
		return 1;		//1 means don't send trigger hit to other players

	if (trig->flags & TF_ONE_SHOT)		//if this is a one-shot...
		trig->flags |= TF_DISABLED;		//..then don't let it happen again

	auto type = trig->type;

	int had_no_net_trigger = 0;
	int executed = 0;
	
	bool shouldExit = false;
	bool shouldSecretExit = false;

	if (type & HTT_EXIT && pnum == Player_num) {
		executed++;
		digi_stop_all();		//kill the sounds

		shouldExit = true;

		had_no_net_trigger = 1;
	}

	if (type & HTT_SECRET_EXIT)
	{
		executed++;
		shouldSecretExit = true;
	}

	if (type & HTT_OPEN_DOOR) {
		executed++;
		mprintf((0, "D"));
		do_link(trigger_num);
		print_trigger_message(pnum, trigger_num, shot, "Door%s opened!");
	}

	if (type & HTT_CLOSE_DOOR) {
		executed++;
		do_close_door(trigger_num);
		print_trigger_message(pnum, trigger_num, shot, "Door%s closed!");
	}

	if (type & HTT_UNLOCK_DOOR) {
		executed++;
		mprintf((0, "U"));
		do_unlock_doors(trigger_num);
		print_trigger_message(pnum, trigger_num, shot, "Door%s unlocked!");
	}
	
	if (type & HTT_LOCK_DOOR) {
		executed++;
		mprintf((0, "L"));
		do_lock_doors(trigger_num);
		print_trigger_message(pnum, trigger_num, shot, "Door%s locked!");
	}
	
	if (type & HTT_OPEN_WALL) {
		executed++;
		if (do_change_walls(trigger_num))
			if (wall_is_forcefield(trig))
				print_trigger_message(pnum, trigger_num, shot, "Force field%s deactivated!");
			else
				print_trigger_message(pnum, trigger_num, shot, "Wall%s opened!");
	}

	if (type & HTT_CLOSE_WALL) {
		executed++;
		if (do_change_walls(trigger_num))
			if (wall_is_forcefield(trig))
				print_trigger_message(pnum, trigger_num, shot, "Force field%s activated!");
			else
				print_trigger_message(pnum, trigger_num, shot, "Wall%s closed!");
	}

	if (type & HTT_ILLUSORY_WALL) {
		executed++;
		//don't know what to say, so say nothing
		do_change_walls(trigger_num);
	}

	if (type & HTT_MATCEN) {
		executed++;
		if (!(Game_mode & GM_MULTI) || (Game_mode & GM_MULTI_ROBOTS))
			do_matcen(trigger_num);
	}
	
	if (type & HTT_ILLUSION_ON) {
		executed++;
		mprintf((0, "I"));
		do_il_on(trigger_num);
		print_trigger_message(pnum, trigger_num, shot, "Illusion%s on!");
	}
	
	if (type & HTT_ILLUSION_OFF) {
		executed++;
		mprintf((0, "i"));
		do_il_off(trigger_num);
		print_trigger_message(pnum, trigger_num, shot, "Illusion%s off!");
	}

	if (type & HTT_LIGHT_OFF) {
		executed++;
		if (do_light_off(trigger_num))
			print_trigger_message(pnum, trigger_num, shot, "Lights off!");
	}

	if (type & HTT_LIGHT_ON) {
		executed++;
		if (do_light_on(trigger_num))
			print_trigger_message(pnum, trigger_num, shot, "Lights on!");
	}

	mprintf((0, "Executed %d triggers\n", executed));

	Assert(executed > 0);

	if (shouldSecretExit) {
		if (ExecSecretLevel(pnum))
			had_no_net_trigger = 1;
	} else if (shouldExit) {
		if (Current_level_num > 0)
		{
			start_endlevel_sequence();
			mprintf((0, "WOOHOO! (leaving the mine!)\n"));
		} else if (Current_level_num < 0) {
			if (!((Players[Player_num].shields < 0) || Player_is_dead)) {
				if (currentGame == G_DESCENT_1)
					start_endlevel_sequence();
				else
					ExitSecretLevel();
			}
		} else {
#ifdef EDITOR
			nm_messagebox("Yo!", 1, "You have hit the exit trigger!", "");
#else
			Int3();		//level num == 0, but no editor!
#endif
		}
	}
	
	return had_no_net_trigger;
}

//-----------------------------------------------------------------
// Checks for a trigger whenever an object hits a trigger side.
void check_trigger(segment *seg, short side, short objnum,int shot)
{
	int wall_num, trigger_num;	//, ctrigger_num;
	//segment *csegp;
 	//short cside;

//	mprintf(0,"T");

	if ((objnum == Players[Player_num].objnum) || ((Objects[objnum].type == OBJ_ROBOT) && (activeBMTable->robots[Objects[objnum].id].companion)))
	{

		if ( Newdemo_state == ND_STATE_RECORDING )
			newdemo_record_trigger( seg-Segments.data(), side, objnum,shot);

		wall_num = seg->sides[side].wall_num;
		if ( wall_num == -1 ) return;
		
		trigger_num = Walls[wall_num].trigger;

		if (trigger_num == -1)
			return;

		//##if ( Newdemo_state == ND_STATE_PLAYBACK ) {
		//##	if (Triggers[trigger_num].type == TT_EXIT) {
		//##		start_endlevel_sequence();
		//##	}
		//##	//return;
		//##}

		if (check_trigger_sub(trigger_num, Player_num,shot))
			return;

		//@@if (Triggers[trigger_num].flags & TRIGGER_ONE_SHOT) {
		//@@	Triggers[trigger_num].flags &= ~TRIGGER_ON;
		//@@
		//@@	csegp = &Segments[seg->children[side]];
		//@@	cside = find_connect_side(seg, csegp);
		//@@	Assert(cside != -1);
		//@@
		//@@	wall_num = csegp->sides[cside].wall_num;
		//@@	if ( wall_num == -1 ) return;
		//@@	
		//@@	ctrigger_num = Walls[wall_num].trigger;
		//@@
		//@@	Triggers[ctrigger_num].flags &= ~TRIGGER_ON;
		//@@}

#ifdef NETWORK
		if (Game_mode & GM_MULTI)
			multi_send_trigger(trigger_num);
#endif
	}
}
  
void triggers_frame_process()
{
	int i;

	for (i=0;i<Triggers.size();i++)
		if (Triggers[i].time >= 0)
			Triggers[i].time -= FrameTime;
}

#include "cfile/cfile.h"

void read_trigger(trigger* trig, FILE* fp)
{
	int j;
	trig->type = file_read_byte(fp);
	trig->flags = file_read_byte(fp);
	trig->num_links = file_read_byte(fp);
	j = file_read_byte(fp); //new trigger struct doesn't have pad

	trig->value = file_read_int(fp);
	trig->time = file_read_int(fp);
	for (j = 0; j < MAX_WALLS_PER_LINK; j++)
		trig->seg[j] = file_read_short(fp);
	for (j = 0; j < MAX_WALLS_PER_LINK; j++)
		trig->side[j] = file_read_short(fp);
}

void write_trigger(trigger* trig, FILE* fp)
{
	int j;
	file_write_byte(fp, trig->type);
	file_write_byte(fp, trig->flags);
	file_write_byte(fp, trig->num_links);
	file_write_byte(fp, 0);

	file_write_int(fp, trig->value);
	file_write_int(fp, trig->time);
	for (j = 0; j < MAX_WALLS_PER_LINK; j++)
		file_write_short(fp, trig->seg[j]);
	for (j = 0; j < MAX_WALLS_PER_LINK; j++)
		file_write_short(fp, trig->side[j]);
}
