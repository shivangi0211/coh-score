/***************************************************************************
 *     Copyright (c) 2004, Cryptic Studios
 *     All Rights Reserved
 *     Confidential Property of Cryptic Studios
 *
 ***************************************************************************/
#include "entcon.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include "CommandFileParser.h"
#include "SimpleParser.h"
#include "MessageStoreUtil.h"
#include "strings_opt.h"
#include "missionMapInit.h"
#include "comm_game.h"
#include "entVarUpdate.h"
#include "beacon.h"
#include "svr_base.h"
#include "entserver.h"
#include "beaconDebug.h"
#include "beaconPath.h"
#include "entaiPrivate.h"
#include "entaiPriority.h"
#include "sendToClient.h"
#include "villainDef.h"
#include "clientEntityLink.h"
#include "seq.h"
#include "entgen.h"
#include "NpcServer.h"
#include "Npc.h"
#include "EString.h"
#include "motion.h"
#include "character_attribs.h"
#include "entity.h"
#include "powers.h"
#include "character_combat.h"
#include "character_pet.h"
#include "utils.h"
#include "cmdserver.h"
#include "entaiLog.h"
#include "entai.h"
#include "earray.h"
#include "entplayer.h"
#include "entGameActions.h"
#include "NwWrapper.h"
#include "storyarcutil.h"
#include "ragdoll.h"
#include "error.h"
#include "seqstate.h"
#include "StashTable.h"
#include "foldercache.h"
#include "aiBehaviorPublic.h"
#include "costume.h"

typedef struct EntconCommand
{
	int					access_level;
						// Accesslevel needed to use this subcommand. Note that you must have
						// a high enough level to use the entcontrol command in the first place;
						// so a 0 here means "anyone who can use entcontrol"
	char				*name;
	char				*comment;
	CommandHandler		handler;
} EntconCommand;

static StashTable commandTable = 0;
static ClientLink* cur_client = NULL;
int entconInit();

/*
 *	returns:
 *		0 - unable to set access level
 *		otherwise - access level set successfully
 */
int entconSetCommandAccess(char* commandName, int accesslevel)
{
	EntconCommand* cmd;

	// if there isn't a command hash table, create one
	if(!commandTable && !entconInit())
		return 0;

	stashFindPointer( commandTable, commandName, (void*)&cmd );
	if (!cmd)
		return 0;

	cmd->access_level = accesslevel;

	return 1;
}

static int parseEntType(const char* token) {
	if(!stricmp(token,"critter"))
		return ENTTYPE_CRITTER;
	if(!stricmp(token,"player"))
		return ENTTYPE_PLAYER;
	if(!stricmp(token,"npc"))
		return ENTTYPE_NPC;
	if(!stricmp(token,"door"))
		return ENTTYPE_DOOR;
	if(!stricmp(token,"car"))
		return ENTTYPE_CAR;
	return ENTTYPE_NOT_SPECIFIED;
}

/* Function entControl
 *	Meant to receive text messages directly from the console in the form of
 *	a entcon command.  This function extracts the entity ID and command name
 *	from the message.  The function then looks up the address the command handler.
 *	If a handler is found, the function is invoked, passing the parameter
 *	part of the given message to the handler.
 */
void entControl(ClientLink* client, const char* commandParam){
	static char* staticBuffer;

	char* command;
	char* token;
	char* rad;
	char delim;
	int entID;
	Entity* e;
	EntconCommand* cmd;
	EntType entType = ENTTYPE_NOT_SPECIFIED;
	int radius = 0;
	EntType radiusFilter = ENTTYPE_NOT_SPECIFIED;

	estrPrintCharString(&staticBuffer, commandParam);

	command = staticBuffer;

	// if there isn't a command hash table, create one
	if(!commandTable && !entconInit())
		return;

	// extract entity ID
	token = strsep2(&command, " ", &delim);
	rad = strchr(token, '+');
	if (rad) {
		char *flt;
		*rad++ = '\0';
		flt = strchr(rad, ':');
		if (flt)	// AOE entcontrol can be filtered...
			*flt++ = '\0';

		radius = atoi(rad);

		if (flt)
			radiusFilter = parseEntType(flt);
	}
	entID = atoi(token);

	// check if the given entity is "valid"
	//	Entity ID not valid?
	//	Allow entID's of a negative value.  This simply means that the
	//	operation is not directed at any entity.

	if((entType = parseEntType(token)) != ENTTYPE_NOT_SPECIFIED){
		e = NULL;
	}
	else if(!stricmp(token,"me")){
		e = clientGetControlledEntity(client);
	}
	else if(!stricmp(token,"target")){
		e = validEntFromId(client->controls.selected_ent_server_index);
		if(!e)
			return;
	}
	else if(entID > 0){
		if(entID > MAX_ENTITIES_PRIVATE)
			return;

		//	Entity in use?
		if(!( entity_state[entID] & ENTITY_IN_USE ) )	//entities_used[entID])
			return;

		//	Entity is non-player?
		e = entities[entID];

	}
	else if(entID == 0){
		e = validEntFromId(client->controls.selected_ent_server_index);
		if(!e)
			return;
	}
	else if(entID < 0){
		e = NULL;  // command not directed at any one entity
	}

	// extract command name

	token = strsep2(&command, " ", &delim);

	// can't do anything further if there is no command available

	if(isEmptyStr(token))
		return;

	// lookup entity handler

	stashFindPointer( commandTable, token, (void*)&cmd );

	// if handler cannot be found or is restricted, report error

	if(!cmd || (client->entity->access_level < cmd->access_level)){
		conPrintf(client, "Entcon Error: No handler for command \"%s\"\n", token);
		return;
	}

	// otherwise, pass the paramters to the handler

	command[-1] = delim;
	token = command;

	while(*token && isspace((unsigned char)*token)){
		token++;
	}

	cur_client = client;

	if(e && radius == 0){
		cmd->handler(e, token);
	}else if (e && radius > 0){
		Entity* affectedEnts[MAX_ENTITIES_PRIVATE];		// ugh this API sucks
		int entCount, i;

		entCount = entWithinDistance(ENTMAT(e), radius, affectedEnts, radiusFilter, 0);

		for (i = 0; i < entCount; i++) {
			char params[10000];
			Strncpyt(params, token);
			cmd->handler(affectedEnts[i], params);
		}
	}else if (entType != ENTTYPE_NOT_SPECIFIED){
		int i;

		// Run command for all entities of the specified ent type.

		for(i = 0; i < entities_max; i++){
			e = validEntFromId(i);

			if(e && ENTTYPE_BY_ID(i) == entType){
				char params[10000];
				Strncpyt(params, token);
				cmd->handler(e, params);
			}
		}
	}else{
		cmd->handler(NULL, token);
	}

	cur_client = NULL;
}

