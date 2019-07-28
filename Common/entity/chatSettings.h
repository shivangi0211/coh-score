#ifndef CHAT_SETTINGS_H__
#define CHAT_SETTINGS_H__

typedef struct Packet Packet;
typedef struct Entity Entity;

#include "stdtypes.h"
#include "entvarupdate.h"

#define MAX_CHAT_WINDOWS		5
#define MAX_CHAT_CHANNELS		15		// # of USER channels
#define MAX_CHAT_TABS			20		
#define MAX_TAB_NAME_LEN		18
#define MAX_CHANNEL_NAME_LEN	25


typedef enum {
	ChannelType_None = 0,
	ChannelType_System, 
	ChannelType_User
} ChannelType;

typedef enum {
	ChannelOption_Top = 0,
	ChannelOption_Bottom, 
	ChannelOption_Err,
} ChannelOption;

// container used to load/save from database
typedef struct ChatWindowSettings{

	int		tabListBF;
	int		selectedtop;
	int		selectedbot;
	float	divider;

}ChatWindowSettings;

typedef struct ChatTabSettings{

	char name[MAX_TAB_NAME_LEN+1];

	int systemBF; // deprecated

	U32	systemChannels[SYSTEM_CHANNEL_BITFIELD_SIZE];	

	int userBF;
	int optionsBF;
	int defaultChannel;
	int defaultType;

}ChatTabSettings;

typedef struct ChatChannelSettings{

	char name[MAX_CHANNEL_NAME_LEN+1];
	int optionsBF;
	int color1;
	int color2;
}ChatChannelSettings;

typedef struct ChatSettings{

	ChatWindowSettings	windows[MAX_CHAT_WINDOWS];
	ChatTabSettings		tabs[MAX_CHAT_TABS];
	ChatChannelSettings channels[MAX_CHAT_CHANNELS];

	int options;	
	int primaryChatMinimized;

	int userSendChannel;	// index in 'channels' of current send-to channel (if any)

#ifdef SERVER
	int systemChannels[SYSTEM_CHANNEL_BITFIELD_SIZE];	// (NOT STORED IN DATABASE) flag of all system channels assigned to at least one filter/tab -- this allows fast lookups for server-side filtering
#endif

}ChatSettings;


// ChatSettings Options
typedef enum {
	CSFlags_DoNotLocalize			= (1 << 0),		// set to one after player has had a chance to localize their tab names
	CSFlags_DoNotPromptRenameHandle = (1 << 1),		// do not prompt user to rename their chat handle on login (if they can rename)
	CSFlags_ActiveChatWindowIdx		= (1 << 2) | (1 << 3) | (1 << 4),	// store index 0-4 as 3 bits
	CSFlags_AddedCombat				= (1 << 5),
	CSFlags_BottomDividerSelected	= (1 << 6),
	CSFlags_AddedArchitect			= (1 << 7),
	CSFlags_AddedLeague				= (1 << 8),
	CSFlags_AddedLookingForGroup	= (1 << 9),
	CSFlags_AddedLegacyUINote		= (1 << 10),
	CSFlags_MovedVisitedMaps		= (1 << 11),
	CSFlags_ClearMARTYHistory		= (1 << 12),
	// Primary Minimzed Flag takes up 11 MSG bits (21-31)

}CSFlags_Type;


#ifdef SERVER
// unpack/fixup entity from db
void unpackChatSettings(Entity * e);
void updatePraetorianEventChannel(Entity *e);
#endif

// send/recieve
void receiveSelectedChatTabs(Packet * pak, Entity * e);
void receiveChatSettings( Packet *pak, Entity *e );
void sendChatSettings( Packet *pak, Entity *e );

// utility functions... (need a better home :)
const char * getHandleFromString(const char * str);

bool isWatchedChannel( Entity *e, int type );

#endif // CHAT_SETTINGS_H__