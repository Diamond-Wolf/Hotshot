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

#ifdef EDITOR

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <process.h>

#include "2d/gr.h"
#include "ui/ui.h"
#include "3d/3d.h"

#include "mem/mem.h"
#include "misc/error.h"
#include "platform/mono.h"
#include "platform/key.h"
#include "func.h"

#include "main/inferno.h"
#include "editor.h"
#include "main/segment.h"

#include "main/render.h"
#include "main/screens.h"
#include "main/object.h"

#include "texpage.h"		// For texpage_goto_first
#include "meddraw.h"		// For draw_World
#include "main/game.h"
#include "main/gamepal.h"

//return 2d distance, i.e, sqrt(x*x + y*y)
int dist_2d(int x, int y)
{
	int64_t in = ((int64_t)x * (int64_t)x) + ((int64_t)y * (int64_t)y);
	return quad_sqrt(in);
}

/*#pragma aux dist_2d parm [eax] [ebx] value [eax] modify [ecx edx] = \
	"imul	eax"			\
	"xchg	ebx,eax"		\
	"mov	ecx,edx"		\
	"imul	eax"			\
	"add	eax,ebx"		\
	"adc	edx,ecx"		\
	"call	quad_sqrt";*/

// Given mouse movement in dx, dy, returns a 3x3 rotation matrix in RotMat.
// Taken from Graphics Gems III, page 51, "The Rolling Ball"

void GetMouseRotation( int idx, int idy, vms_matrix * RotMat )
{
	fix dr, cos_theta, sin_theta, denom, cos_theta1;
	fix Radius = i2f(100);
	fix dx,dy;
	fix dxdr,dydr;

	idy *= -1;

	dx = i2f(idx); dy = i2f(idy);

	dr = dist_2d(dx,dy);

	denom = dist_2d(Radius,dr);

	cos_theta = fixdiv(Radius,denom);
	sin_theta = fixdiv(dr,denom);

	cos_theta1 = f1_0 - cos_theta;

	dxdr = fixdiv(dx,dr);
	dydr = fixdiv(dy,dr);

	RotMat->rvec.x = cos_theta + fixmul(fixmul(dydr,dydr),cos_theta1);
	RotMat->uvec.x = - fixmul(fixmul(dxdr,dydr),cos_theta1);
	RotMat->fvec.x = fixmul(dxdr,sin_theta);

	RotMat->rvec.y = RotMat->uvec.x;
	RotMat->uvec.y = cos_theta + fixmul(fixmul(dxdr,dxdr),cos_theta1);
	RotMat->fvec.y = fixmul(dydr,sin_theta);

	RotMat->rvec.z = -RotMat->fvec.x;
	RotMat->uvec.z = -RotMat->fvec.y;
	RotMat->fvec.z = cos_theta;

}

int Gameview_lockstep;		//if set, view is locked to Curseg

int ToggleLockstep()
{
	Gameview_lockstep = !Gameview_lockstep;
    if (Gameview_lockstep == 0) {
        if (last_keypress != KEY_L)
            diagnostic_message("[L] - Lock mode OFF");
        else
            diagnostic_message("Lock mode OFF");
    }
    if (Gameview_lockstep) {
        if (last_keypress != KEY_L)
            diagnostic_message("[L] Lock mode ON");
        else
            diagnostic_message("Lock mode ON");

      Cursegp = &Segments[ConsoleObject->segnum];
		med_create_new_segment_from_cursegp();
		set_view_target_from_segment(Cursegp);
		Update_flags = UF_ED_STATE_CHANGED;
	}
    return Gameview_lockstep;
}

int medlisp_delete_segment(void)
{
    if (!med_delete_segment(Cursegp)) {
        if (Lock_view_to_cursegp)
            set_view_target_from_segment(Cursegp);
		  autosave_mine(mine_filename);
		  strcpy(undo_status[Autosave_count], "Delete Segment UNDONE.");
        Update_flags |= UF_WORLD_CHANGED;
        mine_changed = 1;
        diagnostic_message("Segment deleted.");
        warn_if_concave_segments();     // This could be faster -- just check if deleted segment was concave, warn accordingly
    }

	return 1;
}

