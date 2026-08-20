//========= Copyright � 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef IGAMECLIENTEXPORTS_H
#define IGAMECLIENTEXPORTS_H
#ifdef _WIN32
#pragma once
#endif

#include "interface.h"
#include "../cl_dll/ClientInfo.h"

//-----------------------------------------------------------------------------
// Purpose: Exports a set of functions for the GameUI interface to interact with the game client
//-----------------------------------------------------------------------------
class IGameClientExports : public IBaseInterface
{
public:
	// returns the name of the server the user is connected to, if any
	virtual const char *GetServerHostName() = 0;

	// ingame voice manipulation
	virtual bool IsPlayerGameVoiceMuted(int playerIndex) = 0;
	virtual void MutePlayerGameVoice(int playerIndex) = 0;
	virtual void UnmutePlayerGameVoice(int playerIndex) = 0;

	virtual const char *GetLevelName( void ) = 0;

	virtual int GetLocalPlayerTeam( void ) = 0;
	virtual bool GetPlayerExtraInfo( int num, hud_player_info_t **player, extra_player_info_t **extra, bool *isBot ) = 0;
	virtual bool GetTeamInfo( int num, team_info_t **team ) = 0;
	virtual int GetPlayerSteamInfo( uint64_t steamid, sbrk_player_info_t *player ) = 0;
};

#define GAMECLIENTEXPORTS_INTERFACE_VERSION "GameClientExports001"


#endif // IGAMECLIENTEXPORTS_H
