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

#ifdef NETWORK

#define PATCH12

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "inferno.h"
#include "misc/types.h"
#include "misc/args.h"
#include "platform/platform.h"
#include "platform/platform_filesys.h"
#include "platform/posixstub.h"
#include "platform/timer.h"
#include "platform/mono.h"
#include "platform/i_net.h"
#include "newmenu.h"
#include "platform/key.h"
#include "gameseq.h"
#include "object.h"
#include "misc/error.h"
#include "laser.h"
#include "gamesave.h"
#include "gamemine.h"
#include "player.h"
#include "gauges.h"
#include "fireball.h"
#include "network.h"
#include "game.h"
#include "multi.h"
#include "endlevel.h"
#include "2d/palette.h"
#include "cntrlcen.h"
#include "powerup.h"
#include "menu.h"
#include "sounds.h"
#include "stringtable.h"
#include "kmatrix.h"
#include "newdemo.h"
#include "multibot.h"
#include "wall.h"
#include "bm.h"
#include "effects.h"
#include "physics.h"
#include "switch.h"
#include "automap.h"
#include "misc/byteswap.h"
#include "netmisc.h"
#include "kconfig.h"
#include "playsave.h"
#include "misc/rand.h"
#include "gamestat.h"
#include "songs.h"

#define LHX(x)          ((x)*(MenuHires?2:1))
#define LHY(y)          ((y)*(MenuHires?2.4:1))

#define PID_LITE_INFO                           43
#define PID_SEND_ALL_GAMEINFO      44
#define PID_PLAYERSINFO                         45
#define PID_REQUEST                                     46
#define PID_SYNC                                                47
#define PID_PDATA                                               48
#define PID_ADDPLAYER                           49
#define PID_DUMP                                                51
#define PID_ENDLEVEL                                    52
#define PID_QUIT_JOINING                        54
#define PID_OBJECT_DATA                         55
#define PID_GAME_LIST                           56
#define PID_GAME_INFO                           57
#define PID_PING_SEND                           58
#define PID_PING_RETURN                         59
#define PID_GAME_UPDATE                         60
#define PID_ENDLEVEL_SHORT                      61
#define PID_NAKED_PDATA                         62
#define PID_GAME_PLAYERS								63
#define PID_NAMES_RETURN								64

#define NETGAME_ANARCHY                         0
#define NETGAME_TEAM_ANARCHY            1
#define NETGAME_ROBOT_ANARCHY           2
#define NETGAME_COOPERATIVE             3
#define NETGAME_CAPTURE_FLAG            4
#define NETGAME_HOARD            	 	 5
#define NETGAME_TEAM_HOARD					 6

#define NETSECURITY_OFF 0
#define NETSECURITY_WAIT_FOR_PLAYERS 1
#define NETSECURITY_WAIT_FOR_GAMEINFO 2
#define NETSECURITY_WAIT_FOR_SYNC 3

// MWA -- these structures are aligned -- please save me sanity and
// headaches by keeping alignment if these are changed!!!!  Contact
// me for info.

typedef struct endlevel_info_short 
{
	uint8_t                                   type;
	uint8_t                                   player_num;
	int8_t                                    connected;
	uint8_t                                   seconds_left;
} endlevel_info_short;

// WARNING!!! This is the top part of netgame_info...if that struct changes,
//      this struct much change as well.  ie...they are aligned and the join system will
// not work without it.

// MWA  if this structure changes -- please make appropriate changes to receive_netgame_info
// code for macintosh in netmisc.c

typedef struct lite_info 
{
	uint8_t                           type;
	int                             Security;
	char                            game_name[NETGAME_NAME_LEN + 1];
	char                            mission_title[MISSION_NAME_LEN + 1];
	char                            mission_name[9];
	int                             levelnum;
	uint8_t                           gamemode;
	uint8_t                           RefusePlayers;
	uint8_t                           difficulty;
	uint8_t                           game_status;
	uint8_t                           numplayers;
	uint8_t                           max_numplayers;
	uint8_t                           numconnected;
	uint8_t                           game_flags;
	uint8_t                           protocol_version;
	uint8_t                           version_major;
	uint8_t                           version_minor;
	uint8_t                           team_vector;
} lite_info;

// IF YOU CHANGE THE SIZE OF ANY OF THE FOLLOWING STRUCTURES
// MAKE THE MACINTOSH DEFINES BE THE SAME SIZE AS THE PC OR
// I WILL HAVE TO HURT YOU VERY BADLY!!!  -- MWA

#define NETGAME_INFO_SIZE               sizeof(netgame_info)
#define ALLNETPLAYERSINFO_SIZE  sizeof(AllNetPlayers_info)
#define LITE_INFO_SIZE                  sizeof(lite_info)
#define SEQUENCE_PACKET_SIZE    sizeof(sequence_packet)
#define FRAME_INFO_SIZE                 sizeof(frame_info)

#define MAX_ACTIVE_NETGAMES     12

netgame_info Active_games[MAX_ACTIVE_NETGAMES];
AllNetPlayers_info ActiveNetPlayers[MAX_ACTIVE_NETGAMES];
AllNetPlayers_info* TempPlayersInfo, TempPlayersBase;
int NamesInfoSecurity = -1;

// MWAnetgame_info *TempNetInfo; 
netgame_info TempNetInfo;

extern void multi_send_drop_marker(int player, vms_vector position, char messagenum, char text[]);
extern void multi_send_kill_goal_counts();
extern int newmenu_dotiny(const char* title, const char* subtitle, int nitems, newmenu_item* item, void (*subfunction)(int nitems, newmenu_item* items, int* last_key, int citem));

void network_process_naked_pdata(char*, int);
extern void multi_send_robot_controls(char);

int num_active_games = 0;
int PacketsPerSec = 10;
int MaxXDataSize = NET_XDATA_SIZE;

int 	  Netlife_kills = 0, Netlife_killed = 0;

int     Network_debug = 0;
int     Network_active = 0;

int     Network_status = 0;
int     Network_games_changed = 0;

int     Network_socket = 0;
int     Network_allow_socket_changes = 1;

uint16_t Current_Port = IPX_DEFAULT_SOCKET;

int     NetSecurityFlag = NETSECURITY_OFF;
int     NetSecurityNum = 0;
int       Network_sending_extras = 0;
int   	 VerifyPlayerJoined = -1;
int       Player_joining_extras = -1;  // This is so we know who to send 'latecomer' packets to.
											  // We could just send updates to everyone but that kills the sender!   

// For rejoin object syncing

int     Network_rejoined = 0;       // Did WE rejoin this game?
int     Network_new_game = 0;            // Is this the first level of a new game?
int     Network_send_objects = 0;  // Are we in the process of sending objects to a player?
int     Network_send_objnum = -1;   // What object are we sending next?
int     Network_player_added = 0;   // Is this a new player or a returning player?
int     Network_send_object_mode = 0; // What type of objects are we sending, static or dynamic?
sequence_packet Network_player_rejoining; // Who is rejoining now?

fix     LastPacketTime[MAX_PLAYERS]; // For timeouts of idle/crashed players

int     PacketUrgent = 0;
int   NetGameType = 0;
int     TotalMissedPackets = 0, TotalPacketsGot = 0;

frame_info      MySyncPack, UrgentSyncPack;
uint8_t           MySyncPackInitialized = 0;              // Set to 1 if the MySyncPack is zeroed.
uint16_t          my_segments_checksum = 0;

sequence_packet My_Seq;
char WantPlayersInfo = 0;
char WaitingForPlayerInfo = 0;

const char* RankStrings[] = { "(unpatched) ","Cadet ","Ensign ","Lieutenant ","Lt.Commander ",
								"Commander ","Captain ","Vice Admiral ","Admiral ","Demigod " };

extern obj_position Player_init[MAX_PLAYERS];

extern int force_cockpit_redraw;

#define DUMP_CLOSED 0
#define DUMP_FULL 1
#define DUMP_ENDLEVEL 2
#define DUMP_DORK 3
#define DUMP_ABORTED 4
#define DUMP_CONNECTED 5
#define DUMP_LEVEL 6
#define DUMP_KICKED 7

extern uint8_t Version_major, Version_minor;
extern uint8_t SurfingNet;
extern char MaxPowerupsAllowed[MAX_POWERUP_TYPES];
extern char PowerupsInMine[MAX_POWERUP_TYPES];

extern void multi_send_stolen_items();

int network_wait_for_snyc();
extern void multi_send_wall_status(int, uint8_t, uint8_t, uint8_t);
extern void multi_send_wall_status_specific(int, int, uint8_t, uint8_t, uint8_t);
//now what is a function prototype, anyways?
void network_flush();
void network_send_endlevel_sub(int player_num);
void network_listen();
void network_update_netgame(void);
void network_dump_player(char* node, int why);
void network_send_objects(void);
void network_send_rejoin_sync(int player_num);
void network_update_netgame(void);
void network_send_game_info(sequence_packet* their);
void network_send_data(char* ptr, int len, int urgent);
void network_read_sync_packet(netgame_info* sp, int v);
void network_read_pdata_packet(frame_info* pd);
void network_read_object_packet(char* data);
void network_read_endlevel_packet(char* data);
void network_read_endlevel_short_packet(char* data);
void network_ping(uint8_t flag, int pnum);
void network_handle_ping_return(uint8_t pnum);
void network_process_names_return(char* data);
void network_send_player_names(sequence_packet* their);
void SetAllAllowablesTo(int on);
int network_choose_connect();
void network_more_game_options();
int GetMyNetRanking();
void ClipRank(int8_t* rank);
void network_check_for_old_version(char pnum);
int network_wait_for_all_info(int choice);
void network_do_big_wait(int choice);
int network_wait_for_playerinfo();
void network_send_extras();
void network_read_pdata_short_packet(short_frame_info* pd);
void DoRefuseStuff(sequence_packet* their);
void network_process_pdata(char* data);
void network_send_naked_packet(char* buf, short len, int who);
int GetNewPlayerNumber(sequence_packet* their);

extern void game_disable_cheats();

char IWasKicked = 0;
FILE* SendLogFile, * RecieveLogFile;
int TTSent[100], TTRecv[100];

extern int Final_boss_is_dead;

#define NETWORK_OEM 0x10

//[ISB]
//Descent 2 attempts to broadcast for more information, but internet games need to be sent to a specific address.
char Connecting_to_address[4];
bool Connecting_directly = false;

void network_init(void)
{
	// So you want to play a netgame, eh?  Let's a get a few things
	// straight

	int t;
	int save_pnum = Player_num;

	game_disable_cheats();
	IWasKicked = 0;
	Final_boss_is_dead = 0;
	NamesInfoSecurity = -1;

#ifdef NETPROFILING
	OpenSendLog();
	OpenRecieveLog();
#endif

	for (t = 0; t < MAX_POWERUP_TYPES; t++)
	{
		MaxPowerupsAllowed[t] = 0;
		PowerupsInMine[t] = 0;
	}

	TotalMissedPackets = 0; TotalPacketsGot = 0;

	memset(&Netgame, 0, sizeof(netgame_info));
	memset(&NetPlayers, 0, sizeof(AllNetPlayers_info));
	memset(&My_Seq, 0, sizeof(sequence_packet));
	My_Seq.type = PID_REQUEST;
	memcpy(My_Seq.player.callsign, Players[Player_num].callsign, CALLSIGN_LEN + 1);

#if defined (D2_OEM)
	Version_minor |= NETWORK_OEM;
#endif

	My_Seq.player.version_major = Version_major;
	My_Seq.player.version_minor = Version_minor;
	My_Seq.player.rank = GetMyNetRanking();

	memcpy(My_Seq.player.network.ipx.node, NetGetLocalAddress(), 4);
	memcpy(&My_Seq.player.identifier, My_Seq.player.network.ipx.node, sizeof(My_Seq.player.identifier));
	My_Seq.player.computer_type = DOS;

	for (Player_num = 0; Player_num < MAX_NUM_NET_PLAYERS; Player_num++)
		init_player_stats_game();

	Player_num = save_pnum;
	multi_new_game();
	Network_new_game = 1;
	Control_center_destroyed = 0;
	network_flush();

	Netgame.PacketsPerSec = 10;

	if ((t = FindArg("-packets")))
	{
		Netgame.PacketsPerSec = atoi(Args[t + 1]);
		if (Netgame.PacketsPerSec < 1)
			Netgame.PacketsPerSec = 1;
		else if (Netgame.PacketsPerSec > 20)
			Netgame.PacketsPerSec = 20;
		mprintf((0, "Will send %d packets per second", Netgame.PacketsPerSec));
	}
	if (FindArg("-shortpackets"))
	{
		Netgame.ShortPackets = 1;
		mprintf((0, "Will send short packets.\n"));
	}
}

int network_i_am_master(void)
{
	// I am the lowest numbered player in this game?

	int i;

	if (!(Game_mode & GM_NETWORK))
		return (Player_num == 0);

	for (i = 0; i < Player_num; i++)
		if (Players[i].connected)
			return 0;
	return 1;
}

int network_who_is_master(void)
{
	// Who is the master of this game?

	int i;

	if (!(Game_mode & GM_NETWORK))
		return (Player_num == 0);

	for (i = 0; i < N_players; i++)
		if (Players[i].connected)
			return i;
	return Player_num;
}

int network_how_many_connected()
{
	int num = 0, i;

	for (i = 0; i < N_players; i++)
		if (Players[i].connected)
			num++;
	return (num);
}

#define ENDLEVEL_SEND_INTERVAL (F1_0*2)
#define ENDLEVEL_IDLE_TIME      (F1_0*20)

void network_endlevel_poll2(int nitems, newmenu_item* menus, int* key, int citem)
{
	// Polling loop for End-of-level menu

	static fix t1 = 0;
	int i = 0;
	int num_ready = 0;
	int goto_secret = 0;

	menus = menus;
	citem = citem;
	nitems = nitems;
	key = key;

	// Send our endlevel packet at regular intervals

	if (timer_get_approx_seconds() > (t1 + ENDLEVEL_SEND_INTERVAL))
	{
		network_send_endlevel_packet();
		t1 = timer_get_approx_seconds();
	}

	//   mprintf ((0,"Trying to listen!\n"));
	network_listen();

	for (i = 0; i < N_players; i++)
	{
		if ((Players[i].connected != 1) && (Players[i].connected != 5) && (Players[i].connected != 6))
			num_ready++;
		if (Players[i].connected == 4)
			goto_secret = 1;
	}

	if (num_ready == N_players) // All players have checked in or are disconnected
	{
		if (goto_secret)
			*key = -3;
		else
			*key = -2;
	}
}


extern fix StartAbortMenuTime;

void network_endlevel_poll3(int nitems, newmenu_item* menus, int* key, int citem)
{
	// Polling loop for End-of-level menu

	int num_ready = 0, i;

	menus = menus;
	citem = citem;
	nitems = nitems;
	key = key;

	if (timer_get_approx_seconds() > (StartAbortMenuTime + (F1_0 * 8)))
		*key = -2;


	network_listen();


	for (i = 0; i < N_players; i++)
		if ((Players[i].connected != 1) && (Players[i].connected != 5) && (Players[i].connected != 6))
			num_ready++;

	if (num_ready == N_players) // All players have checked in or are disconnected
		*key = -2;

}


int network_endlevel(int* secret)
{
	// Do whatever needs to be done between levels

	int i;

	*secret = 0;

	//network_flush();

	Network_status = NETSTAT_ENDLEVEL; // We are between levels

	network_listen();

	network_send_endlevel_packet();

	for (i = 0; i < N_players; i++)
	{
		LastPacketTime[i] = timer_get_approx_seconds();
	}

	network_send_endlevel_packet();
	network_send_endlevel_packet();
	MySyncPackInitialized = 0;

	network_update_netgame();

	return(0);
}

int can_join_netgame(netgame_info* game, AllNetPlayers_info* people)
{
	// Can this player rejoin a netgame in progress?

	int i, num_players;

	if (game->game_status == NETSTAT_STARTING)
		return 1;

	if (game->game_status != NETSTAT_PLAYING)
	{
		mprintf((0, "Error: Can't join because game_status !=NETSTAT_PLAYING\n"));
		return 0;
	}

	if (game->version_major == 0 && Version_major > 0)
	{
		mprintf((0, "Error:Can't join because version majors don't match!\n"));
		return (0);
	}

	if (game->version_major > 0 && Version_major == 0)
	{
		mprintf((0, "Error:Can't join because version majors2 don't match!\n"));
		return (0);
	}

	// Game is in progress, figure out if this guy can re-join it

	num_players = game->numplayers;

	if (!(game->game_flags & NETGAME_FLAG_CLOSED)) 
	{
		// Look for player that is not connected
		if (game->numconnected == game->max_numplayers)
			return (2);

		//      mprintf ((0,"Refuse = %d\n",game->RefusePlayers));

		if (game->RefusePlayers)
			return (3);

		if (game->numplayers < game->max_numplayers)
			return 1;

		if (game->numconnected < num_players)
			return 1;

	}

	if (people == NULL)
	{
		mprintf((0, "Error! Can't join because people==NULL!\n"));
		return 0;
	}

	// Search to see if we were already in this closed netgame in progress

	for (i = 0; i < num_players; i++)
	{
		if ((!_stricmp(Players[Player_num].callsign, people->players[i].callsign)) && (My_Seq.player.identifier == people->players[i].identifier))
		{
			mprintf((0, "Found ourselves in a closed netgame\n"));
			break;
		}
	}

	if (i != num_players)
		return 1;

	mprintf((0, "Error: Can't join because at end of list!\n"));
	return 0;
}

void network_disconnect_player(int playernum)
{
	// A player has disconnected from the net game, take whatever steps are
	// necessary 

	if (playernum == Player_num)
	{
		Int3(); // Weird, see Rob
		return;
	}

	Players[playernum].connected = 0;
	NetPlayers.players[playernum].connected = 0;
	if (VerifyPlayerJoined == playernum)
		VerifyPlayerJoined = -1;

	//create_player_appearance_effect(&Objects[Players[playernum].objnum]);
	multi_make_player_ghost(playernum);

	if (Newdemo_state == ND_STATE_RECORDING)
		newdemo_record_multi_disconnect(playernum);

	multi_strip_robots(playernum);
}

void network_new_player(sequence_packet* their)
{
	int objnum;
	int pnum;

	pnum = their->player.connected;

	Assert(pnum >= 0);
	Assert(pnum < MaxNumNetPlayers);

	NetGetLastPacketOrigin(their->player.network.ipx.node);

	objnum = Players[pnum].objnum;

	if (Newdemo_state == ND_STATE_RECORDING) 
	{
		int new_player;

		if (pnum == N_players)
			new_player = 1;
		else
			new_player = 0;
		newdemo_record_multi_connect(pnum, new_player, their->player.callsign);
	}

	memcpy(Players[pnum].callsign, their->player.callsign, CALLSIGN_LEN + 1);
	memcpy(NetPlayers.players[pnum].callsign, their->player.callsign, CALLSIGN_LEN + 1);

	ClipRank((int8_t*)&their->player.rank);
	NetPlayers.players[pnum].rank = their->player.rank;
	NetPlayers.players[pnum].version_major = their->player.version_major;
	NetPlayers.players[pnum].version_minor = their->player.version_minor;
	network_check_for_old_version(pnum);

	memcpy(NetPlayers.players[pnum].network.ipx.node, their->player.network.ipx.node, 4);
	NetPlayers.players[pnum].identifier = their->player.identifier;
	NetGetLastPacketOrigin(NetPlayers.players[pnum].network.ipx.node);

	Players[pnum].n_packets_got = 0;
	Players[pnum].connected = 1;
	Players[pnum].net_kills_total = 0;
	Players[pnum].net_killed_total = 0;
	memset(kill_matrix[pnum], 0, MAX_PLAYERS * sizeof(short));
	Players[pnum].score = 0;
	Players[pnum].flags = 0;
	Players[pnum].KillGoalCount = 0;

	if (pnum == N_players)
	{
		N_players++;
		Netgame.numplayers = N_players;
	}

	digi_play_sample(SOUND_HUD_MESSAGE, F1_0);

	ClipRank((int8_t*)&their->player.rank);

	if (FindArg("-norankings"))
		HUD_init_message("'%s' %s\n", their->player.callsign, TXT_JOINING);
	else
		HUD_init_message("%s'%s' %s\n", RankStrings[their->player.rank], their->player.callsign, TXT_JOINING);

	multi_make_ghost_player(pnum);

	multi_send_score();
	multi_sort_kill_list();

	//      create_player_appearance_effect(&Objects[objnum]);
}

char RefuseThisPlayer = 0, WaitForRefuseAnswer = 0, RefuseTeam;
char RefusePlayerName[12];
fix RefuseTimeLimit = 0;

void network_welcome_player(sequence_packet* their)
{
	// Add a player to a game already in progress
	char local_address[6];
	int player_num;
	int i;

	WaitForRefuseAnswer = 0;

	if (FindArg("-NoMatrixCheat"))
	{
		if ((their->player.version_minor & 0x0F) < 3)
		{
			network_dump_player(their->player.network.ipx.node, DUMP_DORK);
			return;
		}
	}

	if (HoardEquipped())
	{
		// If hoard game, and this guy isn't D2 Christmas (v1.2), dump him

		if ((Game_mode & GM_HOARD) && ((their->player.version_minor & 0x0F) < 2))
		{
			network_dump_player(their->player.network.ipx.node, DUMP_DORK);
			return;
		}
	}

	// Don't accept new players if we're ending this level.  Its safe to
	// ignore since they'll request again later

	if ((Endlevel_sequence) || (Control_center_destroyed))
	{
		mprintf((0, "Ignored request from new player to join during endgame.\n"));
		network_dump_player(their->player.network.ipx.node, DUMP_ENDLEVEL);
		return;
	}

	if (Network_send_objects || Network_sending_extras)
	{
		// Ignore silently, we're already responding to someone and we can't
		// do more than one person at a time.  If we don't dump them they will
		// re-request in a few seconds.
		return;
	}

	if (their->player.connected != Current_level_num)
	{
		mprintf((0, "Dumping player due to old level number.\n"));
		network_dump_player(their->player.network.ipx.node, DUMP_LEVEL);
		return;
	}

	player_num = -1;
	memset(&Network_player_rejoining, 0, sizeof(sequence_packet));
	Network_player_added = 0;

	//memcpy(local_address, their->player.network.ipx.node, 4);
	NetGetLastPacketOrigin(local_address);
	//[ISB] This needs to be done to ensure object sync is broadcast to the right address, since this is where objects will be sent to. 
	memcpy(their->player.network.ipx.node, local_address, 4);

	for (i = 0; i < N_players; i++)
	{
		if ((!_stricmp(Players[i].callsign, their->player.callsign)) && (!memcmp(Players[i].net_address, local_address, 4)))
		{
			player_num = i;
			break;
		}
	}

	if (player_num == -1)
	{
		// Player is new to this game

		if (!(Netgame.game_flags & NETGAME_FLAG_CLOSED) && (N_players < MaxNumNetPlayers))
		{
			// Add player in an open slot, game not full yet

			player_num = N_players;
			Network_player_added = 1;
		}
		else if (Netgame.game_flags & NETGAME_FLAG_CLOSED)
		{
			// Slots are open but game is closed
			network_dump_player(their->player.network.ipx.node, DUMP_CLOSED);
			return;
		}
		else
		{
			// Slots are full but game is open, see if anyone is
			// disconnected and replace the oldest player with this new one

			int oldest_player = -1;
			fix oldest_time = timer_get_approx_seconds();

			Assert(N_players == MaxNumNetPlayers);

			for (i = 0; i < N_players; i++)
			{
				if ((!Players[i].connected) && (LastPacketTime[i] < oldest_time))
				{
					oldest_time = LastPacketTime[i];
					oldest_player = i;
				}
			}

			if (oldest_player == -1)
			{
				// Everyone is still connected 
				network_dump_player(their->player.network.ipx.node, DUMP_FULL);
				return;
			}
			else
			{
				// Found a slot!

				player_num = oldest_player;
				Network_player_added = 1;
			}
		}
	}
	else
	{
		// Player is reconnecting

		if (Players[player_num].connected)
		{
			mprintf((0, "Extra REQUEST from player ignored.\n"));
			return;
		}

		if (Newdemo_state == ND_STATE_RECORDING)
			newdemo_record_multi_reconnect(player_num);

		Network_player_added = 0;

		digi_play_sample(SOUND_HUD_MESSAGE, F1_0);

		if (FindArg("-norankings"))
			HUD_init_message("'%s' %s", Players[player_num].callsign, TXT_REJOIN);
		else
			HUD_init_message("%s'%s' %s", RankStrings[NetPlayers.players[player_num].rank], Players[player_num].callsign, TXT_REJOIN);
	}

	Players[player_num].KillGoalCount = 0;

	// Send updated Objects data to the new/returning player


	Network_player_rejoining = *their;
	Network_player_rejoining.player.connected = player_num;
	Network_send_objects = 1;
	Network_send_objnum = -1;

	network_send_objects();
}

