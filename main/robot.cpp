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

#include "misc/error.h"

#include "inferno.h"

#include "robot.h"
#include "object.h"
#include "polyobj.h"
#include "platform/mono.h"

#include "bm.h"

#define deg(a) ((int) (a) * 32768 / 180)

//given an object and a gun number, return position in 3-space of gun
//fills in gun_point
void calc_gun_point(vms_vector* gun_point,object *obj,int gun_num)
{
	polymodel *pm;
	robot_info *r;
	vms_vector pnt;
	vms_matrix m;
	int mn;				//submodel number

	Assert(obj->render_type==RT_POLYOBJ || obj->render_type==RT_MORPH);
	Assert(obj->id < activeBMTable->robots.size());

	r = &activeBMTable->robots[obj->id];
	pm =&activeBMTable->models[r->model_num];

	if (gun_num >= r->n_guns)
	{
		mprintf((1, "Bashing gun num %d to 0.\n", gun_num));
		//Int3();
		gun_num = 0;
	}

//	Assert(gun_num < r->n_guns);

	pnt = r->gun_points[gun_num];
	mn = r->gun_submodels[gun_num];

	//instance up the tree for this gun
	while (mn != 0) 
	{
		vms_vector tpnt;

		vm_angles_2_matrix(&m,&obj->rtype.pobj_info.anim_angles[mn]);
		vm_transpose_matrix(&m);
		vm_vec_rotate(&tpnt,&pnt,&m);

		vm_vec_add(&pnt,&tpnt,&pm->submodel_offsets[mn]);

		mn = pm->submodel_parents[mn];
	}

	//now instance for the entire object

	vm_copy_transpose_matrix(&m,&obj->orient);
	vm_vec_rotate(gun_point,&pnt,&m);
	vm_vec_add2(gun_point,&obj->pos);
}

//fills in ptr to list of joints, and returns the number of joints in list
//takes the robot type (object id), gun number, and desired state
int robot_get_anim_state(jointpos **jp_list_ptr,int robot_type,int gun_num,int state)
{
	Assert(gun_num <= activeBMTable->robots[robot_type].n_guns);
	*jp_list_ptr = &activeBMTable->robotJoints[activeBMTable->robots[robot_type].anim_states[gun_num][state].offset];
	return activeBMTable->robots[robot_type].anim_states[gun_num][state].n_joints;
}


//for test, set a robot to a specific state
void set_robot_state(object *obj,int state)
{
	int g,j,jo;
	robot_info *ri;
	jointlist *jl;

	Assert(obj->type == OBJ_ROBOT);

	ri = &activeBMTable->robots[obj->id];

	for (g=0;g<ri->n_guns+1;g++)
	{
		jl = &ri->anim_states[g][state];

		jo = jl->offset;

		for (j=0;j<jl->n_joints;j++,jo++) 
		{
			int jn;

			jn = activeBMTable->robotJoints[jo].jointnum;

			obj->rtype.pobj_info.anim_angles[jn] = activeBMTable->robotJoints[jo].angles;

		}
	}
}

#include "platform/mono.h"

//set the animation angles for this robot.  Gun fields of robot info must
//be filled in.
void robot_set_angles(robot_info *r,polymodel *pm,vms_angvec angs[N_ANIM_STATES][MAX_SUBMODELS])
{
	int m,g,state;
	int gun_nums[MAX_SUBMODELS];			//which gun each submodel is part of

	for (m=0;m<pm->n_models;m++)
		gun_nums[m] = r->n_guns;		//assume part of body...

	gun_nums[0] = -1;		//body never animates, at least for now

	for (g=0;g<r->n_guns;g++) 
	{
		m = r->gun_submodels[g];

		while (m != 0) 
		{
			gun_nums[m] = g;				//...unless we find it in a gun
			m = pm->submodel_parents[m];
		}
	}

	for (g=0;g<r->n_guns+1;g++) 
	{
		//mprintf(0,"Gun %d:\n",g);

		for (state=0;state<N_ANIM_STATES;state++) 
		{
			//mprintf(0," State %d:\n",state);

			r->anim_states[g][state].n_joints = 0;
			r->anim_states[g][state].offset = activeBMTable->robotJoints.size();

			for (m=0;m<pm->n_models;m++) 
			{
				if (gun_nums[m] == g) 
				{
					//mprintf(0,"  Joint %d: %x %x %x\n",m,angs[state][m].pitch,angs[state][m].bank,angs[state][m].head);
					jointpos pos;
					pos.jointnum = m;
					pos.angles = angs[state][m];
					r->anim_states[g][state].n_joints++;
					activeBMTable->robotJoints.push_back(std::move(pos));
				}
			}
		}
	}
}
