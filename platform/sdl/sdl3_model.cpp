/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION("PARALLAX").PARALLAX, IN DISTRIBUTING THE CODE TO
END - USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY - FREE, PERPETUAL LICENSE TO SUCH END - USERS FOR USE BY SUCH END - USERS
IN USING, DISPLAYING, AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON - COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.IN NO EVENT SHALL THE END - USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE - BEARING PURPOSES.THE END - USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993 - 1999 PARALLAX SOFTWARE CORPORATION.ALL RIGHTS RESERVED.
*/

#include <cstdint>
#include <vector>
#include <bit>
#include <cmath>

#include "platform/renderapi.h"
#include "sdl3_render.h"

#include "vecmat/vecmat.h"
#include "3d/3d.h"
#include "main/newcheat.h"
#include <main/game.h>
#include <misc/error.h>
#include <platform/mono.h>
#include <main/bm.h>

typedef polymodel VanillaModel;

#define OP_EOF				0	//eof
#define OP_DEFPOINTS		1	//defpoints
#define OP_FLATPOLY		2	//flat-shaded polygon
#define OP_TMAPPOLY		3	//texture-mapped polygon
#define OP_SORTNORM		4	//sort by normal
#define OP_RODBM			5	//rod bitmap
#define OP_SUBCALL		6	//call a subobject
#define OP_DEFP_START	7	//defpoints with start
#define OP_GLOW			8	//glow value for next poly

#define w(p)  (*((short *) (p)))
#define wp(p)  ((short *) (p))
#define vp(p)  ((vms_vector *) (p))

thread_local std::vector<g3s_point> interpPointList;
thread_local std::vector<g3s_point*> pointList;

extern vms_matrix View_matrix;

struct InterpColor {
	short pal_entry;
	unsigned short rgb15;
};
thread_local std::vector<InterpColor> interpColorTable;

const vms_angvec zero_angles = { 0,0,0 };

static void rotate_point_list(g3s_point* dest, vms_vector* src, int n) {
	while (n--)
		g3_rotate_point(dest++, src++);
}

namespace HRender {

	size_t xlatPosCache;

	enum PolymodelAllocationMode {
		PAM_NOT_SPECIAL,
		PAM_NEW_SUBMODEL,
		PAM_NEW_MODEL
	};

	Polymodel* AllocatePolymodel(size_t* xlatIndex);

	void InitPolymodelInterpreter() {
		interpPointList.resize(1000);
		pointList.resize(25);
		interpColorTable.resize(100);
	}