int network_objnum_is_past(int objnum)
{
	// determine whether or not a given object number has already been sent
	// to a re-joining player.

	int player_num = Network_player_rejoining.player.connected;
	int obj_mode = !((object_owner[objnum] == -1) || (object_owner[objnum] == player_num));

	if (!Network_send_objects)
		return 0; // We're not sending objects to a new player

	if (obj_mode > Network_send_object_mode)
		return 0;
	else if (obj_mode < Network_send_object_mode)
		return 1;
	else if (objnum < Network_send_objnum)
		return 1;
	else
		return 0;
}

#define OBJ_PACKETS_PER_FRAME 1
extern void multi_send_active_door(char);
extern void multi_send_door_open_specific(int, int, int, uint8_t);

void network_send_door_updates(int pnum)
{
	// Send door status when new player joins

	int i;

	//   Assert (pnum>-1 && pnum<N_players);

	for (i = 0; i < Walls.size(); i++)
	{
		if ((Walls[i].type == WALL_DOOR) && ((Walls[i].state == WALL_DOOR_OPENING) || (Walls[i].state == WALL_DOOR_WAITING) || (Walls[i].state == WALL_DOOR_OPEN)))
			multi_send_door_open_specific(pnum, Walls[i].segnum, Walls[i].sidenum, Walls[i].flags);
		else if ((Walls[i].type == WALL_BLASTABLE) && (Walls[i].flags & WALL_BLASTED))
			multi_send_door_open_specific(pnum, Walls[i].segnum, Walls[i].sidenum, Walls[i].flags);
		else if ((Walls[i].type == WALL_BLASTABLE) && (Walls[i].hps != WALL_HPS))
			multi_send_hostage_door_status(i);
		else
			multi_send_wall_status_specific(pnum, i, Walls[i].type, Walls[i].flags, Walls[i].state);
	}
}

extern vms_vector MarkerPoint[];
void network_send_markers()
{
	// send marker positions/text to new player
	int i;

	for (i = 0; i < 8; i++)
	{
		if (MarkerObject[(i * 2)] != -1)
			multi_send_drop_marker(i, MarkerPoint[(i * 2)], 0, MarkerMessage[i * 2]);
		if (MarkerObject[(i * 2) + 1] != -1)
			multi_send_drop_marker(i, MarkerPoint[(i * 2) + 1], 1, MarkerMessage[(i * 2) + 1]);
	}
}

void network_process_monitor_vector(int vector)
{
	int i, j;
	int count = 0;
	segment* seg;

	for (i = 0; i <= Highest_segment_index; i++)
	{
		int tm, ec, bm;
		seg = &Segments[i];
		for (j = 0; j < 6; j++)
		{
			if (((tm = seg->sides[j].tmap_num2) != 0) &&
				((ec = activeBMTable->tmaps[tm & 0x3fff].eclip_num) != -1) &&
				((bm = activeBMTable->eclips[ec].dest_bm_num) != -1))
			{
				if (vector & (1 << count))
				{
					seg->sides[j].tmap_num2 = bm | (tm & 0xc000);
					//      mprintf((0, "Monitor %d blown up.\n", count));
				}
				//else
				  //    mprintf((0, "Monitor %d intact.\n", count));
				count++;
				Assert(count < 32);
			}
		}
	}
}

int network_create_monitor_vector(void)
{
	int i, j, k;
	int num_blown_bitmaps = 0;
	int monitor_num = 0;
#define NUM_BLOWN_BITMAPS 20
	int blown_bitmaps[NUM_BLOWN_BITMAPS];
	int vector = 0;
	segment* seg;

	for (i = 0; i < activeBMTable->eclips.size(); i++)
	{
		if (activeBMTable->eclips[i].dest_bm_num > 0)
		{
			for (j = 0; j < num_blown_bitmaps; j++)
				if (blown_bitmaps[j] == activeBMTable->eclips[i].dest_bm_num)
					break;
			if (j == num_blown_bitmaps) {
				blown_bitmaps[num_blown_bitmaps++] = activeBMTable->eclips[i].dest_bm_num;
				Assert(num_blown_bitmaps < NUM_BLOWN_BITMAPS);
			}
		}
	}

	//	for (i = 0; i < num_blown_bitmaps; i++)
	//		mprintf((0, "Blown bitmap #%d = %d.\n", i, blown_bitmaps[i]));

	for (i = 0; i <= Highest_segment_index; i++)
	{
		int tm, ec;
		seg = &Segments[i];
		for (j = 0; j < 6; j++)
		{
			if ((tm = seg->sides[j].tmap_num2) != 0)
			{
				if (((ec = activeBMTable->tmaps[tm & 0x3fff].eclip_num) != -1) &&
					(activeBMTable->eclips[ec].dest_bm_num != -1))
				{
					//      mprintf((0, "Monitor %d intact.\n", monitor_num));
					monitor_num++;
					Assert(monitor_num < 32);
				}
				else
				{
					for (k = 0; k < num_blown_bitmaps; k++)
					{
						if ((tm & 0x3fff) == blown_bitmaps[k])
						{
							//mprintf((0, "Monitor %d destroyed.\n", monitor_num));
							vector |= (1 << monitor_num);
							monitor_num++;
							Assert(monitor_num < 32);
							break;
						}
					}
				}
			}
		}
	}
	//	mprintf((0, "Final monitor vector %x.\n", vector));
	return(vector);
}

void network_stop_resync(sequence_packet* their)
{
	NetGetLastPacketOrigin(their->player.network.ipx.node);

	if (Network_player_rejoining.player.identifier == their->player.identifier &&
		(!_stricmp(Network_player_rejoining.player.callsign, their->player.callsign)))
	{
		mprintf((0, "Aborting resync for player %s.\n", their->player.callsign));
		Network_send_objects = 0;
		Network_sending_extras = 0;
		Network_rejoined = 0;
		Player_joining_extras = -1;
		Network_send_objnum = -1;
	}
}

char object_buffer[IPX_MAX_DATA_SIZE];

void network_send_objects(void)
{
	short remote_objnum;
	int8_t owner;
	int loc, i, h;

	static int obj_count = 0;
	static int frame_num = 0;

	int obj_count_frame = 0;
	int player_num = Network_player_rejoining.player.connected;

	// Send clear objects array trigger and send player num

	Assert(Network_send_objects != 0);
	Assert(player_num >= 0);
	Assert(player_num < MaxNumNetPlayers);

	if (Endlevel_sequence || Control_center_destroyed)
	{
		// Endlevel started before we finished sending the goods, we'll
		// have to stop and try again after the level.

		network_dump_player(Network_player_rejoining.player.network.ipx.node, DUMP_ENDLEVEL);
		Network_send_objects = 0;
		return;
	}

	for (h = 0; h < OBJ_PACKETS_PER_FRAME; h++) // Do more than 1 per frame, try to speed it up without
															  // over-stressing the receiver.
	{
		obj_count_frame = 0;
		memset(object_buffer, 0, IPX_MAX_DATA_SIZE);
		object_buffer[0] = PID_OBJECT_DATA;
		loc = 3;

		if (Network_send_objnum == -1)
		{
			obj_count = 0;
			Network_send_object_mode = 0;
			//       mprintf((0, "Sending object array to player %d.\n", player_num));
			//*(short*)(object_buffer + loc) = (-1);        loc += 2;
			netmisc_encode_int16(object_buffer, &loc, -1);
			//object_buffer[loc] = player_num;                loc += 1;
			netmisc_encode_int8(object_buffer, &loc, player_num);
			loc += 2; // Placeholder for remote_objnum, not used here
			Network_send_objnum = 0;
			obj_count_frame = 1;
			frame_num = 0;
		}

		for (i = Network_send_objnum; i <= Highest_object_index; i++)
		{
			if ((Objects[i].type != OBJ_POWERUP) && (Objects[i].type != OBJ_PLAYER) &&
				(Objects[i].type != OBJ_CNTRLCEN) && (Objects[i].type != OBJ_GHOST) &&
				(Objects[i].type != OBJ_ROBOT) && (Objects[i].type != OBJ_HOSTAGE) &&
				!(Objects[i].type == OBJ_WEAPON && Objects[i].id == PMINE_ID))
				continue;
			if ((Network_send_object_mode == 0) && ((object_owner[i] != -1) && (object_owner[i] != player_num)))
				continue;
			if ((Network_send_object_mode == 1) && ((object_owner[i] == -1) || (object_owner[i] == player_num)))
				continue;

			if (((IPX_MAX_DATA_SIZE - 1) - loc) < (sizeof(object) + 5))
				break; // Not enough room for another object

			obj_count_frame++;
			obj_count++;

			remote_objnum = objnum_local_to_remote((short)i, &owner);
			Assert(owner == object_owner[i]);

			netmisc_encode_int16(object_buffer, &loc, i);
			netmisc_encode_int8(object_buffer, &loc, owner);
			netmisc_encode_int16(object_buffer, &loc, remote_objnum);
			netmisc_encode_object(object_buffer, &loc, &Objects[i]);
			//mprintf((0, "..packing object %d, remote %d\n", i, remote_objnum));
		}

		if (obj_count_frame) // Send any objects we've buffered
		{
			frame_num++;

			Network_send_objnum = i;
			object_buffer[1] = obj_count_frame; 
			object_buffer[2] = frame_num;
			//mprintf((0, "Object packet %d contains %d objects.\n", frame_num, obj_count_frame));

			Assert(loc <= IPX_MAX_DATA_SIZE);

			NetSendInternetworkPacket((char*)object_buffer, loc, Network_player_rejoining.player.network.ipx.node);
		}

		if (i > Highest_object_index)
		{
			if (Network_send_object_mode == 0)
			{
				Network_send_objnum = 0;
				Network_send_object_mode = 1; // go to next mode
			}
			else
			{
				Assert(Network_send_object_mode == 1);

				frame_num++;
				// Send count so other side can make sure he got them all
//                              mprintf((0, "Sending end marker in packet #%d.\n", frame_num));
				mprintf((0, "Sent %d objects.\n", obj_count));
				object_buffer[0] = PID_OBJECT_DATA;
				object_buffer[1] = 1;
				object_buffer[2] = frame_num;
				loc = 3;
				netmisc_encode_int16(object_buffer, &loc, -2); loc++;
				netmisc_encode_int16(object_buffer, &loc, obj_count);
				NetSendInternetworkPacket((char*)object_buffer, 8, Network_player_rejoining.player.network.ipx.node);

				// Send sync packet which tells the player who he is and to start!
				network_send_rejoin_sync(player_num);
				mprintf((0, "VerfiyPlayerJoined is now set to %d\n", player_num));
				VerifyPlayerJoined = player_num;

				// Turn off send object mode
				Network_send_objnum = -1;
				Network_send_objects = 0;
				obj_count = 0;

				//if (!network_i_am_master ())
				// Int3();  // Bad!! Get Jason.  Someone goofy is trying to get ahold of the game!

				Network_sending_extras = 40; // start to send extras
				Player_joining_extras = player_num;

				return;
			} // mode == 1;
		} // i > Highest_object_index
	} // For PACKETS_PER_FRAME
}

extern void multi_send_powerup_update();

void network_send_rejoin_sync(int player_num)
{
	int i, j;

	Players[player_num].connected = 1; // connect the new guy
	LastPacketTime[player_num] = timer_get_approx_seconds();

	if (Endlevel_sequence || Control_center_destroyed)
	{
		// Endlevel started before we finished sending the goods, we'll
		// have to stop and try again after the level.

		network_dump_player(Network_player_rejoining.player.network.ipx.node, DUMP_ENDLEVEL);
		Network_send_objects = 0;
		Network_sending_extras = 0;
		return;
	}

	if (Network_player_added)
	{
		Network_player_rejoining.type = PID_ADDPLAYER;
		Network_player_rejoining.player.connected = player_num;
		network_new_player(&Network_player_rejoining);

		for (i = 0; i < N_players; i++)
		{
			if ((i != player_num) && (i != Player_num) && (Players[i].connected))
				NetSendPacket((char*)&Network_player_rejoining, sizeof(sequence_packet), NetPlayers.players[i].network.ipx.node, Players[i].net_address);
		}
	}

	// Send sync packet to the new guy

	network_update_netgame();

	// Fill in the kill list
	for (j = 0; j < MAX_PLAYERS; j++)
	{
		for (i = 0; i < MAX_PLAYERS; i++)
			Netgame.kills[j][i] = kill_matrix[j][i];
		Netgame.killed[j] = Players[j].net_killed_total;
		Netgame.player_kills[j] = Players[j].net_kills_total;
		Netgame.player_score[j] = Players[j].score;
	}

	Netgame.level_time = Players[Player_num].time_level;
	Netgame.monitor_vector = network_create_monitor_vector();

	mprintf((0, "Sending rejoin sync packet!!!\n"));
    
	NetSendInternetworkPacket((char*)&Netgame, sizeof(netgame_info), Network_player_rejoining.player.network.ipx.node);
	NetSendInternetworkPacket((char*)&NetPlayers, sizeof(AllNetPlayers_info), Network_player_rejoining.player.network.ipx.node);

	return;
}

void resend_sync_due_to_packet_loss_for_allender()
{
	int i, j;

	mprintf((0, "I'm resending a sync packet! VPJ=%d\n", VerifyPlayerJoined));

	network_update_netgame();

	// Fill in the kill list
	for (j = 0; j < MAX_PLAYERS; j++)
	{
		for (i = 0; i < MAX_PLAYERS; i++)
			Netgame.kills[j][i] = kill_matrix[j][i];
		Netgame.killed[j] = Players[j].net_killed_total;
		Netgame.player_kills[j] = Players[j].net_kills_total;
		Netgame.player_score[j] = Players[j].score;
	}

	Netgame.level_time = Players[Player_num].time_level;
	Netgame.monitor_vector = network_create_monitor_vector();
 
	NetSendInternetworkPacket((char*)&Netgame, sizeof(netgame_info), Network_player_rejoining.player.network.ipx.node);
	NetSendInternetworkPacket((char*)&NetPlayers, sizeof(AllNetPlayers_info), Network_player_rejoining.player.network.ipx.node);
}

char* network_get_player_name(int objnum)
{
	if (objnum < 0) return NULL;
	if (Objects[objnum].type != OBJ_PLAYER) return NULL;
	if (Objects[objnum].id >= MAX_PLAYERS) return NULL;
	if (Objects[objnum].id >= N_players) return NULL;

	return Players[Objects[objnum].id].callsign;
}

void network_add_player(sequence_packet* p)
{
	int i;

	mprintf((0, "Got add player request!\n"));
	//[ISB] The protocol isn't always reliable at specifying an address, and this won't work over the internet either.
	//Always replace what address this player said they have with the address they actually sent from.
	//Port (currently saved in server...) should be trustable. 
	NetGetLastPacketOrigin(p->player.network.ipx.node);

	for (i = 0; i < N_players; i++)
	{
		if (NetPlayers.players[i].identifier == p->player.identifier)
			return;		// already got them
	}

	memcpy(NetPlayers.players[N_players].network.ipx.node, p->player.network.ipx.node, 4);
	NetPlayers.players[N_players].identifier = p->player.identifier;

	ClipRank((int8_t*)&p->player.rank);

	memcpy(NetPlayers.players[N_players].callsign, p->player.callsign, CALLSIGN_LEN + 1);
	NetPlayers.players[N_players].version_major = p->player.version_major;
	NetPlayers.players[N_players].version_minor = p->player.version_minor;
	NetPlayers.players[N_players].rank = p->player.rank;
	NetPlayers.players[N_players].connected = 1;
	network_check_for_old_version(N_players);

	Players[N_players].KillGoalCount = 0;
	Players[N_players].connected = 1;
	LastPacketTime[N_players] = timer_get_approx_seconds();
	N_players++;
	Netgame.numplayers = N_players;

	// Broadcast updated info
	mprintf((0, "sending_game_info!\n"));
	network_send_game_info(NULL);
}

// One of the players decided not to join the game
void network_remove_player(sequence_packet* p)
{
	int i, pn;

	NetGetLastPacketOrigin(p->player.network.ipx.node);

	pn = -1;
	for (i = 0; i < N_players; i++)
	{
		if (NetPlayers.players[i].identifier == p->player.identifier) 
		{
			pn = i;
			break;
		}
	}

	if (pn < 0) return;

	for (i = pn; i < N_players - 1; i++)
	{
		memcpy(NetPlayers.players[i].network.ipx.node, NetPlayers.players[i + 1].network.ipx.node, 4);
		NetPlayers.players[i].identifier = NetPlayers.players[i + 1].identifier;
		memcpy(NetPlayers.players[i].callsign, NetPlayers.players[i + 1].callsign, CALLSIGN_LEN + 1);
		NetPlayers.players[i].version_major = NetPlayers.players[i + 1].version_major;
		NetPlayers.players[i].version_minor = NetPlayers.players[i + 1].version_minor;

		NetPlayers.players[i].rank = NetPlayers.players[i + 1].rank;
		ClipRank((int8_t*)&NetPlayers.players[i].rank);
		network_check_for_old_version(i);
	}

	N_players--;
	Netgame.numplayers = N_players;

	// Broadcast new info
	network_send_game_info(NULL);
}

void network_dump_player(char* node, int why)
{
	// Inform player that he was not chosen for the netgame

	sequence_packet temp;

	temp.type = PID_DUMP;
	memcpy(temp.player.callsign, Players[Player_num].callsign, CALLSIGN_LEN + 1);
	temp.player.connected = why;
	NetSendInternetworkPacket((char*)&temp, sizeof(sequence_packet), node);
}

void network_send_game_list_request()
{
	// Send a broadcast request for game info

	sequence_packet me;

	mprintf((0, "Sending game_list request.\n"));
	me.type = PID_GAME_LIST;
	memcpy(me.player.callsign, Players[Player_num].callsign, CALLSIGN_LEN + 1);
	memcpy(me.player.network.ipx.node, NetGetLocalAddress(), 4);
	me.player.identifier = My_Seq.player.identifier;
    
	NetSendBroadcastPacket((char*)&me, sizeof(sequence_packet));
}

void network_send_game_list_request_to(char* address)
{
	// Send a broadcast request for game info

	sequence_packet me;

	mprintf((0, "Sending game_list request.\n"));
	me.type = PID_GAME_LIST;
	memcpy(me.player.callsign, Players[Player_num].callsign, CALLSIGN_LEN + 1);
	memcpy(me.player.network.ipx.node, NetGetLocalAddress(), 4);
	me.player.identifier = My_Seq.player.identifier;

	NetSendInternetworkPacket((char*)&me, sizeof(sequence_packet), address);
}

void network_send_all_info_request(char type, int which_security)
{
	// Send a broadcast request for game info

	sequence_packet me;

	mprintf((0, "Sending all_info request.\n"));

	me.Security = which_security;
	me.type = type;
	memcpy(me.player.callsign, Players[Player_num].callsign, CALLSIGN_LEN + 1);
	memcpy(me.player.network.ipx.node, NetGetLocalAddress(), 4);
	me.player.identifier = My_Seq.player.identifier;
  
	//hack, this needs to be cleaned up so all routes are using this. 
	if (!Connecting_directly)
		NetSendBroadcastPacket((char*)&me, sizeof(sequence_packet));
	else
		NetSendInternetworkPacket((char*)&me, sizeof(sequence_packet), Connecting_to_address);
}

void network_update_netgame(void)
{
	// Update the netgame struct with current game variables

	int i, j;

	Netgame.numconnected = 0;
	for (i = 0; i < N_players; i++)
		if (Players[i].connected)
			Netgame.numconnected++;

	// This is great: D2 1.0 and 1.1 ignore upper part of the game_flags field of
	//	the lite_info struct when you're sitting on the join netgame screen.  We can
	//	"sneak" Hoard information into this field.  This is better than sending 
	//	another packet that could be lost in transit.

	if (HoardEquipped())
	{
		if (Game_mode & GM_HOARD)
		{
			Netgame.game_flags |= NETGAME_FLAG_HOARD;
			if (Game_mode & GM_TEAM)
				Netgame.game_flags |= NETGAME_FLAG_TEAM_HOARD;
		}
	}

	if (Network_status == NETSTAT_STARTING)
		return;

	Netgame.numplayers = N_players;
	Netgame.game_status = Network_status;
	Netgame.max_numplayers = MaxNumNetPlayers;

	for (i = 0; i < MAX_NUM_NET_PLAYERS; i++)
	{
		NetPlayers.players[i].connected = Players[i].connected;
		for (j = 0; j < MAX_NUM_NET_PLAYERS; j++)
			Netgame.kills[i][j] = kill_matrix[i][j];

		Netgame.killed[i] = Players[i].net_killed_total;
		Netgame.player_kills[i] = Players[i].net_kills_total;
		Netgame.player_score[i] = Players[i].score;
		Netgame.player_flags[i] = (Players[i].flags & (PLAYER_FLAGS_BLUE_KEY | PLAYER_FLAGS_RED_KEY | PLAYER_FLAGS_GOLD_KEY));
	}
	Netgame.team_kills[0] = team_kills[0];
	Netgame.team_kills[1] = team_kills[1];
	Netgame.levelnum = Current_level_num;

}