static void entconSpawn(Entity* e, char* params){
	Mat4 mat;
	int result = 1;
	char* spawnName;

	beginParse(params);
	// extract target coordinates
	result &= getFloat(&mat[3][0]);
	result &= getFloat(&mat[3][1]);
	result &= getFloat(&mat[3][2]);
	result &= getString(&spawnName);
	endParse();

	if(!result)
		return;

	// If a binding does not exist, then try to generate an entity using the
	// given string as the entity type name
	e = generateEntity(spawnName);

	if(!e){
		printf("Unknown SpawnBinding %s.\n", spawnName);
		return;
	}

	entUpdatePosInstantaneous(e, mat[3]);
}

// Loads an NPC defined in NPC definition files.
static void entconSpawnNPC(Entity* e, char* params){
	Mat4 mat;
	int result = 1;
	char* spawnName;		// What is the name of the NPC to be spawned?

	beginParse(params);
	// extract target coordinates
	result &= getFloat(&mat[3][0]);
	result &= getFloat(&mat[3][1]);
	result &= getFloat(&mat[3][2]);
	result &= getString(&spawnName);
	endParse();

	if(!result)
		return;

	// Try to lookup the binding
	e = npcCreate(spawnName, ENTTYPE_NPC);

	if(!e){
		Errorf("SpawnArea error: Spawning unknown entity type: \"%s\"\n", spawnName);
		return;
	}

	SET_ENTTYPE(e) = ENTTYPE_NPC;
	e->fade = 1;
	entUpdatePosInstantaneous(e, mat[3]);

	aiInit(e, NULL);
}

// Loads a villain defined in NPC definition files.
static void entconSpawnVillain(Entity* e, char* params){
	Mat4 mat;
	int result = 1;
	char* spawnName;		// What is the name of the NPC to be spawned?
	int villainLevel = -1;
	int hasVillainLevel;
	Entity* clientEnt = clientGetViewedEntity(cur_client);
	int defaultSpawnLevel;

	if(clientEnt) 
		copyMat3(ENTMAT(clientEnt), mat);

	beginParse(params);
	// extract target coordinates
	result &= getFloat(&mat[3][0]);
	result &= getFloat(&mat[3][1]);
	result &= getFloat(&mat[3][2]);
	result &= getString(&spawnName);
	hasVillainLevel = getInt(&villainLevel);

	endParse();

	if(!result)
		return;

	FolderCacheDoCallbacks(); //This function is only used for testing, so this should be fine here

	if(cur_client->defaultSpawnLevel < 1){
		defaultSpawnLevel = cur_client->entity->pchar->iCombatLevel + 1;
	}else{
		defaultSpawnLevel = cur_client->defaultSpawnLevel;
	}

	e = villainCreateByName(spawnName, hasVillainLevel ? villainLevel : defaultSpawnLevel, NULL, false, NULL, 0, NULL);

	if( !e ) //If it's not a recognized Villain, maybe it's a costume name, make a dummy villain, and use spawnName as costume override
	{
		//TO DO MDO several kinds
		e = villainCreateByNameWithASpecialCostume("ClothesHorseVillain1", hasVillainLevel ? villainLevel : defaultSpawnLevel, NULL, false, NULL, spawnName, 0, NULL);
	}

	if(!e){
		printf("Unknown villain %s.\n", spawnName);
		return;
	}

	SET_ENTTYPE(e) = ENTTYPE_CRITTER;
	e->fade = 1;
	entUpdateMat4Instantaneous(e, mat);

	aiInit(e, NULL);
}