	void RenderPolymodelSub(const ViewTarget target, const int segno, const void* model_ptr, const std::vector<grs_bitmap*>& model_bitmaps, const std::vector<short>& bitmapIDs, const vms_angvec anim_angles[], const fix model_light, const fix glow_values[], const vms_vector* origin, const vms_matrix* rotation) {

		uint8_t* p = (uint8_t*)model_ptr;
		int current_poly = 0;

		int glow_num = -1;		//glow off by default

		int loop = 0;

		while (w(p) != OP_EOF)

			switch (w(p))
			{

			case OP_DEFPOINTS:
			{
				int n = w(p + 2);
				if (n > interpPointList.size())
					interpPointList.resize(n);

				vms_vector* v = vp(p + 4);

				for (int i = 0; i < n; i++) {
					interpPointList[i] = {
						.p3_vec = *v
					};
					v++;
				}

				p += n * sizeof(struct vms_vector) + 4;
				break;
			}

			case OP_DEFP_START:
			{
				int n = w(p + 2);
				int s = w(p + 4);

				if (s + n > interpPointList.size())
					interpPointList.resize(s + n);

				vms_vector* v = vp(p + 8);

				for (int i = 0; i < n; i++) {
					interpPointList[i + s] = {
						.p3_vec = *v
					};
					v++;
				}

				p += n * sizeof(struct vms_vector) + 8;

				break;
			}

			case OP_FLATPOLY:
			{

				int light = 0;
				InterpColor color;
				int nv = w(p + 2);

				//Assert(nv < MAX_POINTS_PER_POLY);
				if (nv > pointList.size())
					pointList.resize(nv);

				//if (g3_check_normal_facing(vp(p + 4), vp(p + 16)) > 0)
				{
					int i;
					if (currentGame == G_DESCENT_2) {
						color = interpColorTable[w(p + 28)];
						if (glow_num != -1)
						{
							light = glow_values[glow_num];
							glow_num = -1;
							if (light == -2)
								color = {
									.pal_entry = 255,
									.rgb15 = 0xffff
							};
						}
					}
					else {
						color = interpColorTable[w(p + 28)];
					}

					if (light != -3)
					{
						//gr_setcolor(drawindex);

						for (i = 0; i < nv; i++)
							pointList[i] = interpPointList.data() + wp(p + 30)[i];
						//g3_draw_poly(nv, pointList.data());
					}
				}

				p += 30 + ((nv & ~1) + 1) * 2;
				break;
			}

			case OP_TMAPPOLY:
			{
				int nv = w(p + 2);
				g3s_uvl* uvl_list;

				//Assert(nv < MAX_POINTS_PER_POLY);
				if (nv < pointList.size())
					pointList.resize(nv);

				//if (g3_check_normal_facing(vp(p + 4), vp(p + 16)) > 0)
				{
					int i;
					fix light;

					//calculate light from surface normal

					if (glow_num < 0) //no glow
					{
						light = -vm_vec_dot(&View_matrix.fvec, vp(p + 16));
						light = f1_0 / 4 + (light * 3) / 4;
						light = fixmul(light, model_light);
					}
					else //yes glow
					{
						light = glow_values[glow_num];
						glow_num = -1;
					}

					//now poke light into l values

					uvl_list = (g3s_uvl*)(p + 30 + ((nv & ~1) + 1) * 2);

					g3s_point* verts = new g3s_point[nv];

					for (i = 0; i < nv; i++) {
						verts[i] = interpPointList[wp(p + 30)[i]];
						//uvl_list[i].l = light;
						verts[i].p3_u = uvl_list[i].u;
						verts[i].p3_v = uvl_list[i].v;
						verts[i].p3_l = light;
					}

					short texind = w(p + 28);

					auto& tp = rendererState.tpageLocations[bitmapIDs[texind]];

					SideDrawKey k {
							&rendererState.tpages[tp.first],
							NULL,
							target
					};

					{

						std::lock_guard lock(modelDrawCallMutex);

						if (modelDrawCalls.count(k) == 0) {
							modelDrawCalls[k] = std::vector<ObjDrawCall>();
						}

						std::vector<ObjDrawCall>& calls = modelDrawCalls[k];

						const vms_vector& pos = *origin;
						const vms_matrix& rot = *rotation;

						calls.emplace_back([tp, segno, verts, nv, light, pos, rot](SDL_GPUCommandBuffer* combuf, SDL_GPURenderPass* rpass, SDL_GPUCopyPass* cpass) {

							/*if (rendererState.drawCallObjID != -1)*/ {

								mat4f matrix;

								memcpy(matrix, M4_IDENTITY_MATRIX, sizeof(matrix));

								for (int m = 0; m < 3; m++) {
									for (int n = 0; n < 3; n++) {
										matrix[m][n] = f2fl(rot[m][n]);
									}

									matrix[3][m] = f2fl(pos[m]);
								}

								UploadVertexMatrix(combuf, &M4_IDENTITY_MATRIX, MID_ANIM);
								UploadVertexMatrix(combuf, &matrix, MID_MODEL);

							}
							//rendererState.drawCallObjID = -1;

							//const auto& segment = Segments[segno];

							int vertStart = worldVertices.size();

							WorldVertex* wverts = new WorldVertex[nv];
							uint32_t* winds = new uint32_t[3 * (nv - 2)];

							for (int i = 0; i < nv; i++) {

								auto& vert = verts[i];

								float lightR, lightG, lightB;
								lightR = lightG = lightB = 1.f;// f2fl(light);

								if (cheatValues[CI_RAVE]) {

									SDL_srand((uint64_t)pos.x * 0xFFFF + (uint64_t)pos.y * 0x00FF + pos.z + GameTime);

									lightR = sqrtf(lightR);
									lightG = sqrtf(lightG);
									lightB = sqrtf(lightB);

									lightR *= SDL_randf() * 2.f;
									lightG *= SDL_randf() * 2.f;
									lightB *= SDL_randf() * 2.f;

								}

								/*lightR *= lightFactorR;
								lightG *= lightFactorG;
								lightB *= lightFactorB;*/

								wverts[i] = WorldVertex {
									.pos = {
										f2fl(vert.p3_vec.x),
										f2fl(vert.p3_vec.y),
										f2fl(vert.p3_vec.z)
									},
									.uv = {
										f2fl(vert.p3_u),
										f2fl(vert.p3_v),
									},
									.props = {
										segno,
										tp.second,
										-1,
										0
									},
									.colormod = {
										lightR,
										lightG,
										lightB,
										1.f
									}
								};

							}

							for (int i = 0; i < nv - 2; i++) {
								winds[i * 3 + 0] = 0;
								winds[i * 3 + 1] = i + 1;
								winds[i * 3 + 2] = i + 2;
							}

							uint32_t vsize = nv * sizeof(*wverts);
							uint32_t isize = (nv - 2) * 3 * sizeof(*winds);

							SDL_GPUBufferCreateInfo bci {
								.usage = SDL_GPU_BUFFERUSAGE_VERTEX | SDL_GPU_BUFFERUSAGE_INDEX,
								.size = vsize + isize
							};

							if (rendererState.drawTransferBuffer.memoryMap == NULL || rendererState.drawTransferOffset + vsize + isize > rendererState.drawTransferBuffer.size) {

								if (rendererState.drawTransferBuffer.memoryMap)
									FreeTransferBuffer(rendererState.drawTransferBuffer);

								uint32_t msize = Segments.size() * 8 * (sizeof(WorldVertex) + sizeof(uint32_t)) * 16;
								if (msize < vsize + isize) {
									Int3(); //Shouldn't ever happen, but just to be safe...
									msize = vsize + isize;
								}

								msize = std::bit_ceil(msize);

								SDL_GPUTransferBufferCreateInfo tbci {
									.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
									.size = msize
								};

								rendererState.drawTransferBuffer = CreateTransferBuffer(&tbci, true);
								rendererState.drawTransferOffset = 0;

								mprintf((0, "Reallocated vertex transfer buffer in polyobj tmap"));

							}

							SDL_GPUBuffer* drawBuffer = SDL_CreateGPUBuffer(rendererState.device, &bci);
							if (drawBuffer == NULL)
								Error("Error creating vertex buffer: %s", SDL_GetError());

							memcpy(rendererState.drawTransferBuffer.memoryMap + rendererState.drawTransferOffset, wverts, vsize);
							memcpy(rendererState.drawTransferBuffer.memoryMap + rendererState.drawTransferOffset + vsize, winds, isize);

							SDL_GPUTransferBufferLocation tbl {
								.transfer_buffer = rendererState.drawTransferBuffer.buffer,
								.offset = rendererState.drawTransferOffset
							};

							SDL_GPUBufferRegion br {
								.buffer = drawBuffer,
								.offset = 0,
								.size = vsize + isize
							};

							SDL_UploadToGPUBuffer(cpass, &tbl, &br, true);

							SDL_GPUBufferBinding bb {
								.buffer = drawBuffer,
								.offset = 0
							};
							SDL_BindGPUVertexBuffers(rpass, 0, &bb, 1);

							//bb.buffer = indexBuffer;
							bb.offset = vsize;
							SDL_BindGPUIndexBuffer(rpass, &bb, SDL_GPU_INDEXELEMENTSIZE_32BIT);

							SDL_GPUTextureSamplerBinding tsb[] { {
								.texture = rendererState.secondaryTexture,
								.sampler = rendererState.defaultSampler
							}, {
								.texture = rendererState.primaryTexture,
								.sampler = rendererState.defaultSampler
							} };
							SDL_BindGPUFragmentSamplers(rpass, 0, tsb, 2);

							SDL_GPUBuffer* storageBuffers[] { rendererState.paletteBuffer };// , rendererState.paletteBuffer}; //TODO: need portal buffer
							SDL_BindGPUFragmentStorageBuffers(rpass, 0, storageBuffers, SDL_arraysize(storageBuffers));

							SDL_DrawGPUIndexedPrimitives(rpass, (nv - 2) * 3, 1, 0, 0, 0);

							SDL_ReleaseGPUBuffer(rendererState.device, drawBuffer);

							rendererState.drawTransferOffset += vsize + isize;

							delete[] verts;
							delete[] wverts;
							delete[] winds;

							});

						//g3_draw_tmap(nv, pointList.data(), uvl_list, model_bitmaps[w(p + 28)]);

					}
				}

				p += 30 + ((nv & ~1) + 1) * 2 + nv * 12;

				break;
			}

			case OP_SORTNORM:

				/*vms_matrix mat;
				vms_matrix rotated;

				if (anim_angles)
					vm_angles_2_matrix(&mat, const_cast<vms_angvec*>(anim_angles) + w(p + 2));
				else
					mat = IDENTITY_MATRIX;

				vms_vector pos = *vp(p + 4);
				vm_vec_add2(&pos, parentOrigin);*/

				//Hardware will handle sorting now

				//if (g3_check_normal_facing(vp(p + 16), vp(p + 4)) > 0) //facing
				//{
					//draw back then front
				RenderPolymodelSub(target, segno, p + w(p + 30), model_bitmaps, bitmapIDs, anim_angles, model_light, glow_values, origin, rotation);
				RenderPolymodelSub(target, segno, p + w(p + 28), model_bitmaps, bitmapIDs, anim_angles, model_light, glow_values, origin, rotation);
				/*}
				else //not facing.  draw front then back
				{
					RenderPolymodelSub(target, segno, p + w(p + 28), model_bitmaps, bitmapIDs, anim_angles, model_light, glow_values, &pos, vm_matrix_x_matrix(&rotated, &mat, const_cast<vms_matrix*>(parentRotation)));
					RenderPolymodelSub(target, segno, p + w(p + 30), model_bitmaps, bitmapIDs, anim_angles, model_light, glow_values, &pos, vm_matrix_x_matrix(&rotated, &mat, const_cast<vms_matrix*>(parentRotation)));
				}*/

				p += 32;
				break;

			case OP_RODBM:
			{

				g3s_point rod_bot_p, rod_top_p;

				g3_rotate_point(&rod_bot_p, vp(p + 20));
				g3_rotate_point(&rod_top_p, vp(p + 4));

				//g3_draw_rod_tmap(model_bitmaps[w(p + 2)], &rod_bot_p, w(p + 16), &rod_top_p, w(p + 32), f1_0);

				p += 36;
				break;
			}

			case OP_SUBCALL:
			{
				vms_matrix mat;
				vms_matrix rmat;

				if (anim_angles)
					vm_angles_2_matrix(&mat, const_cast<vms_angvec*>(anim_angles) + w(p + 2));
				else
					mat = IDENTITY_MATRIX;

				vms_vector* mpos = vp(p + 4);
				vms_vector rpos;
				vm_copy_transpose_matrix(&rmat, const_cast<vms_matrix*>(rotation));
				vm_vec_rotate(&rpos, mpos, &rmat);
				vm_vec_add2(&rpos, origin);

				RenderPolymodelSub(target, segno, p + w(p + 16), model_bitmaps, bitmapIDs, anim_angles, model_light, glow_values, &rpos, vm_matrix_x_matrix(&rmat, &mat, const_cast<vms_matrix*>(rotation)));

				p += 20;
				break;
			}

			case OP_GLOW:

				if (glow_values)
					glow_num = w(p + 2);
				p += 4;
				break;

			default:
				Int3();
			}

	}