void network_send_endlevel_short_sub(int from_player_num, int to_player)
{
	endlevel_info_short end;

	// Send an endlevel packet for a player
	end.type = PID_ENDLEVEL_SHORT;
	end.player_num = from_player_num;
	end.connected = Players[from_player_num].connected;
	end.seconds_left = Countdown_seconds_left;

	if (Players[from_player_num].connected == 1) // Still playing
	{
		Assert(Control_center_destroyed);
	}

	if ((to_player != Player_num) && (to_player != from_player_num) && (Players[to_player].connected))
	{
		mprintf((0, "Sending short endlevel packet to %s.\n", Players[to_player].callsign));
		NetSendPacket((char*)&end, sizeof(endlevel_info_short), NetPlayers.players[to_player].network.ipx.node, Players[to_player].net_address);
	}
}

void network_send_endlevel_sub(int player_num)
{
	endlevel_info end;
	int i;

	// Send an endlevel packet for a player
	end.type = PID_ENDLEVEL;
	end.player_num = player_num;
	end.connected = Players[player_num].connected;
	end.kills = (Players[player_num].net_kills_total);
	end.killed = (Players[player_num].net_killed_total);
	memcpy(end.kill_matrix, kill_matrix[player_num], MAX_PLAYERS * sizeof(short));

	if (Players[player_num].connected == 1) // Still playing
	{
		Assert(Control_center_destroyed);
		end.seconds_left = Countdown_seconds_left;
	}

	//      mprintf((0, "Sending endlevel packet.\n"));

	for (i = 0; i < N_players; i++)
	{
		if ((i != Player_num) && (i != player_num) && (Players[i].connected)) 
		{
			if (Players[i].connected == 1)
				network_send_endlevel_short_sub(player_num, i);
			else
			{
				NetSendPacket((char*)&end, sizeof(endlevel_info), NetPlayers.players[i].network.ipx.node, Players[i].net_address);
			}
		}
	}
}

void network_send_endlevel_packet(void)
{
	// Send an updated endlevel status to other hosts
	network_send_endlevel_sub(Player_num);
}

extern fix ThisLevelTime;

void network_send_game_info(sequence_packet* their)
{
	// Send game info to someone who requested it

	char old_type, old_status;
	fix timevar;
	int i;

	mprintf((0, "Sending game info.\n"));

	network_update_netgame(); // Update the values in the netgame struct

	old_type = Netgame.type;
	old_status = Netgame.game_status;

	Netgame.type = PID_GAME_INFO;
	NetPlayers.type = PID_PLAYERSINFO;

	NetPlayers.Security = Netgame.Security;
	Netgame.version_major = Version_major;
	Netgame.version_minor = Version_minor;

	if (Endlevel_sequence || Control_center_destroyed)
		Netgame.game_status = NETSTAT_ENDLEVEL;

	if (Netgame.PlayTimeAllowed)
	{
		timevar = i2f(Netgame.PlayTimeAllowed * 5 * 60);
		i = f2i(timevar - ThisLevelTime);
		if (i < 30)
			Netgame.game_status = NETSTAT_ENDLEVEL;
	}

	if (!their) 
	{
		//Can't just broadcast this since all our clients are hypothetically on different networks.
		//A mess, but it'll hopefully work. 
		for (i = 0; i < N_players; i++)
		{
			NetSendInternetworkPacket((char*)&Netgame, sizeof(netgame_info), NetPlayers.players[i].network.ipx.node);
			NetSendInternetworkPacket((char*)&NetPlayers, sizeof(AllNetPlayers_info), NetPlayers.players[i].network.ipx.node);
		}
	}
	else 
	{ 
		NetGetLastPacketOrigin(their->player.network.ipx.node); //Ensure we're sending to the correct place.
		NetSendInternetworkPacket((char*)&Netgame, sizeof(netgame_info), their->player.network.ipx.node);
		NetSendInternetworkPacket((char*)&NetPlayers, sizeof(AllNetPlayers_info), their->player.network.ipx.node);
	}

	Netgame.type = old_type;
	Netgame.game_status = old_status;
}

void network_send_lite_info(sequence_packet* their)
{
	char source_address[4];
	NetGetLastPacketOrigin(source_address);
	// Send game info to someone who requested it

	char old_type, old_status, oldstatus;

	mprintf((0, "Sending lite game info.\n"));

	network_update_netgame(); // Update the values in the netgame struct

	old_type = Netgame.type;
	old_status = Netgame.game_status;

	Netgame.type = PID_LITE_INFO;

	if (Endlevel_sequence || Control_center_destroyed)
		Netgame.game_status = NETSTAT_ENDLEVEL;

	// If hoard mode, make this game look closed even if it isn't
	if (HoardEquipped())
	{
		if (Game_mode & GM_HOARD)
		{
			oldstatus = Netgame.game_status;
			Netgame.game_status = NETSTAT_ENDLEVEL;
			Netgame.gamemode = NETGAME_CAPTURE_FLAG;
			if (oldstatus == NETSTAT_ENDLEVEL)
				Netgame.game_flags |= NETGAME_FLAG_REALLY_ENDLEVEL;
			if (oldstatus == NETSTAT_STARTING)
				Netgame.game_flags |= NETGAME_FLAG_REALLY_FORMING;
		}
	}

	if (!their) 
		NetSendBroadcastPacket((char*)&Netgame, sizeof(lite_info));
	else 
		NetSendInternetworkPacket((char*)&Netgame, sizeof(lite_info), source_address);

	//  Restore the pre-hoard mode
	if (HoardEquipped())
	{
		if (Game_mode & GM_HOARD)
		{
			if (!(Game_mode & GM_TEAM))
				Netgame.gamemode = NETGAME_HOARD;
			else
				Netgame.gamemode = NETGAME_TEAM_HOARD;
			Netgame.game_flags &= ~NETGAME_FLAG_REALLY_ENDLEVEL;
			Netgame.game_flags &= ~NETGAME_FLAG_REALLY_FORMING;
			Netgame.game_flags &= ~NETGAME_FLAG_TEAM_HOARD;
		}
	}

	Netgame.type = old_type;
	Netgame.game_status = old_status;
}

void network_send_netgame_update()
{
	// Send game info to someone who requested it

	char old_type, old_status, i;

	mprintf((0, "Sending updated game info.\n"));

	network_update_netgame(); // Update the values in the netgame struct

	old_type = Netgame.type;
	old_status = Netgame.game_status;

	Netgame.type = PID_GAME_UPDATE;

	if (Endlevel_sequence || Control_center_destroyed)
		Netgame.game_status = NETSTAT_ENDLEVEL;

	for (i = 0; i < N_players; i++) 
	{
		if ((Players[i].connected) && (i != Player_num))
			NetSendPacket((char*)&Netgame, sizeof(lite_info), NetPlayers.players[i].network.ipx.node, Players[i].net_address);
	}

	Netgame.type = old_type;
	Netgame.game_status = old_status;
}

int network_send_request(void)
{
	// Send a request to join a game 'Netgame'.  Returns 0 if we can join this
	// game, non-zero if there is some problem.
	int i;

	if (Netgame.numplayers < 1)
		return 1;

	for (i = 0; i < MAX_NUM_NET_PLAYERS; i++)
		if (NetPlayers.players[i].connected)
			break;

	Assert(i < MAX_NUM_NET_PLAYERS);

	mprintf((0, "Sending game enroll request to player %d (%s). Node=%x Level=%d\n", i, Players[i].callsign, NetPlayers.players[i].network.ipx.node, Netgame.levelnum));

	//      segments_checksum = netmisc_calc_checksum( Segments, sizeof(segment)*(Highest_segment_index+1) );       

	My_Seq.type = PID_REQUEST;
	My_Seq.player.connected = Current_level_num;

	NetSendInternetworkPacket((char*)&My_Seq, sizeof(sequence_packet), NetPlayers.players[i].network.ipx.node);

	return i;
}

int SecurityCheck = 0;

void network_process_gameinfo(char* data)
{
	int i, j;
	netgame_info* newgame = (netgame_info*)data;
	char origin_address[4];

	WaitingForPlayerInfo = 0;

	if (newgame->Security != TempPlayersInfo->Security)
	{
		Int3();     // Get Jason
		return;     // If this first half doesn't go with the second half
	}

	Network_games_changed = 1;

	//mprintf((0, "Got game data for game %s.\n", new->game_name));
	NetGetLastPacketOrigin(origin_address);
	Assert(TempPlayersInfo != NULL);

	for (i = 0; i < num_active_games; i++)
	{
		//mprintf ((0,"GAMEINFO: Game %d is %s!\n",i,Active_games[i].game_name));

		if (!_stricmp(Active_games[i].game_name, newgame->game_name) &&
			Active_games[i].Security == newgame->Security)
			break;
	}

	if (i == MAX_ACTIVE_NETGAMES)
	{
		mprintf((0, "Too many netgames.\n"));
		return;
	}

	// MWA  memcpy(&Active_games[i], data, sizeof(netgame_info));
	memcpy(&Active_games[i], (char*)newgame, sizeof(netgame_info));
	memcpy(&ActiveNetPlayers[i], TempPlayersInfo, sizeof(AllNetPlayers_info));

	if (SecurityCheck)
		if (Active_games[i].Security == SecurityCheck)
			SecurityCheck = -1;

	//mprintf ((0,"Recieved %d unique games...\n",i));

	if (i == num_active_games)
		num_active_games++;

	if (Active_games[i].numplayers == 0)
	{
		mprintf((0, "DELETING THIS GAME!!!\n"));
		// Delete this game
		for (j = i; j < num_active_games - 1; j++)
		{
			memcpy(&Active_games[j], &Active_games[j + 1], sizeof(netgame_info));
			memcpy(&ActiveNetPlayers[j], &ActiveNetPlayers[j + 1], sizeof(AllNetPlayers_info));
		}
		num_active_games--;
		SecurityCheck = 0;
	}
}

void network_process_lite_info(char* data)
{
	int i, j;
	lite_info* newgame = (lite_info*)data;

	Network_games_changed = 1;

	//mprintf((0, "Got game data for game %s.\n", new->game_name));

	for (i = 0; i < num_active_games; i++)
	{
		//mprintf ((0,"GAMEINFO: Game %d is %s!\n",i,Active_games[i].game_name));

		if ((!_stricmp(Active_games[i].game_name, newgame->game_name)) &&
			Active_games[i].Security == newgame->Security)
			break;
	}

	if (i == MAX_ACTIVE_NETGAMES)
	{
		mprintf((0, "Too many netgames.\n"));
		return;
	}

	memcpy(&Active_games[i], (char*)newgame, sizeof(lite_info));

	// See if this is really a Hoard game
	// If so, adjust all the data accordingly

	if (HoardEquipped())
	{
		if (Active_games[i].game_flags & NETGAME_FLAG_HOARD)
		{
			Active_games[i].gamemode = NETGAME_HOARD;
			Active_games[i].game_status = NETSTAT_PLAYING;

			if (Active_games[i].game_flags & NETGAME_FLAG_TEAM_HOARD)
				Active_games[i].gamemode = NETGAME_TEAM_HOARD;
			if (Active_games[i].game_flags & NETGAME_FLAG_REALLY_ENDLEVEL)
				Active_games[i].game_status = NETSTAT_ENDLEVEL;
			if (Active_games[i].game_flags & NETGAME_FLAG_REALLY_FORMING)
				Active_games[i].game_status = NETSTAT_STARTING;
		}
	}

	//mprintf ((0,"Recieved %d unique games...\n",i));

	if (i == num_active_games)
		num_active_games++;

	if (Active_games[i].numplayers == 0)
	{
		mprintf((0, "DELETING THIS GAME!!!\n"));
		// Delete this game
		for (j = i; j < num_active_games - 1; j++)
		{
			memcpy(&Active_games[j], &Active_games[j + 1], sizeof(netgame_info));
		}
		num_active_games--;
	}
}

void network_process_dump(sequence_packet* their)
{
	// Our request for join was denied.  Tell the user why.

	char temp[40], i;

	mprintf((0, "Dumped by player %s, type %d.\n", their->player.callsign, their->player.connected));

	if (their->player.connected != 7)
		nm_messagebox(NULL, 1, TXT_OK, NET_DUMP_STRINGS(their->player.connected));
	else
	{
		for (i = 0; i < N_players; i++)
			if (!_stricmp(their->player.callsign, Players[i].callsign))
			{
				if (i != network_who_is_master())
				{
					HUD_init_message("%s attempted to kick you out.", their->player.callsign);
				}
				else
				{
					sprintf(temp, "%s has kicked you out!", their->player.callsign);
					nm_messagebox(NULL, 1, TXT_OK, &temp);
					if (Network_status == NETSTAT_PLAYING)
					{
						IWasKicked = 1;
						multi_leave_game();
					}
					else
						Network_status = NETSTAT_MENU;
				}
			}
	}
}
void network_process_request(sequence_packet* their)
{
	// Player is ready to receieve a sync packet
	int i;

	mprintf((0, "Player %s ready for sync.\n", their->player.callsign));
	NetGetLastPacketOrigin(their->player.network.ipx.node); //ensure correct node

	for (i = 0; i < N_players; i++)
	{
		if (!memcmp(their->player.network.ipx.node, NetPlayers.players[i].network.ipx.node, 4) && (!_stricmp(their->player.callsign, NetPlayers.players[i].callsign)))
		{
			Players[i].connected = 1;
			break;
		}
	}
}

#define REFUSE_INTERVAL F1_0 * 8
extern void multi_reset_object_texture(object*);

void network_process_packet(char* data, int length)
{
	sequence_packet* their = (sequence_packet*)data;
	char source_address[4];
	NetGetLastPacketOrigin(source_address);

	//mprintf( (0, "Got packet of length %d, type %d\n", length, their->type ));

 //      if ( length < sizeof(sequence_packet) ) return;

	length = length;

	switch (data[0]) 
	{

	case PID_GAME_INFO:		// Jason L. says we can safely ignore this type.
		break;

	case PID_PLAYERSINFO:

		mprintf((0, "Got a PID_PLAYERSINFO!\n"));

		if (Network_status == NETSTAT_WAITING)
		{                  
			memcpy(&TempPlayersBase, data, sizeof(AllNetPlayers_info));

			if (TempPlayersBase.Security != Netgame.Security)
			{
				mprintf((0, "Bad security for PLAYERSINFO\n"));
				break;
			}

			mprintf((0, "Got a waiting PID_PLAYERSINFO!\n"));
			if (length != ALLNETPLAYERSINFO_SIZE)
			{
				mprintf((0, "Invalid size for netplayers packet!\n"));
				return;
			}

			TempPlayersInfo = &TempPlayersBase;
			memcpy(TempPlayersInfo->players[0].network.ipx.node, source_address, 4);
			WaitingForPlayerInfo = 0;
			NetSecurityNum = TempPlayersInfo->Security;
			NetSecurityFlag = NETSECURITY_WAIT_FOR_SYNC;
		}

		break;

	case PID_LITE_INFO:
		if (length != LITE_INFO_SIZE)
		{
			mprintf((0, "WARNING! Recieved invalid size for PID_LITE_INFO\n"));
			return;
		}

		if (Network_status == NETSTAT_BROWSING)
			network_process_lite_info(data);
		break;

	case PID_GAME_LIST:
		// Someone wants a list of games

		if (length != SEQUENCE_PACKET_SIZE)
		{
			mprintf((0, "WARNING! Recieved invalid size for PID_GAME_LIST\n"));
			return;
		}

		mprintf((0, "Got a PID_GAME_LIST!\n"));
		if ((Network_status == NETSTAT_PLAYING) || (Network_status == NETSTAT_STARTING) || (Network_status == NETSTAT_ENDLEVEL))
			if (network_i_am_master())
				network_send_lite_info(their);
		break;


	case PID_SEND_ALL_GAMEINFO:

		if (length != SEQUENCE_PACKET_SIZE)
		{
			mprintf((0, "WARNING! Recieved invalid size for PID_SEND_ALL_GAMEINFO\n"));
			return;
		}

		if ((Network_status == NETSTAT_PLAYING) || (Network_status == NETSTAT_STARTING) || (Network_status == NETSTAT_ENDLEVEL))
			if (network_i_am_master() && their->Security == Netgame.Security)
				network_send_game_info(their);
		break;

	case PID_ADDPLAYER:

		mprintf((0, "Got NEWPLAYER message from %s.\n", their->player.callsign));

		if (length != SEQUENCE_PACKET_SIZE)
		{
			mprintf((0, "WARNING! Recieved invalid size for PID_ADDPLAYER\n"));
			return;
		}
		mprintf((0, "Got NEWPLAYER message from %s.\n", their->player.callsign));
		network_new_player(their);
		break;
	case PID_REQUEST:
		if (length != SEQUENCE_PACKET_SIZE)
		{
			mprintf((0, "WARNING! Recieved invalid size for PID_REQUEST\n"));
			return;
		}

		mprintf((0, "Got REQUEST from '%s'\n", their->player.callsign));
		if (Network_status == NETSTAT_STARTING)
		{
			// Someone wants to join our game!
			network_add_player(their);
		}
		else if (Network_status == NETSTAT_WAITING)
		{
			// Someone is ready to recieve a sync packet
			network_process_request(their);
		}
		else if (Network_status == NETSTAT_PLAYING)
		{
			// Someone wants to join a game in progress!
			if (Netgame.RefusePlayers)
				DoRefuseStuff(their);
			else
				network_welcome_player(their);
		}
		break;
	case PID_DUMP:

		if (length != SEQUENCE_PACKET_SIZE)
		{
			mprintf((0, "WARNING! Recieved invalid size for PID_DUMP\n"));
			return;
		}

		if (Network_status == NETSTAT_WAITING || Network_status == NETSTAT_PLAYING)
			network_process_dump(their);
		break;
	case PID_QUIT_JOINING:

		if (length != SEQUENCE_PACKET_SIZE)
		{
			mprintf((0, "WARNING! Recieved invalid size for PID_QUIT_JOINING\n"));
			return;
		}
		if (Network_status == NETSTAT_STARTING)
			network_remove_player(their);
		else if ((Network_status == NETSTAT_PLAYING) && (Network_send_objects))
			network_stop_resync(their);
		break;
	case PID_SYNC:

		mprintf((0, "Got a sync packet! Network_status=%d\n", NETSTAT_WAITING));

		if (Network_status == NETSTAT_WAITING)
		{
			memcpy((char*)&(TempNetInfo), data, sizeof(netgame_info));

			if (TempNetInfo.Security != Netgame.Security)
			{
				mprintf((0, "Bad security on sync packet.\n"));
				break;
			}

			if (NetSecurityFlag == NETSECURITY_WAIT_FOR_SYNC)
			{
				if (TempNetInfo.Security == TempPlayersInfo->Security)
				{
					network_read_sync_packet(&TempNetInfo, 0);
					NetSecurityFlag = 0;
					NetSecurityNum = 0;
				}
			}
			else
			{
				NetSecurityFlag = NETSECURITY_WAIT_FOR_PLAYERS;
				NetSecurityNum = TempNetInfo.Security;

				if (network_wait_for_playerinfo())
					network_read_sync_packet((netgame_info*)data, 0);

				NetSecurityFlag = 0;
				NetSecurityNum = 0;
			}
		}
		break;

	case PID_PDATA:
		if ((Game_mode & GM_NETWORK) && ((Network_status == NETSTAT_PLAYING) || (Network_status == NETSTAT_ENDLEVEL) || Network_status == NETSTAT_WAITING)) {
			network_process_pdata((char*)data);
		}
		break;
	case PID_NAKED_PDATA:
		if ((Game_mode & GM_NETWORK) && ((Network_status == NETSTAT_PLAYING) || (Network_status == NETSTAT_ENDLEVEL) || Network_status == NETSTAT_WAITING))
			network_process_naked_pdata((char*)data, length);
		break;

	case PID_OBJECT_DATA:
		if (Network_status == NETSTAT_WAITING)
			network_read_object_packet(data);
		break;
	case PID_ENDLEVEL:
		if ((Network_status == NETSTAT_ENDLEVEL) || (Network_status == NETSTAT_PLAYING))
			network_read_endlevel_packet(data);
		else
			mprintf((0, "Junked endlevel packet.\n"));
		break;
	case PID_ENDLEVEL_SHORT:
		if ((Network_status == NETSTAT_ENDLEVEL) || (Network_status == NETSTAT_PLAYING))
			network_read_endlevel_short_packet(data);
		else
			mprintf((0, "Junked short endlevel packet!\n"));
		break;

	case PID_GAME_UPDATE:
		mprintf((0, "Got a GAME_UPDATE!\n"));

		if (Network_status == NETSTAT_PLAYING)
		{
			memcpy((char*)&TempNetInfo, data, sizeof(lite_info));
			if (TempNetInfo.Security == Netgame.Security)
				memcpy(&Netgame, (char*)&TempNetInfo, sizeof(lite_info));
		}
		if (Game_mode & GM_TEAM)
		{
			int i;

			for (i = 0; i < N_players; i++)
				if (Players[i].connected)
					multi_reset_object_texture(&Objects[Players[i].objnum]);
			reset_cockpit();
		}
		break;
	case PID_PING_SEND:
		network_ping(PID_PING_RETURN, data[1]);
		break;
	case PID_PING_RETURN:
		network_handle_ping_return(data[1]);  // data[1] is player who told us of their ping time
		break;
	case PID_NAMES_RETURN:
		if (Network_status == NETSTAT_BROWSING && NamesInfoSecurity != -1)
			network_process_names_return((char*)data);
		break;
	case PID_GAME_PLAYERS:
		// Someone wants a list of players in this game

		if (length != SEQUENCE_PACKET_SIZE)
		{
			mprintf((0, "WARNING! Recieved invalid size for PID_GAME_PLAYERS\n"));
			return;
		}

		mprintf((0, "Got a PID_GAME_PLAYERS!\n"));
		if ((Network_status == NETSTAT_PLAYING) || (Network_status == NETSTAT_STARTING) || (Network_status == NETSTAT_ENDLEVEL))
			if (network_i_am_master() && their->Security == Netgame.Security)
				network_send_player_names(their);
		break;

	default:
		mprintf((0, "Ignoring invalid packet type.\n"));
		Int3(); // Invalid network packet type, see ROB
		break;
	}
}

#ifndef NDEBUG
void dump_segments()
{
	FILE* fp;

	fp = fopen("TEST.DMP", "wb");
	fwrite(Segments.data(), sizeof(segment) * (Highest_segment_index + 1), 1, fp);
	fclose(fp);
	mprintf((0, "SS=%d\n", sizeof(segment)));
}
#endif


