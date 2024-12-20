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

#include <stdlib.h>

//#include "pa_enabl.h"                   //$$POLY_ACC
#include "2d/gr.h"
#include "misc/error.h"
#include "game.h"
#include "textures.h"
#include "platform/mono.h"
#include "2d/rle.h"
#include "piggy.h"

#if defined(POLY_ACC)
#include "poly_acc.h"
#endif

#define MAX_NUM_CACHE_BITMAPS 50

typedef struct	{
	grs_bitmap * bitmap;
	grs_bitmap * bottom_bmp;
	grs_bitmap * top_bmp;
	int 		orient;
	int		last_frame_used;
} TEXTURE_CACHE;

static TEXTURE_CACHE Cache[MAX_NUM_CACHE_BITMAPS];

static int num_cache_entries = 0;

static int cache_hits = 0;
static int cache_misses = 0;

void texmerge_close();
void merge_textures_super_xparent(int type, grs_bitmap *bottom_bmp, grs_bitmap *top_bmp,
											 uint8_t *dest_data);
void merge_textures_new(int type, grs_bitmap *bottom_bmp, grs_bitmap *top_bmp,
								uint8_t *dest_data);

#if defined(POLY_ACC)       // useful to all of D2 I think.
extern grs_bitmap * rle_get_id_sub( grs_bitmap * bmp );

//----------------------------------------------------------------------
// Given pointer to a bitmap returns a unique value that describes the bitmap.
// Returns 0xFFFFFFFF if this bitmap isn't a texmerge'd bitmap.
uint texmerge_get_unique_id( grs_bitmap * bmp )
{

    int i,n;
    uint tmp;
    grs_bitmap * tmpbmp;
    // Check in texmerge cache
    for (i=0; i<num_cache_entries; i++ )    {
        if ( (Cache[i].last_frame_used > -1) && (Cache[i].bitmap == bmp) )  {
            tmp = (uint)Cache[i].orient<<30;
            tmp |= ((uint)(Cache[i].top_bmp - activePiggyTable->gameBitmaps.data()))<<16;
            tmp |= (uint)(Cache[i].bottom_bmp - activePiggyTable->gameBitmaps.data());
            return tmp;
        }
    }
    // Check in rle cache
    tmpbmp = rle_get_id_sub( bmp );
    if ( tmpbmp )   {
        return (uint)(tmpbmp - activePiggyTable->gameBitmaps.data());
    }
    // Must be a normal bitmap
     return (uint)(bmp - activePiggyTable->gameBitmaps.data());
}
#endif

//----------------------------------------------------------------------

int texmerge_init(int num_cached_textures)
{
	int i;

	if ( num_cached_textures <= MAX_NUM_CACHE_BITMAPS )
		num_cache_entries = num_cached_textures;
	else
		num_cache_entries = MAX_NUM_CACHE_BITMAPS;
	
	for (i=0; i<num_cache_entries; i++ )	
	{
			// Make temp tmap for use when combining
		Cache[i].bitmap = gr_create_bitmap( 64, 64 );

		//if (get_selector( Cache[i].bitmap->bm_data, 64*64,  &Cache[i].bitmap->bm_selector))
		//	Error( "ERROR ALLOCATING CACHE BITMAP'S SELECTORS!!!!" );

		Cache[i].last_frame_used = -1;
		Cache[i].top_bmp = NULL;
		Cache[i].bottom_bmp = NULL;
		Cache[i].orient = -1;
	}
	atexit( texmerge_close );

	return 1;
}

void texmerge_flush()
{
	int i;

	for (i=0; i<num_cache_entries; i++ )	
	{
		Cache[i].last_frame_used = -1;
		Cache[i].top_bmp = NULL;
		Cache[i].bottom_bmp = NULL;
		Cache[i].orient = -1;
	}
}


//-------------------------------------------------------------------------
void texmerge_close()
{
	int i;

	for (i=0; i<num_cache_entries; i++ )	
	{
		gr_free_bitmap( Cache[i].bitmap );
		Cache[i].bitmap = NULL;
	}
}

//--unused-- int info_printed = 0;

