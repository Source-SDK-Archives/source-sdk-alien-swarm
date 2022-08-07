#include "cbase.h"
#include "filesystem.h"
#include "missionchooser/iasw_mission_chooser.h"
#include "missionchooser/iasw_mission_chooser_source.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Alien Swarm: Infested Campaign Console Commands

// Resume playing a savegame - launches to the campaign lobby with the specified save data loaded
void ASW_LaunchSaveGame(const char *szSaveName, bool bMultiplayer, bool bChangeLevel)
{
	IASW_Mission_Chooser_Source* pSource = missionchooser ? missionchooser->LocalMissionSource() : NULL;
	if (!pSource)
		return;

	int iMissions = pSource->GetNumMissionsCompleted(szSaveName);
	if (iMissions == -1)	// invalid save
		return;

	// decide on lobby map (if singleplayer and no missions complete, we launch the intro)
	const char *szLobby = "Lobby";

	if (!bMultiplayer && iMissions == 0)	// if we're in singleplayer and starting the campaign, find the intro
	{
		szLobby = pSource->GetCampaignSaveIntroMap(szSaveName);
	}		

	// launch the campaign lobby map
	char buffer[512];
	if (bChangeLevel)		
		Q_snprintf(buffer, sizeof(buffer), "changelevel %s campaign %s\n", szLobby, szSaveName);
	else
		Q_snprintf(buffer, sizeof(buffer), "map %s campaign %s\n", szLobby, szSaveName);

#ifdef CLIENT_DLL
	engine->ClientCmd(buffer);
#else
	engine->ServerCommand(buffer);
#endif
}

namespace
{
	const char * GenerateNewSaveGameName()
	{
		static char szNewSaveName[256];	
		// count up save names until we find one that doesn't exist
		for (int i=1;i<10000;i++)
		{
			Q_snprintf(szNewSaveName, sizeof(szNewSaveName), "save/save%d.campaignsave", i);
			if (!filesystem->FileExists(szNewSaveName))
			{
				Q_snprintf(szNewSaveName, sizeof(szNewSaveName), "save%d.campaignsave", i);
				return szNewSaveName;
			}
		}

		return NULL;
	}
}

// con command for new campaign
void ASW_StartNewCampaign_cc( const CCommand &args )
{
	IASW_Mission_Chooser_Source* pSource = missionchooser ? missionchooser->LocalMissionSource() : NULL;
	if (!pSource)
		return;

	if ( args.ArgC() < 4 )
	{
		Msg( "Usage: ASW_StartCampaign [CampaignName] [SaveName] [SP|MP]\n   Use 'auto' for SaveName to have an autonumbered save.\n" );
		return;
	}

	// create a new savegame at the start of the campaign
	char buffer[256];
	if (!Q_strnicmp( args[2], "auto", 4 ))
	{
		const char *pNewSaveName = GenerateNewSaveGameName();
		if (!pNewSaveName)
		{
			Msg("Error! No more room for autogenerated save names, delete some save games!\n");
			return;
		}
		Q_snprintf(buffer, sizeof(buffer), "%s", pNewSaveName);
	}
	else
		Q_snprintf(buffer, sizeof(buffer), "%s", args[2]);

	// Use an arg to detect MP (would be better to check maxplayers here, but we can't?)
	bool bMulti = (!Q_strnicmp( args[3], "MP", 2 ));
	if (!pSource->ASW_Campaign_CreateNewSaveGame(&buffer[0], sizeof(buffer), args[1], bMulti, NULL))
	{
		Msg( "Unable to create new save game.\n" );
		return;
	}

	// go to the load savegame code
	ASW_LaunchSaveGame(buffer, bMulti, args.ArgC() == 5);	// if 3rd parameter specified, do a changelevel instead of launching a new
}
#ifdef CLIENT_DLL
ConCommand ASW_StartCampaign( "ASW_StartCampaign", ASW_StartNewCampaign_cc, "Starts a new campaign game", FCVAR_DONTRECORD );
#else
ConCommand ASW_Server_StartCampaign( "ASW_Server_StartCampaign", ASW_StartNewCampaign_cc, "Server starts a new campaign game", FCVAR_DONTRECORD );
#endif

// con command for loading a savegame
void ASW_LoadCampaignGame_cc( const CCommand &args )
{
	if ( args.ArgC() < 3 )
	{
		Msg("Usage: ASW_LoadCampaign [SaveName] [SP|MP]\n" );
		return;
	}

	// Use an arg to detect MP (would be better to check maxplayers here, but we can't?)
	bool bMulti = (!Q_strnicmp( args[2], "MP", 2 ));
	ASW_LaunchSaveGame(args[1], bMulti, args.ArgC() == 4);	// if 2nd parameter specified, do a changelevel instead of launching a new
}
#ifdef CLIENT_DLL
ConCommand ASW_LoadCampaignGame( "ASW_LoadCampaign", ASW_LoadCampaignGame_cc, "Loads a previously saved campaign game", FCVAR_DONTRECORD );
#else
ConCommand ASW_Server_LoadCampaignGame( "ASW_Server_LoadCampaign", ASW_LoadCampaignGame_cc, "Server loads a previously saved campaign game", FCVAR_DONTRECORD );
#endif