void
network_read_endlevel_packet(char* data)
{
	// Special packet for end of level syncing
	int playernum;
	endlevel_info* end;

	end = (endlevel_info*)data;

	playernum = end->player_num;

	Assert(playernum != Player_num);

	if (playernum >= N_players)
	{
		Int3(); // weird, but it an happen in a coop restore game
		return; // if it happens in a coop restore, don't worry about it
	}

	if ((Network_status == NETSTAT_PLAYING) && (end->connected != 0))
		return; // Only accept disconnect packets if we're not out of the level yet

	Players[playernum].connected = end->connected;
	memcpy(&kill_matrix[playernum][0], end->kill_matrix, MAX_PLAYERS * sizeof(short));
	Players[playernum].net_kills_total = end->kills;
	Players[playernum].net_killed_total = end->killed;

	if ((Players[playernum].connected == 1) && (end->seconds_left < Countdown_seconds_left))
		Countdown_seconds_left = end->seconds_left;

	LastPacketTime[playernum] = timer_get_approx_seconds();

	//      mprintf((0, "Got endlevel packet from player %d.\n", playernum));
}

void
network_read_endlevel_short_packet(char* data)
{
	// Special packet for end of level syncing

	int playernum;
	endlevel_info_short* end;

	end = (endlevel_info_short*)data;

	playernum = end->player_num;

	Assert(playernum != Player_num);

	if (playernum >= N_players)
	{
		Int3(); // weird, but it can happen in a coop restore game
		return; // if it happens in a coop restore, don't worry about it
	}

	if ((Network_status == NETSTAT_PLAYING) && (end->connected != 0))
	{
		//mprintf ((0,"Returning early for short_endlevel\n"));
		return; // Only accept disconnect packets if we're not out of the level yet
	}

	Players[playernum].connected = end->connected;

	if ((Players[playernum].connected == 1) && (end->seconds_left < Countdown_seconds_left))
		Countdown_seconds_left = end->seconds_left;

	LastPacketTime[playernum] = timer_get_approx_seconds();
	}


void
network_pack_objects(void)
{
	// Switching modes, pack the object array

	special_reset_objects();
}

int network_verify_objects(int remote, int local)
{
	int i;
	int nplayers, got_controlcen = 0;

	mprintf((0, "NETWORK:remote=%d local=%d\n", remote, local));

	if ((remote - local) > 10)
		return(-1);

	//[ISB] Many fan levels don't use reactors, so allow connections otherwise. 
	//Not very chocolately, but it's a bit late...
	//if (Game_mode & GM_MULTI_ROBOTS)
	//	got_controlcen = 1;

	nplayers = 0;

	for (i = 0; i <= Highest_object_index; i++)
	{
		if ((Objects[i].type == OBJ_PLAYER) || (Objects[i].type == OBJ_GHOST))
			nplayers++;
		//if (Objects[i].type == OBJ_CNTRLCEN)
		//	got_controlcen = 1;
	}

	if (/*got_controlcen &&*/ (MaxNumNetPlayers <= nplayers))
		return(0);

	return(1);
}

void network_read_object_packet(char* data)
{
	// Object from another net player we need to sync with

	short objnum, remote_objnum;
	int8_t obj_owner;
	int segnum, i;
	object* obj;

	static int my_pnum = 0;
	static int mode = 0;
	static int object_count = 0;
	static int frame_num = 0;
	int nobj = data[1];
	int loc = 3;
	int remote_frame_num = data[2];

	frame_num++;

	//      mprintf((0, "Object packet %d (remote #%d) contains %d objects.\n", frame_num, remote_frame_num, nobj));

	for (i = 0; i < nobj; i++)
	{
		//objnum = (*(short*)(data + loc));                   loc += 2;
		netmisc_decode_int16(data, &loc, &objnum);
		obj_owner = data[loc]; loc += 1;
		//remote_objnum = (*(short*)(data + loc));    loc += 2;
		netmisc_decode_int16(data, &loc, &remote_objnum);

		if (objnum == -1)
		{
			// Clear object array
			mprintf((0, "Clearing object array.\n"));

			init_objects();
			Network_rejoined = 1;
			my_pnum = obj_owner;
			change_playernum_to(my_pnum);
			mode = 1;
			object_count = 0;
			frame_num = 1;
		}
		else if (objnum == -2)
		{
			// Special debug checksum marker for entire send
			if (mode == 1)
			{
				network_pack_objects();
				mode = 0;
			}
			mprintf((0, "Objnum -2 found in frame local %d remote %d.\n", frame_num, remote_frame_num));
			mprintf((0, "Got %d objects, expected %d.\n", object_count, remote_objnum));
			if (remote_objnum != object_count) 
			{
				Int3();
			}
			if (network_verify_objects(remote_objnum, object_count))
			{
				// Failed to sync up 
				nm_messagebox(NULL, 1, TXT_OK, TXT_NET_SYNC_FAILED);
				Network_status = NETSTAT_MENU;
				return;
			}
			frame_num = 0;
		}
		else
		{
			if (frame_num != remote_frame_num)
				Int3();
			mprintf((0, "Got a type 3 object packet!\n"));
			object_count++;
			if ((obj_owner == my_pnum) || (obj_owner == -1))
			{
				if (mode != 1)
					Int3(); // SEE ROB
				objnum = remote_objnum;
				//if (objnum > Highest_object_index)
				//{
				//      Highest_object_index = objnum;
				//      num_objects = Highest_object_index+1;
				//}
			}
			else
			{
				if (mode == 1)
				{
					network_pack_objects();
					mode = 0;
				}
				objnum = obj_allocate();
			}
			if (objnum != -1) 
			{
				obj = &Objects[objnum];
				if (obj->segnum != -1)
					obj_unlink(objnum);
				Assert(obj->segnum == -1);
				Assert(objnum < local_to_remote.size());
				netmisc_decode_object(data, &loc, obj);
				segnum = obj->segnum;
				obj->next = obj->prev = obj->segnum = -1;
				obj->attached_obj = -1;
				if (segnum > -1)
					obj_link(obj - Objects.data(), segnum);
				if (obj_owner == my_pnum)
					map_objnum_local_to_local(objnum);
				else if (obj_owner != -1)
					map_objnum_local_to_remote(objnum, remote_objnum, obj_owner);
				else
					object_owner[objnum] = -1;
			}
		} // For a standard onbject
	} // For each object in packet
}

void network_sync_poll(int nitems, newmenu_item* menus, int* key, int citem)
{
	// Polling loop waiting for sync packet to start game

	static fix t1 = 0;

	menus = menus;
	citem = citem;
	nitems = nitems;

	network_listen();

	if (Network_status != NETSTAT_WAITING)  // Status changed to playing, exit the menu
		*key = -2;

	if (!Network_rejoined && (timer_get_approx_seconds() > t1 + F1_0 * 2))
	{
		int i;

		// Poll time expired, re-send request

		t1 = timer_get_approx_seconds();

		mprintf((0, "Re-sending join request.\n"));
		i = network_send_request();
		if (i < 0)
			*key = -2;
	}
}

void network_start_poll(int nitems, newmenu_item* menus, int* key, int citem)
{
	int i, n, nm;

	key = key;
	citem = citem;

	Assert(Network_status == NETSTAT_STARTING);

	if (!menus[0].value)
	{
		menus[0].value = 1;
		menus[0].redraw = 1;
	}

	for (i = 1; i < nitems; i++) 
	{
		if ((i >= N_players) && (menus[i].value))
		{
			menus[i].value = 0;
			menus[i].redraw = 1;
		}
	}

	nm = 0;
	for (i = 0; i < nitems; i++) 
	{
		if (menus[i].value)
		{
			nm++;
			if (nm > N_players) 
			{
				menus[i].value = 0;
				menus[i].redraw = 1;
			}
		}
	}

	if (nm > MaxNumNetPlayers) 
	{
		nm_messagebox(TXT_ERROR, 1, TXT_OK, "%s %d %s", TXT_SORRY_ONLY, MaxNumNetPlayers, TXT_NETPLAYERS_IN);
		// Turn off the last player highlighted
		for (i = N_players; i > 0; i--)
			if (menus[i].value == 1)
			{
				menus[i].value = 0;
				menus[i].redraw = 1;
				break;
			}
	}

	//       if (nitems > MAX_PLAYERS ) return; 

	n = Netgame.numplayers;
	network_listen();

	if (n < Netgame.numplayers)
	{
		digi_play_sample(SOUND_HUD_MESSAGE, F1_0);

		mprintf((0, "More players are printed!"));
		if (FindArg("-norankings"))
			sprintf(menus[N_players - 1].text, "%d. %-20s", N_players, NetPlayers.players[N_players - 1].callsign);
		else
			sprintf(menus[N_players - 1].text, "%d. %s%-20s", N_players, RankStrings[NetPlayers.players[N_players - 1].rank], NetPlayers.players[N_players - 1].callsign);

		menus[N_players - 1].redraw = 1;
		if (N_players <= MaxNumNetPlayers)
		{
			menus[N_players - 1].value = 1;
		}
	}
	else if (n > Netgame.numplayers)
	{
		// One got removed...

		digi_play_sample(SOUND_HUD_KILL, F1_0);

		for (i = 0; i < N_players; i++)
		{

			if (FindArg("-norankings"))
				sprintf(menus[i].text, "%d. %-20s", i + 1, NetPlayers.players[i].callsign);
			else
				sprintf(menus[i].text, "%d. %s%-20s", i + 1, RankStrings[NetPlayers.players[i].rank], NetPlayers.players[i].callsign);
			if (i < MaxNumNetPlayers)
				menus[i].value = 1;
			else
				menus[i].value = 0;
			menus[i].redraw = 1;
		}
		for (i = N_players; i < n; i++)
		{
			sprintf(menus[i].text, "%d. ", i + 1);          // Clear out the deleted entries...
			menus[i].value = 0;
			menus[i].redraw = 1;
		}
	}
}

int opt_cinvul, opt_team_anarchy, opt_coop, opt_show_on_map, opt_closed, opt_maxnet;
int last_cinvul = 0, last_maxnet, opt_team_hoard;
int opt_refuse, opt_capture;

void network_game_param_poll(int nitems, newmenu_item* menus, int* key, int citem)
{
	static int oldmaxnet = 0;

	if (((HoardEquipped() && menus[opt_team_hoard].value) || (menus[opt_team_anarchy].value || menus[opt_capture].value)) && !menus[opt_closed].value && !menus[opt_refuse].value) 
	{
		menus[opt_refuse].value = 1;
		menus[opt_refuse].redraw = 1;
		menus[opt_refuse - 1].value = 0;
		menus[opt_refuse - 1].redraw = 1;
		menus[opt_refuse - 2].value = 0;
		menus[opt_refuse - 2].redraw = 1;
	}

	if (menus[opt_coop].value)
	{
		oldmaxnet = 1;

		if (menus[opt_maxnet].value > 2)
		{
			menus[opt_maxnet].value = 2;
			menus[opt_maxnet].redraw = 1;
		}

		if (menus[opt_maxnet].max_value > 2)
		{
			menus[opt_maxnet].max_value = 2;
			menus[opt_maxnet].redraw = 1;
		}

		if (!(Netgame.game_flags & NETGAME_FLAG_SHOW_MAP))
			Netgame.game_flags |= NETGAME_FLAG_SHOW_MAP;

		if (Netgame.PlayTimeAllowed || Netgame.KillGoal)
		{
			Netgame.PlayTimeAllowed = 0;
			Netgame.KillGoal = 0;
	}

}
	else // if !Coop game
	{
		if (oldmaxnet)
		{
			oldmaxnet = 0;
			menus[opt_maxnet].value = 6;
			menus[opt_maxnet].max_value = 6;
		}
	}

	if (last_maxnet != menus[opt_maxnet].value)
	{
		sprintf(menus[opt_maxnet].text, "Maximum players: %d", menus[opt_maxnet].value + 2);
		last_maxnet = menus[opt_maxnet].value;
		menus[opt_maxnet].redraw = 1;
	}
}

int netgame_difficulty;

int network_get_game_params(char* game_name, int* mode, int* game_flags, int* level)
{
	int i;
	int opt, opt_name, opt_level, opt_mode, opt_moreopts;
	newmenu_item m[20];
	char name[NETGAME_NAME_LEN + 1];
	char slevel[5];
	char level_text[32];
	char srmaxnet[50];

	int new_mission_num;
	int anarchy_only;

	*game_flags = *game_flags;

	SetAllAllowablesTo(1);
	NamesInfoSecurity = -1;

	for (i = 0; i < MAX_PLAYERS; i++)
		if (i != Player_num)
			Players[i].callsign[0] = 0;

	MaxNumNetPlayers = 8;
	Netgame.KillGoal = 0;
	Netgame.PlayTimeAllowed = 0;
	Netgame.Allow_marker_view = 1;
	netgame_difficulty = Player_default_difficulty;

	new_mission_num = multi_choose_mission(&anarchy_only);

	if (new_mission_num < 0)
		return -1;

	if (!(FindArg("-packets") && FindArg("-shortpackets")))
		if (!network_choose_connect())
			return -1;

	strcpy(Netgame.mission_name, Mission_list[new_mission_num].filename);
	strcpy(Netgame.mission_title, Mission_list[new_mission_num].mission_name);
	Netgame.control_invul_time = control_invul_time;

	sprintf(name, "%s%s", Players[Player_num].callsign, TXT_S_GAME);
	sprintf(slevel, "1");

	opt = 0;
	m[opt].type = NM_TYPE_TEXT; m[opt].text = TXT_DESCRIPTION; opt++;

	opt_name = opt;
	m[opt].type = NM_TYPE_INPUT; m[opt].text = name; m[opt].text_len = NETGAME_NAME_LEN; opt++;

	sprintf(level_text, "%s (1-%d)", TXT_LEVEL_, Last_level);

	//      if (Last_secret_level < -1)
	//              sprintf(level_text+strlen(level_text)-1, ", S1-S%d)", -Last_secret_level);
	//      else if (Last_secret_level == -1)
	//              sprintf(level_text+strlen(level_text)-1, ", S1)");

	Assert(strlen(level_text) < 32);

	m[opt].type = NM_TYPE_TEXT; m[opt].text = level_text; opt++;

	opt_level = opt;
	m[opt].type = NM_TYPE_INPUT; m[opt].text = slevel; m[opt].text_len = 4; opt++;
	//	m[opt].type = NM_TYPE_TEXT; m[opt].text = TXT_OPTIONS; opt++;

	opt_mode = opt;
	m[opt].type = NM_TYPE_RADIO; m[opt].text = TXT_ANARCHY; m[opt].value = 1; m[opt].group = 0; opt++;
	m[opt].type = NM_TYPE_RADIO; m[opt].text = TXT_TEAM_ANARCHY; m[opt].value = 0; m[opt].group = 0; opt_team_anarchy = opt; opt++;
	m[opt].type = NM_TYPE_RADIO; m[opt].text = TXT_ANARCHY_W_ROBOTS; m[opt].value = 0; m[opt].group = 0; opt++;
	m[opt].type = NM_TYPE_RADIO; m[opt].text = TXT_COOPERATIVE; m[opt].value = 0; m[opt].group = 0; opt_coop = opt; opt++;
	m[opt].type = NM_TYPE_RADIO; m[opt].text = const_cast<char*>("Capture the flag"); m[opt].value = 0; m[opt].group = 0; opt_capture = opt; opt++;

	if (HoardEquipped())
	{
		m[opt].type = NM_TYPE_RADIO; m[opt].text = const_cast<char*>("Hoard"); m[opt].value = 0; m[opt].group = 0; opt++;
		m[opt].type = NM_TYPE_RADIO; m[opt].text = const_cast<char*>("Team Hoard"); m[opt].value = 0; m[opt].group = 0; opt_team_hoard = opt; opt++;
		m[opt].type = NM_TYPE_TEXT; m[opt].text = const_cast<char*>(""); opt++;
	}
	else
	{
		m[opt].type = NM_TYPE_TEXT; m[opt].text = const_cast<char*>(""); opt++;
	}

	m[opt].type = NM_TYPE_RADIO; m[opt].text = const_cast<char*>("Open game"); m[opt].group = 1; m[opt].value = 0; opt++;
	opt_closed = opt;
	m[opt].type = NM_TYPE_RADIO; m[opt].text = TXT_CLOSED_GAME; m[opt].group = 1; m[opt].value = 0; opt++;
	opt_refuse = opt;
	m[opt].type = NM_TYPE_RADIO; m[opt].text = const_cast<char*>("Restricted Game              "); m[opt].group = 1; m[opt].value = Netgame.RefusePlayers; opt++;

	//      m[opt].type = NM_TYPE_CHECK; m[opt].text = TXT_SHOW_IDS; m[opt].value=0; opt++;

	opt_maxnet = opt;
	sprintf(srmaxnet, "Maximum players: %d", MaxNumNetPlayers);
	m[opt].type = NM_TYPE_SLIDER; m[opt].value = MaxNumNetPlayers - 2; m[opt].text = srmaxnet; m[opt].min_value = 0;
	m[opt].max_value = MaxNumNetPlayers - 2; opt++;
	last_maxnet = MaxNumNetPlayers - 2;

	opt_moreopts = opt;
	m[opt].type = NM_TYPE_MENU;  m[opt].text = const_cast<char*>("More options..."); opt++;

	Assert(opt <= 20);

menu:
	ExtGameStatus = GAMESTAT_NETGAME_OPTIONS;
	i = newmenu_do1(NULL, NULL, opt, m, network_game_param_poll, 1);
	//TXT_NETGAME_SETUP
	if (i == opt_moreopts)
	{
		if (m[opt_mode + 3].value)
			Game_mode = GM_MULTI_COOP;
		network_more_game_options();
		Game_mode = 0;
		goto menu;
	}
	Netgame.RefusePlayers = m[opt_refuse].value;


	if (i > -1) 
	{
		int j;

		MaxNumNetPlayers = m[opt_maxnet].value + 2;
		Netgame.max_numplayers = MaxNumNetPlayers;

		for (j = 0; j < num_active_games; j++)
			if (!_stricmp(Active_games[j].game_name, name))
			{
				nm_messagebox(TXT_ERROR, 1, TXT_OK, TXT_DUPLICATE_NAME);
				goto menu;
			}

		strcpy(game_name, name);

		*level = atoi(slevel);

		if ((*level < 1) || (*level > Last_level))
		{
			nm_messagebox(TXT_ERROR, 1, TXT_OK, TXT_LEVEL_OUT_RANGE);
			sprintf(slevel, "1");
			goto menu;
		}
		if (m[opt_mode].value)
			*mode = NETGAME_ANARCHY;

#ifdef SHAREWARE
		else
		{
			nm_messagebox(TXT_SORRY, 1, TXT_OK, TXT_REGISTERED_ONLY);
			m[opt_mode + 1].value = 0;
			m[opt_mode + 2].value = 0;
			m[opt_mode + 3].value = 0;
			if (HoardEquipped())
				m[opt_mode + 4].value = 0;

			m[opt_mode].value = 1;
			goto menu;
		}
#else
		else if (m[opt_mode + 1].value) 
		{
			*mode = NETGAME_TEAM_ANARCHY;
		}
		else if (m[opt_capture].value)
			*mode = NETGAME_CAPTURE_FLAG;
		else if (HoardEquipped() && m[opt_capture + 1].value)
			*mode = NETGAME_HOARD;
		else if (HoardEquipped() && m[opt_capture + 2].value)
			*mode = NETGAME_TEAM_HOARD;
		else if (anarchy_only) {
			nm_messagebox(NULL, 1, TXT_OK, TXT_ANARCHY_ONLY_MISSION);
			m[opt_mode + 2].value = 0;
			m[opt_mode + 3].value = 0;
			m[opt_mode].value = 1;
			goto menu;
		}
		else if (m[opt_mode + 2].value)
			*mode = NETGAME_ROBOT_ANARCHY;
		else if (m[opt_mode + 3].value)
			*mode = NETGAME_COOPERATIVE;
		else Int3(); // Invalid mode -- see Rob
#endif
		if (m[opt_closed].value)
			Netgame.game_flags |= NETGAME_FLAG_CLOSED;

	}

	return i;
}

void
network_set_game_mode(int gamemode)
{
	Show_kill_list = 1;

	if (gamemode == NETGAME_ANARCHY)
		Game_mode = GM_NETWORK;
	else if (gamemode == NETGAME_ROBOT_ANARCHY)
		Game_mode = GM_NETWORK | GM_MULTI_ROBOTS;
	else if (gamemode == NETGAME_COOPERATIVE)
		Game_mode = GM_NETWORK | GM_MULTI_COOP | GM_MULTI_ROBOTS;
	else if (gamemode == NETGAME_CAPTURE_FLAG)
	{
		Game_mode = GM_NETWORK | GM_TEAM | GM_CAPTURE;
		Show_kill_list = 3;
	}

	else if (HoardEquipped() && gamemode == NETGAME_HOARD)
		Game_mode = GM_NETWORK | GM_HOARD;
	else if (HoardEquipped() && gamemode == NETGAME_TEAM_HOARD)
	{
		Game_mode = GM_NETWORK | GM_HOARD | GM_TEAM;
		Show_kill_list = 3;
	}
	else if (gamemode == NETGAME_TEAM_ANARCHY)
	{
		Game_mode = GM_NETWORK | GM_TEAM;
		Show_kill_list = 3;
	}
	else
		Int3();

	}

int network_find_game(void)
{
	// Find out whether or not there is space left on this socket

	fix t1;

	Network_status = NETSTAT_BROWSING;

	num_active_games = 0;

	show_boxed_message(TXT_WAIT);

	network_send_game_list_request();
	t1 = timer_get_approx_seconds() + F1_0 * 3;

	plat_present_canvas(0);

	while (timer_get_approx_seconds() < t1) // Wait 3 seconds for replies
	{
		plat_do_events();
		network_listen();
	}

	clear_boxed_message();

	//  mprintf((0, "%s %d %s\n", TXT_FOUND, num_active_games, TXT_ACTIVE_GAMES));

	if (num_active_games < MAX_ACTIVE_NETGAMES)
		return 0;
	return 1;
}