// Loads a villain defined in NPC definition files and creates him as a pet
static void entconSpawnPet(Entity* e, char* params){
	Mat4 mat;
	int result = 1;
	char* spawnName;		// What is the name of the NPC to be spawned?
	int villainLevel = -1;
	int hasVillainLevel;
	Entity* clientEnt = clientGetViewedEntity(cur_client);
	int defaultSpawnLevel;

	if(clientEnt)
		copyMat3(ENTMAT(clientEnt), mat);

	beginParse(params);
	// extract target coordinates
	result &= getFloat(&mat[3][0]);
	result &= getFloat(&mat[3][1]);
	result &= getFloat(&mat[3][2]);
	result &= getString(&spawnName);
	hasVillainLevel = getInt(&villainLevel);
	endParse();

	if(!result)
		return;

	if(cur_client->defaultSpawnLevel < 1){
		defaultSpawnLevel = cur_client->entity->pchar->iCombatLevel + 1;
	}else{
		defaultSpawnLevel = cur_client->defaultSpawnLevel;
	}

	character_CreateArenaPet(clientEnt, spawnName, hasVillainLevel ? villainLevel : defaultSpawnLevel);

}



static void entconToggleDebugInfo(Entity* e, char* params){
	int flag = 0;

	if(!e)
		return;

	switch(ENTTYPE(e)){
		case ENTTYPE_CRITTER:
			flag = ENTDEBUGINFO_CRITTER;
			break;
		case ENTTYPE_PLAYER:
			if(e == clientGetControlledEntity(cur_client))
				flag = ENTDEBUGINFO_SELF;
			else
				flag = ENTDEBUGINFO_PLAYER;
			break;
		case ENTTYPE_CAR:
			flag = ENTDEBUGINFO_CAR;
			break;
		case ENTTYPE_NPC:
			flag = ENTDEBUGINFO_NPC;
			break;
		case ENTTYPE_DOOR:
			flag = ENTDEBUGINFO_DOOR;
			break;
	}

	cur_client->entDebugInfo ^= flag;
}

static void entconKillEntity(Entity* e, char* params){
	void entKillAll();

	if(NULL == e){
		entKillAll();
	}
	else if(ENTTYPE(e) != ENTTYPE_PLAYER){
		entFree(e);
	}
}

static void entconSetSequencerState(Entity* e, char* params){
	char* statename;

	beginParse(params);
	while(getString(&statename)){
		seqSetStateByName(e->seq->state, 1, statename);
	}
	endParse();
}

static Entity* entconEntityFromString(char* entString, Entity* self){
	int index;
	
	if(!stricmp(entString, "me")){
		return cur_client->entity;
	}
	
	if(!stricmp(entString, "self")){
		return self;
	}
	
	if(!stricmp(entString, "0")){
		return validEntFromId(cur_client->controls.selected_ent_server_index);
	}
	
	index = atoi(entString);
	
	return validEntFromId(index);
}

static Entity* entconParseEntity(Entity* self){
	char* tempString;
	
	if(!getString(&tempString)){
		return NULL;
	}
	
	return entconEntityFromString(tempString, self);
}

static int entconGetPowerAttributeFromName(int* offsetOut){
	extern TokenizerParseInfo ParseCharacterAttributes[];
	extern DefineContext *g_pParsePowerDefines;

	char buffer[1000];
	char* tempString;
	const char* defineValue;
	TokenizerParseInfo* table = ParseCharacterAttributes;
	
	if(!getString(&tempString)){
		return 0;
	}
	
	sprintf(buffer, "k%s", tempString);
	
	defineValue = DefineLookup(g_pParsePowerDefines, buffer);
	
	if(defineValue){
		*offsetOut = atoi(defineValue);
		return 1;
	}

	for(; table->name; table++){
		if(!stricmp(table->name, tempString)){
			*offsetOut = table->storeoffset;
			return 1;
		}
	}
	
	return 0;
}

static void entconAIEvent(Entity* e, char* textParams){
	AIEventNotifyParams params;
	char* eventName;
	int result = 1;
	
	memset(&params, 0, sizeof(params));
	beginParse(textParams);
	
	params.source = entconParseEntity(e);
	params.target = entconParseEntity(params.source);
	
	if(!params.source || !params.target){
		return;
	}

	result &= getString(&eventName);
	
	if(result){
		if(!stricmp(eventName, "powersucceeded")){
			params.notifyType = AI_EVENT_NOTIFY_POWER_SUCCEEDED;
			
			params.powerSucceeded.revealSource = 1;
			
			result &= entconGetPowerAttributeFromName(&params.powerSucceeded.offAttrib);
			result &= getFloat(&params.powerSucceeded.fAmount);
			result &= getFloat(&params.powerSucceeded.fRadius);
			
			getInt(&params.powerSucceeded.uiInvokeID);
		}
		else if(!stricmp(eventName, "powerended")){
			params.notifyType = AI_EVENT_NOTIFY_POWER_ENDED;
			
			result &= entconGetPowerAttributeFromName(&params.powerEnded.offAttrib);
			result &= getInt(&params.powerSucceeded.uiInvokeID);
		}
		else{
			conPrintf(cur_client, "Unknown event \"%s\"", eventName);
			result = 0;
		}
		
		if(result){
			aiEventNotify(&params);
		}
	}
	
	endParse();
}

