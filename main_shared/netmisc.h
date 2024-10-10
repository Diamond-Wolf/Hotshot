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

#pragma once

#include "misc/types.h"

 //Returns a checksum of a block of memory.
extern uint16_t netmisc_calc_checksum(void* vptr, int len);

#define netmisc_encode_buffer(ptr, offset, buf, length) do { memcpy(&ptr[*offset], buf, length); *offset+=length; } while(0)
#define netmisc_decode_buffer(ptr, offset, buf, length) do { memcpy(buf, &ptr[*offset], length); *offset+=length; } while(0)

#ifdef NETWORK
//Functions for encoding values into a block of memory.
//All values are written little-endian indepenedent of alignment. 
void netmisc_encode_int8(char* ptr, int* offset, char v);
void netmisc_encode_int16(char* ptr, int* offset, short v);
void netmisc_encode_int32(char* ptr, int* offset, int v);
void netmisc_encode_shortpos(char* ptr, int* offset, shortpos* v);
void netmisc_encode_vector(char* ptr, int* offset, vms_vector* vec);
void netmisc_encode_matrix(char* ptr, int* offset, vms_matrix* mat);

//Functions for extracting values from a block of memory, as encoded by the above functions.
void netmisc_decode_int8(char* ptr, int* offset, char* v);
void netmisc_decode_int16(char* ptr, int* offset, short* v);
void netmisc_decode_int32(char* ptr, int* offset, int* v);
void netmisc_decode_shortpos(char* ptr, int* offset, shortpos* v);
void netmisc_decode_vector(char* ptr, int* offset, vms_vector* vec);
void netmisc_decode_matrix(char* ptr, int* offset, vms_matrix* mat);

//Game-specific functions for encoding packet structures.
void netmisc_encode_netgameinfo(char* ptr, int* offset, netgame_info* info);
void netmisc_encode_sequence_packet(char* ptr, int* offset, sequence_packet* info);
void netmisc_encode_frame_info(char* ptr, int* offset, frame_info* info);
void netmisc_encode_endlevel_info(char* ptr, int* offset, endlevel_info* info);
void netmisc_encode_object(char* ptr, int* offset, object* objp);

//Game-specific functions for decoding packet structures.
void netmisc_decode_netgameinfo(char* ptr, int* offset, netgame_info * info);
void netmisc_decode_sequence_packet(char* ptr, int* offset, sequence_packet* info);
void netmisc_decode_frame_info(char* ptr, int* offset, frame_info* info);
void netmisc_decode_endlevel_info(char* ptr, int* offset, endlevel_info* info);
void netmisc_decode_object(char* ptr, int* offset, object* objp);
#endif