void network_read_sync_packet(netgame_info* sp, int rsinit)
{
	int i, j;
	char temp_callsign[CALLSIGN_LEN + 1];
	char origin_address[4];

	if (rsinit)
		TempPlayersInfo = &NetPlayers;

	// This function is now called by all people entering the netgame.

	// mprintf( (0, "%s %d\n", TXT_STARTING_NETGAME, sp->levelnum ));
	NetGetLastPacketOrigin(origin_address);

	if (sp != &Netgame)
	{
		memcpy(&Netgame, sp, sizeof(netgame_info));
		memcpy(&NetPlayers, TempPlayersInfo, sizeof(AllNetPlayers_info));
	}

	memcpy(NetPlayers.players[0].network.ipx.node, origin_address, 4);

	N_players = sp->numplayers;
	Difficulty_level = sp->difficulty;
	Network_status = sp->game_status;

	//Assert(Function_mode != FMODE_GAME);

	//New code, 11/27

	mprintf((1, "Netgame.checksum = %d, calculated checksum = %d.\n", Netgame.segments_checksum, my_segments_checksum));

	if (Netgame.segments_checksum != my_segments_checksum)
	{
		Network_status = NETSTAT_MENU;
		nm_messagebox(TXT_ERROR, 1, TXT_OK, TXT_NETLEVEL_NMATCH);
#ifdef RELEASE
		return;
#endif
	}

	// Discover my player number

	memcpy(temp_callsign, Players[Player_num].callsign, CALLSIGN_LEN + 1);
	temp_callsign[CALLSIGN_LEN] = '\0'; //lint

	Player_num = -1;

	for (i = 0; i < MAX_NUM_NET_PLAYERS; i++) 
	{
		Players[i].net_kills_total = 0;
		//              Players[i].net_killed_total = 0;
	}

	for (i = 0; i < N_players; i++)
	{
		if (TempPlayersInfo->players[i].identifier == My_Seq.player.identifier && (!_stricmp(TempPlayersInfo->players[i].callsign, temp_callsign))) 
		{
			if (Player_num != -1)
			{
				Int3(); // Hey, we've found ourselves twice
				mprintf((0, "Hey, we've found ourselves twice!\n"));
				Network_status = NETSTAT_MENU;
				return;
			}
			change_playernum_to(i);
		}
		memcpy(Players[i].callsign, TempPlayersInfo->players[i].callsign, CALLSIGN_LEN + 1);

		memcpy(Players[i].net_address, TempPlayersInfo->players[i].network.ipx.node, 4);

		Players[i].n_packets_got = 0;                             // How many packets we got from them
		Players[i].n_packets_sent = 0;                            // How many packets we sent to them
		Players[i].connected = TempPlayersInfo->players[i].connected;
		Players[i].net_kills_total = sp->player_kills[i];
		Players[i].net_killed_total = sp->killed[i];
		if ((Network_rejoined) || (i != Player_num))
			Players[i].score = sp->player_score[i];
		for (j = 0; j < MAX_NUM_NET_PLAYERS; j++)
		{
			kill_matrix[i][j] = sp->kills[i][j];
		}
	}

	if (Player_num < 0) 
	{
		mprintf((0, "Bad Player_num, resetting to NETSTAT_MENU!\n"));
		Network_status = NETSTAT_MENU;
		return;
	}

	if (Network_rejoined)
		for (i = 0; i < N_players; i++)
			Players[i].net_killed_total = sp->killed[i];

	if (Network_rejoined) 
	{
		network_process_monitor_vector(sp->monitor_vector);
		Players[Player_num].time_level = sp->level_time;
	}

	team_kills[0] = sp->team_kills[0];
	team_kills[1] = sp->team_kills[1];

	Players[Player_num].connected = 1;
	NetPlayers.players[Player_num].connected = 1;
	NetPlayers.players[Player_num].rank = GetMyNetRanking();
	memcpy(NetPlayers.players[0].network.ipx.node, origin_address, 4);

	if (!Network_rejoined)
		for (i = 0; i < NumNetPlayerPositions; i++)
		{
			Objects[Players[i].objnum].pos = Player_init[Netgame.locations[i]].pos;
			Objects[Players[i].objnum].orient = Player_init[Netgame.locations[i]].orient;
			obj_relink(Players[i].objnum, Player_init[Netgame.locations[i]].segnum);
		}

	Objects[Players[Player_num].objnum].type = OBJ_PLAYER;

	mprintf((0, "Changing to NETSTAT_PLAYING!\n"));
	Network_status = NETSTAT_PLAYING;
	Function_mode = FMODE_GAME;
	multi_sort_kill_list();
}

void
network_send_sync(void)
{
	int i, j, np;

	// Randomize their starting locations...

	P_SRand(timer_get_fixed_seconds());
	for (i = 0; i < NumNetPlayerPositions; i++)
	{
		if (Players[i].connected)
			Players[i].connected = 1; // Get rid of endlevel connect statuses
		if (Game_mode & GM_MULTI_COOP)
			Netgame.locations[i] = i;
		else
		{
			do
			{
				np = P_Rand() % NumNetPlayerPositions;
				for (j = 0; j < i; j++)
				{
					if (Netgame.locations[j] == np)
					{
						np = -1;
						break;
					}
				}
			} while (np < 0);
			// np is a location that is not used anywhere else..
			Netgame.locations[i] = np;
			//                      mprintf((0, "Player %d starting in location %d\n" ,i ,np ));
		}
	}

	// Push current data into the sync packet

	network_update_netgame();
	Netgame.game_status = NETSTAT_PLAYING;
	Netgame.type = PID_SYNC;
	Netgame.segments_checksum = my_segments_checksum;

	for (i = 0; i < N_players; i++) 
	{
		if ((!Players[i].connected) || (i == Player_num))
			continue;

		// Send several times, extras will be ignored
		NetSendInternetworkPacket((char*)&Netgame, sizeof(netgame_info), NetPlayers.players[i].network.ipx.node);
		NetSendInternetworkPacket((char*)&NetPlayers, sizeof(AllNetPlayers_info), NetPlayers.players[i].network.ipx.node);
	}
	network_read_sync_packet(&Netgame, 1); // Read it myself, as if I had sent it
}

int
network_select_teams(void)
{
#ifndef SHAREWARE
	newmenu_item m[MAX_PLAYERS + 4];
	int choice, opt, opt_team_b;
	uint8_t team_vector = 0;
	char team_names[2][CALLSIGN_LEN + 1];
	int i;
	int pnums[MAX_PLAYERS + 2];

	// One-time initialization

	for (i = N_players / 2; i < N_players; i++) // Put first half of players on team A
	{
		team_vector |= (1 << i);
	}

	sprintf(team_names[0], "%s", TXT_BLUE);
	sprintf(team_names[1], "%s", TXT_RED);

	// Here comes da menu
menu:
	m[0].type = NM_TYPE_INPUT; m[0].text = team_names[0]; m[0].text_len = CALLSIGN_LEN;

	opt = 1;
	for (i = 0; i < N_players; i++)
	{
		if (!(team_vector & (1 << i)))
		{
			m[opt].type = NM_TYPE_MENU; m[opt].text = NetPlayers.players[i].callsign; pnums[opt] = i; opt++;
		}
	}
	opt_team_b = opt;
	m[opt].type = NM_TYPE_INPUT; m[opt].text = team_names[1]; m[opt].text_len = CALLSIGN_LEN; opt++;
	for (i = 0; i < N_players; i++)
	{
		if (team_vector & (1 << i))
		{
			m[opt].type = NM_TYPE_MENU; m[opt].text = NetPlayers.players[i].callsign; pnums[opt] = i; opt++;
		}
	}
	m[opt].type = NM_TYPE_TEXT; m[opt].text = const_cast<char*>(""); opt++;
	m[opt].type = NM_TYPE_MENU; m[opt].text = TXT_ACCEPT; opt++;

	Assert(opt <= MAX_PLAYERS + 4);

	choice = newmenu_do(NULL, TXT_TEAM_SELECTION, opt, m, NULL);

	if (choice == opt - 1)
	{
		if ((opt - 2 - opt_team_b < 2) || (opt_team_b == 1))
		{
			nm_messagebox(NULL, 1, TXT_OK, TXT_TEAM_MUST_ONE);
#ifdef RELEASE
			goto menu;
#endif
		}

		Netgame.team_vector = team_vector;
		strcpy(Netgame.team_name[0], team_names[0]);
		strcpy(Netgame.team_name[1], team_names[1]);
		return 1;
	}

	else if ((choice > 0) && (choice < opt_team_b)) 
	{
		team_vector |= (1 << pnums[choice]);
	}
	else if ((choice > opt_team_b) && (choice < opt - 2)) 
	{
		team_vector &= ~(1 << pnums[choice]);
	}
	else if (choice == -1)
		return 0;
	goto menu;
#else
	return 0;
#endif
}

int network_select_players(void)
{
	int i, j;
	newmenu_item m[MAX_PLAYERS + 4];
	char text[MAX_PLAYERS + 4][45];
	char title[50];
	int save_nplayers;              //how may people would like to join

	network_add_player(&My_Seq);

	for (i = 0; i < MAX_PLAYERS + 4; i++) 
	{
		sprintf(text[i], "%d.  %-20s", i + 1, "");
		m[i].type = NM_TYPE_CHECK; m[i].text = text[i]; m[i].value = 0;
}

	m[0].value = 1;                         // Assume server will play...

	if (FindArg("-norankings"))
		sprintf(text[0], "%d. %-20s", 1, Players[Player_num].callsign);
	else
		sprintf(text[0], "%d. %s%-20s", 1, RankStrings[NetPlayers.players[Player_num].rank], Players[Player_num].callsign);

	sprintf(title, "%s %d %s", TXT_TEAM_SELECT, MaxNumNetPlayers, TXT_TEAM_PRESS_ENTER);

GetPlayersAgain:
	ExtGameStatus = GAMESTAT_NETGAME_PLAYER_SELECT;
	j = newmenu_do1(NULL, title, MAX_PLAYERS + 4, m, network_start_poll, 1);

	save_nplayers = N_players;

	if (j < 0)
	{
		// Aborted!                                      
		// Dump all players and go back to menu mode

	abort:
		for (i = 1; i < save_nplayers; i++) 
			network_dump_player(NetPlayers.players[i].network.ipx.node, DUMP_ABORTED);


		Netgame.numplayers = 0;
		network_send_game_info(0); // Tell everyone we're bailing

		Network_status = NETSTAT_MENU;
		return(0);
	}

	// Count number of players chosen

	N_players = 0;
	for (i = 0; i < save_nplayers; i++)
	{
		if (m[i].value)
			N_players++;
	}

	if (N_players > Netgame.max_numplayers) 
	{
		nm_messagebox(TXT_ERROR, 1, TXT_OK, "%s %d %s", TXT_SORRY_ONLY, MaxNumNetPlayers, TXT_NETPLAYERS_IN);
		N_players = save_nplayers;
		goto GetPlayersAgain;
	}

#ifdef RELEASE
	if (N_players < 2) 
	{
		nm_messagebox(TXT_ERROR, 1, TXT_OK, TXT_TEAM_ATLEAST_TWO);
		N_players = save_nplayers;
		goto GetPlayersAgain;
	}
#endif

#ifdef RELEASE
	if ((Netgame.gamemode == NETGAME_TEAM_ANARCHY ||
		Netgame.gamemode == NETGAME_CAPTURE_FLAG || Netgame.gamemode == NETGAME_TEAM_HOARD) && (N_players < 2)) 
	{
		nm_messagebox(TXT_ERROR, 1, TXT_OK, "You must select at least two\nplayers to start a team game");
		N_players = save_nplayers;
		goto GetPlayersAgain;
	}
#endif

	// Remove players that aren't marked.
	N_players = 0;
	for (i = 0; i < save_nplayers; i++) 
	{
		if (m[i].value)
		{
			if (i > N_players)
			{
				memcpy(NetPlayers.players[N_players].network.ipx.node, NetPlayers.players[i].network.ipx.node, 4);
				//memcpy(NetPlayers.players[N_players].network.ipx.server, NetPlayers.players[i].network.ipx.server, 4);
				NetPlayers.players[N_players].identifier = NetPlayers.players[i].identifier;
				memcpy(NetPlayers.players[N_players].callsign, NetPlayers.players[i].callsign, CALLSIGN_LEN + 1);
				NetPlayers.players[N_players].version_major = NetPlayers.players[i].version_major;
				NetPlayers.players[N_players].version_minor = NetPlayers.players[i].version_minor;
				NetPlayers.players[N_players].rank = NetPlayers.players[i].rank;
				ClipRank((int8_t*)&NetPlayers.players[N_players].rank);
				network_check_for_old_version(i);
			}
			Players[N_players].connected = 1;
			N_players++;
		}
		else
			network_dump_player(NetPlayers.players[i].network.ipx.node, DUMP_DORK);
	}

	for (i = N_players; i < MAX_NUM_NET_PLAYERS; i++) 
	{
		memset(NetPlayers.players[i].network.ipx.node, 0, 4);
		NetPlayers.players[i].identifier = 0;
		memset(NetPlayers.players[i].callsign, 0, CALLSIGN_LEN + 1);
		NetPlayers.players[i].version_major = 0;
		NetPlayers.players[i].version_minor = 0;
		NetPlayers.players[i].rank = 0;
	}

	mprintf((0, "Select teams: Game mode is %d\n", Netgame.gamemode));

	if (Netgame.gamemode == NETGAME_TEAM_ANARCHY ||
		Netgame.gamemode == NETGAME_CAPTURE_FLAG ||
		Netgame.gamemode == NETGAME_TEAM_HOARD)
		if (!network_select_teams())
			goto abort;

	return(1);
	}

void network_start_game()
{
	int i;
	char game_name[NETGAME_NAME_LEN + 1];
	int chosen_game_mode, game_flags, level;

	Assert(FRAME_INFO_SIZE < IPX_MAX_DATA_SIZE);
	mprintf((0, "Using frame_info len %d, max %d.\n", FRAME_INFO_SIZE, IPX_MAX_DATA_SIZE));

	if (!Network_active)
	{
		nm_messagebox(NULL, 1, TXT_OK, TXT_IPX_NOT_FOUND);
		return;
	}

	network_init();
	change_playernum_to(0);

	if (network_find_game())
	{
		nm_messagebox(NULL, 1, TXT_OK, TXT_NET_FULL);
		return;
	}

	game_flags = 0;
	i = network_get_game_params(game_name, &chosen_game_mode, &game_flags, &level);

	if (i < 0) return;

	N_players = 0;

	// LoadLevel(level); Old, no longer used.

	Netgame.difficulty = Difficulty_level;
	Netgame.gamemode = chosen_game_mode;
	Netgame.game_status = NETSTAT_STARTING;
	Netgame.numplayers = 0;
	Netgame.max_numplayers = MaxNumNetPlayers;
	Netgame.levelnum = level;
	Netgame.protocol_version = MULTI_PROTO_VERSION;

	strcpy(Netgame.game_name, game_name);

	Network_status = NETSTAT_STARTING;

	network_set_game_mode(Netgame.gamemode);

	srand(timer_get_fixed_seconds());
	Netgame.Security = rand();  // For syncing Netgames with player packets

	if (network_select_players())
	{
		songs_init(); //reset descent.sng
		StartNewLevel(Netgame.levelnum, 0);
	}
	else
		Game_mode = GM_GAME_OVER;

}

void restart_net_searching(newmenu_item* m)
{
	int i;
	N_players = 0;
	num_active_games = 0;

	memset(Active_games, 0, sizeof(netgame_info) * MAX_ACTIVE_NETGAMES);

	for (i = 0; i < MAX_ACTIVE_NETGAMES; i++)
	{
		sprintf(m[i + 2].text, "%d.                                                     ", i + 1);
		m[i + 2].redraw = 1;
	}

	NamesInfoSecurity = -1;
	Network_games_changed = 1;
}

const char* ModeLetters[] = { "ANRCHY","TEAM","ROBO","COOP","FLAG","HOARD","TMHOARD" };

int NumActiveNetgames = 0;

void network_join_poll(int nitems, newmenu_item* menus, int* key, int citem)
{
	// Polling loop for Join Game menu
	static fix t1 = 0;
	int i, osocket, join_status, temp;

	menus = menus;
	citem = citem;
	nitems = nitems;
	key = key;

	if (Network_allow_socket_changes) 
	{
		osocket = Network_socket;

		if (*key == KEY_PAGEDOWN) { Network_socket--; *key = 0; }
		if (*key == KEY_PAGEUP) { Network_socket++; *key = 0; }

		if (Network_socket > 99)
			Network_socket = 99;
		if (Network_socket < -99)
			Network_socket = -99;

		if (Network_socket + IPX_DEFAULT_SOCKET > 0x8000)
			Network_socket = 0x8000 - IPX_DEFAULT_SOCKET;

		if (Network_socket + IPX_DEFAULT_SOCKET < 0)
			Network_socket = IPX_DEFAULT_SOCKET;

		if (Network_socket != osocket) {
			sprintf(menus[0].text, "\t%s %+d (PgUp/PgDn to change)", TXT_CURRENT_IPX_SOCKET, Network_socket);
			menus[0].redraw = 1;
			mprintf((0, "Changing to socket %d\n", Network_socket));
			network_listen();
			mprintf((0, "netgood 1!\n"));
			NetChangeDefaultSocket(IPX_DEFAULT_SOCKET + Network_socket);
			mprintf((0, "netgood 2!\n"));
			restart_net_searching(menus);
			mprintf((0, "netgood 3!\n"));
			network_send_game_list_request();
			mprintf((0, "netgood 4!\n"));
			return;
		}
	}

	// send a request for game info every 3 seconds
	if (timer_get_approx_seconds() > t1 + F1_0 * 3) 
	{
		t1 = timer_get_approx_seconds();
		network_send_game_list_request();
	}

	temp = num_active_games;

	network_listen();

	NumActiveNetgames = num_active_games;

	if (!Network_games_changed)
		return;

	if (temp != num_active_games)
		digi_play_sample(SOUND_HUD_MESSAGE, F1_0);

	Network_games_changed = 0;
	mprintf((0, "JOIN POLL: I'm looking at %d games!\n", num_active_games));

	// Copy the active games data into the menu options
	for (i = 0; i < num_active_games; i++)
	{
		int game_status = Active_games[i].game_status;
		int j, x, k, tx, ty, ta, nplayers = 0;
		char levelname[8], MissName[25], GameName[25], thold[2];
		thold[1] = 0;

		// These next two loops protect against menu skewing
		// if missiontitle or gamename contain a tab

		for (x = 0, tx = 0, k = 0, j = 0; j < 15; j++)
		{
			if (Active_games[i].mission_title[j] == '\t')
				continue;
			thold[0] = Active_games[i].mission_title[j];
			gr_get_string_size(thold, &tx, &ty, &ta);

			if ((x += tx) >= LHX(55))
			{
				MissName[k] = MissName[k + 1] = MissName[k + 2] = '.';
				k += 3;
				break;
			}

			MissName[k++] = Active_games[i].mission_title[j];
		}
		MissName[k] = 0;

		for (x = 0, tx = 0, k = 0, j = 0; j < 15; j++)
		{
			if (Active_games[i].game_name[j] == '\t')
				continue;
			thold[0] = Active_games[i].game_name[j];
			gr_get_string_size(thold, &tx, &ty, &ta);

			if ((x += tx) >= LHX(55))
			{
				GameName[k] = GameName[k + 1] = GameName[k + 2] = '.';
				k += 3;
				break;
			}
			GameName[k++] = Active_games[i].game_name[j];
		}
		GameName[k] = 0;


		nplayers = Active_games[i].numconnected;

		if (Active_games[i].levelnum < 0)
			sprintf(levelname, "S%d", -Active_games[i].levelnum);
		else
			sprintf(levelname, "%d", Active_games[i].levelnum);

		if (game_status == NETSTAT_STARTING)
		{
			sprintf(menus[i + 2].text, "%d.\t%s \t%s \t  %d/%d \t%s \t %s \t%s",
				i + 1, GameName, ModeLetters[Active_games[i].gamemode], nplayers,
				Active_games[i].max_numplayers, MissName, levelname, "Forming");
		}
		else if (game_status == NETSTAT_PLAYING)
		{
			join_status = can_join_netgame(&Active_games[i], NULL);
			//		 mprintf ((0,"Joinstatus=%d\n",join_status));

			if (join_status == 1)
				sprintf(menus[i + 2].text, "%d.\t%s \t%s \t  %d/%d \t%s \t %s \t%s",
					i + 1, GameName, ModeLetters[Active_games[i].gamemode], nplayers,
					Active_games[i].max_numplayers, MissName, levelname, "Open");
			else if (join_status == 2)
				sprintf(menus[i + 2].text, "%d.\t%s \t%s \t  %d/%d \t%s \t %s \t%s",
					i + 1, GameName, ModeLetters[Active_games[i].gamemode], nplayers,
					Active_games[i].max_numplayers, MissName, levelname, "Full");
			else if (join_status == 3)
				sprintf(menus[i + 2].text, "%d.\t%s \t%s \t  %d/%d \t%s \t %s \t%s",
					i + 1, GameName, ModeLetters[Active_games[i].gamemode], nplayers,
					Active_games[i].max_numplayers, MissName, levelname, "Restrict");
			else
				sprintf(menus[i + 2].text, "%d.\t%s \t%s \t  %d/%d \t%s \t %s \t%s",
					i + 1, GameName, ModeLetters[Active_games[i].gamemode], nplayers,
					Active_games[i].max_numplayers, MissName, levelname, "Closed");

		}
		else
			sprintf(menus[i + 2].text, "%d.\t%s \t%s \t  %d/%d \t%s \t %s \t%s",
				i + 1, GameName, ModeLetters[Active_games[i].gamemode], nplayers,
				Active_games[i].max_numplayers, MissName, levelname, "Between");


		Assert(strlen(menus[i + 2].text) < 100);
		menus[i + 2].redraw = 1;
}

	for (i = num_active_games; i < MAX_ACTIVE_NETGAMES; i++)
	{
		sprintf(menus[i + 2].text, "%d.                                                     ", i + 1);
		menus[i + 2].redraw = 1;
	}
}

int network_wait_for_sync()
{
	char text[60];
	newmenu_item m[2];
	int i, choice;

	Network_status = NETSTAT_WAITING;

	m[0].type = NM_TYPE_TEXT; m[0].text = text;
	m[1].type = NM_TYPE_TEXT; m[1].text = TXT_NET_LEAVE;

	i = network_send_request();

	if (i < 0)
		return(-1);

	sprintf(m[0].text, "%s\n'%s' %s", TXT_NET_WAITING, NetPlayers.players[i].callsign, TXT_NET_TO_ENTER);

menu:
	choice = newmenu_do(NULL, TXT_WAIT, 2, m, network_sync_poll);

	if (choice > -1)
		goto menu;

	if (Network_status != NETSTAT_PLAYING)
	{
		sequence_packet me;

		//              if (Network_status == NETSTAT_ENDLEVEL)
		//              {
		//                      network_send_endlevel_packet(0);
		//                      longjmp(LeaveGame, 0);
		//              }               

		mprintf((0, "Aborting join.\n"));
		me.type = PID_QUIT_JOINING;
		memcpy(me.player.callsign, Players[Player_num].callsign, CALLSIGN_LEN + 1);
		memcpy(me.player.network.ipx.node, NetGetLocalAddress(), 4);
		me.player.identifier = My_Seq.player.identifier;

		NetSendInternetworkPacket((char*)&me, sizeof(sequence_packet), NetPlayers.players[0].network.ipx.node);
		N_players = 0;
		Function_mode = FMODE_MENU;
		Game_mode = GM_GAME_OVER;
		return(-1);     // they cancelled               
	}
	return(0);
}

void network_request_poll(int nitems, newmenu_item* menus, int* key, int citem)
{
	// Polling loop for waiting-for-requests menu

	int i = 0;
	int num_ready = 0;

	menus = menus;
	citem = citem;
	nitems = nitems;
	key = key;

	// Send our endlevel packet at regular intervals

//      if (timer_get_approx_seconds() > t1+ENDLEVEL_SEND_INTERVAL)
//      {
//              network_send_endlevel_packet();
//              t1 = timer_get_approx_seconds();
//      }

	network_listen();

	for (i = 0; i < N_players; i++)
	{
		if ((Players[i].connected == 1) || (Players[i].connected == 0))
			num_ready++;
	}

	if (num_ready == N_players) // All players have checked in or are disconnected
	{
		*key = -2;
	}
}

