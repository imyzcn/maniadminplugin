//
// Mani Admin Plugin
//
// Copyright � 2009-2015 Giles Millward (Mani). All rights reserved.
//
// This file is part of ManiAdminPlugin.
//
// Mani Admin Plugin is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Mani Admin Plugin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Mani Admin Plugin.  If not, see <http://www.gnu.org/licenses/>.
//
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "interface.h"
#include "filesystem.h"
#include "engine/iserverplugin.h"
#include "iplayerinfo.h"
#include "eiface.h"
#include "igameevents.h"
#include "mrecipientfilter.h"
#include "bitbuf.h"
#include "engine/IEngineSound.h"
#include "inetchannelinfo.h"
#include "ivoiceserver.h"
#include "networkstringtabledefs.h"
#include "mani_main.h"
#include "mani_convar.h"
#include "mani_parser.h"
#include "mani_player.h"
#include "mani_language.h"
#include "mani_menu.h"
#include "mani_memory.h"
#include "mani_output.h"
#include "mani_client_flags.h"
#include "mani_client.h"
#include "mani_gametype.h"
#include "mani_reservedslot.h"
#include "shareddefs.h"
#include "steamclientpublic.h"
#include "mani_detours.h"
#include "detour_macros.h"
#include "mani_mainclass.h"
#include "mani_playerkick.h" 
#if defined ( GAME_CSGO )
#include "iclient.h"
#endif

int m_iUnaccountedPlayers = 0;

extern	IVEngineServer *engine;
extern	IVoiceServer *voiceserver;
extern IFileSystem	*filesystem;

extern	IPlayerInfoManager *playerinfomanager;
extern	CGlobalVars *gpGlobals;
extern	int	max_players;
extern	bool war_mode;
extern void *connect_client_addr;
extern void *netsendpacket_addr;
extern	ICvar *g_pCVar;
extern	IServerPluginHelpers *helpers;
extern	IServerPluginCallbacks *gpManiISPCCallback;

static int sort_active_players_by_ping ( const void *m1,  const void *m2);
static int sort_active_players_by_connect_time ( const void *m1,  const void *m2);
static int sort_active_players_by_kill_rate ( const void *m1,  const void *m2);
static int sort_active_players_by_kd_ratio ( const void *m1,  const void *m2);

static int sort_reserve_slots_by_steam_id ( const void *m1,  const void *m2);
static CDetour * ManiClientConnectDetour;
static CDetour * ManiNetSendPacketDetour;

inline bool FStruEq(const char *sz1, const char *sz2)
{
	return(Q_strcmp(sz1, sz2) == 0);
}

inline bool FStrEq(const char *sz1, const char *sz2)
{
	return(Q_stricmp(sz1, sz2) == 0);
}

bool FillSlotMode() {
	if ( war_mode )
		return false;

	if ( !mani_reserve_slots.GetBool() )
		return false;

	if ( (mani_reserve_slots_allow_slot_fill.GetInt() == 0) && (mani_reserve_slots_number_of_slots.GetInt() != 0))
		return false;

	return true;
	
}

#define COPY_BYTE(target) \
	target = data[0]; \
	data++; \
	if ( (int)((mem_t *)data-start) > data_len ) \
		return false

#define COPY_STRING(target) \
	strcount = 0; \
	while (data[0] != 0) { \
		target[strcount++]=data[0]; \
		data++; \
	} \
	data++; \
	if ( (int)((mem_t *)data-start) > data_len ) \
		return false

#define COPY_SHORT(target) \
	memcpy (target, data, 2); \
	data+=2; \
	if ( (int)((mem_t *)data-start) > data_len ) \
		return false

#define COPY_BOOL(target) \
	target = ( data[0] != 0); \
	data++; \
	if ( (int)((mem_t *)data-start) > data_len ) \
		return false