int medlisp_scale_segment(void)
{
	vms_matrix	rotmat;
	vms_vector	scale;

	scale.x = fl2f((float) func_get_param(0));
	scale.y = fl2f((float) func_get_param(1));
	scale.z = fl2f((float) func_get_param(2));
	med_create_new_segment(&scale);
	med_rotate_segment(Cursegp,vm_angles_2_matrix(&rotmat,&Seg_orientation));
	Update_flags |= UF_WORLD_CHANGED;
	mine_changed = 1;

	return 1;
}

int medlisp_rotate_segment(void)
{
	vms_matrix	rotmat;

	Seg_orientation.p = func_get_param(0);
	Seg_orientation.b = func_get_param(1);
	Seg_orientation.h = func_get_param(2);
	med_rotate_segment(Cursegp,vm_angles_2_matrix(&rotmat,&Seg_orientation));
	Update_flags |= UF_WORLD_CHANGED | UF_VIEWPOINT_MOVED;
	mine_changed = 1;
	return 1;
}

int ToggleLockViewToCursegp(void)
{
	Lock_view_to_cursegp = !Lock_view_to_cursegp;
	Update_flags = UF_ED_STATE_CHANGED;
	if (Lock_view_to_cursegp) {
        if (last_keypress != KEY_V+KEY_CTRLED)
            diagnostic_message("[ctrl-V] View locked to Cursegp.");
        else
            diagnostic_message("View locked to Cursegp.");
        set_view_target_from_segment(Cursegp);
    } else {
        if (last_keypress != KEY_V+KEY_CTRLED)
            diagnostic_message("[ctrl-V] View not locked to Cursegp.");
        else
            diagnostic_message("View not locked to Cursegp.");
    }
    return Lock_view_to_cursegp;
}

int ToggleDrawAllSegments()
{
	Draw_all_segments = !Draw_all_segments;
	Update_flags = UF_ED_STATE_CHANGED;
    if (Draw_all_segments == 1) {
        if (last_keypress != KEY_A+KEY_CTRLED)
            diagnostic_message("[ctrl-A] Draw all segments ON.");
        else
            diagnostic_message("Draw all segments ON.");
    }
    if (Draw_all_segments == 0) {
        if (last_keypress != KEY_A+KEY_CTRLED)
            diagnostic_message("[ctrl-A] Draw all segments OFF.");
        else
            diagnostic_message("Draw all segments OFF.");
    }
    return Draw_all_segments;
}

int	Big_depth=6;

int IncreaseDrawDepth(void)
{
	Big_depth++;
	Update_flags = UF_ED_STATE_CHANGED;
	return 1;
}

int DecreaseDrawDepth(void)
{
	if (Big_depth > 1) {
		Big_depth--;
		Update_flags = UF_ED_STATE_CHANGED;
	}
	return 1;
}


int ToggleCoordAxes()
{
			//  Toggle display of coordinate axes.
	Show_axes_flag = !Show_axes_flag;
	LargeView.ev_changed = 1;
    if (Show_axes_flag == 1) {
        if (last_keypress != KEY_D+KEY_CTRLED)
            diagnostic_message("[ctrl-D] Coordinate axes ON.");
        else
            diagnostic_message("Coordinate axes ON.");
    }
    if (Show_axes_flag == 0) {
        if (last_keypress != KEY_D+KEY_CTRLED)
            diagnostic_message("[ctrl-D] Coordinate axes OFF.");
        else
            diagnostic_message("Coordinate axes OFF.");
    }
    return Show_axes_flag;
}

int med_keypad_goto_prev()
{
	ui_pad_goto_prev();
	return 0;
}

int med_keypad_goto_next()
{
	ui_pad_goto_next();
	return 0;
}

int med_keypad_goto()
{
	ui_pad_goto(func_get_param(0));
	return 0;
}

int render_3d_in_big_window=0;