grs_bitmap * texmerge_get_cached_bitmap( int tmap_bottom, int tmap_top )
{
	grs_bitmap *bitmap_top, *bitmap_bottom;
	int i, orient;
	int lowest_frame_count;
	int least_recently_used;

//	if ( ((FrameCount % 1000)==0) && ((cache_hits+cache_misses)>0) && (!info_printed) )	{
//		mprintf( 0, "Texmap caching:  %d hits, %d misses. (Missed=%d%%)\n", cache_hits, cache_misses, (cache_misses*100)/(cache_hits+cache_misses)  );
//		info_printed = 1;
//	} else {
//		info_printed = 0;
//	}

	//printf("\ntmap data: %d (%d) %d / %d // %d %d / %d\n", tmap_top & 0x3FFF, tmap_top, tmap_bottom, Textures.size(), Textures[tmap_top&0x3FFF].index, Textures[tmap_bottom].index, GameBitmaps.size());
	//Assert((tmap_top&0x3FFF) < Textures.size() && tmap_bottom < Textures.size());
	//Assert(Textures[tmap_top&0x3FFF].index < GameBitmaps.size() && Textures[tmap_bottom].index < GameBitmaps.size());

	if ((tmap_top & 0x3fff) >= activeBMTable->textures.size())
		bitmap_top = &activePiggyTable->gameBitmaps[0]; //bogus
	else
		bitmap_top = &activePiggyTable->gameBitmaps[activeBMTable->textures[tmap_top&0x3FFF].index];

	if (tmap_bottom >= activeBMTable->textures.size())
		bitmap_bottom = &activePiggyTable->gameBitmaps[0];
	else
		bitmap_bottom = &activePiggyTable->gameBitmaps[activeBMTable->textures[tmap_bottom].index];
	
	orient = ((tmap_top&0xC000)>>14) & 3;

	least_recently_used = 0;
	lowest_frame_count = Cache[0].last_frame_used;
	
	for (i=0; i<num_cache_entries; i++ )	
	{
		if ( (Cache[i].last_frame_used > -1) && (Cache[i].top_bmp==bitmap_top) && (Cache[i].bottom_bmp==bitmap_bottom) && (Cache[i].orient==orient ))	{
			cache_hits++;
			Cache[i].last_frame_used = FrameCount;
			return Cache[i].bitmap;
		}	
		if ( Cache[i].last_frame_used < lowest_frame_count )	
		{
			lowest_frame_count = Cache[i].last_frame_used;
			least_recently_used = i;
		}
	}

	//---- Page out the LRU bitmap;
	cache_misses++;

	// Make sure the bitmaps are paged in...
	piggy_page_flushed = 0;

	if ((tmap_top & 0x3fff) < activeBMTable->textures.size())
		PIGGY_PAGE_IN(activeBMTable->textures[tmap_top&0x3FFF]);
	if (tmap_bottom < activeBMTable->textures.size())
		PIGGY_PAGE_IN(activeBMTable->textures[tmap_bottom]);

	if (piggy_page_flushed)	
	{
		// If cache got flushed, re-read 'em.
		piggy_page_flushed = 0;
		if ((tmap_top & 0x3fff) < activeBMTable->textures.size())
			PIGGY_PAGE_IN(activeBMTable->textures[tmap_top&0x3FFF]);
		if (tmap_bottom < activeBMTable->textures.size())
			PIGGY_PAGE_IN(activeBMTable->textures[tmap_bottom]);
	}
	Assert( piggy_page_flushed == 0 );

	if (bitmap_top->bm_flags & BM_FLAG_SUPER_TRANSPARENT)	
	{
		merge_textures_super_xparent( orient, bitmap_bottom, bitmap_top, Cache[least_recently_used].bitmap->bm_data );
		Cache[least_recently_used].bitmap->bm_flags = BM_FLAG_TRANSPARENT;
		Cache[least_recently_used].bitmap->avg_color = bitmap_top->avg_color;
	} else	
	{
		merge_textures_new( orient, bitmap_bottom, bitmap_top, Cache[least_recently_used].bitmap->bm_data );
		Cache[least_recently_used].bitmap->bm_flags = bitmap_bottom->bm_flags & (~BM_FLAG_RLE);
		Cache[least_recently_used].bitmap->avg_color = bitmap_bottom->avg_color;
	}
		
	Cache[least_recently_used].top_bmp = bitmap_top;
	Cache[least_recently_used].bottom_bmp = bitmap_bottom;
	Cache[least_recently_used].last_frame_used = FrameCount;
	Cache[least_recently_used].orient = orient;

	return Cache[least_recently_used].bitmap;
}