// we don't actually have to fill this data structure, but thought it might be easier
// to work with if we did when changes come about.
bool FillINFOQuery ( const mem_t* data, int data_len, A2S_INFO_t &info, mem_t **pPlayers, mem_t **pPassword ) {
	if ( (data[0]!=0xFF) && (data[1]!=0xFF) && (data[2]!=0xFF) && (data[3]!=0xFF) ) return false;
	int strcount=0;
	mem_t* start = (mem_t *)data;

	//byte	type;
	//byte	netversion;
	//char	server_name[256];
	//char	map[256];
	//char	gamedir[256];
	//char	gamedesc[256];
	//short	appid;
	//byte	players;
	//byte	maxplayers;
	//byte	bots;
	//char	dedicated;
	//char	os;
	//bool	passwordset;
	//bool	secure;
	//char	version[256];

	data+=4;
	COPY_BYTE(info.type);
	COPY_BYTE(info.netversion);
	COPY_STRING(info.server_name);
	COPY_STRING(info.map);
	COPY_STRING(info.gamedir);
	COPY_STRING(info.gamedesc);
	COPY_SHORT(&info.appid);
	*pPlayers = (mem_t *)data;
	COPY_BYTE(info.players);
	COPY_BYTE(info.maxplayers);
	COPY_BYTE(info.bots);
	COPY_BYTE(info.dedicated);
	COPY_BYTE(info.os);
	*pPassword = (mem_t *)data;
	COPY_BOOL(info.passwordset);
	COPY_BOOL(info.secure);
	COPY_STRING(info.version);

	return true;
}