int medlisp_update_screen()
{
	int vn;

if (!render_3d_in_big_window)
	for (vn=0;vn<N_views;vn++)
		if (Views[vn]->ev_changed || (Update_flags & (UF_WORLD_CHANGED|UF_VIEWPOINT_MOVED|UF_ED_STATE_CHANGED))) 
		{
			draw_world(Views[vn]->ev_canv,Views[vn],Cursegp,Big_depth);
			Views[vn]->ev_changed = 0;
		}

	if (Update_flags & (UF_WORLD_CHANGED|UF_GAME_VIEW_CHANGED|UF_ED_STATE_CHANGED)) 
	{
		grs_canvas temp_canvas;
		grs_canvas *render_canv,*show_canv;
		
		if (render_3d_in_big_window) 
		{
			
			gr_init_sub_canvas(&temp_canvas,canv_offscreen,0,0,
				LargeView.ev_canv->cv_bitmap.bm_w,LargeView.ev_canv->cv_bitmap.bm_h);

			render_canv = &temp_canvas;
			show_canv = LargeView.ev_canv;

		}
		else {
			render_canv	= VR_offscreen_buffer;
			show_canv	= Canv_editor_game;
		}

		gr_set_current_canvas(render_canv);
		render_frame(0, 0);

		Assert(render_canv->cv_bitmap.bm_w == show_canv->cv_bitmap.bm_w &&
				 render_canv->cv_bitmap.bm_h == show_canv->cv_bitmap.bm_h);

		ui_mouse_hide();
		gr_bm_ubitblt(show_canv->cv_bitmap.bm_w,show_canv->cv_bitmap.bm_h,
						  0,0,0,0,&render_canv->cv_bitmap,&show_canv->cv_bitmap);
		ui_mouse_show();
	}

	Update_flags=UF_NONE;       //clear flags

	return 1;
}

int med_point_2_vec(grs_canvas *canv,vms_vector *v,short sx,short sy)
{
	gr_set_current_canvas(canv);

	g3_start_frame();
	g3_set_view_matrix(&Viewer->pos,&Viewer->orient,Render_zoom);

	g3_point_2_vec(v,sx,sy);

	g3_end_frame();

	return 0;
}


 
void draw_world_from_game(void)
{
	if (ModeFlag == 2)
		draw_world(Views[0]->ev_canv,Views[0],Cursegp,Big_depth);
}

int UndoCommand()
{   int u;

    u = undo();
    if (Lock_view_to_cursegp)
		set_view_target_from_segment(Cursegp);
    if (u == 0) {
        if (Autosave_count==9) diagnostic_message(undo_status[0]);
            else
                diagnostic_message(undo_status[Autosave_count+1]);
        }
        else
	 if (u == 1) diagnostic_message("Can't Undo.");
        else
	 if (u == 2) diagnostic_message("Can't Undo - Autosave OFF");
    Update_flags |= UF_WORLD_CHANGED;
	 mine_changed = 1;
    warn_if_concave_segments();
    return 1;
}


int ToggleAutosave()
{
	Autosave_flag = !Autosave_flag;
	if (Autosave_flag == 1) 
      diagnostic_message("Autosave ON.");
        else
		diagnostic_message("Autosave OFF.");
   return Autosave_flag;
}


int AttachSegment()
{
   if (med_attach_segment(Cursegp, &New_segment, Curside, AttachSide)==4) // Used to be WBACK instead of Curside
        diagnostic_message("Cannot attach segment - already a connection on current side.");
   else {
		if (Lock_view_to_cursegp)
			set_view_target_from_segment(Cursegp);
		vm_angvec_make(&Seg_orientation,0,0,0);
		Curside = WBACK;
		Update_flags |= UF_WORLD_CHANGED;
	   autosave_mine(mine_filename);
	   strcpy(undo_status[Autosave_count], "Attach Segment UNDONE.\n");
		mine_changed = 1;
		warn_if_concave_segment(Cursegp);
      }
	return 1;
}

int ForceTotalRedraw()
{
	Update_flags = UF_ALL;
	return 1;
}


#if ORTHO_VIEWS
int SyncLargeView()
{
	// Make large view be same as one of the orthogonal views.
	Large_view_index = (Large_view_index + 1) % 3;  // keep in 0,1,2 for top, front, right
	switch (Large_view_index) {
		case 0: LargeView.ev_matrix = TopView.ev_matrix; break;
		case 1: LargeView.ev_matrix = FrontView.ev_matrix; break;
		case 2: LargeView.ev_matrix = RightView.ev_matrix; break;
	}
	Update_flags |= UF_VIEWPOINT_MOVED;
	return 1;
}
#endif