void network_wait_for_requests(void)
{
	// Wait for other players to load the level before we send the sync
	int choice, i;
	newmenu_item m[1];

	Network_status = NETSTAT_WAITING;

	m[0].type = NM_TYPE_TEXT; m[0].text = TXT_NET_LEAVE;

	mprintf((0, "Entered wait_for_requests : N_players = %d.\n", N_players));

	for (choice = 0; choice < N_players; choice++)
		mprintf((0, "Players[%d].connected = %d.\n", choice, Players[choice].connected));

	Network_status = NETSTAT_WAITING;
	network_flush();

	Players[Player_num].connected = 1;

menu:
	choice = newmenu_do(NULL, TXT_WAIT, 1, m, network_request_poll);

	if (choice == -1)
	{
		// User aborted
		choice = nm_messagebox(NULL, 3, TXT_YES, TXT_NO, TXT_START_NOWAIT, TXT_QUITTING_NOW);
		if (choice == 2)
			return;
		if (choice != 0)
			goto menu;

		// User confirmed abort
		for (i = 0; i < N_players; i++) 
		{
			if ((Players[i].connected != 0) && (i != Player_num)) 
					network_dump_player(NetPlayers.players[i].network.ipx.node, DUMP_ABORTED);
		}

		longjmp(LeaveGame, 0);
	}
	else if (choice != -2)
		goto menu;
}

extern int multi_powerup_is_4pack(int);

void network_count_powerups_in_mine(void)
{
	int i;

	for (i = 0; i < MAX_POWERUP_TYPES; i++)
		PowerupsInMine[i] = 0;

	for (i = 0; i <= Highest_object_index; i++)
	{
		if (Objects[i].type == OBJ_POWERUP)
		{
			PowerupsInMine[Objects[i].id]++;
			if (multi_powerup_is_4pack(Objects[i].id))
				PowerupsInMine[Objects[i].id - 1] += 4;
		}
	}

}

int network_level_sync(void)
{
	// Do required syncing between (before) levels

	int result;

	mprintf((0, "Player %d entering network_level_sync.\n", Player_num));

	MySyncPackInitialized = 0;

	//      my_segments_checksum = netmisc_calc_checksum(Segments, sizeof(segment)*(Highest_segment_index+1));

	network_flush(); // Flush any old packets

	if (N_players == 0)
		result = network_wait_for_sync();
	else if (network_i_am_master())
	{
		network_wait_for_requests();
		network_send_sync();
		result = 0;
}
	else
		result = network_wait_for_sync();

	network_count_powerups_in_mine();

	if (result)
	{
		Players[Player_num].connected = 0;
		network_send_endlevel_packet();
		longjmp(LeaveGame, 0);
	}
	return(0);
}

void nm_draw_background1(const char* filename);

void network_AdjustMaxDataSize()
{
	if (Netgame.ShortPackets)
		MaxXDataSize = NET_XDATA_SIZE;
	else
		MaxXDataSize = NET_XDATA_SIZE;
}

void network_join_game()
{
	int choice, i;
	char menu_text[MAX_ACTIVE_NETGAMES + 2][200];

	newmenu_item m[MAX_ACTIVE_NETGAMES + 2];

	if (!Network_active)
	{
		nm_messagebox(NULL, 1, TXT_OK, TXT_IPX_NOT_FOUND);
		return;
	}

	network_init();

	N_players = 0;

	setjmp(LeaveGame);

	Network_send_objects = 0;
	Network_sending_extras = 0;
	Network_rejoined = 0;

	Network_status = NETSTAT_BROWSING; // We are looking at a game menu

	network_flush();
	network_listen();  // Throw out old info

	network_send_game_list_request(); // broadcast a request for lists

	num_active_games = 0;

	memset(m, 0, sizeof(newmenu_item) * MAX_ACTIVE_NETGAMES);
	memset(Active_games, 0, sizeof(netgame_info) * MAX_ACTIVE_NETGAMES);
	memset(ActiveNetPlayers, 0, sizeof(AllNetPlayers_info) * MAX_ACTIVE_NETGAMES);

	gr_set_fontcolor(BM_XRGB(15, 15, 23), -1);

	m[0].text = menu_text[0];
	m[0].type = NM_TYPE_TEXT;
	if (Network_allow_socket_changes)
		sprintf(m[0].text, "\tCurrent IPX Socket is default %+d (PgUp/PgDn to change)", Network_socket);
	else
		sprintf(m[0].text, "");

	m[1].text = menu_text[1];
	m[1].type = NM_TYPE_TEXT;
	sprintf(m[1].text, "\tGAME \tMODE \t#PLYRS \tMISSION \tLEV \tSTATUS");

	for (i = 0; i < MAX_ACTIVE_NETGAMES; i++) 
	{
		m[i + 2].text = menu_text[i + 2];
		m[i + 2].type = NM_TYPE_MENU;
		sprintf(m[i + 2].text, "%d.                                                               ", i + 1);
		m[i + 2].redraw = 1;
	}

	Network_games_changed = 1;
remenu:
	SurfingNet = 1;
	nm_draw_background1(Menu_pcx_name);             //load this here so if we abort after loading level, we restore the palette
	gr_palette_load(gr_palette);
	ExtGameStatus = GAMESTAT_JOIN_NETGAME;
	choice = newmenu_dotiny("NETGAMES", NULL, MAX_ACTIVE_NETGAMES + 2, m, network_join_poll);
	SurfingNet = 0;

	if (choice == -1) 
	{
		Network_status = NETSTAT_MENU;
		return; // they cancelled               
	}
	choice -= 2;

	if (choice >= num_active_games)
	{
		nm_messagebox(TXT_SORRY, 1, TXT_OK, TXT_INVALID_CHOICE);
		goto remenu;
	}

	// Choice has been made and looks legit
	if (Active_games[choice].game_status == NETSTAT_ENDLEVEL)
	{
		nm_messagebox(TXT_SORRY, 1, TXT_OK, TXT_NET_GAME_BETWEEN2);
		goto remenu;
	}

	if (Active_games[choice].protocol_version != MULTI_PROTO_VERSION)
	{
		if (Active_games[choice].protocol_version == 3) {
#ifndef SHAREWARE
			nm_messagebox(TXT_SORRY, 1, TXT_OK, "Your version of Descent 2\nis incompatible with the\nDemo version");
#endif
		}
		else if (Active_games[choice].protocol_version == 4) {
#ifdef SHAREWARE
			nm_messagebox(TXT_SORRY, 1, TXT_OK, "This Demo version of\nDescent 2 is incompatible\nwith the full commercial version");
#endif
		}
		else
			nm_messagebox(TXT_SORRY, 1, TXT_OK, TXT_VERSION_MISMATCH);

		goto remenu;
	}

#ifndef SHAREWARE
	{
		// Check for valid mission name
		mprintf((0, "Loading mission:%s.\n", Active_games[choice].mission_name));
		if (!load_mission_by_name(Active_games[choice].mission_name))
		{
			nm_messagebox(NULL, 1, TXT_OK, TXT_MISSION_NOT_FOUND);
			goto remenu;
		}
	}
#endif

#if defined (D2_OEM)
	{
		if (Active_games[choice].levelnum > 8)
		{
			nm_messagebox(NULL, 1, TXT_OK, "This OEM version only supports\nthe first 8 levels!");
			goto remenu;
		}
	}
#endif                  

	if (!network_wait_for_all_info(choice))
	{
		nm_messagebox(TXT_SORRY, 1, TXT_OK, "There was a join error!");
		Network_status = NETSTAT_BROWSING; // We are looking at a game menu
		goto remenu;
	}

	Network_status = NETSTAT_BROWSING; // We are looking at a game menu

	if (!can_join_netgame(&Active_games[choice], &ActiveNetPlayers[choice]))
	{
		if (Active_games[choice].numplayers == Active_games[choice].max_numplayers)
			nm_messagebox(TXT_SORRY, 1, TXT_OK, TXT_GAME_FULL);
		else
			nm_messagebox(TXT_SORRY, 1, TXT_OK, TXT_IN_PROGRESS);
		goto remenu;
	}

	// Choice is valid, prepare to join in

	memcpy(&Netgame, &Active_games[choice], sizeof(netgame_info));
	memcpy(&NetPlayers, &ActiveNetPlayers[choice], sizeof(AllNetPlayers_info));

	Difficulty_level = Netgame.difficulty;
	MaxNumNetPlayers = Netgame.max_numplayers;
	change_playernum_to(1);

	network_set_game_mode(Netgame.gamemode);

	network_AdjustMaxDataSize();

	StartNewLevel(Netgame.levelnum, 0);

	return;         // look ma, we're in a game!!!
}

void network_join_game_at(char* address)
{
	int i;
	fix start_time;

	if (!Network_active)
	{
		nm_messagebox(NULL, 1, TXT_OK, TXT_IPX_NOT_FOUND);
		return;
	}

	network_init();

	Connecting_directly = true;
	memcpy(Connecting_to_address, address, sizeof(Connecting_to_address));

	N_players = 0;

	setjmp(LeaveGame);

	Network_send_objects = 0;
	Network_sending_extras = 0;
	Network_rejoined = 0;

	Network_status = NETSTAT_BROWSING; // We are looking at a game menu

	network_flush();
	network_listen();  // Throw out old info

	network_send_game_list_request_to(address); // broadcast a request for lists
	start_time = timer_get_fixed_seconds();

	show_boxed_message(TXT_WAIT);
	plat_present_canvas(0);

	num_active_games = 0;
	memset(Active_games, 0, sizeof(netgame_info) * MAX_ACTIVE_NETGAMES);
	memset(ActiveNetPlayers, 0, sizeof(AllNetPlayers_info) * MAX_ACTIVE_NETGAMES);

	gr_set_fontcolor(BM_XRGB(15, 15, 23), -1);

	if (setjmp(LeaveGame)) //aborting game
	{
		Connecting_directly = false;
		return;
	}
	Network_games_changed = 1;

	for (;;)
	{
		plat_do_events();
		//listen for requests
		network_listen();

		//Look if a game has been made available yet
		if (num_active_games > 0)
			break;

		if (timer_get_fixed_seconds() > start_time + F1_0 * 5)
		{
			nm_messagebox(TXT_SORRY, 1, TXT_OK, "No game found within 5 seconds.");

			Connecting_directly = false;
			return;
		}
	}

	// Choice has been made and looks legit
	if (Active_games[0].game_status == NETSTAT_ENDLEVEL)
	{
		nm_messagebox(TXT_SORRY, 1, TXT_OK, TXT_NET_GAME_BETWEEN2);

		Connecting_directly = false;
		return;
	}

	if (Active_games[0].protocol_version != MULTI_PROTO_VERSION)
	{
		if (Active_games[0].protocol_version == 3)
		{
#ifndef SHAREWARE
			nm_messagebox(TXT_SORRY, 1, TXT_OK, "Your version of Descent 2\nis incompatible with the\nDemo version");
#endif
		}
		else if (Active_games[0].protocol_version == 4) 
		{
#ifdef SHAREWARE
			nm_messagebox(TXT_SORRY, 1, TXT_OK, "This Demo version of\nDescent 2 is incompatible\nwith the full commercial version");
#endif
		}
		else
			nm_messagebox(TXT_SORRY, 1, TXT_OK, TXT_VERSION_MISMATCH);

		Connecting_directly = false;
		return;
	}

#ifndef SHAREWARE
	{
		// Check for valid mission name
		mprintf((0, "Loading mission:%s.\n", Active_games[0].mission_name));
		if (!load_mission_by_name(Active_games[0].mission_name))
		{
			nm_messagebox(NULL, 1, TXT_OK, TXT_MISSION_NOT_FOUND);

			Connecting_directly = false;
			return;
		}
	}
#endif

#if defined (D2_OEM)
	{
		if (Active_games[0].levelnum > 8)
		{
			nm_messagebox(NULL, 1, TXT_OK, "This OEM version only supports\nthe first 8 levels!");

			Connecting_directly = false;
			return;
		}
	}
#endif                  

	if (!network_wait_for_all_info(0))
	{
		nm_messagebox(TXT_SORRY, 1, TXT_OK, "There was a join error!");
		Network_status = NETSTAT_BROWSING; // We are looking at a game menu
		return;
	}

	Network_status = NETSTAT_BROWSING; // We are looking at a game menu

	if (!can_join_netgame(&Active_games[0], &ActiveNetPlayers[0]))
	{
		if (Active_games[0].numplayers == Active_games[0].max_numplayers)
			nm_messagebox(TXT_SORRY, 1, TXT_OK, TXT_GAME_FULL);
		else
			nm_messagebox(TXT_SORRY, 1, TXT_OK, TXT_IN_PROGRESS);

		Connecting_directly = false;
		return;
	}

	// Choice is valid, prepare to join in

	memcpy(&Netgame, &Active_games[0], sizeof(netgame_info));
	memcpy(&NetPlayers, &ActiveNetPlayers[0], sizeof(AllNetPlayers_info));

	Difficulty_level = Netgame.difficulty;
	MaxNumNetPlayers = Netgame.max_numplayers;
	change_playernum_to(1);

	network_set_game_mode(Netgame.gamemode);

	network_AdjustMaxDataSize();

	StartNewLevel(Netgame.levelnum, 0);

	Connecting_directly = false;
	return;         // look ma, we're in a game!!!
}

fix StartWaitAllTime = 0;
int WaitAllChoice = 0;
#define ALL_INFO_REQUEST_INTERVAL F1_0*3

void network_wait_all_poll(int nitems, newmenu_item* menus, int* key, int citem)
{
	static fix t1 = 0;

	menus = menus;
	nitems = nitems;
	citem = citem;

	if (timer_get_approx_seconds() > t1 + ALL_INFO_REQUEST_INTERVAL)
	{
		network_send_all_info_request(PID_SEND_ALL_GAMEINFO, SecurityCheck);
		t1 = timer_get_approx_seconds();
		}

	network_do_big_wait(WaitAllChoice);

	if (SecurityCheck == -1)
		*key = -2;
	}

int network_wait_for_all_info(int choice)
{
	int pick;

	newmenu_item m[2];

	m[0].type = NM_TYPE_TEXT; m[0].text = const_cast<char*>("Press Escape to cancel");

	WaitAllChoice = choice;
	StartWaitAllTime = timer_get_approx_seconds();
	SecurityCheck = Active_games[choice].Security;
	NetSecurityFlag = 0;

GetMenu:
	pick = newmenu_do(NULL, "Connecting...", 1, m, network_wait_all_poll);

	if (pick > -1 && SecurityCheck != -1)
		goto GetMenu;

	if (SecurityCheck == -1)
	{
		SecurityCheck = 0;
		return (1);
	}
	SecurityCheck = 0;
	return (0);
}

void network_do_big_wait(int choice)
{
	int size;
	char packet[IPX_MAX_DATA_SIZE], * data;
	char source_address[4];
	AllNetPlayers_info* temp_info;

	size = NetGetPacketData(packet);
	NetGetLastPacketOrigin(source_address);

	if (size > 0)
	{
		data = packet;

		switch (data[0])
		{
		case PID_GAME_INFO:

			memcpy((char*)&TempNetInfo, data, sizeof(netgame_info));
			mprintf((0, "This is %s game with a security of %d\n", TempNetInfo.game_name, TempNetInfo.Security));

			if (TempNetInfo.Security != SecurityCheck)
			{
				mprintf((0, "Bad security on big_wait...rejecting.\n"));
				break;
			}

			if (NetSecurityFlag == NETSECURITY_WAIT_FOR_GAMEINFO)
			{
				if (TempPlayersInfo->Security == TempNetInfo.Security)
				{
					mprintf((0, "EQUAL !: Game=%d Players=%d ", TempPlayersInfo->Security, TempNetInfo.Security));
					if (TempPlayersInfo->Security == SecurityCheck)
					{
						memcpy(&Active_games[choice], (char*)&TempNetInfo, sizeof(netgame_info));
						memcpy(&ActiveNetPlayers[choice], TempPlayersInfo, sizeof(AllNetPlayers_info));
						SecurityCheck = -1;
					}
				}
			}
			else
			{
				NetSecurityFlag = NETSECURITY_WAIT_FOR_PLAYERS;
				NetSecurityNum = TempNetInfo.Security;

				if (network_wait_for_playerinfo())
				{
					mprintf((0, "HUH? Game=%d Player=%d\n", NetSecurityNum, TempPlayersInfo->Security));
					memcpy(&Active_games[choice], (char*)&TempNetInfo, sizeof(netgame_info));
					memcpy(&ActiveNetPlayers[choice], TempPlayersInfo, sizeof(AllNetPlayers_info));
					SecurityCheck = -1;
				}
				NetSecurityFlag = 0;
				NetSecurityNum = 0;
			}
			break;
		case PID_PLAYERSINFO:
			mprintf((0, "Got a PID_PLAYERSINFO!\n"));

			temp_info = (AllNetPlayers_info*)data;
			NetGetLastPacketOrigin(source_address);

			if (temp_info->Security != SecurityCheck)
				break;     // If this isn't the guy we're looking for, move on

			memcpy(&TempPlayersBase, (char*)&temp_info, sizeof(AllNetPlayers_info));
			TempPlayersInfo = &TempPlayersBase;

			//[ISB] ugh, this still sucks. 
			mprintf((0, "bashing source address of PID_PLAYERSINFO"));
			memcpy(TempPlayersInfo->players[0].network.ipx.node, source_address, 4);
			WaitingForPlayerInfo = 0;
			NetSecurityNum = TempPlayersInfo->Security;
			NetSecurityFlag = NETSECURITY_WAIT_FOR_GAMEINFO;
			break;
		}
	}
}

void network_leave_game()
{
	int nsave;

	network_do_frame(1, 1);

#ifdef NETPROFILING
	fclose(SendLogFile);
	fclose(RecieveLogFile);
#endif

	if ((network_i_am_master()))
	{
		while (Network_sending_extras > 1 && Player_joining_extras != -1)
			network_send_extras();

		Netgame.numplayers = 0;
		nsave = N_players;
		N_players = 0;
		network_send_game_info(NULL);
		N_players = nsave;

		mprintf((0, "HEY! I'm master and I've left.\n"));
	}

	Players[Player_num].connected = 0;
	network_send_endlevel_packet();
	change_playernum_to(0);
	Game_mode = GM_GAME_OVER;
	write_player_file();

	//	WIN(ipx_destroy_read_thread());

	network_flush();
}

void network_flush()
{
	char packet[IPX_MAX_DATA_SIZE];

	if (!Network_active) 
		return;

	while (NetGetPacketData(packet) > 0);
}

void network_listen()
{
	int size;
	char packet[IPX_MAX_DATA_SIZE];
	int i, loopmax = 999;

	if (Network_status == NETSTAT_PLAYING && Netgame.ShortPackets && !Network_send_objects)
	{
		loopmax = N_players * Netgame.PacketsPerSec;
	}

	if (!Network_active) 
		return;

	if (!(Game_mode & GM_NETWORK) && (Function_mode == FMODE_GAME))
		mprintf((0, "Calling network_listen() when not in net game.\n"));

	WaitingForPlayerInfo = 1;
	NetSecurityFlag = NETSECURITY_OFF;

	i = 1;
	size = NetGetPacketData(packet);
	while (size > 0) 
	{
		network_process_packet(packet, size);
		if (++i > loopmax)
			break;
		size = NetGetPacketData(packet);
	}
}

int network_wait_for_playerinfo()
{
	int size, retries = 0;
	char packet[IPX_MAX_DATA_SIZE];
	struct AllNetPlayers_info* TempInfo;
	char source_address[4];
	fix basetime;
	uint8_t id;

	if (!Network_active) 
		return(0);

	//  if (!WaitingForPlayerInfo)
  // return (1);

	if (!(Game_mode & GM_NETWORK) && (Function_mode == FMODE_GAME))
	{
		mprintf((0, "Calling network_wait_for_playerinfo() when not in net game.\n"));
	}
	if (Network_status == NETSTAT_PLAYING)
	{
		Int3(); //MY GOD! Get Jason...this is the source of many problems
		return (0);
	}
	basetime = timer_get_approx_seconds();

	while (WaitingForPlayerInfo && retries < 50 && (timer_get_approx_seconds() < (basetime + (F1_0 * 5))))
	{
		size = NetGetPacketData(packet);
		NetGetLastPacketOrigin(source_address);
		id = packet[0];

		if (size > 0 && id == PID_PLAYERSINFO)
		{
			TempInfo = (AllNetPlayers_info*)packet;

			retries++;

			if (NetSecurityFlag == NETSECURITY_WAIT_FOR_PLAYERS)
			{
				if (NetSecurityNum == TempInfo->Security)
				{
					mprintf((0, "HEYEQUAL: Player=%d Game=%d\n", TempInfo->Security, NetSecurityNum));
					memcpy(&TempPlayersBase, (char*)TempInfo, sizeof(AllNetPlayers_info));
					TempPlayersInfo = &TempPlayersBase;
					NetSecurityFlag = NETSECURITY_OFF;
					NetSecurityNum = 0;
					WaitingForPlayerInfo = 0;
					memcpy(TempPlayersInfo->players[0].network.ipx.node, source_address, 4);
					return (1);
				}
				else
					continue;
			}
			else
			{
				mprintf((0, "I'm original!\n"));

				NetSecurityNum = TempInfo->Security;
				NetSecurityFlag = NETSECURITY_WAIT_FOR_GAMEINFO;

				memcpy(&TempPlayersBase, (char*)TempInfo, sizeof(AllNetPlayers_info));
				TempPlayersInfo = &TempPlayersBase;
				WaitingForPlayerInfo = 0;
				memcpy(TempPlayersInfo->players[0].network.ipx.node, source_address, 4);
				return (1);
			}
		}
	}
	return (0);
}


void network_send_data(char* ptr, int len, int urgent)
{
	char check;

#ifdef NETPROFILING
	TTSent[ptr[0]]++;
	fprintf(SendLogFile, "Packet type: %d Len:%d Urgent:%d TT=%d\n", ptr[0], len, urgent, TTSent[ptr[0]]);
	fflush(SendLogFile);
#endif

	if (Endlevel_sequence)
		return;

	if (!MySyncPackInitialized) 
	{
		MySyncPackInitialized = 1;
		memset(&MySyncPack, 0, sizeof(frame_info));
	}

	if (urgent)
		PacketUrgent = 1;

	if ((MySyncPack.data_size + len) > MaxXDataSize) 
	{
		check = ptr[0];
		network_do_frame(1, 0);
		if (MySyncPack.data_size != 0) 
		{
			mprintf((0, "%d bytes were added to data by network_do_frame!\n", MySyncPack.data_size));
			Int3();
		}
		//              Int3();         // Trying to send too much!
		//              return;
		mprintf((0, "Packet overflow, sending additional packet, type %d len %d.\n", ptr[0], len));
		Assert(check == ptr[0]);
	}

	Assert(MySyncPack.data_size + len <= MaxXDataSize);

	memcpy(&MySyncPack.data[MySyncPack.data_size], ptr, len);
	MySyncPack.data_size += len;
}