void merge_textures_new( int type, grs_bitmap * bottom_bmp, grs_bitmap * top_bmp, uint8_t * dest_data )
{

	uint8_t * top_data, *bottom_data;

	if ( top_bmp->bm_flags & BM_FLAG_RLE )
		top_bmp = rle_expand_texture(top_bmp);

	if ( bottom_bmp->bm_flags & BM_FLAG_RLE )
		bottom_bmp = rle_expand_texture(bottom_bmp);

//	Assert( bottom_bmp != top_bmp );

	top_data = top_bmp->bm_data;
	bottom_data = bottom_bmp->bm_data;

//	Assert( bottom_data != top_data );

	//printf("Type=%d\n", type );

	switch( type )	{
		case 0:
			// Normal
			gr_merge_textures( bottom_data, top_data, dest_data );
			break;
		case 1:
			gr_merge_textures_1( bottom_data, top_data, dest_data );
			break;
		case 2:
			gr_merge_textures_2( bottom_data, top_data, dest_data );
			break;
		case 3:
			gr_merge_textures_3( bottom_data, top_data, dest_data );
			break;
	}
}

void merge_textures_super_xparent( int type, grs_bitmap * bottom_bmp, grs_bitmap * top_bmp, uint8_t * dest_data )
{
	uint8_t c;
	int x,y;

	uint8_t * top_data, *bottom_data;

	if ( top_bmp->bm_flags & BM_FLAG_RLE )
		top_bmp = rle_expand_texture(top_bmp);

	if ( bottom_bmp->bm_flags & BM_FLAG_RLE )
		bottom_bmp = rle_expand_texture(bottom_bmp);

//	Assert( bottom_bmp != top_bmp );

	top_data = top_bmp->bm_data;
	bottom_data = bottom_bmp->bm_data;

//	Assert( bottom_data != top_data );

	//mprintf( 0, "SuperX remapping type=%d\n", type );
	//Int3();
	 
	switch( type )	{
		case 0:
			// Normal
			for (y=0; y<64; y++ )
				for (x=0; x<64; x++ )	{
					c = top_data[ 64*y+x ];		
					if (c==TRANSPARENCY_COLOR)
						c = bottom_data[ 64*y+x ];
					else if (c==254)
						c = TRANSPARENCY_COLOR;
					*dest_data++ = c;
				}
			break;
		case 1:
			// 
			for (y=0; y<64; y++ )
				for (x=0; x<64; x++ )	{
					c = top_data[ 64*x+(63-y) ];		
					if (c==TRANSPARENCY_COLOR)
						c = bottom_data[ 64*y+x ];
					else if (c==254)
						c = TRANSPARENCY_COLOR;
					*dest_data++ = c;
				}
			break;
		case 2:
			// Normal
			for (y=0; y<64; y++ )
				for (x=0; x<64; x++ )	{
					c = top_data[ 64*(63-y)+(63-x) ];
					if (c==TRANSPARENCY_COLOR)
						c = bottom_data[ 64*y+x ];
					else if (c==254)
						c = TRANSPARENCY_COLOR;
					*dest_data++ = c;
				}
			break;
		case 3:
			// Normal
			for (y=0; y<64; y++ )
				for (x=0; x<64; x++ )	{
					c = top_data[ 64*(63-x)+y  ];
					if (c==TRANSPARENCY_COLOR)
						c = bottom_data[ 64*y+x ];
					else if (c==254)
						c = TRANSPARENCY_COLOR;
					*dest_data++ = c;
				}
			break;
	}
}