static void entconBehaviorAdd(Entity* e, char* params)
{
	if(e)
		aiBehaviorAddString(e, ENTAI(e)->base, params);
}

static void entconBehaviorRemoveTop(Entity* e, char* params)
{
	AIVars* ai;

	if(!e || !(ai = ENTAI(e)))
		return;

	aiBehaviorMarkFinished(e, ENTAI(e)->base, aiBehaviorGetCurBehavior(e, ENTAI(e)->base));
}

static void entconBreakOnEnt(Entity* e, char* params)
{
	if(e)
	{
		AIVars* ai = ENTAI(e);

		e = e;
	}
}

static void entconControlEntGenerators(Entity* e, char* params){
	char* token;
	int result = 1;

	beginParse(params);
	// extract target coordinates
	result &= getString(&token);
	endParse();

	if(!result)
		return;

	if(stricmp(token, "pause") == 0){
		entgenPause();
	}else if(stricmp(token, "resume") == 0){
		entgenResume();
	}
}

static void entconFindPath(Entity* e, char* params){
	int result = 1;
	Vec3 pos;
	AIVars* ai;

	if(!e)
		return;

	beginParse(params);
	result &= getFloat(&pos[0]);
	result &= getFloat(&pos[1]);
	result &= getFloat(&pos[2]);
	endParse();

	if(!result)
		return;

	ai = ENTAI(e);

	if(!ai)
		return;

	aiDiscardFollowTarget(e, __FUNCTION__, false);
	aiSetFollowTargetPosition(e, ai, pos, AI_FOLLOWTARGETREASON_ESCAPE);
	aiConstructPathToTarget(e);
	aiStartPath(e, 0, 0);
}

static AIPowerInfo* entconGetPower(AIPowerInfo* list, int index){
	for(; list && index--; list = list->next);

	return list;
}

static void entconRechargePower(Entity* e, char* params){
	int index = atoi(params);
	AIVars* ai;
	int i, j;

	if(!e || !e->pchar)
		return;

	ai = ENTAI(e);

	if(!ai)
		return;

	if(index < 0){
		PowerSet** powerSets = e->pchar->ppPowerSets;
		int powerSetCount = eaSize(&powerSets);

		for(i = 0; i < powerSetCount; i++){
			int powerCount = eaSize(&powerSets[i]->ppPowers);

			for(j = 0; j < powerCount; j++){
				Power* power = powerSets[i]->ppPowers[j];
				if (power) {
					power_ChangeRechargeTimer(power, 0);
				}
			}
		}
	}else{
		AIPowerInfo* info = entconGetPower(ai->power.list, index);

		if(info){
			power_ChangeRechargeTimer(info->power, 0);
		}
	}
}

static void entconPauseEnt(Entity* e, char* params){
	int toggle = !stricmp(params, "toggle");
	int pause = atoi(params);

	if(!e){
		int i;
		for(i = 0; i < entities_max; i++){
			e = entities[i];

			if(e && entity_state[i] & ENTITY_IN_USE && ENTTYPE_BY_ID(i) != ENTTYPE_PLAYER){
				if(toggle){
					SET_ENTPAUSED_BY_ID(i) = !ENTPAUSED_BY_ID(i);
				}else{
					SET_ENTPAUSED_BY_ID(i) = pause;
				}
			}
		}
	}
	else if(ENTTYPE(e) != ENTTYPE_PLAYER){
		if(toggle){
			SET_ENTPAUSED(e) = !ENTPAUSED(e);
		}else{
			SET_ENTPAUSED(e) = pause;
		}
	}
}

static void entconSetAlwaysHit(Entity* e, char* params){
	if(e){
		e->alwaysHit = atoi(params);
	}
}

static void entconSetAIConfig(Entity* e, char* params){
	if(e){
		aiSetConfigByName(e, ENTAI(e), params);
	}
}

static void entconSetCreator(Entity* e, char* params){
	if(e){
		int id = atoi(params);
		Entity* creator = validEntFromId(id);

		if(!stricmp(params, "me")){
			creator = cur_client->entity;
		}

		// This can produce a NULL creator, and that is fine.

		e->erCreator = erGetRef(creator);
	}
}

static void entconSetDoNotMove(Entity* e, char* params){
	int doNotMove = atoi(params) ? 1 : 0;

	if(e && ENTAI(e)){
		ENTAI(e)->doNotMove = doNotMove;
	}
}

static void entconSetDoNotUsePowers(Entity* e, char* params){
	int doNotUsePowers = atoi(params) ? 1 : 0;

	if(e && ENTAI(e)){
		ENTAI(e)->doNotUsePowers = doNotUsePowers;
	}
}