int DeleteCurSegment()
{
	// Delete current segment.
    med_delete_segment(Cursegp);
    autosave_mine(mine_filename);
    strcpy(undo_status[Autosave_count], "Delete segment UNDONE.");
    if (Lock_view_to_cursegp)
		set_view_target_from_segment(Cursegp);
    Update_flags |= UF_WORLD_CHANGED;
    mine_changed = 1;
    diagnostic_message("Segment deleted.");
    warn_if_concave_segments();     // This could be faster -- just check if deleted segment was concave, warn accordingly

    return 1;
}

int CreateDefaultNewSegment()
{
	// Create a default segment for New_segment.
	vms_vector  tempvec;
	med_create_new_segment(vm_vec_make(&tempvec,DEFAULT_X_SIZE,DEFAULT_Y_SIZE,DEFAULT_Z_SIZE));
	mine_changed = 1;

	return 1;
}

int CreateDefaultNewSegmentandAttach()
{
	CreateDefaultNewSegment();
	AttachSegment();

	return 1;
}

int ExchangeMarkAndCurseg()
{
	// If Markedsegp != Cursegp, and Markedsegp->segnum != -1, exchange Markedsegp and Cursegp
	if (Markedsegp)
		if (Markedsegp->segnum != -1) {
			segment *tempsegp;
			int     tempside;
			tempsegp = Markedsegp;  Markedsegp = Cursegp;   Cursegp = tempsegp;
			tempside = Markedside;  Markedside = Curside;   Curside = tempside;
			med_create_new_segment_from_cursegp();
			Update_flags |= UF_ED_STATE_CHANGED;
			mine_changed = 1;
		}
	return 1;
}

int medlisp_add_segment()
{
	AttachSegment();
//segment *ocursegp = Cursegp;
//	med_attach_segment(Cursegp, &New_segment, Curside, WFRONT); // Used to be WBACK instead of Curside
//med_propagate_tmaps_to_segments(ocursegp,Cursegp);
//	set_view_target_from_segment(Cursegp);
////	while (!vm_angvec_make(&Seg_orientation,0,0,0));
//	Curside = WBACK;

	return 1;
}


int ClearSelectedList(void)
{
	N_selected_segs = 0;
	Update_flags |= UF_WORLD_CHANGED;

	diagnostic_message("Selected list cleared.");

	return 1;
}


int ClearFoundList(void)
{
	N_found_segs = 0;
	Update_flags |= UF_WORLD_CHANGED;

	diagnostic_message("Found list cleared.");

	return 1;
}




// ---------------------------------------------------------------------------------------------------
// Do chase mode.
//	View current segment (Cursegp) from the previous segment.
void set_chase_matrix(segment *sp)
{
	int			v;
	vms_vector	forvec = ZERO_VECTOR, upvec;
	vms_vector	tv = ZERO_VECTOR;
	segment		*psp;

	// move back two segments, if possible, else move back one, if possible, else use current
	if (IS_CHILD(sp->children[WFRONT])) {
		psp = &Segments[sp->children[WFRONT]];
		if (IS_CHILD(psp->children[WFRONT]))
			psp = &Segments[psp->children[WFRONT]];
	} else
		psp = sp;

	for (v=0; v<MAX_VERTICES_PER_SEGMENT; v++)
		vm_vec_add2(&forvec,&Vertices[sp->verts[v]]);
	vm_vec_scale(&forvec,F1_0/MAX_VERTICES_PER_SEGMENT);

	for (v=0; v<MAX_VERTICES_PER_SEGMENT; v++)
		vm_vec_add2(&tv,&Vertices[psp->verts[v]]);
	vm_vec_scale(&tv,F1_0/MAX_VERTICES_PER_SEGMENT);

	Ed_view_target = forvec;

	vm_vec_sub2(&forvec,&tv);

	extract_up_vector_from_segment(psp,&upvec);

	if (!((forvec.x == 0) && (forvec.y == 0) && (forvec.z == 0)))
		vm_vector_2_matrix(&LargeView.ev_matrix,&forvec,&upvec,NULL);
}