void network_timeout_player(int playernum)
{
	// Remove a player from the game if we haven't heard from them in 
	// a long time.
	int i, n = 0;

	Assert(playernum < N_players);
	Assert(playernum > -1);

	network_disconnect_player(playernum);
	create_player_appearance_effect(&Objects[Players[playernum].objnum]);

	digi_play_sample(SOUND_HUD_MESSAGE, F1_0);

	HUD_init_message("%s %s", Players[playernum].callsign, TXT_DISCONNECTING);
	for (i = 0; i < N_players; i++)
		if (Players[i].connected)
			n++;

	if (n == 1)
	{
		nm_messagebox(NULL, 1, TXT_OK, TXT_YOU_ARE_ONLY);
	}
}

fix last_send_time = 0;
fix last_timeout_check = 0;

char NakedBuf[NET_XDATA_SIZE + 4];
int NakedPacketLen = 0;
int NakedPacketDestPlayer = -1;

void network_do_frame(int force, int listen)
{
	int i;
	short_frame_info ShortSyncPack;
	static fix LastEndlevel = 0;

	if (!(Game_mode & GM_NETWORK)) return;

	if ((Network_status != NETSTAT_PLAYING) || (Endlevel_sequence)) // Don't send postion during escape sequence...
		goto listen;

	if (NakedPacketLen)
	{
		Assert(NakedPacketDestPlayer > -1);
		//         mprintf ((0,"Sending a naked packet to %s (%d bytes)!\n",Players[NakedPacketDestPlayer].callsign,NakedPacketLen));

		NetSendPacket((char*)NakedBuf, NakedPacketLen, NetPlayers.players[NakedPacketDestPlayer].network.ipx.node, Players[NakedPacketDestPlayer].net_address);
		NakedPacketLen = 0;
		NakedPacketDestPlayer = -1;
	}

	if (WaitForRefuseAnswer && timer_get_approx_seconds() > (RefuseTimeLimit + (F1_0 * 12)))
		WaitForRefuseAnswer = 0;

	last_send_time += FrameTime;
	last_timeout_check += FrameTime;

	// Send out packet PacksPerSec times per second maximum... unless they fire, then send more often...
	if ((last_send_time > F1_0 / Netgame.PacketsPerSec) || (Network_laser_fired) || force || PacketUrgent) 
	{
		if (Players[Player_num].connected) 
		{
			int objnum = Players[Player_num].objnum;
			PacketUrgent = 0;

			if (listen) {
				multi_send_robot_frame(0);
				multi_send_fire();              // Do firing if needed..
			}

			last_send_time = 0;

			if (Netgame.ShortPackets)
			{
				create_shortpos(&ShortSyncPack.thepos, &Objects[objnum]);
				ShortSyncPack.type = PID_PDATA;
				ShortSyncPack.playernum = Player_num;
				ShortSyncPack.obj_render_type = Objects[objnum].render_type;
				ShortSyncPack.level_num = Current_level_num;
				ShortSyncPack.data_size = MySyncPack.data_size;
				memcpy(&ShortSyncPack.data[0], &MySyncPack.data[0], MySyncPack.data_size);

				for (i = 0; i < N_players; i++)
				{
					if ((Players[i].connected) && (i != Player_num)) 
					{
						MySyncPack.numpackets = Players[i].n_packets_sent++;
						ShortSyncPack.numpackets = MySyncPack.numpackets;
						NetSendPacket((char*)&ShortSyncPack, sizeof(short_frame_info) - MaxXDataSize + MySyncPack.data_size, NetPlayers.players[i].network.ipx.node, Players[i].net_address);
					}
				}
			}
			else  // If long packets
			{
				int send_data_size;

				MySyncPack.type = PID_PDATA;
				MySyncPack.playernum = Player_num;
				MySyncPack.obj_render_type = Objects[objnum].render_type;
				MySyncPack.level_num = Current_level_num;
				MySyncPack.obj_segnum = Objects[objnum].segnum;
				MySyncPack.obj_pos = Objects[objnum].pos;
				MySyncPack.obj_orient = Objects[objnum].orient;
				MySyncPack.phys_velocity = Objects[objnum].mtype.phys_info.velocity;
				MySyncPack.phys_rotvel = Objects[objnum].mtype.phys_info.rotvel;

				send_data_size = MySyncPack.data_size;                  // do this so correct size data is sent

				for (i = 0; i < N_players; i++) 
				{
					if ((Players[i].connected) && (i != Player_num)) 
					{
						MySyncPack.numpackets = (Players[i].n_packets_sent);

						Players[i].n_packets_sent++;
						NetSendPacket((char*)&MySyncPack, sizeof(frame_info) - MaxXDataSize + send_data_size, NetPlayers.players[i].network.ipx.node, Players[i].net_address);
					}
				}
			}

			MySyncPack.data_size = 0;               // Start data over at 0 length.
			if (Control_center_destroyed)
			{
				if (Player_is_dead)
					Players[Player_num].connected = 3;
				if (timer_get_approx_seconds() > (LastEndlevel + (F1_0 / 2)))
				{
					network_send_endlevel_packet();
					LastEndlevel = timer_get_approx_seconds();
				}
			}
			//mprintf( (0, "Packet has %d bytes appended (TS=%d)\n", MySyncPack.data_size, sizeof(frame_info)-MaxXDataSize+MySyncPack.data_size ));
		}
	}

	if (!listen)
		return;

	if ((last_timeout_check > F1_0) && !(Control_center_destroyed))
	{
		fix approx_time = timer_get_approx_seconds();
		// Check for player timeouts
		for (i = 0; i < N_players; i++)
		{
			if ((i != Player_num) && (Players[i].connected == 1))
			{
				if ((LastPacketTime[i] == 0) || (LastPacketTime[i] > approx_time))
				{
					LastPacketTime[i] = approx_time;
					continue;
				}
				if ((approx_time - LastPacketTime[i]) > (15 * F1_0))
					network_timeout_player(i);
			}
		}
		last_timeout_check = 0;
	}

listen:
	if (!listen)
	{
		MySyncPack.data_size = 0;
		return;
	}
	network_listen();

	if (VerifyPlayerJoined != -1 && !(FrameCount & 63))
		resend_sync_due_to_packet_loss_for_allender(); // This will resend to network_player_rejoining

	if (Network_send_objects)
		network_send_objects();

	if (Network_sending_extras && VerifyPlayerJoined == -1)
	{
		network_send_extras();
		return;
	}
}

int missed_packets = 0;

int ConsistencyCount = 0;

void network_consistency_error(void)
{
	if (++ConsistencyCount < 10)
		return;

	Function_mode = FMODE_MENU;
#ifndef MACINTOSH
	nm_messagebox(NULL, 1, TXT_OK, TXT_CONSISTENCY_ERROR);
#else
	nm_messagebox(NULL, 1, TXT_OK, "Failed to join the netgame.\nYou are missing packets.  Check\nyour network connection and\ntry again.");
#endif
	Function_mode = FMODE_GAME;
	ConsistencyCount = 0;
	multi_quit_game = 1;
	multi_leave_menu = 1;
	multi_reset_stuff();
	Function_mode = FMODE_MENU;
}

void network_process_pdata(char* data)
{
	Assert(Game_mode & GM_NETWORK);

	if (Netgame.ShortPackets)
		network_read_pdata_short_packet((short_frame_info*)data);
	else
		network_read_pdata_packet((frame_info*)data);
}

void network_read_pdata_packet(frame_info* pd)
{
	int TheirPlayernum;
	int TheirObjnum;
	object* TheirObj = NULL;

	// frame_info should be aligned...for mac, make the necessary adjustments

	TheirPlayernum = pd->playernum;
	TheirObjnum = Players[pd->playernum].objnum;

	if (TheirPlayernum < 0) 
	{
		Int3(); // This packet is bogus!!
		return;
	}

	if (VerifyPlayerJoined != -1 && TheirPlayernum == VerifyPlayerJoined)
	{
		// Hurray! Someone really really got in the game (I think).
		mprintf((0, "Hurray! VPJ (%d) reset!\n", VerifyPlayerJoined));
		VerifyPlayerJoined = -1;
	}

	if (!multi_quit_game && (TheirPlayernum >= N_players)) 
	{
		if (Network_status != NETSTAT_WAITING)
		{
			Int3(); // We missed an important packet!
			network_consistency_error();
			return;
		}
		else
			return;
	}
	if (Endlevel_sequence || (Network_status == NETSTAT_ENDLEVEL))
	{
		int old_Endlevel_sequence = Endlevel_sequence;
		Endlevel_sequence = 1;
		if (pd->data_size > 0) 
		{
			// pass pd->data to some parser function....
			multi_process_bigdata((char*)pd->data, pd->data_size);
		}
		Endlevel_sequence = old_Endlevel_sequence;
		return;
	}
	//      mprintf((0, "Gametime = %d, Frametime = %d.\n", GameTime, FrameTime));

	if ((int8_t)pd->level_num != Current_level_num)
	{
		mprintf((0, "Got frame packet from player %d wrong level %d!\n", pd->playernum, pd->level_num));
		return;
	}

	TheirObj = &Objects[TheirObjnum];

	//------------- Keep track of missed packets -----------------
	Players[TheirPlayernum].n_packets_got++;
	TotalPacketsGot++;
	LastPacketTime[TheirPlayernum] = timer_get_approx_seconds();

	if (pd->numpackets != Players[TheirPlayernum].n_packets_got) 
	{
		int missed_packets;

		missed_packets = pd->numpackets - Players[TheirPlayernum].n_packets_got;
		if ((pd->numpackets - Players[TheirPlayernum].n_packets_got) > 0)
			TotalMissedPackets += pd->numpackets - Players[TheirPlayernum].n_packets_got;

		if (missed_packets > 0)
			mprintf((0, "Missed %d packets from player #%d (%d total)\n", pd->numpackets - Players[TheirPlayernum].n_packets_got, TheirPlayernum, missed_packets));
		else
			mprintf((0, "Got %d late packets from player #%d (%d total)\n", Players[TheirPlayernum].n_packets_got - pd->numpackets, TheirPlayernum, missed_packets));

		Players[TheirPlayernum].n_packets_got = pd->numpackets;
	}

	//------------ Read the player's ship's object info ----------------------
	TheirObj->pos = pd->obj_pos;
	TheirObj->orient = pd->obj_orient;
	TheirObj->mtype.phys_info.velocity = pd->phys_velocity;
	TheirObj->mtype.phys_info.rotvel = pd->phys_rotvel;

	if ((TheirObj->render_type != pd->obj_render_type) && (pd->obj_render_type == RT_POLYOBJ))
		multi_make_ghost_player(TheirPlayernum);

	obj_relink(TheirObjnum, pd->obj_segnum);

	if (TheirObj->movement_type == MT_PHYSICS)
		set_thrust_from_velocity(TheirObj);

	//------------ Welcome them back if reconnecting --------------
	if (!Players[TheirPlayernum].connected) 
	{
		Players[TheirPlayernum].connected = 1;

		if (Newdemo_state == ND_STATE_RECORDING)
			newdemo_record_multi_reconnect(TheirPlayernum);

		multi_make_ghost_player(TheirPlayernum);

		create_player_appearance_effect(&Objects[TheirObjnum]);

		digi_play_sample(SOUND_HUD_MESSAGE, F1_0);

		ClipRank((int8_t*)&NetPlayers.players[TheirPlayernum].rank);

		if (FindArg("-norankings"))
			HUD_init_message("'%s' %s", Players[TheirPlayernum].callsign, TXT_REJOIN);
		else
			HUD_init_message("%s'%s' %s", RankStrings[NetPlayers.players[TheirPlayernum].rank], Players[TheirPlayernum].callsign, TXT_REJOIN);


		multi_send_score();
}

	//------------ Parse the extra data at the end ---------------

	if (pd->data_size > 0) {
		// pass pd->data to some parser function....
		multi_process_bigdata((char*)pd->data, pd->data_size);
	}
}


void network_read_pdata_short_packet(short_frame_info* pd)
{
	int TheirPlayernum;
	int TheirObjnum;
	object* TheirObj = NULL;
	short_frame_info new_pd;

	// short frame info is not aligned because of shortpos.  The mac
	// will call totally hacked and gross function to fix this up.

	memcpy(&new_pd, (char*)pd, sizeof(short_frame_info));

	TheirPlayernum = new_pd.playernum;
	TheirObjnum = Players[new_pd.playernum].objnum;

	if (TheirPlayernum < 0) 
	{
		Int3(); // This packet is bogus!!
		return;
	}
	if (!multi_quit_game && (TheirPlayernum >= N_players)) 
	{
		if (Network_status != NETSTAT_WAITING)
		{
			Int3(); // We missed an important packet!
			network_consistency_error();
			return;
}
		else
			return;
		}

	if (VerifyPlayerJoined != -1 && TheirPlayernum == VerifyPlayerJoined)
	{
		// Hurray! Someone really really got in the game (I think).
		mprintf((0, "Hurray! VPJ (%d) reset!\n", VerifyPlayerJoined));
		VerifyPlayerJoined = -1;
	}

	if (Endlevel_sequence || (Network_status == NETSTAT_ENDLEVEL)) 
	{
		int old_Endlevel_sequence = Endlevel_sequence;
		Endlevel_sequence = 1;
		if (new_pd.data_size > 0) 
		{
			// pass pd->data to some parser function....
			multi_process_bigdata((char*)new_pd.data, new_pd.data_size);
	}
		Endlevel_sequence = old_Endlevel_sequence;
		return;
	}
	//      mprintf((0, "Gametime = %d, Frametime = %d.\n", GameTime, FrameTime));

	if ((int8_t)new_pd.level_num != Current_level_num)
	{
		mprintf((0, "Got frame packet from player %d wrong level %d!\n", new_pd.playernum, new_pd.level_num));
		return;
	}

	TheirObj = &Objects[TheirObjnum];

	//------------- Keep track of missed packets -----------------
	Players[TheirPlayernum].n_packets_got++;
	TotalPacketsGot++;
	LastPacketTime[TheirPlayernum] = timer_get_approx_seconds();

	if (new_pd.numpackets != Players[TheirPlayernum].n_packets_got) 
	{
		int missed_packets;

		missed_packets = new_pd.numpackets - Players[TheirPlayernum].n_packets_got;
		if ((new_pd.numpackets - Players[TheirPlayernum].n_packets_got) > 0)
			TotalMissedPackets += new_pd.numpackets - Players[TheirPlayernum].n_packets_got;

		if (missed_packets > 0)
			mprintf((0, "Missed %d packets from player #%d (%d total)\n", new_pd.numpackets - Players[TheirPlayernum].n_packets_got, TheirPlayernum, missed_packets));
		else
			mprintf((0, "Got %d late packets from player #%d (%d total)\n", Players[TheirPlayernum].n_packets_got - new_pd.numpackets, TheirPlayernum, missed_packets));

		Players[TheirPlayernum].n_packets_got = new_pd.numpackets;
	}

	//------------ Read the player's ship's object info ----------------------

	extract_shortpos(TheirObj, &new_pd.thepos);

	if ((TheirObj->render_type != new_pd.obj_render_type) && (new_pd.obj_render_type == RT_POLYOBJ))
		multi_make_ghost_player(TheirPlayernum);

	if (TheirObj->movement_type == MT_PHYSICS)
		set_thrust_from_velocity(TheirObj);

	//------------ Welcome them back if reconnecting --------------
	if (!Players[TheirPlayernum].connected) 
	{
		Players[TheirPlayernum].connected = 1;

		if (Newdemo_state == ND_STATE_RECORDING)
			newdemo_record_multi_reconnect(TheirPlayernum);

		multi_make_ghost_player(TheirPlayernum);

		create_player_appearance_effect(&Objects[TheirObjnum]);

		digi_play_sample(SOUND_HUD_MESSAGE, F1_0);
		ClipRank((int8_t*)&NetPlayers.players[TheirPlayernum].rank);

		if (FindArg("-norankings"))
			HUD_init_message("'%s' %s", Players[TheirPlayernum].callsign, TXT_REJOIN);
		else
			HUD_init_message("%s'%s' %s", RankStrings[NetPlayers.players[TheirPlayernum].rank], Players[TheirPlayernum].callsign, TXT_REJOIN);


		multi_send_score();
	}

	//------------ Parse the extra data at the end ---------------

	if (new_pd.data_size > 0) 
	{
		// pass pd->data to some parser function....
		multi_process_bigdata((char*)new_pd.data, new_pd.data_size);
	}
}

void network_set_power(void)
{
	int opt = 0, choice, opt_primary, opt_second, opt_power;
	newmenu_item m[40];

	opt_primary = opt;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Laser upgrade"); m[opt].value = Netgame.DoLaserUpgrade; opt++;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Super lasers"); m[opt].value = Netgame.DoSuperLaser; opt++;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Quad Lasers"); m[opt].value = Netgame.DoQuadLasers; opt++;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Vulcan cannon"); m[opt].value = Netgame.DoVulcan; opt++;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Spreadfire cannon"); m[opt].value = Netgame.DoSpread; opt++;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Plasma cannon"); m[opt].value = Netgame.DoPlasma; opt++;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Fusion cannon"); m[opt].value = Netgame.DoFusions; opt++;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Gauss cannon"); m[opt].value = Netgame.DoGauss; opt++;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Helix cannon"); m[opt].value = Netgame.DoHelix; opt++;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Phoenix cannon"); m[opt].value = Netgame.DoPhoenix; opt++;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Omega cannon"); m[opt].value = Netgame.DoOmega; opt++;

	opt_second = opt;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Homing Missiles"); m[opt].value = Netgame.DoHoming; opt++;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Proximity Bombs"); m[opt].value = Netgame.DoProximity; opt++;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Smart Missiles"); m[opt].value = Netgame.DoSmarts; opt++;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Mega Missiles"); m[opt].value = Netgame.DoMegas; opt++;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Flash Missiles"); m[opt].value = Netgame.DoFlash; opt++;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Guided Missiles"); m[opt].value = Netgame.DoGuided; opt++;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Smart Mines"); m[opt].value = Netgame.DoSmartMine; opt++;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Mercury Missiles"); m[opt].value = Netgame.DoMercury; opt++;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("EarthShaker Missiles"); m[opt].value = Netgame.DoEarthShaker; opt++;

	opt_power = opt;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Invulnerability"); m[opt].value = Netgame.DoInvulnerability; opt++;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Cloaking"); m[opt].value = Netgame.DoCloak; opt++;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Afterburners"); m[opt].value = Netgame.DoAfterburner; opt++;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Ammo rack"); m[opt].value = Netgame.DoAmmoRack; opt++;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Energy Converter"); m[opt].value = Netgame.DoConverter; opt++;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Headlight"); m[opt].value = Netgame.DoHeadlight; opt++;

	choice = newmenu_do(NULL, "Objects to allow", opt, m, NULL);

	Netgame.DoLaserUpgrade = m[opt_primary].value;
	Netgame.DoSuperLaser = m[opt_primary + 1].value;
	Netgame.DoQuadLasers = m[opt_primary + 2].value;
	Netgame.DoVulcan = m[opt_primary + 3].value;
	Netgame.DoSpread = m[opt_primary + 4].value;
	Netgame.DoPlasma = m[opt_primary + 5].value;
	Netgame.DoFusions = m[opt_primary + 6].value;
	Netgame.DoGauss = m[opt_primary + 7].value;
	Netgame.DoHelix = m[opt_primary + 8].value;
	Netgame.DoPhoenix = m[opt_primary + 9].value;
	Netgame.DoOmega = m[opt_primary + 10].value;

	Netgame.DoHoming = m[opt_second].value;
	Netgame.DoProximity = m[opt_second + 1].value;
	Netgame.DoSmarts = m[opt_second + 2].value;
	Netgame.DoMegas = m[opt_second + 3].value;
	Netgame.DoFlash = m[opt_second + 4].value;
	Netgame.DoGuided = m[opt_second + 5].value;
	Netgame.DoSmartMine = m[opt_second + 6].value;
	Netgame.DoMercury = m[opt_second + 7].value;
	Netgame.DoEarthShaker = m[opt_second + 8].value;

	Netgame.DoInvulnerability = m[opt_power].value;
	Netgame.DoCloak = m[opt_power + 1].value;
	Netgame.DoAfterburner = m[opt_power + 2].value;
	Netgame.DoAmmoRack = m[opt_power + 3].value;
	Netgame.DoConverter = m[opt_power + 4].value;
	Netgame.DoHeadlight = m[opt_power + 5].value;

}

void SetAllAllowablesTo(int on)
{
	last_cinvul = control_invul_time = 0;   //default to zero

	Netgame.DoMegas = Netgame.DoSmarts = Netgame.DoFusions = Netgame.DoHelix = \
		Netgame.DoPhoenix = Netgame.DoCloak = Netgame.DoInvulnerability = \
		Netgame.DoAfterburner = Netgame.DoGauss = Netgame.DoVulcan = Netgame.DoPlasma = on;

	Netgame.DoOmega = Netgame.DoSuperLaser = Netgame.DoProximity = \
		Netgame.DoSpread = Netgame.DoMercury = Netgame.DoSmartMine = Netgame.DoFlash = \
		Netgame.DoGuided = Netgame.DoEarthShaker = on;

	Netgame.DoConverter = Netgame.DoAmmoRack = Netgame.DoHeadlight = on;
	Netgame.DoHoming = Netgame.DoLaserUpgrade = Netgame.DoQuadLasers = on;
	Netgame.BrightPlayers = Netgame.invul = on;
}

fix LastPTA;
int LastKillGoal;

// Jeez -- mac compiler can't handle all of these on the same decl line.
int opt_setpower, opt_playtime, opt_killgoal, opt_socket, opt_marker_view, opt_light;
int opt_difficulty, opt_packets, opt_short_packets, opt_bright, opt_start_invul;
int opt_show_names;

void network_more_options_poll(int nitems, newmenu_item* menus, int* key, int citem);