static void entconSetEndurance(Entity* e, char* params){
	int end = atoi(params);

	if(e && e->pchar){
		e->pchar->attrCur.fEndurance = max(0, min(e->pchar->attrMax.fEndurance, end));
	}
}

static void entconSetHealth(Entity* e, char* params){
	int health = atoi(params);

	if (!e) { // Make them all weak!
		int i;
		for(i = 1; i < entities_max; i++){
			if(entity_state[i] & ENTITY_IN_USE){
				if(ENTTYPE_BY_ID(i) != ENTTYPE_PLAYER && entities[i] && entities[i]->pchar){
					entities[i]->pchar->attrCur.fHitPoints = max(0, min(entities[i]->pchar->attrMax.fHitPoints, health));
				}
			}
		}
	} else if(e && e->pchar){
		e->pchar->attrCur.fHitPoints = max(0, min(e->pchar->attrMax.fHitPoints, health));
	}
}

static void entconSetInvincible(Entity* e, char* params){
	if(e){
		e->invincible = atoi(params);
	}
}

static void entconSetInvisible(Entity* e, char* params){
	if(e){
		SET_ENTHIDE(e) = atoi(params);
		e->draw_update = 1;
	}
}

static void entconSetNeverForget(Entity* e, char* params){
	char* targetName;
	int on;
	Entity* target;
	int result = 1;

	if(!e || !params)
		return;

	beginParse(params);

	result &= getString(&targetName);
	result &= getInt(&on);

	endParse();

	if(!result){
		return;
	}

	if(!stricmp(params, "me")){
		target = cur_client->entity;
	}else{
		target = validEntFromId(atoi(params));
	}

	if(target){
		AIVars* ai = ENTAI(e);

		if(ai){
			AIProxEntStatus* status = aiGetProxEntStatus(ai->proxEntStatusTable, target, 1);

			if(status){
				status->neverForgetMe = on;
			}
		}
	}
}

static void entconSetNPCGroundSnap(Entity* e, char* params){
	ai_state.noNPCGroundSnap = atoi(params) ? 1 : 0;
}

static void entconSetOwner(Entity* e, char* params){
	if(e){
		int id = atoi(params);
		Entity* owner = validEntFromId(id);

		if(!stricmp(params, "me")){
			owner = cur_client->entity;
		}

		// This can produce a NULL creator, and that is fine.

		e->erOwner = erGetRef(owner);
	}
}

static void entconSetUnstoppable(Entity* e, char* params){
	if(e){
		e->unstoppable = atoi(params);
	}
}

static void entconSetUntargetable(Entity* e, char* params){
	if(e){
		e->untargetable = atoi(params);
	}
	if( e->untargetable & UNTARGETABLE_DBFLAG )
		e->admin = 1;
	else
		e->admin = 0;
}

static void entconSetPos(Entity* e, char* params){
	Vec3 pos;
	Vec3 dir;
	int result = 1;
	Vec3 pyr;
	AIVars* ai;
	char* extraCommand = NULL;
	char* firstToken;

	if(!e)
		return;

	ai = ENTAI(e);

	beginParse(params);

	result &= getString(&firstToken);

	if(!result)
		return;

	if(!stricmp(firstToken, "me")){
		copyVec3(ENTPOS(cur_client->entity), pos);
	}else{
		pos[0] = atof(firstToken);
		result &= getFloat(&pos[1]);
		result &= getFloat(&pos[2]);

		if(!result)
			return;
	}

	result &= getString(&extraCommand);

	endParse();

	if(ai){
		char buffer[1000];

		sprintf(buffer, "Placed by %s^d.", AI_LOG_ENT_NAME(cur_client->entity));

		aiDiscardFollowTarget(e, buffer, false);

		memset(&ai->followTarget, 0, sizeof(ai->followTarget));

		if(ENTTYPE(e) == ENTTYPE_NPC){
			ai->npc.lastWireBeacon = NULL;
		}

		aiMotionSwitchToNone(e, ai, buffer);

		//aiSetFlying(e, ai, 0);
		
		e->motion->input.flying = ai->isFlying;

		if(ai->navSearch){
			clearNavSearch(ai->navSearch);
		}

		AI_LOG(	AI_LOG_PATH,
				(e,
				"%s ^0placed me at (^4%1.f^0,^4%1.f^0,^4%1.f^0).\n",
				AI_LOG_ENT_NAME(cur_client->entity),
				vecParamsXYZ(pos)));
	}

	subVec3(pos, ENTPOS(e), dir);

#ifdef RAGDOLL
	if ( e->ragdoll && e->ragdoll->pActor )
	{
		Vec3 vPosDiff;
		subVec3(pos, ENTPOS(e), vPosDiff);
		scaleVec3(vPosDiff, 0.5f, vPosDiff);
		nwSetActorLinearVelocity(e->ragdoll->pActor, vPosDiff);
	}
	else
#endif
		entUpdatePosInstantaneous(e, pos);

	entGridUpdate(e, ENTTYPE(e));

	e->move_instantly = 0;

	zeroVec3(e->motion->vel);
	zeroVec3(e->motion->input.vel);
	e->motion->jumping = 0;

	if(!ai)
	{
		e->motion->input.flying = 0;
	}

	pyr[0] = 0;
	pyr[1] = getVec3Yaw(dir);
	pyr[2] = 0;

	aiTurnBodyByPYR(e, ai, pyr);

	if(extraCommand && !stricmp(extraCommand, "setspawn")){
		copyVec3(ENTPOS(e), ai->spawn.pos);
		copyVec3(pyr, ai->spawn.pyr);
	}
}