// ---------------------------------------------------------------------------------------------------
void set_view_target_from_segment(segment *sp)
{
	vms_vector	tv = ZERO_VECTOR;
	int			v;

	if (Funky_chase_mode)
		{
		mprintf((0, "Trying to set chase mode\n"));
		//set_chase_matrix(sp);
		}
	else {
		for (v=0; v<MAX_VERTICES_PER_SEGMENT; v++)
			vm_vec_add2(&tv,&Vertices[sp->verts[v]]);

		vm_vec_scale(&tv,F1_0/MAX_VERTICES_PER_SEGMENT);

		Ed_view_target = tv;

	}
	Update_flags |= UF_VIEWPOINT_MOVED;

}

int set_level_palette()
{
	UI_WINDOW* NameWindow = NULL;
	UI_GADGET_INPUTBOX* NameText;
	UI_GADGET_BUTTON* QuitButton;

	// Open a window with a quit button
	NameWindow = ui_open_window(20, 20, 300, 110, WIN_DIALOG);
	QuitButton = ui_add_gadget_button(NameWindow, 150 - 24, 60, 48, 40, "Done", NULL);

	ui_wprintf_at(NameWindow, 10, 12, "Enter palette name:");
	NameText = ui_add_gadget_inputbox(NameWindow, 10, 30, FILENAME_LEN, FILENAME_LEN, Current_level_palette);

	NameWindow->keyboard_focus_gadget = (UI_GADGET*)NameText;
	QuitButton->hotkey = KEY_ENTER;

	ui_gadget_calc_keys(NameWindow);

	while (!QuitButton->pressed && last_keypress != KEY_ENTER) {
		ui_mega_process();
		ui_window_do_gadgets(NameWindow);
	}

	strcpy(Current_level_palette, NameText->text);

	if (NameWindow != NULL) {
		ui_close_window(NameWindow);
		NameWindow = NULL;
	}

	load_palette(Current_level_palette, 1, 0);
	Update_flags = UF_ALL;

	return 0;
}

int med_create_flickering_light()
{
	static char maskbuffer[33] = "0";
	static char timebuffer[11] = "0.1";
	int count = 32, i;
	uint32_t mask = 0;
	fix time;
	UI_WINDOW* NameWindow = NULL;
	UI_GADGET_INPUTBOX* NameText, *TimeText;
	UI_GADGET_BUTTON* QuitButton;

	// Open a window with a quit button
	NameWindow = ui_open_window(20, 20, 500, 110, WIN_DIALOG);
	QuitButton = ui_add_gadget_button(NameWindow, 150 - 24, 60, 48, 40, "Done", NULL);

	ui_wprintf_at(NameWindow, 10, 12, "Light mask:");
	NameText = ui_add_gadget_inputbox(NameWindow, 10, 30, 32, 32, maskbuffer);
	ui_wprintf_at(NameWindow, 274, 12, "Time:");
	TimeText = ui_add_gadget_inputbox(NameWindow, 274, 30, 10, 10, timebuffer);

	NameWindow->keyboard_focus_gadget = (UI_GADGET*)NameText;
	QuitButton->hotkey = KEY_ENTER;

	ui_gadget_calc_keys(NameWindow);

	while (!QuitButton->pressed && last_keypress != KEY_ENTER) {
		ui_mega_process();
		ui_window_do_gadgets(NameWindow);
	}

	strcpy(maskbuffer, NameText->text);
	strcpy(timebuffer, TimeText->text);

	if (NameWindow != NULL) {
		ui_close_window(NameWindow);
		NameWindow = NULL;
	}

	count = strlen(maskbuffer);

	for (i = 0; i < count; i++)
	{
		if (maskbuffer[i] == '1')
			mask |= (1 << i);
	}
	time = fl2f(atof(timebuffer));

	if (add_flicker(Cursegp - Segments, Curside, time, mask))
	{
		editor_status("Flickering light updated");
	}
	else
	{
		editor_status("Failed to create flickering light");
	}

	return 0;
}

#endif