	void BuildPolymodelSub(Polymodel* model, const void* model_ptr, const std::vector<short>& bitmapIDs) {

		uint8_t* p = (uint8_t*)model_ptr;
		int current_poly = 0;

		int glow_num = -1;		//glow off by default

		int loop = 0;

		while (w(p) != OP_EOF)

			switch (w(p))
			{

			case OP_DEFPOINTS:
			{
				int n = w(p + 2);
				if (n > interpPointList.size())
					interpPointList.resize(n);

				vms_vector* v = vp(p + 4);

				for (int i = 0; i < n; i++) {
					interpPointList[i] = {
						.p3_vec = *v
					};
					v++;
				}

				p += n * sizeof(struct vms_vector) + 4;
				break;
			}

			case OP_DEFP_START:
			{
				int n = w(p + 2);
				int s = w(p + 4);

				if (s + n > interpPointList.size())
					interpPointList.resize(s + n);

				vms_vector* v = vp(p + 8);

				for (int i = 0; i < n; i++) {
					interpPointList[i + s] = {
						.p3_vec = *v
					};
					v++;
				}

				p += n * sizeof(struct vms_vector) + 8;

				break;
			}

			case OP_FLATPOLY:
			{

				int light = 0;
				InterpColor color;
				int nv = w(p + 2);

				//Assert(nv < MAX_POINTS_PER_POLY);
				if (nv > pointList.size())
					pointList.resize(nv);

				//if (g3_check_normal_facing(vp(p + 4), vp(p + 16)) > 0)
				{
					int i;
					if (currentGame == G_DESCENT_2) {
						color = interpColorTable[w(p + 28)];
						if (glow_num != -1)
						{
							//light = glow_values[glow_num];
							light = 0;
							glow_num = -1;
							if (light == -2)
								color = {
									.pal_entry = 255,
									.rgb15 = 0xffff
							};
						}
					}
					else {
						color = interpColorTable[w(p + 28)];
					}

					if (light != -3)
					{
						//gr_setcolor(drawindex);

						for (i = 0; i < nv; i++)
							pointList[i] = interpPointList.data() + wp(p + 30)[i];
						//g3_draw_poly(nv, pointList.data());
					}
				}

				p += 30 + ((nv & ~1) + 1) * 2;
				break;
			}

			case OP_TMAPPOLY:
			{
				int nv = w(p + 2);
				g3s_uvl* uvl_list;

				if (nv < pointList.size())
					pointList.resize(nv);

				////////////////////////////////////

				int i;
				fix light;

				//calculate light from surface normal

				if (glow_num < 0) //no glow
				{
					light = -vm_vec_dot(&View_matrix.fvec, vp(p + 16));
					light = f1_0 / 4 + (light * 3) / 4;
					//light = fixmul(light, model_light);
				}
				else //yes glow
				{
					light = 1;// glow_values[glow_num];
					glow_num = -1;
				}

				//now poke light into l values

				uvl_list = (g3s_uvl*)(p + 30 + ((nv & ~1) + 1) * 2);

				auto& tloc = rendererState.tpageLocations[bitmapIDs[w(p + 28)]];
				ModelFaceBatch& batch = model->batches[&rendererState.tpages[tloc.first]];

				uint32_t startIndex = batch.verts.size();

				for (i = 0; i < nv; i++) {
					//uvl_list[i].l = light;

					auto point = interpPointList.data() + wp(p + 30)[i];
					batch.verts.emplace_back(WorldVertex {
						.pos = {
							f2fl(point->p3_vec.x),
							f2fl(point->p3_vec.y),
							f2fl(point->p3_vec.z)
						},
						.uv = {
							f2fl(uvl_list[i].u),
							f2fl(uvl_list[i].v)
						},
						.props = {
							tloc.second,
							-1,
							0,
							0
						},
						.colormod = {
							1,
							1,
							1,
							1
						}
					});

				}

				for (int i = 1; i <= nv - 2; i++) {
					batch.indices.push_back(startIndex);
					batch.indices.push_back(startIndex + i);
					batch.indices.push_back(startIndex + i + 1);
				}

				//g3_draw_tmap(nv, pointList, uvl_list, model_bitmaps[w(p + 28)]);

				



				////////////////////////////////////

				p += 30 + ((nv & ~1) + 1) * 2 + nv * 12;

				break;
			}

			case OP_SORTNORM:

				BuildPolymodelSub(AllocatePolymodel(nullptr), p + w(p + 28), bitmapIDs);
				BuildPolymodelSub(AllocatePolymodel(nullptr), p + w(p + 30), bitmapIDs);

				p += 32;
				break;

			case OP_RODBM:
			{

				g3s_point rod_bot_p, rod_top_p;

				g3_rotate_point(&rod_bot_p, vp(p + 20));
				g3_rotate_point(&rod_top_p, vp(p + 4));

				//g3_draw_rod_tmap(model_bitmaps[w(p + 2)], &rod_bot_p, w(p + 16), &rod_top_p, w(p + 32), f1_0);

				p += 36;
				break;
			}

			case OP_SUBCALL:
			{
				
				xlatPosCache++;
				BuildPolymodelSub(AllocatePolymodel(&modelIDXlat[xlatPosCache]), p + w(p + 16), bitmapIDs);

				p += 20;
				break;
			}

			case OP_GLOW:

				//if (glow_values)
				//	glow_num = w(p + 2);
				p += 4;
				break;

			default:
				Int3();
			}

	}