static void entconSetPosInstantaneous(Entity* e, char* params){
	int result = 1;
	Vec3 newPos;
	AIVars* ai;
	char* firstToken;

	if(!e)
		return;

	ai = ENTAI(e);

	beginParse(params);

	result &= getString(&firstToken);

	if(!result)
		return;

	if(!stricmp(firstToken, "me")){
		copyVec3(ENTPOS(cur_client->entity), newPos);
	}else{
		newPos[0] = atof(firstToken);
		result &= getFloat(&newPos[1]);
		result &= getFloat(&newPos[2]);

		if(!result)
			return;
	}

	entUpdatePosInstantaneous(e, newPos);
}

static void entconSetPYR(Entity* e, char* params){
	int result = 1;
	Vec3 pyr;
	AIVars* ai;
	char* firstToken;

	if(!e)
		return;

	ai = ENTAI(e);

	beginParse(params);

	result &= getString(&firstToken);

	if(!result)
		return;

	if(!stricmp(firstToken, "me")){
		getMat3YPR(ENTMAT(cur_client->entity), pyr);
	}else{
		pyr[0] = atof(firstToken);
		result &= getFloat(&pyr[1]);
		result &= getFloat(&pyr[2]);
		
		if(!result)
			return;
			
		pyr[0] = RAD(pyr[0]);
		pyr[1] = RAD(pyr[1]);
		pyr[2] = RAD(pyr[2]);
	}
	
	aiTurnBodyByPYR(e, ai, pyr);
}

static void entconSetPower(Entity* e, char* params){
	AIVars* ai = ENTAI(e);

	if(!ai || !e->pchar)
		return;

	if(!params[0]){
		aiDiscardCurrentPower(e);

		ai->doNotChangePowers = 0;
	}else{
		int index = atoi(params);

		AIPowerInfo* info = entconGetPower(ai->power.list, index);

		if(info){
			Power* power = info->power;

			if(info->isAttack){
				aiSetCurrentPower(e, info, 1, 0);

				ai->power.preferred = info;

				ai->doNotChangePowers = 1;
			}
		}
	}
}

static void entconSetPriorityList(Entity* e, char* params){
	if(e){
		aiPriorityQueuePriorityList(e, params, 1, NULL);
	}
}

static void entconSetSpawnCostumeType(Entity* e, char* params){
	if(params[0]){
		cur_client->spawnCostumeType = atoi(params);
	}
}

static void entconSetSpawnLevel(Entity* e, char* params){
	if(params[0]){
		cur_client->defaultSpawnLevel = atoi(params);
	}
}

static void entconSetPlayerType(Entity* e, char* params){

	if (e) {
		if (ENTTYPE(e) == ENTTYPE_PLAYER)
		{
			int type;

			beginParse(params);

			if( getInt(&type) )
				entSetPlayerType(e, type);

			endParse();
		}
	}
}

static void entconSetPlayerSubType(Entity* e, char* params){

	if (e) {
		if (ENTTYPE(e) == ENTTYPE_PLAYER)
		{
			int type;

			beginParse(params);

			if( getInt(&type) )
				entSetPlayerSubType(e, type);

			endParse();
		}
	}
}

static void entconSetPraetorianProgress(Entity* e, char* params){

	if (e) {
		if (ENTTYPE(e) == ENTTYPE_PLAYER)
		{
			int type;

			beginParse(params);

			if( getInt(&type) )
				entSetPraetorianProgress(e, type);

			endParse();
		}
	}
}

static void entconSetInfluenceType(Entity* e, char* params){

	if (e) {
		if (ENTTYPE(e) == ENTTYPE_PLAYER)
		{
			int type;

			beginParse(params);

			if( getInt(&type) )
				entSetPlayerTypeByLocation(e, type);

			endParse();
		}
	}
}

static void entconSetGang(Entity* e, char* params){
	if(e) {
		AIVars* ai = ENTAI(e);

		if(ai){
			int gang;

			beginParse(params);

			if( getInt(&gang) )
				entSetOnGang( e, gang );

			endParse();
		}
	}
}



static void entconSetTeam(Entity* e, char* params){
	if(e){
		AIVars* ai = ENTAI(e);

		if(ai){
			int result = 1;
			int team;
			int member;

			beginParse(params);

			result &= getInt(&team);
			result &= getInt(&member);

			if(result){
				switch(team){
					case 0:
						entSetOnTeamHero(e, member);
						break;
					case 1:
						entSetOnTeamMonster(e, member);
						break;
					case 2:
						entSetOnTeamVillain(e, member);
						break;
					default:
						entSetOnTeam(e, team);
				}
			}

			endParse();
		}
	}
}