#if defined ( GAME_CSGO )
#define CCD_MEMBER_CALL(pw) MEMBER_CALL(ConnectClientDetour)(p1,p2,p3,p4,p5,pw,p7,p8,p9,pA,pB)
DECL_MEMBER_DETOUR11_void(ConnectClientDetour, void *, int, int, int, const char *, const char *, const char*, int, void *, bool, CrossPlayPlatform_t ) {
	CSteamID SteamID;
	Q_memset ( &SteamID, 0, sizeof(SteamID) );
	if ( p8 >= 20 )
		memcpy ( &SteamID, &p7[20], sizeof(SteamID) );
#elif defined ( GAME_ORANGE )
#define CCD_MEMBER_CALL(pw) MEMBER_CALL(ConnectClientDetour)(p1,p2,p3,p4,p5,p6,pw,p8,p9)
DECL_MEMBER_DETOUR9_void(ConnectClientDetour, void *, int, int, int, int, const char *, const char *, const char*, int ) {
	CSteamID SteamID;
	Q_memset ( &SteamID, 0, sizeof(SteamID) );
	if ( p9 >= 20 )
		memcpy ( &SteamID, &p8[20], sizeof(SteamID) );
#else
#define CCD_MEMBER_CALL(pw) MEMBER_CALL(ConnectClientDetour)(p1,p2,p3,p4,p5,pw,p7,p8,p9,pA)
DECL_MEMBER_DETOUR10_void(ConnectClientDetour, void *, int, int, int, const char *, const char *, const char*, int, char const*, int ) {
	CSteamID SteamID;
	Q_memset ( &SteamID, 0, sizeof(SteamID) );
	if ( pA >= 16 )
		memcpy ( &SteamID, &p9[8], sizeof(SteamID) );
#endif
	int total_players = m_iUnaccountedPlayers + GetNumberOfActivePlayers(true);
	player_t player;
	Q_memset ( &player, 0, sizeof(player) );
	strcpy ( player.steam_id, SteamID.Render() );
	bool AdminAccess = gpManiClient->HasAccess(&player, ADMIN, ADMIN_BASIC_ADMIN) && ( mani_reserve_slots_include_admin.GetInt() == 1 );

	if ( FillSlotMode() ) {

		if ( total_players == max_players ) {
			if(SteamID.GetEAccountType() != 1 || SteamID.GetEUniverse() != 1)
#if defined ( GAME_CSGO )
				return CCD_MEMBER_CALL(p6);
#elif defined ( GAME_ORANGE )
				return CCD_MEMBER_CALL(p7);
#else
				return CCD_MEMBER_CALL(p6);
#endif
			bool ReservedAccess = gpManiReservedSlot->IsPlayerInReserveList(&player);

			if ( AdminAccess || ReservedAccess ) {
				int kick_index = gpManiReservedSlot->FindPlayerToKick();

				if ( kick_index < 1 ) {
					engine->LogPrint("MAP:  Error, couldn't find anybody to kick for reserved slots!!!\n");

#if defined ( GAME_CSGO )
					return CCD_MEMBER_CALL(p6);
#elif defined ( GAME_ORANGE )
					return CCD_MEMBER_CALL(p7);
#else
					return CCD_MEMBER_CALL(p6);
#endif
				}

				Q_memset ( &player, 0, sizeof(player) );
				player.index = kick_index;
				FindPlayerByIndex(&player);
				gpManiReservedSlot->DisconnectPlayer(&player);
			}
		}
	}

	ConVar *pwd = g_pCVar->FindVar( "sv_password" );

	if ( pwd && !FStrEq(pwd->GetString(),"")) {
		if ( AdminAccess && !war_mode && !mani_reserve_slots_enforce_password.GetBool() ) {
			return CCD_MEMBER_CALL(pwd->GetString());
		}
	}

#if defined ( GAME_CSGO )
	return CCD_MEMBER_CALL(p6);
#elif defined ( GAME_ORANGE )
	return CCD_MEMBER_CALL(p7);
#else
	return CCD_MEMBER_CALL(p6);
#endif
}

#if defined ( GAME_CSGO )
const char * CSteamID::Render() const {
   static char szSteamID[64];
	_snprintf(szSteamID, sizeof(szSteamID), "STEAM_1:%u:%u", (m_steamid.m_comp.m_unAccountID % 2) ? 1 : 0, (int32)m_steamid.m_comp.m_unAccountID/2);
#else
const char * CSteamID::Render() const {
   static char szSteamID[64];
   _snprintf(szSteamID, sizeof(szSteamID), "STEAM_0:%u:%u", (m_unAccountID % 2) ? 1 : 0, (int32)m_unAccountID/2);
#endif
	return szSteamID;
}

#if defined ( GAME_CSGO )
#define NSPD_NON_MEMBER_CALL NON_MEMBER_CALL(NET_SendPacketDetour)(p1,p2,p3,p4,p5,p6,p7,p8)
DECL_DETOUR8_void( NET_SendPacketDetour, void *, int, void *, const mem_t *, int, void *, bool, unsigned int) {
#elif defined ( GAME_ORANGE )
#define NSPD_NON_MEMBER_CALL NON_MEMBER_CALL(NET_SendPacketDetour)(p1,p2,p3,p4,p5,p6,p7)
DECL_DETOUR7_void( NET_SendPacketDetour, void *, int, void *, const mem_t *, int, void *, bool) {
#else
#define NSPD_NON_MEMBER_CALL NON_MEMBER_CALL(NET_SendPacketDetour)(p1,p2,p3,p4,p5)
DECL_DETOUR5_void( NET_SendPacketDetour, void *, int, void *, const mem_t *, int ) {
#endif
	if (war_mode)
		return NSPD_NON_MEMBER_CALL;

	if ( (p5 > 4) && ( (p4[0]==0xFF) && (p4[1]==0xFF) && (p4[2]==0xFF) && (p4[3]==0xFF) && (p4[4]=='I') )) {
		char strIP[128];
		if ( p3 ) {
			int ip = *(int *)((const char *)p3 + 4);
			snprintf(strIP, sizeof(strIP), "%u.%u.%u.%u", ip & 0xFF, ( ip >> 8 ) & 0xFF, ( ip >> 16 ) & 0xFF, (ip >> 24) & 0xFF);
		}

		mem_t *pPassword = NULL;
		mem_t *pPlayers = NULL;
		A2S_INFO_t QueryData;
		memset (&QueryData, 0, sizeof(QueryData));
		if ( FillINFOQuery( p4, p5, QueryData, &pPlayers, &pPassword ) ) {

			bool AdminAccess = gpManiClient->IPLinksToAdmin ( strIP ) && ( mani_reserve_slots_include_admin.GetInt() == 1 );
			bool ReservedAccess = gpManiClient->IPLinksToReservedSlot( strIP );
			if ( FillSlotMode() && (AdminAccess || ReservedAccess)) {
				if ( pPlayers ) {
					if (pPlayers[0] == pPlayers[1]) 
						pPlayers[1] = (mem_t)max_players+1;
				}
			}

			if ( AdminAccess && pPassword && !war_mode && !mani_reserve_slots_enforce_password.GetBool() )
				pPassword[0]=0;
		}
	}
	return NSPD_NON_MEMBER_CALL;
}



ManiReservedSlot::ManiReservedSlot()
{
	// Init
	active_player_list_size = 0;
	active_player_list = NULL;
	reserve_slot_list = NULL;
	reserve_slot_list_size = 0;
	gpManiReservedSlot = this;
	kick_list.RemoveAll();
}

ManiReservedSlot::~ManiReservedSlot()
{
	// Cleanup
	this->CleanUp();
}
//---------------------------------------------------------------------------------
// Purpose: Init stuff for plugin load
//---------------------------------------------------------------------------------
void ManiReservedSlot::CleanUp(void)
{
	FreeList((void **) &reserve_slot_list, &reserve_slot_list_size);
	FreeList((void **) &active_player_list, &active_player_list_size);
}

//---------------------------------------------------------------------------------
// Purpose: Init stuff for plugin load
//---------------------------------------------------------------------------------
void ManiReservedSlot::Load(void)
{
	m_iUnaccountedPlayers = 0;

	if (!connect_client_addr || !netsendpacket_addr) return; // something is wrong with the sigscan

	ManiClientConnectDetour = CDetourManager::CreateDetour( "ConnectClient", connect_client_addr, GET_MEMBER_CALLBACK(ConnectClientDetour), GET_MEMBER_TRAMPOLINE(ConnectClientDetour));
	if ( ManiClientConnectDetour )
		ManiClientConnectDetour->DetourFunction( );

	ASSIGN_ORIGINAL(NET_SendPacketDetour, netsendpacket_addr);

	ManiNetSendPacketDetour = CDetourManager::CreateDetour( "NETSendPacket", netsendpacket_addr, GET_NON_MEMBER_CALLBACK(NET_SendPacketDetour), GET_NON_MEMBER_TRAMPOLINE(NET_SendPacketDetour) );
	if ( ManiNetSendPacketDetour )
		ManiNetSendPacketDetour->DetourFunction( );

	this->CleanUp();
}

//---------------------------------------------------------------------------------
// Purpose: Level load
//---------------------------------------------------------------------------------
void ManiReservedSlot::LevelInit(void)
{
	FileHandle_t file_handle;
	char	steam_id[MAX_NETWORKID_LENGTH];
	char	base_filename[256];

	m_iUnaccountedPlayers = 0;
	
	this->CleanUp();
	//Get reserve player list
	Q_snprintf(base_filename, sizeof (base_filename), "./cfg/%s/reserveslots.txt", mani_path.GetString());
	file_handle = filesystem->Open (base_filename,"rt",NULL);
	if (file_handle != NULL)
	{
		//		MMsg("Reserve Slot list\n");
		while (filesystem->ReadLine (steam_id, sizeof(steam_id), file_handle) != NULL)
		{
			if (!ParseLine(steam_id, true, false))
			{
				// String is empty after parsing
				continue;
			}

			AddToList((void **) &reserve_slot_list, sizeof(reserve_slot_t), &reserve_slot_list_size);
			Q_strcpy(reserve_slot_list[reserve_slot_list_size - 1].steam_id, steam_id);
			//			MMsg("[%s]\n", steam_id);
		}

		qsort(reserve_slot_list, reserve_slot_list_size, sizeof(reserve_slot_t), sort_reserve_slots_by_steam_id);
		filesystem->Close(file_handle);
	}
}

//---------------------------------------------------------------------------------
// Purpose: Check if weapon restricted for this weapon name
//---------------------------------------------------------------------------------
void ManiReservedSlot::DisconnectPlayer(player_t *player_ptr)
{
	if ( !mani_reserve_slots_allow_slot_fill.GetBool() && !FStrEq(mani_reserve_slots_redirect.GetString(), "") )
	{
		// Redirect
		if ( !player_ptr->is_bot ) {
			KeyValues *kv = new KeyValues("msg");
			kv->SetString("time","10");
			kv->SetString("title", mani_reserve_slots_redirect.GetString());
			helpers->CreateMessage( player_ptr->entity, DIALOG_ASKCONNECT, kv, gpManiISPCCallback);
		}
		gpManiPlayerKick->AddPlayer( player_ptr->index, 10.0f, mani_reserve_slots_kick_message.GetString() );
	} else if (!mani_reserve_slots_allow_slot_fill.GetBool()) {
		if ( !player_ptr->is_bot )
			PrintToClientConsole( player_ptr->entity, "%s\n", mani_reserve_slots_kick_message.GetString());

		gpManiPlayerKick->AddPlayer (player_ptr->index, 0.5f, mani_reserve_slots_kick_message.GetString());
	} else {
		if ( !player_ptr->is_bot )
			PrintToClientConsole( player_ptr->entity, "%s\n", mani_reserve_slots_kick_message.GetString());

		// need to kick them NOW!!!!  Use KickPlayer instead of AddPlayer
		gpManiPlayerKick->KickPlayer (player_ptr->index, mani_reserve_slots_kick_message.GetString());
	}
	LogCommand (NULL, "Kick (%s) [%s] [%s] [%s] kickid %i %s\n", mani_reserve_slots_kick_message.GetString(), player_ptr->name, player_ptr->steam_id, player_ptr->ip_address, player_ptr->user_id, mani_reserve_slots_kick_message.GetString());
	return;
}

//---------------------------------------------------------------------------------
// Purpose: Remove the detours
//---------------------------------------------------------------------------------
void ManiReservedSlot::Unload() {
	if ( ManiClientConnectDetour )
		ManiClientConnectDetour->Destroy();

	if ( ManiNetSendPacketDetour )
		ManiNetSendPacketDetour->Destroy();
}

//---------------------------------------------------------------------------------
// Purpose: See if player is on reserve list
//---------------------------------------------------------------------------------
bool ManiReservedSlot::IsPlayerInReserveList(player_t *player_ptr)
{

	reserve_slot_t	reserve_slot_key;

	// Do BSearch for steam ID
	Q_strcpy(reserve_slot_key.steam_id, player_ptr->steam_id);

	if (NULL == (reserve_slot_t *) bsearch
		(
		&reserve_slot_key,
		reserve_slot_list,
		reserve_slot_list_size,
		sizeof(reserve_slot_t),
		sort_reserve_slots_by_steam_id
		))
	{
		return false;
	}

	return true;
}

static int sort_reserve_slots_by_steam_id ( const void *m1,  const void *m2)
{
	struct reserve_slot_t *mi1 = (struct reserve_slot_t *) m1;
	struct reserve_slot_t *mi2 = (struct reserve_slot_t *) m2;
	return strcmp(mi1->steam_id, mi2->steam_id);
}

int	ManiReservedSlot::FindPlayerToKick ( ) {
	// FIRST LOOK FOR BOTS!
	for ( int i = 1; i <= max_players; i++ ) {
#if defined ( GAME_CSGO )
		edict_t *pEdict = PEntityOfEntIndex(i);
#else
		edict_t *pEdict = engine->PEntityOfEntIndex(i);
#endif

		IServerUnknown *unknown = pEdict->GetUnknown();
		if (!unknown)
			continue;

		CBaseEntity *base = unknown->GetBaseEntity();

		if (!base)
			continue;
		
		IPlayerInfo *pi_player = playerinfomanager->GetPlayerInfo( pEdict );

		if ( !pi_player )
			continue;

		if ( FStrEq(pi_player->GetNetworkIDString(), "BOT") ) 
			return i;
	}

	BuildPlayerKickList();

	if ( active_player_list_size == 0 )
		return 0;

	int KickMethod = mani_reserve_slots_kick_method.GetInt();
	//0 = by ping
	//1 = by connection time
	//2 = by kills per minute
	//3 = kill/death ratio

	if ( KickMethod == 0 ) {
		qsort(active_player_list, active_player_list_size, sizeof(active_player_t), sort_active_players_by_ping);
	} else if ( KickMethod == 1 ) {
		qsort(active_player_list, active_player_list_size, sizeof(active_player_t), sort_active_players_by_connect_time);
	} else if ( KickMethod == 2 ) {
		qsort(active_player_list, active_player_list_size, sizeof(active_player_t), sort_active_players_by_kill_rate);
	} else if ( KickMethod == 3 ) {
		qsort(active_player_list, active_player_list_size, sizeof(active_player_t), sort_active_players_by_kd_ratio);
	}

	return active_player_list[0].index;
}

//---------------------------------------------------------------------------------
// Purpose: Builds up a list of players that are 'kickable'
//---------------------------------------------------------------------------------
void ManiReservedSlot::BuildPlayerKickList( player_t *player_ptr, int *players_on_server )
{
	player_t	temp_player;
	active_player_t active_player;

	FreeList((void **) &active_player_list, &active_player_list_size);

	for (int i = 1; i <= max_players; i ++)
	{
#if defined ( GAME_CSGO )
		edict_t *pEntity = PEntityOfEntIndex(i);
#else
		edict_t *pEntity = engine->PEntityOfEntIndex(i);
#endif
		if( pEntity && !pEntity->IsFree())
		{
			if ( player_ptr && ( pEntity == player_ptr->entity ) )
				continue;

			IPlayerInfo *playerinfo = playerinfomanager->GetPlayerInfo( pEntity );
			if (playerinfo && playerinfo->IsConnected())
			{
				Q_strcpy(active_player.steam_id, playerinfo->GetNetworkIDString());
				if (FStrEq("BOT", active_player.steam_id))
				{
					continue;
				}

				INetChannelInfo *nci = engine->GetPlayerNetInfo(i);
				if (!nci)
				{
					continue;
				}

				active_player.entity = pEntity;

				active_player.ping = nci->GetAvgLatency(0);
				const char * szCmdRate = engine->GetClientConVarValue( i, "cl_cmdrate" );
				int nCmdRate = (20 > Q_atoi( szCmdRate )) ? 20 : Q_atoi(szCmdRate);
				active_player.ping -= (0.5f/nCmdRate) + TICKS_TO_TIME( 1.0f ); // correct latency

				// in GoldSrc we had a different, not fixed tickrate. so we have to adjust
				// Source pings by half a tick to match the old GoldSrc pings.
				active_player.ping -= TICKS_TO_TIME( 0.5f );
				active_player.ping = active_player.ping * 1000.0f; // as msecs
				active_player.ping = ((5 > active_player.ping) ? 5:active_player.ping); // set bounds, dont show pings under 5 msecs

				active_player.time_connected = nci->GetTimeConnected();
				Q_strcpy(active_player.ip_address, nci->GetAddress());
				if (gpManiGameType->IsSpectatorAllowed() &&
					playerinfo->GetTeamIndex () == gpManiGameType->GetSpectatorIndex())
				{
					active_player.is_spectator = true;
				}
				else
				{
					active_player.is_spectator = false;
				}
				active_player.user_id = playerinfo->GetUserID();
				Q_strcpy(active_player.name, playerinfo->GetName());

				if ( players_on_server )
					*players_on_server = *players_on_server + 1;

				active_player.kills = playerinfo->GetFragCount();
				active_player.deaths = playerinfo->GetDeathCount();
				Q_strcpy(temp_player.steam_id, active_player.steam_id);
				Q_strcpy(temp_player.ip_address, active_player.ip_address);
				Q_strcpy(temp_player.name, active_player.name);
				temp_player.is_bot = false;

				if (IsPlayerInReserveList(&temp_player))
				{
					continue;
				}

				active_player.index = i;

				if (mani_reserve_slots_include_admin.GetInt() == 1 &&
					gpManiClient->HasAccess(active_player.index, ADMIN, ADMIN_BASIC_ADMIN))
				{
					continue;
				}

				if (gpManiClient->HasAccess(active_player.index, IMMUNITY, IMMUNITY_RESERVE))
				{
					continue;
				}

				AddToList((void **) &active_player_list, sizeof(active_player_t), &active_player_list_size);
				active_player_list[active_player_list_size - 1] = active_player;
			}
		}
	}
}

static int sort_active_players_by_ping ( const void *m1,  const void *m2)
{
	struct active_player_t *mi1 = (struct active_player_t *) m1;
	struct active_player_t *mi2 = (struct active_player_t *) m2;

	if (mi1->is_spectator == true && mi2->is_spectator == false)
	{
		return -1;
	}

	if (mi1->is_spectator == false && mi2->is_spectator == true)
	{
		return 1;
	}

	if (mi1->ping > mi2->ping)
	{
		return -1;
	}
	else if (mi1->ping < mi2->ping)
	{
		return 1;
	}

	return strcmp(mi1->steam_id, mi2->steam_id);

}

static int sort_active_players_by_connect_time ( const void *m1,  const void *m2)
{
	struct active_player_t *mi1 = (struct active_player_t *) m1;
	struct active_player_t *mi2 = (struct active_player_t *) m2;

	if (mi1->is_spectator == true && mi2->is_spectator == false)
	{
		return -1;
	}

	if (mi1->is_spectator == false && mi2->is_spectator == true)
	{
		return 1;
	}

	if (mi1->time_connected < mi2->time_connected)
	{
		return -1;
	}
	else if (mi1->time_connected > mi2->time_connected)
	{
		return 1;
	}

	return strcmp(mi1->steam_id, mi2->steam_id);

}

static int sort_active_players_by_kill_rate ( const void *m1,  const void *m2)
{
	struct active_player_t *mi1 = (struct active_player_t *) m1;
	struct active_player_t *mi2 = (struct active_player_t *) m2;

	if (mi1->is_spectator == true && mi2->is_spectator == false)
	{
		return -1;
	}

	if (mi1->is_spectator == false && mi2->is_spectator == true)
	{
		return 1;
	}

	float mi1_kill_rate = ( mi1->time_connected == 0.0f ) ? FLT_MAX : ( mi1->kills / mi1->time_connected );
	float mi2_kill_rate = ( mi2->time_connected == 0.0f ) ? FLT_MAX : ( mi1->kills / mi2->time_connected );

	if (mi1_kill_rate < mi2_kill_rate)
	{
		return -1;
	}
	else if (mi1_kill_rate > mi2_kill_rate)
	{
		return 1;
	}

	return strcmp(mi1->steam_id, mi2->steam_id);

}

static int sort_active_players_by_kd_ratio ( const void *m1,  const void *m2)
{
	struct active_player_t *mi1 = (struct active_player_t *) m1;
	struct active_player_t *mi2 = (struct active_player_t *) m2;

	if (mi1->is_spectator == true && mi2->is_spectator == false)
	{
		return -1;
	}

	if (mi1->is_spectator == false && mi2->is_spectator == true)
	{
		return 1;
	}
	float mi1_kd_ratio = ( mi1->deaths == 0 ) ? FLT_MAX : ( mi1->kills / mi1->deaths );
	float mi2_kd_ratio = ( mi2->deaths == 0 ) ? FLT_MAX : ( mi1->kills / mi2->deaths );

	if (mi1_kd_ratio < mi2_kd_ratio)
	{
		return -1;
	}
	else if (mi1_kd_ratio > mi2_kd_ratio)
	{
		return 1;
	}

	return strcmp(mi1->steam_id, mi2->steam_id);

}

//---------------------------------------------------------------------------------
// Purpose: Check Player on connect
//---------------------------------------------------------------------------------
bool ManiReservedSlot::NetworkIDValidated(player_t	*player_ptr)
{
	bool		is_reserve_player = false;
	player_t	temp_player;
	int			allowed_players;
	int			total_players;
	
	m_iUnaccountedPlayers--;
	
	if ((war_mode) || (!mani_reserve_slots.GetBool()) || (mani_reserve_slots_number_of_slots.GetInt() == 0))   // with the other method ( zero slots )
	{																		// other player is kicked BEFORE
		return true;														// NetworkIDValidated
	}

	total_players = m_iUnaccountedPlayers + GetNumberOfActivePlayers(true);
	// DirectLogCommand("[DEBUG] Total players on server [%i]\n", total_players);

	if (total_players <= (max_players - mani_reserve_slots_number_of_slots.GetInt()))
	{
		// DirectLogCommand("[DEBUG] No reserve slot action required\n");
		return true;
	}


	GetIPAddressFromPlayer(player_ptr);
	Q_strcpy (player_ptr->steam_id, engine->GetPlayerNetworkIDString(player_ptr->entity));
	IPlayerInfo *playerinfo = playerinfomanager->GetPlayerInfo(player_ptr->entity);
	if (playerinfo && playerinfo->IsConnected())
	{
		Q_strcpy(player_ptr->name, playerinfo->GetName());
	}
	else
	{
		Q_strcpy(player_ptr->name,"");
	}

	if (FStrEq("BOT", player_ptr->steam_id))
		return true;

	player_ptr->is_bot = false;


	if (IsPlayerInReserveList(player_ptr))
		is_reserve_player = true;

	else if (mani_reserve_slots_include_admin.GetBool() && gpManiClient->HasAccess(player_ptr->index, ADMIN, ADMIN_BASIC_ADMIN))
		is_reserve_player = true;

	if (mani_reserve_slots_allow_slot_fill.GetInt() != 1)
	{
		// Keep reserve slots free at all times
		allowed_players = max_players - mani_reserve_slots_number_of_slots.GetInt();
		if (total_players > allowed_players)
		{
			if (!is_reserve_player) {
				DisconnectPlayer(player_ptr);
				return false;
			}

			temp_player.index = FindPlayerToKick();
			FindPlayerByIndex(&temp_player);
			DisconnectPlayer(&temp_player);
		}
	}
	
	return true;
}

void ManiReservedSlot::ClientDisconnect( player_t *player_ptr ) {
	if ( !player_ptr )
		m_iUnaccountedPlayers--;

	// for sanity
	if ( m_iUnaccountedPlayers < 0 ) m_iUnaccountedPlayers = 0;
}

void ManiReservedSlot::ClientConnect() {
	m_iUnaccountedPlayers++;
}
ManiReservedSlot	g_ManiReservedSlot;
ManiReservedSlot	*gpManiReservedSlot;