void network_more_game_options()
{
	int opt = 0, i;
	char PlayText[80], KillText[80], srinvul[50], socket_string[5], packstring[5];
	newmenu_item m[21];

	sprintf(socket_string, "%d", Network_socket);
	sprintf(packstring, "%d", Netgame.PacketsPerSec);

	opt_difficulty = opt;
	m[opt].type = NM_TYPE_SLIDER; m[opt].value = netgame_difficulty; m[opt].text = TXT_DIFFICULTY; m[opt].min_value = 0; m[opt].max_value = (NDL - 1); opt++;

	opt_cinvul = opt;
	sprintf(srinvul, "%s: %d %s", TXT_REACTOR_LIFE, control_invul_time * 5, TXT_MINUTES_ABBREV);
	m[opt].type = NM_TYPE_SLIDER; m[opt].value = control_invul_time; m[opt].text = srinvul; m[opt].min_value = 0; m[opt].max_value = 10; opt++;

	opt_playtime = opt;
	sprintf(PlayText, "Max time: %d %s", Netgame.PlayTimeAllowed * 5, TXT_MINUTES_ABBREV);
	m[opt].type = NM_TYPE_SLIDER; m[opt].value = Netgame.PlayTimeAllowed; m[opt].text = PlayText; m[opt].min_value = 0; m[opt].max_value = 10; opt++;

	opt_killgoal = opt;
	sprintf(KillText, "Kill Goal: %d kills", Netgame.KillGoal * 5);
	m[opt].type = NM_TYPE_SLIDER; m[opt].value = Netgame.KillGoal; m[opt].text = KillText; m[opt].min_value = 0; m[opt].max_value = 10; opt++;

	opt_start_invul = opt;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Invulnerable when reappearing"); m[opt].value = Netgame.invul; opt++;

	opt_marker_view = opt;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Allow camera views from Markers"); m[opt].value = Netgame.Allow_marker_view; opt++;
	opt_light = opt;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Indestructible lights"); m[opt].value = Netgame.AlwaysLighting; opt++;

	opt_bright = opt;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Bright player ships"); m[opt].value = Netgame.BrightPlayers; opt++;

	opt_show_names = opt;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Show enemy names on HUD"); m[opt].value = Netgame.ShowAllNames; opt++;

	opt_show_on_map = opt;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = TXT_SHOW_ON_MAP; m[opt].value = (Netgame.game_flags & NETGAME_FLAG_SHOW_MAP); opt_show_on_map = opt; opt++;

	opt_short_packets = opt;
	m[opt].type = NM_TYPE_CHECK; m[opt].text = const_cast<char*>("Short packets"); m[opt].value = Netgame.ShortPackets; opt++;

	opt_setpower = opt;
	m[opt].type = NM_TYPE_MENU;  m[opt].text = const_cast<char*>("Set Objects allowed..."); opt++;

	m[opt].type = NM_TYPE_TEXT; m[opt].text = const_cast<char*>("Network socket"); opt++;
	opt_socket = opt;
	m[opt].type = NM_TYPE_INPUT; m[opt].text = socket_string; m[opt].text_len = 4; opt++;
	
	m[opt].type = NM_TYPE_TEXT; m[opt].text = const_cast<char*>("Packets per second (2 - 20)"); opt++;
	opt_packets = opt;
	m[opt].type = NM_TYPE_INPUT; m[opt].text = packstring; m[opt].text_len = 2; opt++;

	LastKillGoal = Netgame.KillGoal;
	LastPTA = Netgame.PlayTimeAllowed;

menu:

	ExtGameStatus = GAMESTAT_MORE_NETGAME_OPTIONS;
	i = newmenu_do1(NULL, "Additional netgame options", opt, m, network_more_options_poll, 0);

	//control_invul_time = atoi( srinvul )*60*F1_0;
	control_invul_time = m[opt_cinvul].value;
	Netgame.control_invul_time = control_invul_time * 5 * F1_0 * 60;

	if (i == opt_setpower)
	{
		network_set_power();
		goto menu;
	}

	Netgame.PacketsPerSec = atoi(packstring);

	if (Netgame.PacketsPerSec > 20)
	{
		Netgame.PacketsPerSec = 20;
		nm_messagebox(TXT_ERROR, 1, TXT_OK, "Packet value out of range\nSetting value to 20");
	}
	if (Netgame.PacketsPerSec < 2)
	{
		nm_messagebox(TXT_ERROR, 1, TXT_OK, "Packet value out of range\nSetting value to 2");
		Netgame.PacketsPerSec = 2;
	}

	mprintf((0, "Hey! Sending out %d packets per second\n", Netgame.PacketsPerSec));

	if ((atoi(socket_string)) != Network_socket)
	{
		Network_socket = atoi(socket_string);
		NetChangeDefaultSocket(IPX_DEFAULT_SOCKET + Network_socket);
	}

	Netgame.invul = m[opt_start_invul].value;
	Netgame.BrightPlayers = m[opt_bright].value;
	Netgame.ShortPackets = m[opt_short_packets].value;
	Netgame.ShowAllNames = m[opt_show_names].value;
	network_AdjustMaxDataSize();

	Netgame.Allow_marker_view = m[opt_marker_view].value;
	Netgame.AlwaysLighting = m[opt_light].value;
	netgame_difficulty = Difficulty_level = m[opt_difficulty].value;
	if (m[opt_show_on_map].value)
		Netgame.game_flags |= NETGAME_FLAG_SHOW_MAP;

}

void network_more_options_poll(int nitems, newmenu_item* menus, int* key, int citem)
{
	menus = menus;
	citem = citem;      // kills compile warnings
	nitems = nitems;
	key = key;

	if (last_cinvul != menus[opt_cinvul].value) 
	{
		sprintf(menus[opt_cinvul].text, "%s: %d %s", TXT_REACTOR_LIFE, menus[opt_cinvul].value * 5, TXT_MINUTES_ABBREV);
		last_cinvul = menus[opt_cinvul].value;
		menus[opt_cinvul].redraw = 1;
	}

	if (menus[opt_playtime].value != LastPTA)
	{
#ifdef SHAREWARE
		LastPTA = 0;
		nm_messagebox("Sorry", 1, TXT_OK, "Registered version only!");
		menus[opt_playtime].value = 0;
		menus[opt_playtime].redraw = 1;
		return;
#endif  

		if (Game_mode & GM_MULTI_COOP)
		{
			LastPTA = 0;
			nm_messagebox("Sorry", 1, TXT_OK, "You can't change those for coop!");
			menus[opt_playtime].value = 0;
			menus[opt_playtime].redraw = 1;
			return;
		}

		Netgame.PlayTimeAllowed = menus[opt_playtime].value;
		sprintf(menus[opt_playtime].text, "Max Time: %d %s", Netgame.PlayTimeAllowed * 5, TXT_MINUTES_ABBREV);
		LastPTA = Netgame.PlayTimeAllowed;
		menus[opt_playtime].redraw = 1;
	}
	if (menus[opt_killgoal].value != LastKillGoal)
	{
#ifdef SHAREWARE
		nm_messagebox("Sorry", 1, TXT_OK, "Registered version only!");
		menus[opt_killgoal].value = 0;
		menus[opt_killgoal].redraw = 1;
		LastKillGoal = 0;
		return;
#endif         

		if (Game_mode & GM_MULTI_COOP)
		{
			nm_messagebox("Sorry", 1, TXT_OK, "You can't change those for coop!");
			menus[opt_killgoal].value = 0;
			menus[opt_killgoal].redraw = 1;
			LastKillGoal = 0;
			return;
		}

		Netgame.KillGoal = menus[opt_killgoal].value;
		sprintf(menus[opt_killgoal].text, "Kill Goal: %d kills", Netgame.KillGoal * 5);
		LastKillGoal = Netgame.KillGoal;
		menus[opt_killgoal].redraw = 1;
		}
	}

extern void multi_send_light(int, uint8_t);
extern void multi_send_light_specific(int, int, uint8_t);

void network_send_smash_lights(int pnum)
{
	// send the lights that have been blown out

	int i;

	pnum = pnum;

	for (i = 0; i <= Highest_segment_index; i++)
		if (Light_subtracted[i])
			multi_send_light_specific(pnum, i, Light_subtracted[i]);
}

extern void multi_send_trigger_specific(char pnum, char);

void network_send_fly_thru_triggers(int pnum)
{
	// send the fly thru triggers that have been disabled

	char i;

	for (i = 0; i < Triggers.size(); i++)
		if (Triggers[i].flags & TF_DISABLED)
			multi_send_trigger_specific(pnum, i);
}

void network_send_player_flags()
{
	int i;

	for (i = 0; i < N_players; i++)
		multi_send_flags(i);
}

void network_ping(uint8_t flag, int pnum)
{
	char mybuf[2];

	mybuf[0] = flag;
	mybuf[1] = Player_num;

	NetSendPacket((char*)mybuf, 2, NetPlayers.players[pnum].network.ipx.node, Players[pnum].net_address);
}

extern fix PingLaunchTime, PingReturnTime;

void network_handle_ping_return(uint8_t pnum)
{
	if (PingLaunchTime == 0 || pnum >= N_players)
	{
		mprintf((0, "Got invalid PING RETURN from %s!\n", Players[pnum].callsign));
		return;
	}

	PingReturnTime = timer_get_fixed_seconds();

	HUD_init_message("Ping time for %s is %d ms!", Players[pnum].callsign,
		f2i(fixmul(PingReturnTime - PingLaunchTime, i2f(1000))));
	PingLaunchTime = 0;
}

void network_send_ping(uint8_t pnum)
{
	network_ping(PID_PING_SEND, pnum);
}

void DoRefuseStuff(sequence_packet* their)
{
	int i, new_player_num;

	ClipRank((int8_t*)&their->player.rank);

	for (i = 0; i < MAX_PLAYERS; i++)
		if (!strcmp(their->player.callsign, Players[i].callsign))
		{
			network_welcome_player(their);
			return;
}

	if (!WaitForRefuseAnswer)
	{
		for (i = 0; i < MAX_PLAYERS; i++)
			if (!strcmp(their->player.callsign, Players[i].callsign))
			{
				network_welcome_player(their);
				return;
			}

		digi_play_sample(SOUND_HUD_JOIN_REQUEST, F1_0 * 2);

		if (Game_mode & GM_TEAM)
		{


			if (!FindArg("-norankings"))
				HUD_init_message("%s %s wants to join", RankStrings[their->player.rank], their->player.callsign);

			HUD_init_message("%s joining. Alt-1 assigns to team %s. Alt-2 to team %s", their->player.callsign, Netgame.team_name[0], Netgame.team_name[1]);
			//                      HUD_init_message ("Alt-1 to place on team %s!",Netgame.team_name[0]);
			//                      HUD_init_message ("Alt-2 to place on team %s!",Netgame.team_name[1]);
		}
		else
			HUD_init_message("%s wants to join...press F6 to connect", their->player.callsign);

		strcpy(RefusePlayerName, their->player.callsign);
		RefuseTimeLimit = timer_get_approx_seconds();
		RefuseThisPlayer = 0;
		WaitForRefuseAnswer = 1;
	}
	else
	{
		for (i = 0; i < MAX_PLAYERS; i++)
			if (!strcmp(their->player.callsign, Players[i].callsign))
			{
				network_welcome_player(their);
				return;
			}

		if (strcmp(their->player.callsign, RefusePlayerName))
			return;

		if (RefuseThisPlayer)
		{
			RefuseTimeLimit = 0;
			RefuseThisPlayer = 0;
			WaitForRefuseAnswer = 0;
			if (Game_mode & GM_TEAM)
			{
				new_player_num = GetNewPlayerNumber(their);
				mprintf((0, "Newplayernum=%d\n", new_player_num));

				Assert(RefuseTeam == 1 || RefuseTeam == 2);

				if (RefuseTeam == 1)
					Netgame.team_vector &= (~(1 << new_player_num));
				else
					Netgame.team_vector |= (1 << new_player_num);
				network_welcome_player(their);
				network_send_netgame_update();
			}
			else
				network_welcome_player(their);
			return;
		}

		if ((timer_get_approx_seconds()) > RefuseTimeLimit + REFUSE_INTERVAL)
		{
			RefuseTimeLimit = 0;
			RefuseThisPlayer = 0;
			WaitForRefuseAnswer = 0;
			if (!strcmp(their->player.callsign, RefusePlayerName)) 
				network_dump_player(their->player.network.ipx.node, DUMP_DORK);
			return;
		}

	}
}

int GetNewPlayerNumber(sequence_packet* their)
{
	int i;

	their = their;

	if (N_players < MaxNumNetPlayers)
		return (N_players);

	else
	{
		// Slots are full but game is open, see if anyone is
		// disconnected and replace the oldest player with this new one

		int oldest_player = -1;
		fix oldest_time = timer_get_approx_seconds();

		Assert(N_players == MaxNumNetPlayers);

		for (i = 0; i < N_players; i++)
		{
			if ((!Players[i].connected) && (LastPacketTime[i] < oldest_time))
			{
				oldest_time = LastPacketTime[i];
				oldest_player = i;
			}
		}
		return (oldest_player);
	}
}

void network_send_extras()
{
	Assert(Player_joining_extras > -1);

	if (!network_i_am_master())
	{
		mprintf((0, "Hey! I'm not the master and I was gonna send info!\n"));
		// Int3();     
		// Network_sending_extras=0;
		// return;
	}

	if (Network_sending_extras == 40)
		network_send_fly_thru_triggers(Player_joining_extras);
	if (Network_sending_extras == 38)
		network_send_door_updates(Player_joining_extras);
	if (Network_sending_extras == 35)
		network_send_markers();
	if (Network_sending_extras == 30 && (Game_mode & GM_MULTI_ROBOTS))
		multi_send_stolen_items();
	if (Network_sending_extras == 25 && (Netgame.PlayTimeAllowed || Netgame.KillGoal))
		multi_send_kill_goal_counts();
	if (Network_sending_extras == 20)
		network_send_smash_lights(Player_joining_extras);
	if (Network_sending_extras == 15)
		network_send_player_flags();
	if (Network_sending_extras == 10)
		multi_send_powerup_update();
	//  if (Network_sending_extras==5)
   //		network_send_door_updates(Player_joining_extras); // twice!

	Network_sending_extras--;
	if (!Network_sending_extras)
		Player_joining_extras = -1;
}


void network_send_naked_packet(char* buf, short len, int who)
{
	if (!(Game_mode & GM_NETWORK)) return;

	if (NakedPacketLen == 0)
	{
		NakedBuf[0] = PID_NAKED_PDATA;
		NakedBuf[1] = Player_num;
		NakedPacketLen = 2;
	}

	if (len + NakedPacketLen > MaxXDataSize)
	{
		//         mprintf ((0,"Sending a naked packet of %d bytes.\n",NakedPacketLen));
		NetSendPacket((char*)NakedBuf, NakedPacketLen, NetPlayers.players[who].network.ipx.node, Players[who].net_address);

		NakedPacketLen = 2;
		memcpy(&NakedBuf[NakedPacketLen], buf, len);
		NakedPacketLen += len;
		NakedPacketDestPlayer = who;
	}
	else
	{
		memcpy(&NakedBuf[NakedPacketLen], buf, len);
		NakedPacketLen += len;
		NakedPacketDestPlayer = who;
	}
}


void network_process_naked_pdata(char* data, int len)
{
	int pnum = data[1];
	Assert(data[0] = PID_NAKED_PDATA);

	//   mprintf ((0,"Processing a naked packet of %d length.\n",len));

	if (pnum < 0)
	{
		mprintf((0, "Naked packet is bad!\n"));
		Int3(); // This packet is bogus!!
		return;
	}

	if (!multi_quit_game && (pnum >= N_players)) 
	{
		if (Network_status != NETSTAT_WAITING)
		{
			Int3(); // We missed an important packet!
			network_consistency_error();
			return;
		}
		else
			return;
	}

	if (Endlevel_sequence || (Network_status == NETSTAT_ENDLEVEL)) 
	{
		int old_Endlevel_sequence = Endlevel_sequence;
		Endlevel_sequence = 1;
		multi_process_bigdata(data + 2, len - 2);
		Endlevel_sequence = old_Endlevel_sequence;
		return;
	}

	multi_process_bigdata(data + 2, len - 2);
}

int ConnectionPacketLevel[] = { 0,1,1,1 };
int ConnectionSecLevel[] = { 12,3,5,7 };

int AppletalkConnectionPacketLevel[] = { 0,1,0 };
int AppletalkConnectionSecLevel[] = { 10,3,8 };

int network_choose_connect()
{
	return (1);
}

#if 0
int network_choose_connect()
{
	newmenu_item m[16];
	int choice, opt = 0;

	if (Network_game_type == IPX_GAME)
	{
#if 0
		m[opt].type = NM_TYPE_MENU;  m[opt].text = "Local Subnet"; opt++;
		m[opt].type = NM_TYPE_MENU;  m[opt].text = "14.4 modem over Internet"; opt++;
		m[opt].type = NM_TYPE_MENU;  m[opt].text = "28.8 modem over Internet"; opt++;
		m[opt].type = NM_TYPE_MENU;  m[opt].text = "ISDN or T1 over Internet"; opt++;

		choice = newmenu_do1(NULL, "Choose connection type", opt, m, NULL, 0);

		if (choice < 0)
			return (NULL);

		Assert(choice >= 0 && choice <= 3);

		Netgame.ShortPackets = ConnectionPacketLevel[choice];
		Netgame.PacketsPerSec = ConnectionSecLevel[choice];
#endif
		return (1);
	}
	return (0);
}
#endif



#ifdef NETPROFILING
void OpenSendLog()
{
	int i;
	SendLogFile = (FILE*)fopen("sendlog.net", "w");
	for (i = 0; i < 100; i++)
		TTSent[i] = 0;
}
void OpenRecieveLog()
{
	int i;
	RecieveLogFile = (FILE*)fopen("recvlog.net", "w");
	for (i = 0; i < 100; i++)
		TTRecv[i] = 0;
}
#endif

int GetMyNetRanking()
{
	int rank;
	int eff;

	if (Netlife_kills + Netlife_killed == 0)
		return (1);

	rank = (int)(((float)Netlife_kills / 3000.0) * 8.0);
#ifndef RELEASE
	if (FindArg("-interplay"))
		rank = (int)(((float)Netlife_kills / 30.0) * 8.0);
#endif

	eff = (int)((float)((float)Netlife_kills / ((float)Netlife_killed + (float)Netlife_kills)) * 100.0);

	if (rank > 8)
		rank = 8;

	if (eff < 0)
		eff = 0;

	if (eff < 60)
		rank -= ((59 - eff) / 10);

	if (rank < 0)
		rank = 0;
	if (rank > 8)
		rank = 8;

	mprintf((0, "Rank is %d (%s)\n", rank + 1, RankStrings[rank + 1]));
	return (rank + 1);
}

void ClipRank(int8_t* rank)
{
	// This function insures no crashes when dealing with D2 1.0


	if (*rank < 0 || *rank>9)
		*rank = 0;
}
void network_check_for_old_version(char pnum)
{
	if (NetPlayers.players[pnum].version_major == 1 && (NetPlayers.players[pnum].version_minor & 0x0F) == 0)
		NetPlayers.players[pnum].rank = 0;
}

void network_request_player_names(int n)
{
	network_send_all_info_request(PID_GAME_PLAYERS, Active_games[n].Security);
	NamesInfoSecurity = Active_games[n].Security;
}

extern char already_showing_info;
extern int newmenu_dotiny2(const char* title, const char* subtitle, int nitems, newmenu_item* item, void (*subfunction)(int nitems, newmenu_item* items, int* last_key, int citem));

void network_process_names_return(char* data)
{
	newmenu_item m[15];
	char mtext[15][50], temp[50];
	int i, t, l, gnum, num = 0, count = 5, numplayers;

	if (NamesInfoSecurity != (*(int*)(data + 1)))
	{
		mprintf((0, "Bad security on names return!\n"));
		mprintf((0, "NIS=%d data=%d\n", NamesInfoSecurity, (*(int*)(data + 1))));
		return;
	}

	numplayers = data[count]; count++;

	if (numplayers == 255)
	{
		SurfingNet = 0;
		NamesInfoSecurity = -1;
		nm_messagebox(NULL, 1, "OK", "That game is refusing\nname requests.\n");
		SurfingNet = 1;
		return;
	}

	Assert(numplayers > 0 && numplayers < MAX_NUM_NET_PLAYERS);

	for (i = 0; i < 12; i++)
	{
		m[i].text = &mtext[i][0];
		m[i].type = NM_TYPE_TEXT;
	}

	for (gnum = -1, i = 0; i < num_active_games; i++)
	{
		if (NamesInfoSecurity == Active_games[i].Security)
		{
			gnum = i;
			break;
		}
	}

	if (gnum == -1)
	{
		SurfingNet = 0;
		NamesInfoSecurity = -1;
		nm_messagebox(NULL, 1, "OK", "The game you have requested\nnames from is gone.\n");
		SurfingNet = 1;
		return;
	}

	sprintf(mtext[num], "Players of game '%s':", Active_games[gnum].game_name); num++;
	for (i = 0; i < numplayers; i++)
	{
		l = data[count++];

		mprintf((0, "%s\n", data + count));

		for (t = 0; t < CALLSIGN_LEN + 1; t++)
			temp[t] = data[count++];
		if (FindArg("-norankings"))
			sprintf(mtext[num], "%s", temp);
		else
			sprintf(mtext[num], "%s%s", RankStrings[l], temp);

		num++;
	}

	if (data[count] == 99)
	{
		sprintf(mtext[num++], " ");
		sprintf(mtext[num++], "Short packets: %s", data[count + 1] ? "On" : "Off");
		sprintf(mtext[num++], "Packets Per Second: %d", data[count + 2]);
	}

	already_showing_info = 1;
	newmenu_dotiny2(NULL, NULL, num, m, NULL);
	already_showing_info = 0;
}

char NameReturning = 1;

void network_send_player_names(sequence_packet* their)
{
	int numconnected = 0, count = 0, i, t;
	char buf[80];

	if (!their)
	{
		mprintf((0, "Got a player name without a return address! Get Jason\n"));
		return;
	}

	buf[0] = PID_NAMES_RETURN; count++;
	(*(int*)(buf + 1)) = Netgame.Security; count += 4;

	if (!NameReturning)
	{
		buf[count] = 255; count++;
		goto sendit;
	}

	mprintf((0, "RealSec=%d DS=%d\n", Netgame.Security, *(int*)(buf + 1)));

	for (i = 0; i < N_players; i++)
		if (Players[i].connected)
			numconnected++;

	buf[count] = numconnected; count++;
	for (i = 0; i < N_players; i++)
		if (Players[i].connected)
		{
			buf[count++] = NetPlayers.players[i].rank;

			for (t = 0; t < CALLSIGN_LEN + 1; t++)
			{
				buf[count] = NetPlayers.players[i].callsign[t];
				count++;
			}
		}

	buf[count++] = 99;
	buf[count++] = Netgame.ShortPackets;
	buf[count++] = Netgame.PacketsPerSec;

sendit:
	;		// empty statement allows compilation on mac which does not have the
			// statement below and kills the compiler when there is no
			// statement following the label "sendit"

	NetSendInternetworkPacket((char*)buf, count, their->player.network.ipx.node);
}

#endif

extern int file_exists(const char*);

int HoardEquipped()
{
	static int checked = -1;

#ifdef WINDOWS
	return 0;
#endif

	if (checked == -1)
	{
#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
		char hoard_full_path[CHOCOLATE_MAX_FILE_PATH_SIZE];
		get_full_file_path(hoard_full_path, "hoard.ham", CHOCOLATE_SYSTEM_FILE_DIR);
		if (file_exists(hoard_full_path))
#else
		if (file_exists("hoard.ham"))
#endif
			checked = 1;
		else
			checked = 0;
	}
	return (checked);
}