static void entconUsePower(Entity* e, char* params){
	AIVars* ai = ENTAI(e);

	if(!ai || !e->pchar)
		return;

	if(params[0]){
		int index = atoi(params);

		AIPowerInfo* info = entconGetPower(ai->power.list, index);

		if(info){
			Power* power = info->power;

			character_ActivatePower(e->pchar, erGetRef(e), ENTPOS(e), power);

			//aiUsePower(e, power, ai->attackTarget);
		}
	}
}

static void entconStumble(Entity* e, char* params)
{
	AIVars* ai = ENTAI(e);
	F32 strength;

	if(!ai || !e->pchar)
		return;

	if(params && params[0])
		strength = atof(params);
	if(!strength)
		strength = 1.0;

	doHitStumble( e, strength );
}

static void entconGetCostume(Entity* e, char* params)
{
	if (e) 			
	{
		if (EAINRANGE(e->npcIndex, npcDefList.npcDefs))
		{
			conPrintf(cur_client, "NPC Costume: %s (file: %s)\n", npcDefList.npcDefs[e->npcIndex]->name, npcDefList.npcDefs[e->npcIndex]->fileName);
		}
	}
}

static void entconIgnoreCombatMods(Entity* e, char* params){
	if(e && e->pchar && params[0]){
		e->pchar->bIgnoreCombatMods = atoi(params) ? true : false;
	}
}

static void entconIgnoreCombatModSetOffset(Entity* e, char* params){
	if(e && e->pchar && params[0]){
		e->pchar->ignoreCombatModLevelShift = atoi(params);
	}
}

static void entconSetPlayerMorphLevel(Entity* e, char* params)
{
	if (params[0])
	{
		clientLinkSetDebugVar(cur_client, "MorphLevel", (void *) atoi(params));
	}
}

static void entconSetPlayerEnhanceLevel(Entity* e, char* params)
{
	if (params[0])
	{
		clientLinkSetDebugVar(cur_client, "EnhanceLevel", (void *) atoi(params));
	}
}


static void entconSetUseEnterable(Entity* e, char* params)
{
	if(e)
		ENTAI(e)->useEnterable = atoi(params);
}

void entconPrintCmds(ClientLink* client, const char* match_str_src);

static void entconCmds(Entity* e, char* params)
{
	entconPrintCmds(cur_client, params);
}

static void entconRename(Entity* e, char* params)
{
	if (ENTTYPE(e) == ENTTYPE_PLAYER)
		return;

	Strncpyt(e->name, params);
	e->namePtr = e->name;		// just in case
	e->name_update = 1;
}

static void entconBeNPC(Entity* e, char* params)
{
	int		index;
	const NPCDef *npc = npcFindByName(params, &index);
	if (index)
	{
		const cCostume *npcCostume = npcDefsGetCostume(index, 0);

		if(npcCostume)
		{
			entSetImmutableCostumeUnowned( e, npcCostume ); // e->costume = npcCostume;
			svrChangeBody(e, entTypeFileName(e));
			e->npcIndex = index;
			e->send_costume = 1;
			if ((ENTTYPE(e) == ENTTYPE_PLAYER) && e->pl)
				e->pl->npc_costume = index;
		}
	}
}