	void GenerateModel(VanillaModel& model, int xlatPos) {
		
		const vms_vector& pos = vmd_zero_vector;
		const vms_matrix& orient = IDENTITY_MATRIX_INST;

		//polymodel& model = activeBMTable->models[model_num];

		std::vector<short> bitmapIDs;
		bitmapIDs.reserve(100);
		bitmapIDs.clear();

		for (int i = 0; i < model.n_textures; i++) {
			short bmpID = activeBMTable->objectBitmaps[activeBMTable->objectBitmapPointers[model.first_texture + i]].index;
			bitmapIDs.push_back(bmpID);
		}

		//modelIDXlat[xlatPos] = models.size();

		xlatPosCache = xlatPos;
		BuildPolymodelSub(AllocatePolymodel(&modelIDXlat[xlatPos]), model.model_data, bitmapIDs);

	}

	void GenerateModels() {

		models.resize(activeBMTable->models.size() * MAX_SUBMODELS);
		models.shrink_to_fit();
		models.clear();

		modelIDXlat.resize(activeBMTable->models.size() * MAX_SUBMODELS);
		modelIDXlat.shrink_to_fit();
		for (auto& id : modelIDXlat)
			id = -1;

		/*for (VanillaModel& model : activeBMTable->models) {
			GenerateModel(model);
		}*/

		for (int i = 0; i < activeBMTable->models.size(); i++) {

			VanillaModel& model = activeBMTable->models[i];

			GenerateModel(model, i * 10);

		}
	
	}

	Polymodel* AllocatePolymodel(size_t* index) {
		models.push_back(Polymodel());
		if (index)
			*index = models.size() - 1;
		return &models[models.size() - 1];
	}

}