static EntconCommand entcon_cmds[] =
{
	{ 0, "aievent", "",
		entconAIEvent },
	{ 0, "beacon", "",
		beaconCmd },
	{ 0, "behavioradd", "Adds an AI behavior string",
		entconBehaviorAdd },
	{ 0, "behaviorremovetop", "Removes the topmost AI behavior from its stack",
		entconBehaviorRemoveTop },
	{ 0, "benpc", "Change the entity's costume to the given NPC",
		entconBeNPC },
	{ 0, "breakonent", "",
		entconBreakOnEnt },
	{ 0, "cmds", "Lists entcontrol subcommands matching the specified pattern",
		entconCmds },
	{ 0, "entgens", "",
		entconControlEntGenerators },
	{ 0, "findpath", "",
		entconFindPath },
	{ 0, "getcostume", "",
		entconGetCostume },
	{ 0, "ignorecombatmods", "",
		entconIgnoreCombatMods },
	{ 0, "ignorecombatmodlevelshift", "",
		entconIgnoreCombatModSetOffset },
	{ 0, "kill", "Instantly destroys an entity. Does not work on players.",
		entconKillEntity },
	{ 0, "pause", "Pause or unpause an entity. Takes a parameter of 1 to pause, 0 to unpause,\n"
				  "or \"toggle\" to flip the paused state",
		entconPauseEnt },
	{ 0, "rechargepower", "",
		entconRechargePower },
	{ 0, "rename", "Rename a non-player entity. Use CSR commands for players",
		entconRename },
	{ 0, "setalwayshit", "",
		entconSetAlwaysHit },
	{ 0, "setaiconfig", "",
		entconSetAIConfig },
	{ 0, "setcreator", "",
		entconSetCreator },
	{ 0, "setdonotmove", "",
		entconSetDoNotMove },
	{ 0, "setdonotusepowers", "",
		entconSetDoNotUsePowers },
	{ 0, "setendurance", "",
		entconSetEndurance },
	{ 0, "sethealth", "",
		entconSetHealth },
	{ 0, "setinvincible", "",
		entconSetInvincible },
	{ 0, "setinvisible", "",
		entconSetInvisible },
	{ 0, "setneverforget", "",
		entconSetNeverForget },
	{ 0, "setnpcgroundsnap", "",
		entconSetNPCGroundSnap },
	{ 0, "setowner", "",
		entconSetOwner },
	{ 0, "setplayermorphlvl", "",
		entconSetPlayerMorphLevel },
	{ 0, "setenhancelvl", "",
		entconSetPlayerEnhanceLevel },
	{ 0, "setpos", "",
		entconSetPos },
	{ 0, "setposinstant", "",
		entconSetPosInstantaneous },
	{ 0, "setpower", "",
		entconSetPower },
	{ 0, "setprioritylist", "",
		entconSetPriorityList },
	{ 0, "setpyr", "",
		entconSetPYR },
	{ 0, "setspawncostumetype", "",
		entconSetSpawnCostumeType },
	{ 0, "setspawnlevel", "",
		entconSetSpawnLevel },
	{ 0, "setseq", "",
		entconSetSequencerState },
	{ 0, "setplayertype", "",
		entconSetPlayerType },
	{ 0, "setplayersubtype", "",
		entconSetPlayerSubType },
	{ 0, "setpraetorian", "",
		entconSetPraetorianProgress },
	{ 0, "setinfluencetype", "",
		entconSetInfluenceType },
	{ 0, "setteam", "",
		entconSetTeam },
	{ 0, "setgang", "",
		entconSetGang },
	{ 0, "setunstoppable", "",
		entconSetUnstoppable },
	{ 0, "setuntargetable", "",
		entconSetUntargetable },
	{ 0, "setuseenterable", "",
		entconSetUseEnterable },
	{ 0, "stumble", "",
		entconStumble },
	{ 0, "spawn", "",
		entconSpawn },
	{ 0, "spawnNPC", "",
		entconSpawnNPC },
	{ 0, "spawnVillain", "",
		entconSpawnVillain },
	{ 0, "spawnPet", "",
		entconSpawnPet },
	{ 0, "toggledebuginfo", "",
		entconToggleDebugInfo },
	{ 0, "usepower", "",
		entconUsePower },
	{ 0 }
};

static int entconInit() {
	EntconCommand *c = entcon_cmds;

	commandTable = stashTableCreateWithStringKeys(32,  StashDeepCopyKeys);
	if (!commandTable)
		return 0;

	while (c && c->name) {
		stashAddPointer(commandTable, c->name, c, true);
		++c;
	}

	return 1;
}

static void entconPrintCmds(ClientLink* client, const char* match_str_src) {
	int		i;
	char	buf[2000];
	EntconCommand		*cmd;
	char	commentBuffer[2000];
	char*	comment;
	char*	str;
	char	delim;
	int		firstLine;
	size_t	maxNameLen = 1;
	char	nameFormat[100];
	char	commentFormat[100];
	char	match_str[1024];

	if( match_str_src )
		Strncpyt(match_str, stripUnderscores(match_str_src));
	else
		match_str[0] = '\0';

	Strncpyt( buf, textStd("LevelCommands", client->entity->access_level) );
	conPrintf(client,"\n%s\n",buf);
	strcat( buf, "\n" );

	for(i=0;entcon_cmds[i].name;i++)
	{
		cmd = &entcon_cmds[i];
		maxNameLen = max(maxNameLen, strlen(cmd->name));
	}

	sprintf_s(SAFESTR(nameFormat), "%%d %%-%d.%ds", maxNameLen, maxNameLen);
	sprintf_s(SAFESTR(commentFormat), "%%s%%-%d.%ds", maxNameLen + 3, maxNameLen + 3);

	for(i=0;entcon_cmds[i].name;i++)
	{
		cmd = &entcon_cmds[i];

		if (cmd->access_level > client->entity->access_level || !cmd->comment || (match_str && !strstri(stripUnderscores(cmd->name),match_str)))
			continue;

		sprintf_s(SAFESTR(buf),nameFormat,cmd->access_level,cmd->name);

		strcat(buf," ");

		assert(strlen(cmd->comment) < 2000);
		Strncpyt(commentBuffer, cmd->comment ? cmd->comment : "");

		comment = commentBuffer;
		firstLine = 1;
		for(str = strsep2(&comment, "\n", &delim); str; str = strsep2(&comment, "\n", &delim)){
			if(firstLine)
				firstLine = 0;
			else
				sprintf_s(SAFESTR(buf), commentFormat, buf, "");

			strcat(buf, str);
			strcat(buf, "\n");
			if(delim)
			comment[-1] = delim;
		}
		conPrintf(client,"%s",buf);
	}
}

/* End of File */
