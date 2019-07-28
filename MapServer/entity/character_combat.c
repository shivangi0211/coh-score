/***************************************************************************
 *     Copyright (c) 2013 Abandonware Holdings, Ltd.
 *     All Rights Reserved
 ***************************************************************************/
#include <assert.h>
#include <float.h>
#include "logcomm.h"
#include "earray.h"
#include "error.h"
#include "entai.h"
#include "entaiLog.h"
#include "entity.h"
#include "entplayer.h"
#include "sendToClient.h" // inexplicably for sendInfoBox
#include "entGameActions.h" // for entAlive
#include "gridfind.h"
#include "utils.h"
#include "motion.h"
#include "npc.h"
#include "dbcomm.h"
#include "dbghelper.h"
#include "entworldcoll.h" // for entHeight
#include "mission.h" // for MissionItemInterruptInteraction()
#include "entserver.h"
#include "megaGrid.h"
#include "teamup.h"

#include "powers.h"
#include "powerinfo.h"
#include "boostset.h"

#include "combat_mod.h"
#include "character_base.h"
#include "character_animfx.h"
#include "character_mods.h"
#include "character_level.h"
#include "character_combat.h"
#include "character_target.h"
#include "character_eval.h"
#include "character_combat_eval.h"
#include "character_workshop.h"
#include "character_net_server.h"
#include "attribmod.h"
#include "seq.h"
#include "dbnamecache.h"
#include "cmdserver.h"
#include "costume.h"
#include "arenamap.h"
#include "sgraid.h"

#include "group.h"
#include "groupfileload.h"
#include "gridcoll.h"
#include "gridcache.h"

#include "eval.h"
#include "timing.h"
#include "trading.h"
#include "Quat.h"
#include "AppLocale.h"
#include "turnstile.h"
#include "scripthook/ScriptHookInternal.h"


static void ApplyTravelSuppression(Character *pChar, float fDuration)
{
	Power *pPower = NULL;
	const BasePower *pBasePower = basepower_FromPath("Temporary_Powers.Temporary_Powers.Travel_Suppression");

	if (pBasePower != NULL)
	{
		int timeRemaining = 0;

		// make sure player has power
		pPower = character_OwnsPower(pChar, pBasePower);

		if (!pPower)
		{
			PowerSet *pset = character_OwnsPowerSet(pChar, pBasePower->psetParent);

			if (pset == NULL)
				pset=character_BuyPowerSet(pChar, pBasePower->psetParent);

			if (pset)
			{
				pPower = character_BuyPower(pChar, pset, pBasePower, 0);
			}
		} else {
			timeRemaining = pBasePower->fLifetimeInGame - pPower->fAvailableTime;
		}

		if (pPower != NULL)
		{
			if (timeRemaining < fDuration)
				timeRemaining = fDuration;
			pPower->fAvailableTime = pBasePower->fLifetimeInGame - timeRemaining;
		}

		eaPush(&pChar->entParent->powerDefChange, powerRef_CreateFromPower(pChar->iCurBuild, pPower));
		character_ActivateAllAutoPowers(pChar);
	}
}

static void ChanceModAccumulate(float *pfBaseChance, const EffectGroup *pEffect, AttribMod **ppmods)
{
	int i;
	float fChance = *pfBaseChance;

	// TODO: Even more horribly inefficient than it used to be.
	// This is on the short list for reworking into something better.
	for(i=eaSize(&ppmods)-1; i>=0; i--)
	{
		AttribMod *pmod = ppmods[i];
		if(fChance < 1.0f || pmod->fMagnitude < 0.0f)
		{
			if(effectfilter_Test(pEffect, TemplateGetParams(pmod->ptemplate, EffectFilter)))
			{
				fChance += pmod->fMagnitude;
			}
		}
	}
	*pfBaseChance = fChance;
}

static int s_bSentHitMessage; // Oh the hackery

static void sendHitMessage( Entity * pSrc, Entity *pTarget, const BasePower * ppow, F32 fToHit, F32 fRoll, bool bForceHit, bool bAlwaysHit, bool bHasParent )
{
	Entity *pSrcParent = erGetEnt(pSrc->erOwner);
	Entity *pTargetParent = erGetEnt(pTarget->erOwner); 
	char *pchHitTarget, *pchHitSrc;

	if (!pSrcParent)
		pSrcParent = pSrc;

	if (!pTargetParent)
		pTargetParent = pTarget;

	// check to see if anyone will get a message
	if (ENTTYPE(pSrcParent) != ENTTYPE_PLAYER
		&& ENTTYPE(pTargetParent) != ENTTYPE_PLAYER)
	{
		return;
	}

	// strduping these strings because I don't trust the localizedPrintf rotor to hold out
	if(ENTTYPE(pTarget)==ENTTYPE_CRITTER)
	{
		if( pTarget->petName && pTarget->petName[0] )
			pchHitTarget = strdup(pTarget->petName);
		else if(pTarget->name==NULL || pTarget->name[0]=='\0' )
			pchHitTarget = strdup(localizedPrintf(pSrcParent, npcDefList.npcDefs[pTarget->npcIndex]->displayName));
		else
			pchHitTarget = strdup(localizedPrintf(pSrcParent, pTarget->name));
	}
	else
		pchHitTarget = strdup(pTarget->name);

	if(ENTTYPE(pSrc)==ENTTYPE_CRITTER)
	{
		if( pSrc->petName && pSrc->petName[0] )
			pchHitSrc = strdup(pSrc->petName);
		else if(pSrc->name==NULL || pSrc->name[0]=='\0')
			pchHitSrc = strdup(localizedPrintf(pTargetParent, npcDefList.npcDefs[pSrc->npcIndex]->displayName));
		else
			pchHitSrc = strdup(localizedPrintf(pSrcParent, pSrc->name));
	}
	else
		pchHitSrc = strdup(pSrc->name);

	// Send Hit Messages to Person Swinging (or pet owner)
	if (ENTTYPE(pSrcParent) == ENTTYPE_PLAYER)
	{
		float fSend = fRoll;

		if( bAlwaysHit )
		{
			fSend = 2.0f;
		}
		else if(bForceHit)
		{
			fSend = 3.0f;
		}
		else
		{
			if( ppow->eType == kPowerType_Click )
				pSrc->pchar->fLastChance = fToHit;
		}

		if (!(bAlwaysHit && bHasParent))	// Don't spam autohit sub-powers
			sendCombatMessage(pSrc, INFO_COMBAT_HITROLLS, CombatMessageType_PowerHitRoll, 0, ppow->crcFullName, 0, pchHitTarget, fToHit, fSend);
	}
	
	// Now send messages to person getting hit
	if( ENTTYPE(pTargetParent) == ENTTYPE_PLAYER )
	{
		float fSend = fRoll;

		if( bAlwaysHit )
		{
			fSend = 2.0f;
		}
		else if(bForceHit)
		{
			fSend = 3.0f;
		}

		sendCombatMessage(pTarget, INFO_COMBAT_HITROLLS, CombatMessageType_PowerHitRoll, 1, ppow->crcFullName, 0, pchHitSrc, fToHit, fSend);
	}

	SAFE_FREE(pchHitSrc);
	SAFE_FREE(pchHitTarget);
}

// DeferredCombatEvent, a clearinghouse for events that need to be deferred
// for proper timing, but can't be attached to AttribMods
typedef struct DeferredCombatEvent {
	U32 abs_time;
	EntityRef erSrc;
	EntityRef erTarget;
	Power *ppow;
	float fToHit;
	float fToHitRoll;
	bool bForceHit;
	bool bAlwaysHit;
	bool bSendInfo;
	bool bHit;
} DeferredCombatEvent;

static DeferredCombatEvent **s_dceList = NULL;

MP_DEFINE(DeferredCombatEvent);

static int dceCompare(const DeferredCombatEvent **a, const DeferredCombatEvent **b)
{
	// Sort in reverse order so that the first events to be processed
	// go at the end of the list and can be quickly removed
	if ((*a)->abs_time > (*b)->abs_time)
		return -1;
	if ((*a)->abs_time < (*b)->abs_time)
		return 1;
	return 0;
}

static void DestroyDeferredCombatEvent(DeferredCombatEvent *dce)
{
	power_ReleaseRef(dce->ppow);
	MP_FREE(DeferredCombatEvent, dce);
}

static void AddDeferredCombatEvent(float fDelay, Entity *pSrc, Entity *pTarget, Power *ppow,
							float fToHit, float fToHitRoll, bool bForceHit, bool bAlwaysHit,
							bool bSendInfo, bool bHit)
{
	DeferredCombatEvent *dce;

	// Ugh, re-use the hacky workaround for this being called on a per-attribmod basis for hits.
	// Seems to be needed for now at least, since by design hit messages aren't sent for powers
	// that have no mods with combat spam (but misses are, so this might should be changed
	// anyway).
	if( s_bSentHitMessage )
		return;

	MP_CREATE(DeferredCombatEvent, 64);
	dce = MP_ALLOC(DeferredCombatEvent);

	dce->abs_time = ABS_TIME + SEC_TO_ABS(fDelay);
	dce->erSrc = erGetRef(pSrc);
	dce->erTarget = erGetRef(pTarget);
	dce->ppow = ppow;
	power_AcquireRef(dce->ppow);
	dce->fToHit = fToHit;
	dce->fToHitRoll = fToHitRoll;
	dce->bForceHit = bForceHit;
	dce->bAlwaysHit = bAlwaysHit;
	dce->bSendInfo = bSendInfo;
	dce->bHit = bHit;
	eaSortedInsert(&s_dceList, dceCompare, dce);

	s_bSentHitMessage = 1;
}

void ProcessDeferredCombatEvents()
{
	int dcecount;
	DeferredCombatEvent *dce;
	Entity *pSrc, *pTarget;

	for(;;) {
		dcecount = eaSize(&s_dceList);
		// list is sorted in reverse time order, check if last item should be processed yet
		if (dcecount == 0 || s_dceList[dcecount-1]->abs_time > ABS_TIME)
			break;

		dce = s_dceList[dcecount-1];
		pSrc = erGetEnt(dce->erSrc);
		pTarget = erGetEnt(dce->erTarget);
		if (pSrc && pTarget) {
			if (dce->bHit) {
				sendHitMessage(pSrc, pTarget, dce->ppow->ppowBase, dce->fToHit, dce->fToHitRoll, dce->bForceHit, dce->bAlwaysHit, dce->ppow->ppowParentInstance != 0);
			} else {
				// Miss
				if (dce->bSendInfo) {
					sendInfoBox(pSrc, INFO_COMBAT, "PowerYouMissed", dce->ppow->ppowBase->pchDisplayName);
					sendHitMessage(pSrc, pTarget, dce->ppow->ppowBase, dce->fToHit, dce->fToHitRoll, 0, 0, dce->ppow->ppowParentInstance != 0);
				}

				AI_LOG(AI_LOG_POWERS, (pSrc, "ApplyPow:    Power misses! (%6.3f < %6.3f)\n", dce->fToHitRoll, dce->fToHit));

				// Notify AI.
				{
					AIEventNotifyParams params = {0};

					params.notifyType = AI_EVENT_NOTIFY_POWER_FAILED;
					params.source = pSrc;
					params.target = pTarget;
					params.powerFailed.revealSource =	dce->ppow->ppowBase->eAIReport==kAIReport_Always
														|| dce->ppow->ppowBase->eAIReport==kAIReport_MissOnly;
					params.powerFailed.ppow = dce->ppow;

					aiEventNotify(&params);
				}
			}
		}

		eaRemoveAndDestroy(&s_dceList, dcecount-1, DestroyDeferredCombatEvent);
		// remove last item and continue checking the end of the list
	}
}

typedef struct EffectApplyContext {
	ModFillContext mf;
	int* eApplicationFilter;
	bool bAlwaysHit;
	bool bForceHit;
	bool bVecFloatLoc;
	bool bIsProc;
	float fEffectDelay;
	float fEffectDelayFromProjectiles;
	float fDelayAccum;
	AttribModEvalInfo *evalInfo;
	AttribMod ***delayEvalAttribMods;
} EffectApplyContext;

static void InitEAContext(EffectApplyContext *ctx)
{
	memset(ctx, 0, sizeof(EffectApplyContext));
	ctx->mf.fEffectiveness = 1.0;
	ctx->mf.fTickChance = 1.0;
}

// pTarget is the power's target when kModTarget_PowerTarget, kModTarget_PowerTargetsOwnerAndAllPets
// pTarget is the caster when kModTarget_SelfsOwnerAndAllPets
// pTarget is an entity from the power's valid target list when kModTarget_Self, kModTarget_Target, kModTarget_TargetsOwnerAndAllPets
// pTarget is the pet when recursing on any of the "OwnerAndAllPets" types.
static void HandleModStacking(EffectApplyContext *ctx, const AttribModTemplate *ptemplate, int iAttribIdx, Character *pSrc, Character *pTarget)
{
	AttribModList *pmodlist;
	Character *pModDirected;
	AttribMod *oldMod, *newMod;
	Character *pStackSrc = 0;
	bool createANewMod = false, destroyNewMod = false, destroyOldMod = false;
	float fTimer = 0;
	float fTimerOldMod = 999999;		// anything >0 is fine to default to
	float fDuration = 0;
	size_t offAttrib;

	offAttrib = ptemplate->piAttrib[iAttribIdx];

	switch(ptemplate->eTarget)
	{
	// some powers target someone else but have effects that target the caster
	case kModTarget_Caster:
	case kModTarget_Marker:
		pmodlist = &pSrc->listMod;
		pModDirected = pSrc;
		break;

	// kModTarget_XXXXsOwnerAndAllPets is here for the recursed calls
	// it's safe for initial call due to if clause at end of CreateAndAttachNewAttribMod.
	case kModTarget_Focus:
	case kModTarget_Affected:
	case kModTarget_CastersOwnerAndAllPets:
	case kModTarget_FocusOwnerAndAllPets:
	case kModTarget_AffectedsOwnerAndAllPets:
	default:
		pmodlist = &pTarget->listMod;
		pModDirected = pTarget;
		break;
	}

	switch (ptemplate->eCasterStack)
	{
	case kCasterStackType_Individual:
		pStackSrc = pSrc;
		break;

	case kCasterStackType_Collective:
		pStackSrc = NULL; // passing NULL into modlist_FindOldestUnsuppressed and modlist_Count means it won't check against the source Character.
		break;
	}

	oldMod = modlist_FindOldestUnsuppressed(pmodlist, ptemplate, offAttrib, ctx->mf.pPow, pStackSrc);

	createANewMod = (oldMod == NULL
					|| ptemplate->eStack == kStackType_Stack
					|| (ptemplate->eStack == kStackType_StackThenIgnore && modlist_Count(pmodlist, ptemplate, offAttrib, ctx->mf.pPow, pStackSrc) < ptemplate->iStackLimit)
					|| ptemplate->eStack == kStackType_Refresh
					|| ptemplate->eStack == kStackType_RefreshToCount
					|| ptemplate->eStack == kStackType_Maximize
					|| ptemplate->eStack == kStackType_Suppress);

	// Just stack if the old mod would fall off before the new one applies anyway
	if (!createANewMod &&
		(ptemplate->eStack == kStackType_Replace ||
		 ptemplate->eStack == kStackType_Ignore ||
		 ptemplate->eStack == kStackType_StackThenIgnore)
		&& oldMod && oldMod->fDuration < (ctx->mf.fDelay + ptemplate->fDelay))
		createANewMod = true;

	if (!createANewMod)
	{
		if (ptemplate->eStack == kStackType_Ignore || ptemplate->eStack == kStackType_StackThenIgnore)
		{
			return;
		}

		// NOTE:  This switch statement has a case for EVERY StackType that can get to this point in the code!
		// If YOU alter this code, make sure you add a case, even if it just breaks.
		// This code is hard enough to understand, I'm trying to enforce this to make it easier.
		switch (ptemplate->eStack)
		{
		case kStackType_Extend:
			{
				fTimer = oldMod->fTimer;
				fDuration = oldMod->fDuration;
				break;
			}

		case kStackType_Replace:
			{
				mod_Cancel(oldMod, pModDirected);
				break;
			}

		case kStackType_Overlap:
			{
				break;
			}
		}

		// ARM NOTE:  If we've gotten here, we're building the new mod inside of the old mod's memory.
		// We should update the invoke ID so the buffs don't get out of sync.
		//uiInvokeID = oldMod->uiInvokeID;
		fTimerOldMod = oldMod->fTimer;
		newMod = oldMod;
	}
	else
	{
		newMod = attribmod_Create();
	}

	assert(newMod != NULL);

	if (newMod->evalInfo)
	{
		attribModEvalInfo_Destroy(newMod->evalInfo);
		newMod->evalInfo = NULL;
	}

	if (ctx->evalInfo)
	{
		newMod->evalInfo = ctx->evalInfo;
		newMod->evalInfo->refCount++;
	}

	PERFINFO_AUTO_START("mod_Fill", 1);
	mod_Fill(newMod, ptemplate, iAttribIdx, pSrc, pTarget, &ctx->mf);
	PERFINFO_AUTO_STOP();
	newMod->fTimer += fTimer;
	newMod->fDuration += fDuration;

	if (newMod == oldMod) {
		// If building the new mod in-place and the old mod is active,
		// need to fudge the delay to ensure there isn't a gap
		if (fTimerOldMod <= 0)
			newMod->fTimer = newMod->fCheckTimer = 0;
	}

	if (ptemplate->eStack == kStackType_Refresh || ptemplate->eStack == kStackType_RefreshToCount)
	{
		// refresh all matching copies of this mod with the duration we just filled, before we add the potential new one
		PERFINFO_AUTO_START("modlist_RefreshAllMatching", 1);
		modlist_RefreshAllMatching(pmodlist, ptemplate, offAttrib, ctx->mf.pPow, pStackSrc, newMod->fDuration);
		PERFINFO_AUTO_STOP();
	}

	switch (ptemplate->eStack)
	{
	case kStackType_RefreshToCount:
		{
			if (modlist_Count(pmodlist, ptemplate, offAttrib, ctx->mf.pPow, pStackSrc) >= ptemplate->iStackLimit)
				destroyNewMod = true;

			break;
		}

	case kStackType_Maximize:
		{
			if (oldMod && oldMod->fDuration >= (ctx->mf.fDelay + ptemplate->fDelay))
			{
				if ( newMod->fMagnitude > oldMod->fMagnitude ||
				     (newMod->fMagnitude == oldMod->fMagnitude &&
					 newMod->fDuration > oldMod->fDuration) ) {
					destroyOldMod = true;
					// Replacing the mod immediately; reset timers
					newMod->fTimer = newMod->fCheckTimer = 0;
				}
				else
					destroyNewMod = true;
			}

			break;
		}

	case kStackType_Suppress:
		{
			if (oldMod)
			{
				if (ABS(newMod->fMagnitude) > ABS(oldMod->fMagnitude))
				{
					oldMod->bSuppressedByStacking = true;
					// Suppressing the old mod immediately; reset timers
					newMod->fTimer = newMod->fCheckTimer = 0;

					// if fAbsorb mods could be kStackType_Suppress, you'd need to add stuff here...
				}
				else
					newMod->bSuppressedByStacking = true;
			}

			break;
		}
	}

	assert(!destroyNewMod || !destroyOldMod); // just in case someone does something stupid...

	if (destroyNewMod && newMod)
	{
		// This should never happen, but since I do a pointer copy above, I'm just making sure...
		if (newMod == oldMod)
			oldMod = NULL;

		attribmod_Destroy(newMod);
		newMod = NULL;
	}

	if (destroyOldMod && oldMod)
	{
		modlist_RemoveAndDestroyMod(pmodlist, oldMod);
		// the above calls attribmod_Destroy().
		oldMod = NULL;
	}

	if (createANewMod && newMod)
	{
		PERFINFO_AUTO_START("AddMod", 1);
		modlist_AddModToTail(pmodlist, newMod);
		PERFINFO_AUTO_STOP();

		if (ptemplate && ptemplate->ppowBase && ptemplate->ppowBase->bShowBuffIcon)
		{
			if (ptemplate->eTarget == kModTarget_Caster)
			{
				pSrc->entParent->team_buff_update = true;
			}
			else
			{
				pTarget->entParent->team_buff_update = true;
			}
		}
	}

	if (newMod)
	{
		if(newMod->ptemplate->peffParent->evalFlags & ATTRIBMOD_EVAL_DELAY)
			eaPush(ctx->delayEvalAttribMods, newMod);

		newMod->bUseLocForReport = ctx->bVecFloatLoc;

		assert(newMod->next != newMod); // another check, just to be sure...
	}
}

void MarkLargestModForPromotionOutOfSuppression(Character *p, AttribMod *pmod)
{
	AttribMod *largestMod;
	Character *pStackSrc;

	if (!p || !pmod)
		return;

	switch (pmod->ptemplate->eCasterStack)
	{
	case kCasterStackType_Individual:
		{
			Entity *eSrc = erGetEnt(pmod->erSource);
			pStackSrc = eSrc ? eSrc->pchar : NULL;
			break;
		}

	case kCasterStackType_Collective:
		pStackSrc = NULL; // passing NULL into modlist_FindLargestMagnitudeUnsuppressed and modlist_Count means it won't check against the source Character.
		break;
	}

	largestMod = modlist_FindLargestMagnitudeSuppressed(&p->listMod, pmod->ptemplate, pmod->offAttrib, pmod->parentPowerInstance, pStackSrc);

	if (largestMod)
	{
		eaPush(&p->ppModsToPromoteOutOfSuppression, largestMod);

		// if fAbsorb mods could be kStackType_Suppress, you'd need to add stuff here...
	}
}

void PromoteModsOutOfSuppression(Character *p)
{
	int index;

	for (index = eaSize(&p->ppModsToPromoteOutOfSuppression) - 1; index >= 0; index--)
	{
		p->ppModsToPromoteOutOfSuppression[index]->bSuppressedByStacking = false;
	}

	eaSetSize(&p->ppModsToPromoteOutOfSuppression, 0);
}

static void HandleExecutePower(EffectApplyContext *ctx, const AttribModTemplate *ptemplate, Character *pSrc, Character *pTarget)
{
	const AttribModParam_Power *param = TemplateGetParams(ptemplate, Power);
	const BasePower **ppowList = 0;
	Power *epow;
	DeferredPower *dp;
	EntityRef erTarget;
	int i;

	if (!ctx || !pSrc || !pTarget || !ctx->mf.pPow || !param)
		return;

	// Save relevant power activation info and push it into the list of deferred powers.
	// The list is processed right after the current power tick in TickPhaseOne, so
	// deferred powers with a delay of zero will activate in the same server tick.

	erTarget = erGetRef(pTarget->entParent);

	for (i = eaSize(&param->ps.ppPowers) - 1; i >= 0; i--)
	{
		const BasePower *ebase = param->ps.ppPowers[i];
		epow = power_Create(ebase);
		if (!epow)
			continue;

		// Keep a reference to the parent power
		epow->ppowParentInstance = power_AcquireRef(ctx->mf.pPow);

		// Copy enhancements if allowed
		if (TemplateHasFlag(ptemplate, CopyBoosts))
			character_SlotPowerFromSource(epow, ctx->mf.pPow, 0);

		dp = deferredpower_Create(epow, erTarget, ctx->mf.vec, ctx->mf.fDelay + ptemplate->fDelay);
		if (dp)
			eaPush(&pSrc->ppowDeferred, dp);
		else		// allocation failure is bad news, but handle anyway
			power_Destroy(epow, "DeferredPower");
	}
}

static int FinalizeNewAttribMod(EffectApplyContext *ctx, const AttribModTemplate *ptemplate, Character *pSrc, Character *pTarget, bool petInherit)
{
	int idx;
	for (idx = eaiSize(&ptemplate->piAttrib) - 1; idx >= 0; idx--)
	{
		HandleModStacking(ctx, ptemplate, idx, pSrc, pTarget);

		if (ptemplate->piAttrib[idx] == kSpecialAttrib_ExecutePower) {
			HandleExecutePower(ctx, ptemplate, pSrc, pTarget);
		}
	}

	if (petInherit)
	{
		int i;
		Character *origPrevTarget = ctx->mf.pPrevTarget;
		for(i = 0; i < pTarget->entParent->petList_size; i++)
		{
			Entity *pet = erGetEnt(pTarget->entParent->petList[i]);
			if (pet && !pet->petByScript)
			{
				// Explicitly use pSrc for PrevTarget in this case, so that *AndAllPets mods
				// that are chain effects appear to chain from the owner to the pet
				ctx->mf.pPrevTarget = pSrc;
				FinalizeNewAttribMod(ctx, ptemplate, pSrc, pet->pchar, true);
			}
		}
		ctx->mf.pPrevTarget = origPrevTarget;
	}

	return 1;
}

// Allows a power to retrive Location targeting information from ScriptMarkers and use it when processing Location EffectArea powers
static int AttachMarkerTargetAttribMod(EffectApplyContext *ctx, const AttribModTemplate *ptemplate, Character *pSrc, Character *pTarget)
{
		int success = 0;

		if(ptemplate->pTargetInfo)
		{
			int mi, msize = eaSize(&ptemplate->pTargetInfo->ppchMarkerNames);
			for (mi = 0; mi < msize; mi++)
			{
				const char *markerName = ptemplate->pTargetInfo->ppchMarkerNames[mi];
				int markerNum = ptemplate->pTargetInfo->piMarkerCount[mi];
				ScriptMarker* marker = MarkerFind(markerName, 0);
				Position** randomizedLocs = NULL;
				int i, n;

				if (!marker || !marker->locs)
					continue;

				n = eaSize(&marker->locs);

				eaCreate(&randomizedLocs);
				eaCopy(&randomizedLocs, &marker->locs);

				if(markerNum > 0) {
					// Randomize the list of possible locations
					for (i = 0; i < n; ++i)
					{
						int index = getCappedRandom(n-i)+i;
				
						if (i != index)
						{
							eaSwap(&randomizedLocs, i, index);
						}
					}
					i = MIN(n, markerNum) - 1;
				}
				else
					i = n - 1;		// All of them

				for(; i >= 0; i--)
				{
					copyVec3( randomizedLocs[i]->mat[3], ctx->mf.vec );
					success |= FinalizeNewAttribMod(ctx, ptemplate, pSrc, pTarget, false);
				}
			}
		}
		return success;
}

static float CalculateEffectChance(const float baseChance, const Character *pSrc, const Power *ppow, const EffectGroup *peffect, const bool bIsProc)
{
	float fChanceFinal = baseChance;
	const Power *ppowOrig = ppow;			// Original power

	while (ppowOrig->ppowParentInstance)	// Follow up through chain of parents if it has one
		ppowOrig = ppowOrig->ppowParentInstance;

	//	HYBRID
	if (bIsProc && peffect->fProcsPerMinute > 0.0f)
	{
		// calculate real chance here
		float AreaFactor = 1.0;
		float ppm = peffect->fProcsPerMinute;
		const Power *ppowTemp;

		// Accumulate AreaFactor for the effect area of this power and all that executed it
		ppowTemp = ppow;
		while (ppowTemp) {
			AreaFactor *= basepower_CalculateAreaFactor(ppowTemp->ppowBase);
			ppowTemp = ppowTemp->ppowParentInstance;
		}
		AreaFactor = AreaFactor * .75f + .25f; // Phil wants it different.  Yay!

		fChanceFinal = ppm;
		if (ppowOrig->ppowBase->eType == kPowerType_Click)
		{
			// Phil wants boosted recharge to reduce proc chance but not global recharge.
			float RechargeDivisor;
			if (ppowOrig->ppowBase->bIgnoreStrength) {
				// Power is ignoring outside strength, so use recharge directly as it will
				// only include enhancements.
				RechargeDivisor = ppowOrig->pattrStrength->fRechargeTime;
			} else if (ppowOrig->pattrStrength->fRechargeTime > pSrc->attrStrength.fRechargeTime) {
				// Calculate based on total boosted recharge - character recharge to subtract
				// out global buffs. Add 1 since both of those are accumulations on top of 1.0.
				RechargeDivisor = (1 + ppowOrig->pattrStrength->fRechargeTime - pSrc->attrStrength.fRechargeTime);
			} else {
				// If we got here, the power must be set to ignore recharge entirely.
				RechargeDivisor = 1;
			}
			fChanceFinal *= ppowOrig->ppowBase->fRechargeTime / RechargeDivisor + ppowOrig->ppowBase->fTimeToActivate;
		}
		else
			fChanceFinal *= peffect->ppowBase->fActivatePeriod;
		fChanceFinal /= (60.0f * AreaFactor);

		fChanceFinal = MINMAX(fChanceFinal, 0.05f + 0.015f * ppm, 0.9f);
	}

	ChanceModAccumulate(&fChanceFinal,peffect,pSrc->ppmodChanceMods);
	ChanceModAccumulate(&fChanceFinal,peffect,ppow->ppmodChanceMods);

	return fChanceFinal;
}

static float CalculateTickChance(const float baseChance, const Character *pSrc, const Power *ppow, const AttribModTemplate *ptemplate, const bool bIsProc)
{
	float fChanceFinal = baseChance;

	ChanceModAccumulate(&fChanceFinal,ptemplate->peffParent,pSrc->ppmodChanceMods);
	ChanceModAccumulate(&fChanceFinal,ptemplate->peffParent,ppow->ppmodChanceMods);

	return fChanceFinal;
}

static int HandleRadiusChecks(EffectApplyContext *ctx, const EffectGroup *peffect,
							  Character *pSrc, Character *pTarget,
							  Character *pMainTarget)
{
	Power *ppow = ctx->mf.pPow;
	Vec3 vecTarget;
	float fDist;
	float fRadiusInner = peffect->fRadiusInner*ppow->pattrStrength->fRadius;
	float fRadiusOuter = peffect->fRadiusOuter*ppow->pattrStrength->fRadius;

	if(ppow->ppowBase->eTargetType == kTargetType_Location
		|| ppow->ppowBase->eTargetType == kTargetType_Teleport
		|| ppow->ppowBase->eTargetType == kTargetType_Position)
	{
		copyVec3(ctx->mf.vec, vecTarget);
	}
	else if(!(character_EntUsesGeomColl(pMainTarget->entParent) && character_FindHitLoc(pSrc->entParent,pMainTarget->entParent,vecTarget)))
	{
		copyVec3(ENTPOS(pMainTarget->entParent), vecTarget);
	}
	else
	{
		return 1; // we don't have a vecTarget, exit gracefully instead of doing something random
	}

	// range is from the edge of the capsule, or the point passed in (for geom collision targets)
	fDist = distance3Squared(vecTarget, ctx->bVecFloatLoc ? ctx->mf.vec : ENTPOS(pTarget->entParent));

	if (fRadiusInner >= 0
		&& MAX(0.0f, sqrt(fDist) - pTarget->entParent->motion->capsule.radius) < fRadiusInner)
	{
		// Too far inside, don't make the Attribmod
		return 0;
	}

	if (fRadiusOuter >= 0
		&& fDist >= SQR(fRadiusOuter + pTarget->entParent->motion->capsule.radius))
	{
		// Too far outside, don't make the Attribmod
		return 0;
	}

	return 1;
}

/**********************************************************************func*
 * CreateAndAttachNewAttribMod
 *
 */
#include "svr_chat.h"
static int CreateAndAttachNewAttribMod(EffectApplyContext *ctx, const AttribModTemplate *ptemplate,
									   Character *pSrc, Character *pTarget,
									   Character *pMainTarget)
{
	Power *ppow = ctx->mf.pPow;
	assert(pSrc!=NULL);
	assert(ppow!=NULL);
	assert(ptemplate!=NULL);

	// Make sure we're supposed to be adding this activation type
	if (eaiSize(&ctx->eApplicationFilter)) {
		bool bMatched = false;
		int i;

		for (i = eaiSize(&ctx->eApplicationFilter) - 1; i >= 0; i--) {
			bMatched |= (ctx->eApplicationFilter[i] == ptemplate->eApplicationType);
		}
		if (!bMatched)
			return 0;
	}

	// If this is a proc then ignore boost templates
	if (ctx->bIsProc && TemplateHasFlag(ptemplate, Boost))
		return 0;

	// Early out of this mod is going to be ineffective anyway
	if (ctx->mf.fEffectiveness <= 0.0)
		return 0;

	if(ptemplate->fTickChance >= 1.0)
	{
		ctx->mf.fTickChance = 1.0;
	}
	else
	{
		ctx->mf.fTickChance = CalculateTickChance(ptemplate->fTickChance, pSrc,
			ppow, ptemplate, ctx->bIsProc);
	}

	// player attacking or being hit by a player
	if(!ppow->bOnLastTick && (TemplateGetSub(ptemplate, pMessages, pchDisplayAttackerHit)||TemplateGetSub(ptemplate, pMessages, pchDisplayVictimHit)) )
		AddDeferredCombatEvent(ctx->mf.fDelay, pSrc->entParent, pTarget->entParent, ppow, ctx->mf.fToHit, ctx->mf.fToHitRoll, ctx->bForceHit, ctx->bAlwaysHit, true, true);

	if (ptemplate->eTarget == kModTarget_CastersOwnerAndAllPets
		|| ptemplate->eTarget == kModTarget_FocusOwnerAndAllPets
		|| ptemplate->eTarget == kModTarget_AffectedsOwnerAndAllPets)
	{
		Character *pTopLevelTarget;
		Entity *temp;
		
		if (ptemplate->eTarget == kModTarget_AffectedsOwnerAndAllPets)
		{
			pTopLevelTarget = pTarget;
		}
		else if (ptemplate->eTarget == kModTarget_FocusOwnerAndAllPets)
		{
			pTopLevelTarget = pMainTarget;
		}
		else
		{
			pTopLevelTarget = pSrc;
		}

		if (pTopLevelTarget->entParent->erOwner &&
			(temp = erGetEnt(pTopLevelTarget->entParent->erOwner)))
		{
			if (ptemplate->eApplicationType != kModApplicationType_OnTick && isDevelopmentMode())
			{
				// See the ARM NOTE in unpackEntPowers() for why this is here...
				ErrorFilenamef(ptemplate->ppowBase->pchSourceFile,
								"Power (%s) has an 'owner and all pets' attrib mod that isn't an onTick and originated from something that isn't the owner!  This doesn't work properly in the code, so don't do this in data!",
								ptemplate->ppowBase->pchFullName);
			}

			pTopLevelTarget = temp->pchar;
		}

		return FinalizeNewAttribMod(ctx, ptemplate, pSrc, pTopLevelTarget, true);
	}
	else if (ptemplate->eTarget == kModTarget_Focus)
	{
		return FinalizeNewAttribMod(ctx, ptemplate, pSrc, pMainTarget, false);
	}
	else if (ptemplate->eTarget == kModTarget_Marker)
	{
		return AttachMarkerTargetAttribMod(ctx, ptemplate, pSrc, pTarget);
	}
	else
	{
		return FinalizeNewAttribMod(ctx, ptemplate, pSrc, pTarget, false);
	}
}

// Prototype for recursion
int ApplyEffectGroups(EffectApplyContext *ctx, const EffectGroup **ppEffects,
							Character *pSrc, Character *pTarget,
							Character *pMainTarget);

static int ApplyEffectGroup(EffectApplyContext *ctx, const EffectGroup *peffect,
							Character *pSrc, Character *pTarget,
							Character *pMainTarget, bool *bApplied)
{
	bool bCreatedDelayedEval = false;
	float fChanceFinal;
	int i, iSize, attached;
	float fOrigDelay = ctx->fDelayAccum;

	assert(pSrc!=NULL);
	assert(peffect!=NULL);

	// Early out of this effect group is going to be ineffective anyway
	if (ctx->mf.fEffectiveness <= 0.0)
		return 0;

	if (EffectHasFlag(peffect, PVPOnly) &&
		!(server_visible_state.isPvP || OnArenaMap() || RaidIsRunning()))
			return 0;				// PVP-only effect on a PVE map, ignore

	if (EffectHasFlag(peffect, PVEOnly) &&
		(server_visible_state.isPvP || OnArenaMap() || RaidIsRunning()))
			return 0;				// PVE-only effect on a PVP map, ignore

	fChanceFinal = CalculateEffectChance(peffect->fChance, pSrc, ctx->mf.pPow, peffect, ctx->bIsProc);
	// Do the chance roll early as well so we can avoid attaching anything if it fails
	if(!mod_ChanceRoll(peffect, fChanceFinal, ctx->mf.uiInvokeID))
		return 0;

	// Set up any necessary evaluator context for any child mods or groups.
	// More efficient to do it here and share it between all of them.
	if(peffect->evalFlags & ATTRIBMOD_EVAL_DELAY) {
		// A delayed expression will end up needing this
		if (!ctx->evalInfo) {
			ctx->evalInfo = attribModEvalInfo_Create();
			ctx->evalInfo->refCount++;
			bCreatedDelayedEval = true;
		}
		ctx->evalInfo->fRand = ctx->mf.fToHitRoll;
		ctx->evalInfo->fToHit = ctx->mf.fToHit;
		ctx->evalInfo->bAlwaysHit = ctx->bAlwaysHit;
		ctx->evalInfo->bForceHit = ctx->bForceHit;
		ctx->evalInfo->iChainJump = ctx->mf.iChainJump;
		ctx->evalInfo->fChainEff = ctx->mf.fChainEff;
		ctx->evalInfo->fChanceFinal = fChanceFinal;
	}

	if(peffect->evalFlags & ATTRIBMOD_EVAL_CUSTOMFX)
		combateval_StoreCustomFXToken(power_GetRootToken(ctx->mf.pPow));
	if(peffect->evalFlags & ATTRIBMOD_EVAL_HIT_ROLL)
		combateval_StoreToHitInfo(ctx->mf.fToHitRoll, ctx->mf.fToHit, ctx->bAlwaysHit, ctx->bForceHit);
	if(peffect->evalFlags & ATTRIBMOD_EVAL_CHANCE)
	{
		float fChanceMods = 0.0;
		fChanceMods = CalculateEffectChance(0.0, pSrc, ctx->mf.pPow, peffect, ctx->bIsProc);
		combateval_StoreChance(fChanceFinal, fChanceMods);

		if(peffect->evalFlags & ATTRIBMOD_EVAL_DELAY)
			ctx->evalInfo->fChanceMods = fChanceMods;
	}
	if (peffect->evalFlags & ATTRIBMOD_EVAL_CHAIN)
		combateval_StoreChainInfo(ctx->mf.iChainJump, ctx->mf.fChainEff);

	// If there's a condition on this effect group, check it.
	if(peffect->ppchRequires &&
		combateval_Eval(pSrc->entParent, pTarget->entParent, ctx->mf.pPow, peffect->ppchRequires, ctx->mf.pPow->ppowBase->pchSourceFile)==0.0f)
	{
		return 0;
	}

	// Let the caller know that we got this far, so it shouldn't use Fallback groups.
	if (bApplied)
		*bApplied = true;

	// If RadiusInner and RadiusOuter is 0, then ignore all critters but 
	//    the originally targeted one.
	if((peffect->fRadiusInner==0 && peffect->fRadiusOuter==0) &&
		(pTarget != pMainTarget))
	{
		return 0;
	}

	// If there is a radius restriction on this attribmod, check it.
	if(peffect->fRadiusInner>=0 || peffect->fRadiusOuter>=0)
	{
		if (!HandleRadiusChecks(ctx, peffect, pSrc, pTarget, pMainTarget))
			return 0;
	}

	ctx->fDelayAccum += peffect->fDelay;

	// Attach all templates in this effect group
	attached = 0;
	iSize = eaSize(&peffect->ppTemplates);
	for (i = 0; i < iSize; i++) {
		const AttribModTemplate *ptemplate = peffect->ppTemplates[i];

		ctx->mf.fDelay = ctx->fDelayAccum +
			(TemplateHasFlag(ptemplate, NoHitDelay) ? 0.0 : ctx->fEffectDelay) +
			(TemplateHasFlag(ptemplate, NoProjectileDelay) ? 0.0 : ctx->fEffectDelayFromProjectiles);
		attached |= CreateAndAttachNewAttribMod(ctx, ptemplate, pSrc, pTarget, pMainTarget);
	}

	// Recursively apply any child effect groups.
	if (peffect->ppEffects) {
		attached |= ApplyEffectGroups(ctx, peffect->ppEffects, pSrc, pTarget, pMainTarget);
	}

	// If we created a delayed eval struct, release our reference, which will
	// destroy it if nothing else held onto it.
	// Only happens at the very top level.
	if (bCreatedDelayedEval) {
		attribModEvalInfo_Destroy(ctx->evalInfo);
		ctx->evalInfo = NULL;
	}
	ctx->fDelayAccum = fOrigDelay;

	return attached;
}

static int ApplyEffectGroups(EffectApplyContext *ctx, const EffectGroup **ppEffects,
							Character *pSrc, Character *pTarget,
							Character *pMainTarget)
{
	bool bApplied = false;
	int i, iSize;
	int attached = 0;

	iSize = eaSize(&ppEffects);
	for (i = 0; i < iSize; i++) {
		if (!EffectHasFlag(ppEffects[i], Fallback)) {
			attached |= ApplyEffectGroup(ctx, ppEffects[i],
				pSrc, pTarget, pMainTarget, &bApplied);
		}
	}

	if (bApplied)
		return attached;		// We applied something and are done here

	// None of the regular groups applied; use Fallback groups
	iSize = eaSize(&ppEffects);
	for (i = 0; i < iSize; i++) {
		if (EffectHasFlag(ppEffects[i], Fallback)) {
			attached |= ApplyEffectGroup(ctx, ppEffects[i],
				pSrc, pTarget, pMainTarget, NULL);
		}
	}

	return attached;
}

/**********************************************************************func*
 * CalcDefense
 *
 */
static float CalcDefense(Character *pTarget, Power *ppow, int *out_iType)
{
	int i;
	bool typed = false;
	float fBase = pTarget->attrCur.fDefense;
	float fMax = -FLT_MAX;

	for(i=eaiSize(&ppow->ppowBase->pAttackTypes)-1; i>=0; i--)
	{
		float f;

		typed = true;

		assert(ppow->ppowBase->pAttackTypes[i]>=offsetof(CharacterAttributes, fDefenseType)
			&& ppow->ppowBase->pAttackTypes[i]<offsetof(CharacterAttributes, fDefense));

		f = *ATTRIB_GET_PTR(&pTarget->attrCur, ppow->ppowBase->pAttackTypes[i]);

		if(f>fMax)
		{
			fMax = f;
			*out_iType = ppow->ppowBase->pAttackTypes[i];
		}
	}

	return (fBase+(typed?fMax:0.0f));
}


static float CalcElusivity(Character *pTarget, Power *ppow, int *out_iType)
{
	int i;
	bool typed = false;
	float fBase = (pTarget->attrStrength.fElusivityBase - 1.f);
	float fMax = -FLT_MAX;
	int offset_diff = offsetof(CharacterAttributes, fElusivity) - offsetof(CharacterAttributes, fDefenseType);

	for(i=eaiSize(&ppow->ppowBase->pAttackTypes)-1; i>=0; i--)
	{
		float f;

		typed = true;

		assert(ppow->ppowBase->pAttackTypes[i]>=offsetof(CharacterAttributes, fDefenseType)
			   && ppow->ppowBase->pAttackTypes[i]<offsetof(CharacterAttributes, fDefense) );

		f = (*ATTRIB_GET_PTR(&pTarget->attrStrength, ppow->ppowBase->pAttackTypes[i] + offset_diff))-1;

		if(f>fMax)
		{
			fMax = f;
			*out_iType = ppow->ppowBase->pAttackTypes[i];
		}
	}

	return fBase+(typed?fMax:0.0f);
}

/**********************************************************************func*
 * CalcToHit
 *
 */
static float CalcToHit(Character *pSrc, Character *pTarget, Power *ppow, float *out_fDefense, int *out_iType)
{
	int i;
	CombatMod *pcm;
	float fToHit;
	int iEffCombatLevel = pTarget->iCombatLevel;
	int sourceModShift = 0;
	int targetModShift = 0;
	float fDefenseAbs, fDefenseRel, fAccuracyFactor, fElusivity;

	assert(pSrc!=NULL);
	assert(pTarget!=NULL);
	assert(ppow!=NULL);

	if(combatmod_Ignored(pSrc, pTarget))
	{
		//	I'm confused, why do we switch to the src's combat level???
		//	12/20/10 - EML
		iEffCombatLevel = pSrc->iCombatLevel;
		if (pSrc != pTarget)
		{
			if (pSrc->bIgnoreCombatMods)
			{
				iEffCombatLevel -= pSrc->ignoreCombatModLevelShift;
				if (iEffCombatLevel < 0)
				{
					iEffCombatLevel = 0;
				}
			}
			else if (pTarget->bIgnoreCombatMods)					//	why is this an else if. what if they both ignore combat mods
			{
				iEffCombatLevel += pSrc->ignoreCombatModLevelShift;
				if (iEffCombatLevel < 0)
				{
					iEffCombatLevel = 0;
				}
			}
		}
	}
	else
	{
		//	see attribmod.c ~803 for where I'm basing this off of
		sourceModShift += pSrc->iCombatModShift;
		targetModShift += pTarget->iCombatModShift;
	}

	combatmod_Get(pSrc, pSrc->iCombatLevel + sourceModShift, iEffCombatLevel + targetModShift, &pcm, &i);

	fToHit = pSrc->attrCur.fToHit;

	fToHit += combatmod_ToHit(pcm, i) - 1.0f;
	fToHit += combatmod_PvPToHitMod(pSrc, pTarget);

	*out_iType = -1;
	fDefenseAbs = CalcDefense(pTarget, ppow, out_iType);
	fDefenseRel = fDefenseAbs;

	if(fDefenseRel>0.0f && fToHit<0.05f) // If ToHit was already below clamp and we're going to reduce it more
	{
		fDefenseRel += (0.05f - fToHit); // Add that much to defense so it's not removed later
	}
	
	fToHit -= fDefenseAbs;

	if(fToHit<0.05f) 
	{
		fDefenseRel -= (0.05f - fToHit); // Any missing that gets thrown away is removed from defense
		fToHit = 0.05f;
	}

	fElusivity = CalcElusivity(pTarget, ppow, out_iType);
	fElusivity *= (1-(combatmod_Magnitude(pcm, i)-1));
	fElusivity = CLAMP( fElusivity, 0.f, .95f );

	if (server_state.enableSpikeSuppression && pSrc != pTarget && pTarget->stTargetingMe)
	{
		int numTargeting = stashGetValidElementCount(pTarget->stTargetingMe);
		float fElusivityMod = 0.0f;

		if (numTargeting > 1)
			fElusivityMod =  (combatmod_PvPElusivityMod(pSrc) * (float) (numTargeting - 1));

		fElusivity += fElusivityMod;
	}

	fElusivity = CLAMP( fElusivity, 0.f, .95f );
	fAccuracyFactor = (ppow->ppowBase->fAccuracy * ppow->pattrStrength->fAccuracy * combatmod_Accuracy(pcm, i) * (1 - fElusivity) );
	fToHit *= fAccuracyFactor;
	fDefenseRel *= fAccuracyFactor;
	
	if(fToHit<0.05f) // Under clamp, penalize defense
	{
		fDefenseRel -= (0.05f - fToHit);
		fToHit = 0.05f;
	}
	else if(fToHit>0.95f) // Over clamp
	{
		fToHit = 0.95f;
	}

	*out_fDefense = CLAMP(fDefenseRel, 0.0f, (1.0f-fToHit)-0.05f);

	return fToHit;
}

/**********************************************************************func*
 * ForceHitDueToMisses
 *
 */
static bool ForceHitDueToMisses(Character *pSrc, float fToHit)
{
	int iDecile;
	int aHitForcedCount[] =
		// 0,  10,  20,  30, 40, 50, 60, 70, 80, 90
		{ 100, 100,  8,   6,  4,  4,  3,  3,  2,  1 };

	if(fToHit < pSrc->fSequentialMinToHit
		|| pSrc->fSequentialMinToHit < 0.05f)
	{
		pSrc->fSequentialMinToHit = fToHit;
	}

	iDecile = (int)(pSrc->fSequentialMinToHit*10.0f);
	if(iDecile<0) iDecile=0;
	if(iDecile>9) iDecile=9;

	return (aHitForcedCount[iDecile]<=pSrc->iSequentialMisses);
}

/**********************************************************************func*
 * character_PowerRange
 *
 */
float character_PowerRange(Character *pSrc, Power *ppow)
{
	assert(pSrc!=NULL);
	assert(ppow!=NULL);

	return ppow->ppowBase->fRange*ppow->pattrStrength->fRange;
}

/**********************************************************************func*
 * character_PowerRange2
 *
 */
float character_PowerRange2(Character *pSrc, Power *ppow)
{
	assert(pSrc!=NULL);
	assert(ppow!=NULL);

	return ppow->ppowBase->fRangeSecondary*ppow->pattrStrength->fRange;
}

/**********************************************************************func*
 * character_EntUsesGeomColl
 *
 */
bool character_EntUsesGeomColl(Entity *pent)
{
	int ret = false;
	PERFINFO_AUTO_START("EntUsesGeomColl", 1);
	assert(pent);
	if( pent->seq && 
		pent->seq->type->collisionType == SEQ_ENTCOLL_LIBRARYPIECE && 
		!(pent->seq->type->flags & SEQ_USE_NORMAL_TARGETING) )//mw 1.13.06 changed to work for all worldgroup objects, not just "dynamic"s
	{
		if(pent->seq->worldGroupPtr == NULL)
			seqUpdateWorldGroupPtr(pent->seq,pent);
		if(pent->seq->worldGroupPtr )
			ret = true;
	}
	PERFINFO_AUTO_STOP();
	return ret;
}

// Callback function and params for geom testing
GroupDef *g_pObjGroupDef = NULL;
Vec3 g_vecObjLoc;
int allowObjectCallback(DefTracker *tracker,int backside)
{
	if (g_pObjGroupDef == NULL) return 0;
	while(tracker != NULL)
	{
		if (tracker->def == g_pObjGroupDef)
		{
			if(distance3Squared(tracker->mat[3],g_vecObjLoc)<1.0)
				return 1;
			return 0;
		}
		tracker = tracker->parent;
	}
	return 0;
}

/**********************************************************************func*
 * character_GeomInCylinder
 *  
 */
bool character_GeomInCylinder(const Vec3 source,const Vec3 endpoint,const Vec3 target,DefTracker* tracker,float fRad,Vec3 result)
{
	U32			collflags = COLL_DISTFROMCENTER|COLL_IGNOREINVISIBLE|COLL_USE_TRACKERS;
	CollInfo	coll;

	float	fDefDistSqr;
	float	fReach;
//	bool bCacheDisabled;
	bool bHit;


	if (!tracker)
		return false;

	PERFINFO_AUTO_START("GeomInCylinder", 1);


	groupLoadIfPreload(tracker->def); // Need this?

	// Quick radius check
	fDefDistSqr = MIN(distance3Squared(endpoint,target),distance3Squared(source,target));
	fReach = fRad + tracker->def->radius;
	if(fReach*fReach < fDefDistSqr)
	{
		PERFINFO_AUTO_STOP();
		return false; // This def is too far away
	}

	g_pObjGroupDef = tracker->def;
	copyVec3(target,g_vecObjLoc);
	
	PERFINFO_AUTO_START("Blahblah", 1);

	// This seems mighty naive
	//bCacheDisabled = gridSetCacheDisabled(true);

	coll.tracker_count = 1;
	coll.trackers = &tracker;

	bHit = collGrid(0,source,endpoint,&coll,fRad,collflags);
	if(bHit) copyVec3(coll.mat[3],result);

	PERFINFO_AUTO_STOP();

	// Cleanup
	g_pObjGroupDef = NULL;
	//gridSetCacheDisabled(bCacheDisabled);

	PERFINFO_AUTO_STOP();
	return bHit;
}


/**********************************************************************func*
 * character_GeomInRange
 *  Calls capped-cylinder check with radius Dist and length 0.001
 */
bool character_GeomInRange(const Vec3 source,const Vec3 target,DefTracker* tracker,float fDist,Vec3 result)
{
	Vec3 endpoint;
	copyVec3(source,endpoint);
	endpoint[1] += 0.001;
	return character_GeomInCylinder(source,endpoint,target,tracker,fDist,result);
}


/**********************************************************************func*
 * character_LocationReachGeomEnt
 * Returns a HitLocation if the test is successful, or NULL if it failed
 */

HitLocation* character_LocationReachGeomEnt(Entity *pSrc, Entity *pTarget, const Vec3 vecTarget, float fDist,bool recalc)
{
	HitLocation *hitloc = NULL;
	EntityRef erTarget = erGetRef(pTarget);
	int i;
	HitLocation **locs = pSrc->ppHitLocs;

	PERFINFO_AUTO_START("ReachGeomEnt", 1);

	// Search for a matching hitloc already calculated
	for(i=eaSize(&locs)-1; i>=0; i--)
	{
		if(locs[i]->erTarget == erTarget)
		{
			hitloc = locs[i];
			break;
		}
	}

	if(!recalc && hitloc)
	{
		if(hitloc->bReached && hitloc->fDistChecked <= fDist)
		{
			PERFINFO_AUTO_STOP();
			return hitloc; // We already know we hit from here with a shorter test
		}
		if(!hitloc->bReached && hitloc->fDistChecked >= fDist)
		{
			PERFINFO_AUTO_STOP();
			return NULL; // We already know we missed from here with a longer test
		}
	}

	if(!pTarget->seq->worldGroupPtr)
	{
		PERFINFO_AUTO_STOP();
		return NULL;
	}

	// If we didn't find a match, make a new one
	if(!hitloc)
	{
		hitloc = calloc(1,sizeof(HitLocation));
		hitloc->erTarget = erTarget;
		eaPush(&pSrc->ppHitLocs,hitloc);
	}
	hitloc->fDistChecked = fDist;

	hitloc->bReached = character_GeomInRange(vecTarget,
											ENTPOS(pTarget),
											pTarget->coll_tracker,
											fDist,
											hitloc->vecHitLoc);

	PERFINFO_AUTO_STOP();
	return hitloc->bReached?hitloc:NULL;
}

/**********************************************************************func*
 * character_ReachGeomEnt
 * Returns a HitLocation if the test is successful, or NULL if it failed
 */

HitLocation* character_ReachGeomEnt(Entity *pSrc, Entity *pTarget,float fDist,bool recalc)
{
	Vec3 srcloc;
	copyVec3(ENTPOS(pSrc),srcloc);
	srcloc[1] += 5.0; // Move to chest

	return character_LocationReachGeomEnt(pSrc,pTarget,srcloc,fDist,recalc);
}



/**********************************************************************func*
 * character_FindHitLoc
 *
 */
bool character_FindHitLoc(Entity *pSrc, Entity *pTarget, Vec3 loc)
{
	HitLocation *hitloc = NULL;
	EntityRef erTarget = erGetRef(pTarget);
	int i;
	HitLocation **locs = pSrc->ppHitLocs;

	PERFINFO_AUTO_START("FindHitLoc", 1);

	// Search for a matching hitloc already calculated
	for(i=eaSize(&locs)-1; i>=0; i--)
	{
		// Reasonable to have multiple hit tests on one ent?
		if(locs[i]->erTarget == erTarget && locs[i]->bReached)
		{
			copyVec3(locs[i]->vecHitLoc,loc);
			PERFINFO_AUTO_STOP();
			return true;
		}
	}
	PERFINFO_AUTO_STOP();
	return false;
}


/**********************************************************************func*
 * character_LocationInRange
 *
 */
bool character_LocationInRange(Character *pSrc, Vec3 vecTarget, Power *ppow)
{
	assert(pSrc!=NULL);
	assert(ppow!=NULL);

	// range is from the edge of the capsule.

	return(distance3Squared(vecTarget, ENTPOS(pSrc->entParent))<=
		SQR(character_PowerRange(pSrc, ppow)+pSrc->entParent->motion->capsule.radius));
}

/**********************************************************************func*
 * character_LocationInRange2
 *
 */
bool character_LocationInRange2(Character *pSrc, Vec3 vecTarget, Power *ppow)
{
	assert(pSrc!=NULL);
	assert(ppow!=NULL);

	// range is from the edge of the capsule.

	return(distance3Squared(vecTarget, ENTPOS(pSrc->entParent))<=
		SQR(character_PowerRange2(pSrc, ppow)+pSrc->entParent->motion->capsule.radius));
}

/**********************************************************************func*
 * character_LocationInRadius
 *
 */
bool character_LocationInRadius(Entity* entSrc, Character *pTarget, const Vec3 vecTarget, Power *ppow, float *pfDistSqr)
{
	float fPowRadius = power_GetRadius(ppow);
	float fDistanceWithCapsules = fPowRadius+pTarget->entParent->motion->capsule.radius;

	assert(pTarget!=NULL);
	assert(ppow!=NULL);

	if(entSrc != pTarget->entParent && character_EntUsesGeomColl(pTarget->entParent))
	{
		// See if we hit the geometry from vecTarget
		HitLocation *loc = character_LocationReachGeomEnt(entSrc,pTarget->entParent,vecTarget,fPowRadius,false);
		if(loc)
		{
			// Distance is to hit location
			*pfDistSqr = distance3Squared(loc->vecHitLoc,vecTarget);
			return true;
		}
		else
		{
			// Not in range, set out distance as to capsule
			*pfDistSqr = distance3Squared(ENTPOS(pTarget->entParent),vecTarget);
			return false;
		}
	}

	if(pTarget->entParent != entSrc)
	{
		Vec3 posTarget;
		entGetPosAtEntTime(pTarget->entParent, entSrc, posTarget,0);
		*pfDistSqr = distance3Squared(vecTarget, posTarget);
	}
	else
	{
		*pfDistSqr = distance3Squared(vecTarget, ENTPOS(pTarget->entParent));
	}

	// range is from the edge of the capsule.
	return(*pfDistSqr<=SQR(fDistanceWithCapsules));
}


/**********************************************************************func*
* character_TargetIsInsideSourceBox
*
* This function is no longer very useful, and really should be replaced with a cylinder cylinder collision
* The box sphere thing was supposed to be useful for walls of fire and such, but that's not ever used and 
* the collision below isn't very precise.
*/
bool character_TargetIsInsideSourceBox(Entity* pSource, Entity *pTarget)
{
	Vec3 posTarget;
	F32 radiusTarget;
	Mat4 matSrc;
	Vec3 boxSrc;
	Vec3 temp2;
	const SeqType * srcType;
	const SeqType * tgtType;
	int hit = 0;

	srcType = pSource->seq->type;
	tgtType = pTarget->seq->type;

	//Get Sphere of target
	entGetPosAtEntTime(pTarget, pSource, posTarget,0);
	radiusTarget = pTarget->motion->capsule.radius;

	//Get Box of Src
	copyVec3( srcType->capsuleSize, boxSrc );
	copyMat4( ENTMAT(pSource), matSrc );
	mulVecMat3( srcType->capsuleOffset, matSrc, temp2 );
	addVec3( temp2, matSrc[3], matSrc[3] );

	// Not sure this is 100% correct for geom, don't think it's likely to be an issue though
	if(pSource != pTarget && character_EntUsesGeomColl(pTarget))
		return (NULL!=character_ReachGeomEnt(pSource,pTarget,MAX(MAX(boxSrc[0],boxSrc[1]),boxSrc[2]), false));

	//Quick Reject
	if( distance3( posTarget, matSrc[3] ) > radiusTarget + MAX( MAX( matSrc[3][0], matSrc[3][1] ), matSrc[3][2] ) )
		return 0;

	//Collide Box and Src
	{
		Vec3 posTargetX; //position of target relative to box
		Vec3 temp;
		subVec3( posTarget, matSrc[3], temp );
		transposeMat3( matSrc );
		mulVecMat3( temp, matSrc, posTargetX );

		//account for the fact the box is centered in X always 
		posTargetX[0] += boxSrc[0]/2.0f;
		//and centered in Y when Z is largest
		if( srcType->capsuleSize[2] > srcType->capsuleSize[1] && srcType->capsuleSize[2] > srcType->capsuleSize[0] )
		{
			posTargetX[1] += boxSrc[1]/2.0f;
		}
		//and centered in Y and Z when X is largest 
		if( srcType->capsuleSize[0] > srcType->capsuleSize[1] && srcType->capsuleSize[0] > srcType->capsuleSize[2] )
		{
			posTargetX[1] += boxSrc[1]/2.0f;
			posTargetX[2] += boxSrc[2]/2.0f;
		}

		//Sphere, Axis-aligned box collision (The programming question!)
		//Reject as squares
		if( posTargetX[0]+radiusTarget < 0							||
			boxSrc[0]                  < posTargetX[0]+radiusTarget	||
			posTargetX[1]+radiusTarget < 0							||
			boxSrc[1]                  < posTargetX[1]+radiusTarget ||
			posTargetX[2]+radiusTarget < 0							||
			boxSrc[2]                  < posTargetX[2]+radiusTarget )
		{
			hit = 0;
		}
		else
		{
			//2. If hit on angles, reject on distance
			//TO DO Check for false positive because you compared a box and a box, not a sphere and a box
			//if( pTarget != pSource )
			//	printf("HIT....");
			hit = 1;
		}
	}

	return hit;
}

/**********************************************************************func*
 * character_TargetIsPerceptible
 *
 */
bool character_TargetIsPerceptible(Character *pSrc, Character *pTarget)
{
	float fPerceptDist;

	assert(pSrc!=NULL);
	assert(pTarget!=NULL);

	if (!entity_TargetIsInVisionPhase(pSrc->entParent, pTarget->entParent))
	{
		return false;
	}

	// Yes if they're friendly
	if (character_TargetIsFriend(pSrc,pTarget))
	{
		return true;
	}

	// Get source perception
	fPerceptDist = pSrc->attrCur.fPerceptionRadius;
	// Add capsules
	fPerceptDist += pSrc->entParent->motion->capsule.radius+pTarget->entParent->motion->capsule.radius;
	// Subtract appropriate stealth radius of target
	fPerceptDist -= (ENTTYPE(pSrc->entParent)==ENTTYPE_PLAYER)?pTarget->attrCur.fStealthRadiusPlayer:pTarget->attrCur.fStealthRadius;
	// Cap
	fPerceptDist = MAX(0.0f,fPerceptDist);
	// Can we see them?
	return(distance3Squared(ENTPOS(pTarget->entParent), ENTPOS(pSrc->entParent))<=SQR(fPerceptDist));
}

/**********************************************************************func*
 * character_WithinDist
 *
 */
INLINEDBG bool character_WithinDist(Entity *pSrc, Entity *pTarget, float fDist)
{
	float fDistanceSquared; // this used to be returned, but it isn't used.  no reason to calculate it for special colliders.
	float fDistanceWithCapsules = fDist+pSrc->motion->capsule.radius+pTarget->motion->capsule.radius;

	if(pSrc != pTarget && character_EntUsesGeomColl(pTarget))
	{
		float fDistWithCap = fDist+pSrc->motion->capsule.radius;
		HitLocation *loc = NULL;
		// See if we hit the geometry
		loc = character_ReachGeomEnt(pSrc,pTarget,fDistWithCap,false);
		return !!loc;
	}

	if(	pTarget->seq->type->collisionType == SEQ_ENTCOLL_BONECAPSULES &&
		pSrc->seq->type->collisionType != SEQ_ENTCOLL_BONECAPSULES )
	{
		int i;
		EntityRef erTarget = erGetRef(pTarget);
		HitLocation *hitloc = NULL;

		// FIXME: this could have an early out if the entity capsules are very far apart.

		for(i = eaSize(&pSrc->ppHitLocs)-1; i >= 0; --i)
		{
			if(pSrc->ppHitLocs[i]->erTarget == erTarget)
			{
				hitloc = pSrc->ppHitLocs[i];
				break;
			}
		}

		// If we didn't find a match, make a new one
		if(!hitloc)
		{
			EntityCapsule *cap;
			Mat4 mat;

			//Get source cap (since src is always in own time domain, it's easier) 
			cap = &pSrc->motion->capsule;
			positionCapsule(ENTMAT(pSrc), cap, mat);

			hitloc = calloc(1,sizeof(HitLocation));
			hitloc->erTarget = erTarget;
			hitloc->fDistChecked = seqCapToBoneCap(	mat[3], mat[1], cap->length, cap->radius, pTarget->seq,
													NULL, hitloc->vecHitLoc, NULL);
			hitloc->bReached = hitloc->fDistChecked >= 0.f;
			eaPush(&pSrc->ppHitLocs, hitloc);
		}

		return hitloc->fDistChecked <= fDist;
	}

	//mw 3.13.06 If either entity is tagged as SEQ_USE_CAPSULE_FOR_RANGE_FINDING, 
	//then do a proper capsule to capsule distance check
	//TO DO this doesn't interact properly with the EntUsesGeomColl flag.  I want to be very careful not to
	//change existing behavior, so I'm loath to fiddle too much with this function without ovehauling it.
	if( pTarget->seq->type->flags & SEQ_USE_CAPSULE_FOR_RANGE_FINDING 
		|| pSrc->seq->type->flags & SEQ_USE_CAPSULE_FOR_RANGE_FINDING )
	{
		EntityCapsule * tgtCap;
		EntityCapsule * srcCap;
		Mat4 tgtCapMat;
		Mat4 srcCapMat;
		Vec3 tgtJunk, srcJunk;

		tgtCap = &pTarget->motion->capsule;   
		srcCap = &pSrc->motion->capsule;  

		//Get tgtCapMat
		{
			Quat qrot;
			Mat4 tgtMatAtSrcTime;

			entGetPosAtEntTime( pTarget, pSrc, tgtMatAtSrcTime[3], qrot ); 
			quatToMat(qrot, tgtMatAtSrcTime); 
			positionCapsule( tgtMatAtSrcTime, tgtCap, tgtCapMat );  
		}	

		//Get srcCapMat (since src is always in own time domain, it's easier) 
		positionCapsule( ENTMAT(pSrc), srcCap, srcCapMat );   
 

//		fDistanceSquared = coh_LineLineDist(tgtCapMat[3], tgtCapMat[tgtCap->dir], tgtCap->length, tgtJunk,
//											srcCapMat[3], srcCapMat[srcCap->dir], srcCap->length, srcJunk );

		fDistanceSquared = coh_LineLineDist(tgtCapMat[3], tgtCapMat[1], tgtCap->length, tgtJunk,
											srcCapMat[3], srcCapMat[1], srcCap->length, srcJunk );

		//printf( "dist %4.4f need %4.4f\n", sqrt(*pfDistSqr), fDistanceWithCapsules );
	}
	else if(pTarget != pSrc)
	{
		Vec3 posTarget;
		entGetPosAtEntTime(pTarget, pSrc, posTarget,0);
		fDistanceSquared = distance3Squared(posTarget, ENTPOS(pSrc));
	}
	else
	{
		fDistanceSquared = distance3Squared(ENTPOS(pTarget), ENTPOS(pSrc));
	}

	return fDistanceSquared <= SQR(fDistanceWithCapsules);
}

/**********************************************************************func*
* character_InRange
*
*/
bool character_InRange(Character *pSrc, Character *pTarget, Power *ppow)
{
	return character_WithinDist(pSrc->entParent, pTarget->entParent, character_PowerRange(pSrc, ppow));
}

/**********************************************************************func*
* character_TargetCollisionRadius
*
*/
float character_TargetCollisionRadius(Entity *entSrc, Entity *entTarget)
{ 
 	if(entSrc != entTarget && character_EntUsesGeomColl(entTarget))
	{
		float fDist = distance3(ENTPOS(entSrc),ENTPOS(entTarget));
		HitLocation *loc = character_ReachGeomEnt(entSrc,entTarget,fDist,false);

		if(loc)
			return distance3(loc->vecHitLoc,ENTPOS(entTarget));
 	}
	return entTarget->motion->capsule.radius;
}



//Chest offset is hardcoded, but really it should be a percentage of the collision height.
#define CHEST_OFFSET 5.0
void getLocationReachableVecOffset( Entity * e, Vec3 vecRealLocVisOffset )
{
	Vec3 v = { 0, CHEST_OFFSET, 0 };
	bool notJustYaw = 0;
	const Vec3 * mat;

	//Kind of weird fast check: is this matrix more than just yaw?
	if( e )
	{
		F32 x;
		mat = ENTMAT( e );
		x = mat[0][1]+mat[1][0]+mat[1][2]+mat[2][1];
		if( !nearSameF32(mat[1][1], 1.0) || !nearSameF32(x, 0.0))
			notJustYaw = 1;
	}
		
	if( notJustYaw )
		mulVecMat3( v, mat, vecRealLocVisOffset );
	else	
		copyVec3( v, vecRealLocVisOffset );	//The old offset, still used 99% of the time to keep behavior the same
}


const F32 * EntPosWithReachableOffset( Entity * e, Vec3 result )
{
	Vec3 vecRealLocVisOffset;

	getLocationReachableVecOffset( e, vecRealLocVisOffset );
	addVec3( vecRealLocVisOffset, ENTPOS( e ), result );

	return result;
}

/**********************************************************************func*
 * character_LocationIsReachable
 *
 */

bool character_LocationIsReachable(const Vec3 vecSrc, const Vec3 vecTarget, float fRadius, DefTracker **res_tracker)
{
	DefTracker *tracker;
	bool bHitWorld;
	Vec3 vecStart;
	Vec3 vecEnd;

	copyVec3(vecSrc, vecStart);
	copyVec3(vecTarget, vecEnd);
 
	tracker = groupFindOnLine(vecStart, vecEnd, NULL, &bHitWorld, 1);

	if(bHitWorld)
	{
		if(distance3(vecEnd, vecTarget) < fRadius)
		{
			// OK it's close enough to hit anyway
			bHitWorld = false;
		}
	}

	if(res_tracker)
		*res_tracker = tracker;
	
	return !bHitWorld;
}

/**********************************************************************func*
 * character_EntLocationIsReachable
 *
 */
bool character_EntLocationIsReachable(Entity* entSrc, Entity *entTarget, const Vec3 vecPowerSrc, float fRadius, Vec3 vecPowerSrcReachableOffset)
{
	if(entSrc != entTarget)
	{
		Vec3 posTarget;
		Vec3 vecPowerSrcCopy;
		Vec3 posSource;
		Vec3 vecTgtOffset;

		// See if we precalculated closest point on the geometry
		if(!(character_EntUsesGeomColl(entTarget) && character_FindHitLoc(entSrc,entTarget,posTarget)))
			entGetPosAtEntTime(entTarget, entSrc, posTarget,0);

		getLocationReachableVecOffset( entTarget, vecTgtOffset );
		addVec3( posTarget, vecTgtOffset, posTarget );

		// If we're trying to hit from a geometry, shorten the cast by twice the radius
		if( character_EntUsesGeomColl(entSrc) )
		{
			Vec3 vecToTarget;
			float fDistScale;
			subVec3( vecPowerSrc, posTarget, vecToTarget);
			fDistScale = 1.0 - (entSrc->seq->worldGroupPtr->radius*2 / lengthVec3(vecToTarget));
			
			if(fDistScale<=0) return true;

			scaleVec3(vecToTarget,fDistScale,vecToTarget);
			addVec3(posTarget,vecToTarget,vecPowerSrcCopy);
		}
		else
		{
			copyVec3( vecPowerSrc, vecPowerSrcCopy );
		}

		addVec3( vecPowerSrcCopy, vecPowerSrcReachableOffset, posSource );

		return character_LocationIsReachable( posSource, posTarget, fRadius, NULL);
	}
	else
	{
		Vec3 v1, v2;
		addVec3( vecPowerSrc, vecPowerSrcReachableOffset, v1 );

		return character_LocationIsReachable(v1, EntPosWithReachableOffset(entTarget, v2), fRadius, NULL);
	}
}

/**********************************************************************func*
 * character_CharacterIsReachable
 *
 */
bool character_CharacterIsReachable(Entity *entSrc, Entity *entTarget)
{
	if(entSrc != entTarget)
	{
		Vec3 posTarget;
		Vec3 vecTgtOffset;
		Vec3 v1;
		float trad = entTarget->motion->capsule.radius;
		bool res = FALSE;
		DefTracker *tracker = NULL;
		if(!(character_EntUsesGeomColl(entTarget) && character_FindHitLoc(entSrc,entTarget,posTarget)))
			entGetPosAtEntTime(entTarget, entSrc, posTarget,0);

		getLocationReachableVecOffset( entTarget, vecTgtOffset );
		addVec3( posTarget, vecTgtOffset, posTarget );

		res = character_LocationIsReachable(EntPosWithReachableOffset(entSrc, v1), posTarget, trad, &tracker);
		if(!res && tracker && character_EntUsesGeomColl(entTarget) && entTarget->seq && entTarget->seq->worldGroupPtr == tracker->def)
			res = TRUE;
		return res;
	}
	else
	{
		Vec3 v1, v2;
		return character_LocationIsReachable(EntPosWithReachableOffset(entSrc, v1), EntPosWithReachableOffset(entTarget, v2), entTarget->motion->capsule.radius, NULL);
	}
}

/**********************************************************************func*
 * ValidateSphereTarget
 *
 */
static INLINEDBG  bool ValidateSphereTarget(Character *pSrc, Entity *pent, const Vec3 vecTarget, Power *ppow, float *pfDistSqr, Vec3 vecTargetReachableOffset)
{
	return (pent->pchar!=NULL
		&& (ENTTYPE(pent)==ENTTYPE_PLAYER || ENTTYPE(pent)==ENTTYPE_NPC || ENTTYPE(pent)==ENTTYPE_CRITTER)
		&& (pSrc->attrCur.fOnlyAffectsSelf<=0.0f
			|| pSrc==pent->pchar)
		&& character_LocationInRadius(pSrc->entParent, pent->pchar, vecTarget, ppow, pfDistSqr)
		&& (pent->alwaysGetHit
			|| character_PowerAffectsTarget(pSrc, pent->pchar, ppow))
		&& (ppow->ppowBase->eTargetVisibility==kTargetVisibility_None
			|| character_EntLocationIsReachable(pSrc->entParent, pent, vecTarget, 3.0f, vecTargetReachableOffset)));
}


/**********************************************************************func*
 * ValidateChainTarget
 *
 */
static INLINEDBG  bool ValidateChainTarget(Character *pSrc, Character *pPrevTarget, Entity *pent, const Vec3 vecTarget, Power *ppow, float *pfDistSqr, Vec3 vecTargetReachableOffset)
{
	return (pent->pchar!=NULL
		&& pPrevTarget!=NULL
		&& (ENTTYPE(pent)==ENTTYPE_PLAYER || ENTTYPE(pent)==ENTTYPE_NPC || ENTTYPE(pent)==ENTTYPE_CRITTER)
		&& (pSrc->attrCur.fOnlyAffectsSelf<=0.0f
			|| pSrc==pent->pchar)
		&& character_LocationInRadius(pPrevTarget->entParent, pent->pchar, vecTarget, ppow, pfDistSqr)
		&& (pent->alwaysGetHit
			|| character_PowerAffectsTarget(pSrc, pent->pchar, ppow))
		&& (ppow->ppowBase->eTargetVisibility==kTargetVisibility_None
			|| character_EntLocationIsReachable(pPrevTarget->entParent, pent, vecTarget, 3.0f, vecTargetReachableOffset)));
}


/**********************************************************************func*
 * ValidateBoxTarget
 * Basically same as ValidateSphere, except it checks that pent is inside a box relative to vecTarget
 */
static INLINEDBG  bool ValidateBoxTarget(Character *pSrc, Entity *pent, Vec3 vecTarget, Power *ppow, Vec3 vecTargetReachableOffset)
{
	if (pent->pchar!=NULL
		&& (ENTTYPE(pent)==ENTTYPE_PLAYER || ENTTYPE(pent)==ENTTYPE_NPC || ENTTYPE(pent)==ENTTYPE_CRITTER)
		&& (pSrc->attrCur.fOnlyAffectsSelf<=0.0f
			|| pSrc==pent->pchar)
		//&& character_LocationInRadius(pSrc->entParent, pent->pchar, vecTarget, ppow, pfDistSqr)
		&& (pent->alwaysGetHit
			|| character_PowerAffectsTarget(pSrc, pent->pchar, ppow))
		&& (ppow->ppowBase->eTargetVisibility==kTargetVisibility_None
			|| character_EntLocationIsReachable(pSrc->entParent, pent, vecTarget, 3.0f, vecTargetReachableOffset)))
	{
		Vec3 vecLowCorner, vecTargetOffset;
		addVec3(vecTarget,ppow->ppowBase->vecBoxOffset,vecLowCorner);
		subVec3(ENTPOS(pent),vecLowCorner,vecTargetOffset);
		if (vecTargetOffset[0] >= 0.0 && vecTargetOffset[0] <= ppow->ppowBase->vecBoxSize[0]
			&& vecTargetOffset[1] >= 0.0 && vecTargetOffset[1] <= ppow->ppowBase->vecBoxSize[1]
			&& vecTargetOffset[2] >= 0.0 && vecTargetOffset[2] <= ppow->ppowBase->vecBoxSize[2])
		{
			return true;
		}
	}
	return false;
}


/**********************************************************************func*
 * ValidateVolumeTarget
 * (only diff between this and Validate sphere is the materialVolume check instead of the radius check)
 */
static INLINEDBG  bool ValidateVolumeTarget(Character *pSrc, Entity *pent, Vec3 vecTarget, Power *ppow )
{
	Vec3 v;
//	int i;


	return (pent->pchar!=NULL
		&& (ENTTYPE(pent)==ENTTYPE_PLAYER || ENTTYPE(pent)==ENTTYPE_NPC || ENTTYPE(pent)==ENTTYPE_CRITTER)
		&& (pSrc->attrCur.fOnlyAffectsSelf<=0.0f || pSrc==pent->pchar)
		&& (pSrc->entParent->volumeList.materialVolume->volumeTracker && (pSrc->entParent->volumeList.materialVolume->volumeTracker == pent->volumeList.materialVolume->volumeTracker))
		&& (pent->alwaysGetHit || character_PowerAffectsTarget(pSrc, pent->pchar, ppow))
		&& (ppow->ppowBase->eTargetVisibility==kTargetVisibility_None || character_LocationIsReachable( EntPosWithReachableOffset( pent, v ), vecTarget, 3.0f, NULL )));

// Ugh.  What a mess.  And almost impossible to determine why it rejects a target unless you dig in and look at the assembly.
// So we break it up into something more readable, and also do some trickery at the bottom to handle the fact that the art
// department are now creating overlapping MaterialVolumes.
/*
	if (pent->pchar == NULL)
	{
		return 0;
	}
	if (ENTTYPE(pent) != ENTTYPE_PLAYER && ENTTYPE(pent) != ENTTYPE_NPC && ENTTYPE(pent) != ENTTYPE_CRITTER)
	{
		return 0;
	}
	if (pSrc->attrCur.fOnlyAffectsSelf > 0.0f && pSrc != pent->pchar)
	{
		return 0;
	}
	if (!pent->alwaysGetHit && !character_PowerAffectsTarget(pSrc, pent->pchar, ppow))
	{
		return 0;
	}
	if (ppow->ppowBase->eTargetVisibility != kTargetVisibility_None && 
			!character_LocationIsReachable(EntPosWithReachableOffset(pent, v), vecTarget, 3.0f, NULL))
	{
		return 0;
	}
	if (pSrc->entParent->volumeList.materialVolume->volumeTracker == NULL)
	{
		return 1;
	}
	if (pSrc->entParent->volumeList.materialVolume->volumeTracker == pent->volumeList.materialVolume->volumeTracker)
	{
		return 1;
	}
	// Everything down to this point duplicates the original test.  We now scan all volumes that the target entity is in
	// to see if any of them match the materialvolume that the source is in.
	for (i = 0; i < pent->volumeList.volumeCount; i++)
	{
		if (pSrc->entParent->volumeList.materialVolume->volumeTracker == pent->volumeList.volumes[i].volumeTracker)
		{
			return 1;
		}
	}
	return 0;
	*/
}

/**********************************************************************func*
* ValidateTouchingTarget
* (only diff between this and Validate sphere is the collision check instead of a sphere check )
*/
static INLINEDBG  bool ValidateTouchingTarget(Character *pSrc, Entity *pent, Vec3 vecTarget, Power *ppow )
{
	Vec3 v;

	return (pent->pchar!=NULL
		&& (ENTTYPE(pent)==ENTTYPE_PLAYER || ENTTYPE(pent)==ENTTYPE_NPC || ENTTYPE(pent)==ENTTYPE_CRITTER)
		&& (pSrc->attrCur.fOnlyAffectsSelf<=0.0f
			|| pSrc==pent->pchar)
		&& character_TargetIsInsideSourceBox(pSrc->entParent, pent)
		&& (pent->alwaysGetHit
			|| character_PowerAffectsTarget(pSrc, pent->pchar, ppow))
		&& (ppow->ppowBase->eTargetVisibility==kTargetVisibility_None
			|| character_LocationIsReachable( EntPosWithReachableOffset( pent, v ), vecTarget, 3.0f, NULL)));
}

/**********************************************************************func*
* NamedVolumeTarget
* (only diff between this and Validate sphere is the materialVolume check instead of the radius check)
*/  //NOT YET IMPLEMENTED
static INLINEDBG  bool ValidateNamedVolumeTarget(Character *pSrc, Entity *pent, Vec3 vecTarget, Power *ppow )
{
	Vec3 v;

	return (pent->pchar!=NULL
		&& (ENTTYPE(pent)==ENTTYPE_PLAYER || ENTTYPE(pent)==ENTTYPE_NPC || ENTTYPE(pent)==ENTTYPE_CRITTER)
		&& (pSrc->attrCur.fOnlyAffectsSelf<=0.0f
			|| pSrc==pent->pchar)
	//	&& TO DO add a nice little function that validates named targets.
		&& (pent->alwaysGetHit
			|| character_PowerAffectsTarget(pSrc, pent->pchar, ppow))
		&& (ppow->ppowBase->eTargetVisibility==kTargetVisibility_None
			|| character_LocationIsReachable( EntPosWithReachableOffset( pent, v ), vecTarget, 3.0f, NULL)));
}


/**********************************************************************func*
* ValidateMap target
* (only diff between this and Validate sphere is there is no sphere check, just hit everybody on the map)
*/
static INLINEDBG  bool ValidateMapTarget(Character *pSrc, Entity *pent, Vec3 vecTarget, Power *ppow )
{
	Vec3 v;

	return (pent->pchar!=NULL
		&& (ENTTYPE(pent)==ENTTYPE_PLAYER || ENTTYPE(pent)==ENTTYPE_NPC || ENTTYPE(pent)==ENTTYPE_CRITTER)
		&& (pSrc->attrCur.fOnlyAffectsSelf<=0.0f
			|| pSrc==pent->pchar)
		&& (pent->alwaysGetHit
			|| character_PowerAffectsTarget(pSrc, pent->pchar, ppow))
		&& (ppow->ppowBase->eTargetVisibility==kTargetVisibility_None
			|| character_LocationIsReachable( EntPosWithReachableOffset( pent, v ), vecTarget, 3.0f, NULL)));
}

/**********************************************************************func*
* ValidateRoomTarget
* (only diff between this and Validate sphere is the RoomVolume check instead of the radius check)
*/
static INLINEDBG  bool ValidateRoomTarget(Character *pSrc, Entity *pent, Vec3 vecTarget, Power *ppow )
{
	Vec3 v;

	return (pent->pchar!=NULL
		&& (ENTTYPE(pent)==ENTTYPE_PLAYER || ENTTYPE(pent)==ENTTYPE_NPC || ENTTYPE(pent)==ENTTYPE_CRITTER)
		&& (pSrc->attrCur.fOnlyAffectsSelf<=0.0f
			|| pSrc==pent->pchar)
		&& (pSrc->entParent->roomImIn && (pSrc->entParent->roomImIn == pent->roomImIn))
		&& (pent->alwaysGetHit
			|| character_PowerAffectsTarget(pSrc, pent->pchar, ppow))
		&& (ppow->ppowBase->eTargetVisibility==kTargetVisibility_None
			|| character_LocationIsReachable( EntPosWithReachableOffset( pent, v ), vecTarget, 3.0f, NULL)));
}


/**********************************************************************func*
 * ValidateConeTarget
 *
 */
bool ValidateConeTarget(Character *pSrc, Entity *pent, const Vec3 vecTarget, Power *ppow, float *pfDistSqr, Vec3 vecTargetReachableOffset)
{
	Vec3 vecCone;
	Vec3 vecVictim;
	float fcosCone = cos(ppow->ppowBase->fArc*ppow->pattrStrength->fArc/2.0f);
	F32 fRange = ppow->ppowBase->fRadius*ppow->pattrStrength->fRange + pSrc->entParent->motion->capsule.radius + pent->motion->capsule.radius;
	bool bInRange;
	bool bIsEntTime = false;
	Vec3 posTarget, posSrc;

	*pfDistSqr = distance3Squared(ENTPOS(pent), ENTPOS(pSrc->entParent));
	bInRange = *pfDistSqr <= SQR(fRange); 

	subVec3(vecTarget, ENTPOS(pSrc->entParent), vecCone);
	normalVec3(vecCone);

	if(!bInRange)
	{
		entGetPosAtEntTime(pSrc->entParent, pSrc->entParent, posSrc,0);
		entGetPosAtEntTime(pent, pSrc->entParent, posTarget,0);
		*pfDistSqr = distance3Squared(posSrc, posTarget);
		bInRange = *pfDistSqr <= SQR(fRange);
		bIsEntTime = true;
		subVec3(vecTarget, posSrc, vecCone);
		normalVec3(vecCone);
	}

	if( bInRange && pent->pchar!=NULL
		&& (ENTTYPE(pent)==ENTTYPE_PLAYER || ENTTYPE(pent)==ENTTYPE_NPC || ENTTYPE(pent)==ENTTYPE_CRITTER)
		&& (pSrc->attrCur.fOnlyAffectsSelf<=0.0f || pSrc==pent->pchar)
		&& (pent->alwaysGetHit || character_PowerAffectsTarget(pSrc, pent->pchar, ppow))
		&& (ppow->ppowBase->eTargetVisibility==kTargetVisibility_None || character_CharacterIsReachable(pSrc->entParent, pent)))
	{
		float fcosVictim;
		// OK, at this point we know that it's in the sphere
		//   and that it's generally hittable, etc.

		// Figure out if it's in the cone.
		if (pSrc->entParent == pent)
			return true;

		if (bIsEntTime)
		{
			subVec3(posTarget, posSrc, vecVictim);
		}
		else
		{
			// For geometry, try to copy the already calculated closest hit point into posTarget
			if(!(character_EntUsesGeomColl(pent) && character_FindHitLoc(pSrc->entParent,pent,posTarget)))
				entGetPosAtEntTime(pent, pSrc->entParent, posTarget,0);

			subVec3(posTarget, ENTPOS(pSrc->entParent), vecVictim);
		}
		fcosVictim = dotVec3(vecVictim, vecCone)/lengthVec3(vecVictim);
		if(fcosVictim > fcosCone)
			return true;

		// HAX for geometry.. throw a cylinder if it missed
		if( character_EntUsesGeomColl(pent) )
		{
			Vec3 srcloc;
			Vec3 targloc;
			// Get the old hitloc.  Should be a valid one already thanks to character_InRange()
			HitLocation *loc = character_ReachGeomEnt(pSrc->entParent,pent,fRange,false);
			float ftanCone = tan(ppow->ppowBase->fArc*ppow->pattrStrength->fArc/2.0f);
			Vec3 hitvec;
			bool bHit;

			copyVec3(ENTPOS(pSrc->entParent),srcloc);
			srcloc[1]+=5.0;
			scaleVec3(vecCone,fRange,vecCone);
			addVec3(srcloc,vecCone,targloc);


			bHit = character_GeomInCylinder(srcloc,targloc,ENTPOS(pent),pent->coll_tracker,ftanCone*fRange,hitvec);
			if(loc)
			{
				// Update the old hitloc
				loc->bReached = bHit;
				if(bHit)
					copyVec3(hitvec,loc->vecHitLoc);
			}
			return bHit;
		}

	}

	return false;
}

/**********************************************************************func*
 * ValidateCharacterTarget
 *
 */
static INLINEDBG  bool ValidateCharacterTarget(Character *pSrc, Entity *pent, Vec3 vecTarget, Power *ppow)
{
	return ((pSrc->attrCur.fOnlyAffectsSelf<=0.0f || pSrc==pent->pchar)
		&& character_PowerAffectsTarget(pSrc, pent->pchar, ppow)
		&& (ppow->ppowBase->eTargetType == kTargetType_Caster
			|| ppow->ppowBase->eTargetVisibility==kTargetVisibility_None
			|| character_CharacterIsReachable(pSrc->entParent, pent)));
}


/**********************************************************************func*
 * DistSort
 *
 */
int DistSort(const void *a, const void *b)
{
	const CharacterRef *prefA = (const CharacterRef *)a;
	const CharacterRef *prefB = (const CharacterRef *)b;

	return (int)((prefA->fDist - prefB->fDist)*10);
}

/**********************************************************************func*
 * GetTargetEntities
 *
 */
static bool GetTargetEntities(Character *pSrc, Character *pPrevTarget, Character *pTarget,
	Vec3 vecTarget, Power *ppow, CharacterList *plist, Vec3 vecTargetReachableOffset, int iChainJump )
{
	EffectArea eEffectArea = ppow->ppowBase->eEffectArea;
	int apEntities[MAX_ENTITIES_PRIVATE];
	int iCntFound;

	assert(pSrc!=NULL);
	assert(vecTarget!=NULL);
	assert(ppow!=NULL);
	assert(plist!=NULL);

	if (eEffectArea == kEffectArea_Chain && iChainJump == 1)
		eEffectArea = kEffectArea_Character;		// first chain jump is directly to target

	//
	// Make a list of the entities which might be affected by this power.
	//

	switch(eEffectArea)
	{
		case kEffectArea_Sphere:
		case kEffectArea_Cone:
		case kEffectArea_Chain:		// chain hops are treated like spheres, ApplyPower takes care of the recursion
			if( power_GetRadius(ppow) <= MIN_PLAYER_CRITTER_RADIUS )
			{
				float fDist;
				PERFINFO_AUTO_START("Get Targets Via Grid", 1);
				// We can use the grid to help use with this
				iCntFound = mgGetNodesInRange(0, vecTarget, (void **)apEntities, MAX_ENTITIES_PRIVATE);

				if(eEffectArea==kEffectArea_Sphere ||
				   eEffectArea==kEffectArea_Chain)
				{
					// ValidateSphere does time travel work, so nothing special needs to be done
					for(iCntFound-=1; iCntFound>=0; iCntFound--)
					{
						Entity *pent = entities[apEntities[iCntFound]];
						if(eEffectArea==kEffectArea_Sphere &&
							ValidateSphereTarget(pSrc, pent, vecTarget, ppow, &fDist, vecTargetReachableOffset))
						{
							charlist_Add(plist, pent->pchar, fDist);
						} else if(eEffectArea==kEffectArea_Chain &&
							ValidateChainTarget(pSrc, pPrevTarget, pent, vecTarget, ppow, &fDist, vecTargetReachableOffset))
						{
							charlist_Add(plist, pent->pchar, fDist);
						}

					}
				}
				else
				{
					// ValidateCone does time travel work, so nothing special needs to be done
					for(iCntFound-=1; iCntFound>=0; iCntFound--)
					{
						Entity *pent = entities[apEntities[iCntFound]];
						if(ValidateConeTarget(pSrc, pent, vecTarget, ppow, &fDist, vecTargetReachableOffset))
						{
							charlist_Add(plist, pent->pchar, fDist);
						}
					}
				}

				PERFINFO_AUTO_STOP();
			}
			else
			{
				float fDist;
				// It's more efficient to do it all ourselves
				PERFINFO_AUTO_START("Get Targets Via Ents", 1);
				iCntFound = entities_max;
				for(iCntFound-=1; iCntFound>0; iCntFound--)
				{
					if(entity_state[iCntFound] & ENTITY_IN_USE)
					{
						Entity *pent = entities[iCntFound];
						if(eEffectArea==kEffectArea_Sphere ||
						   eEffectArea==kEffectArea_Chain)
						{
							if(ValidateSphereTarget(pSrc, pent, vecTarget, ppow, &fDist, vecTargetReachableOffset))
							{
								charlist_Add(plist, pent->pchar, fDist);
							}
						}
						else
						{
							if(ValidateConeTarget(pSrc, pent, vecTarget, ppow, &fDist, vecTargetReachableOffset))
							{
								// it's in
								charlist_Add(plist, pent->pchar, fDist);
							}
						}
					}
				}

				PERFINFO_AUTO_STOP();
			}

			if(eEffectArea == kEffectArea_Chain ||
				ppow->ppowBase->iMaxTargetsHit>0
				&& plist->pBase && plist->iCount > ppow->ppowBase->iMaxTargetsHit)
			{
				// Sort by distance.
				qsort((void *)plist->pBase, plist->iCount, sizeof(CharacterRef), DistSort);
			}

			break;

		case kEffectArea_Character:
			PERFINFO_AUTO_START("Get Character Target", 1);
			if(ValidateCharacterTarget(pSrc, pTarget->entParent, vecTarget, ppow))
			{
				charlist_Add(plist, pTarget, 0.0f);
			}
			PERFINFO_AUTO_STOP();
			break;

		case kEffectArea_Location:
			PERFINFO_AUTO_START("Get Location Target", 1);
			charlist_Add(plist, pSrc, 0.0f);
			PERFINFO_AUTO_STOP();
			break;

		case kEffectArea_Volume:
			PERFINFO_AUTO_START("Get Targets In My Volume", 1);
			iCntFound = entities_max;
			for(iCntFound-=1; iCntFound>0; iCntFound--)
			{
				if(entity_state[iCntFound] & ENTITY_IN_USE)
				{
					Entity *pent = entities[iCntFound];
					Vec3 v;
					addVec3( vecTargetReachableOffset, vecTarget, v );
					if(ValidateVolumeTarget(pSrc, pent, v, ppow))
					{
						charlist_Add(plist, pent->pchar, 0.0f);
					}
				}
			}
			PERFINFO_AUTO_STOP();
			break;

		case kEffectArea_Touch:
			PERFINFO_AUTO_START("Get Targets In My Volume", 1);
			iCntFound = entities_max;
			for(iCntFound-=1; iCntFound>0; iCntFound--)
			{
				if(entity_state[iCntFound] & ENTITY_IN_USE)
				{
					Entity *pent = entities[iCntFound];
					Vec3 v;

					addVec3( vecTargetReachableOffset, vecTarget, v );

					if(ValidateTouchingTarget(pSrc, pent, v, ppow ))
					{
						charlist_Add(plist, pent->pchar, 0.0f);
					}
				}
			}
			PERFINFO_AUTO_STOP();
			break;

		case kEffectArea_NamedVolume:
			PERFINFO_AUTO_START("Get Targets In All Volumes with my name", 1); //Not implemented
			iCntFound = entities_max;
			for(iCntFound-=1; iCntFound>0; iCntFound--)
			{
				if(entity_state[iCntFound] & ENTITY_IN_USE)
				{
					Entity *pent = entities[iCntFound];
					Vec3 v;

					addVec3( vecTargetReachableOffset, vecTarget, v );

					if(ValidateNamedVolumeTarget(pSrc, pent, v, ppow))
					{
						charlist_Add(plist, pent->pchar, 0.0f);
					}
				}
			}
			PERFINFO_AUTO_STOP();
			break;

		case kEffectArea_Map:
			PERFINFO_AUTO_START("Get All Targets On the Map", 1);
			iCntFound = entities_max;
			for(iCntFound-=1; iCntFound>0; iCntFound--)
			{
				if(entity_state[iCntFound] & ENTITY_IN_USE)
				{
					Entity *pent = entities[iCntFound];
					Vec3 v;

					addVec3( vecTargetReachableOffset, vecTarget, v );

					if(ValidateMapTarget(pSrc, pent, v, ppow))
					{
						charlist_Add(plist, pent->pchar, 0.0f);
					}
				}
			}
			PERFINFO_AUTO_STOP();
			break;

		case kEffectArea_Room:
			PERFINFO_AUTO_START("Get Targets In My Volume", 1);
			iCntFound = entities_max;
			for(iCntFound-=1; iCntFound>0; iCntFound--)
			{
				if(entity_state[iCntFound] & ENTITY_IN_USE)
				{
					Entity *pent = entities[iCntFound];
					Vec3 v;

					addVec3( vecTargetReachableOffset, vecTarget, v );

					if(ValidateRoomTarget(pSrc, pent, v, ppow))
					{
						charlist_Add(plist, pent->pchar, 0.0f);
					}
				}
			}
			PERFINFO_AUTO_STOP();
			break;
		case kEffectArea_Box:
			PERFINFO_AUTO_START("Get Targets In Box", 1);
			iCntFound = entities_max;
			for(iCntFound-=1; iCntFound>0; iCntFound--)
			{
				if(entity_state[iCntFound] & ENTITY_IN_USE)
				{
					Entity *pent = entities[iCntFound];
					if(ValidateBoxTarget(pSrc, pent, vecTarget, ppow, vecTargetReachableOffset))
					{
						charlist_Add(plist, pent->pchar, 0.0f);
					}
				}
			}
			PERFINFO_AUTO_STOP();
			break;
	}

	return true;
}


/**********************************************************************func*
 * KillPower
 *
 */
static void KillPower(Character *pSrc, Power *ppow)
{
	if(ppow == pSrc->ppowCurrent)
	{
		character_KillCurrentPower(pSrc);
	}

	// Toggle powers which are in queue are taken care of in ApplyPower's
	// caller.

	// if(ppow->ppowBase->eType==kPowerType_Toggle)
	// {
	// 	character_ShutOffAndRemoveTogglePower(pSrc, ppow);
	// }
}

unsigned int g_uiInvokeID=1; // Shouldn't really matter if it overflows. 0 is an invalid ID.

U32 nextInvokeId()
{
	g_uiInvokeID++;
	if(!g_uiInvokeID) // skip zero
		g_uiInvokeID++;
	return g_uiInvokeID;
}

int character_combatGetLocalizedMessage(char *outputMessage, int bufferLength, Entity* e, int translateSourceName, int translateDestinationName, const char* messageID, ...)
{
	static int initted = 0;
	static Array* CombatMessage[2][2];
	static int locale = -1;
	int messageLength;
	Entity * sendToMe = erGetEnt(e->erOwner);

	int translateSource = translateSourceName ? 1 : 0;
	int translateDestination = translateDestinationName ? 1 : 0;

	// If no messages are given, do nothing.
	if(!messageID || messageID[0] == '\0')
		return 0;

	if (locale == -1)	
	{
		locale = getCurrentLocale();
	}

	if(!initted)
	{ 
		CombatMessage[0][0] = msCompileMessageType("{PlayerDest, %s}, {PowerName, %r}, {Damage, %g}, {PlayerSource, %s}");
		CombatMessage[0][1] = msCompileMessageType("{PlayerDest, %s}, {PowerName, %r}, {Damage, %g}, {PlayerSource, %r}");
		CombatMessage[1][0] = msCompileMessageType("{PlayerDest, %r}, {PowerName, %r}, {Damage, %g}, {PlayerSource, %s}");
		CombatMessage[1][1] = msCompileMessageType("{PlayerDest, %r}, {PowerName, %r}, {Damage, %g}, {PlayerSource, %r}");
		initted = 1;
	}

	VA_START(va, messageID);
	messageLength = msxPrintfVA(svrMenuMessages, outputMessage, bufferLength, locale, messageID, CombatMessage[translateDestination][translateSource], NULL, translateFlag(sendToMe), va);
	VA_END();

	return messageLength;
}

// Stuff that needs to be tracked separately for each fork
typedef struct ChainFork {
	Character *pPrevTarget;
	float fChainEff;
	float fChainDelay;		// accumulated projectile delay
} ChainFork;

static ChainFork *chainfork_Create(Character *pOrigin, float fEff, float fDelay) {
	ChainFork *n = calloc(1, sizeof(ChainFork));
	n->pPrevTarget = pOrigin;
	n->fChainEff = fEff;
	n->fChainDelay = fDelay;
	return n;
}

/**********************************************************************func*
 * character_ApplyPower
 *
 * This is actually called when the power is begun (as opposed to when the
 * power is complete and the effect should take place).
 *
 */ 
bool character_ApplyPower(Character *pSrc, Power *ppow, EntityRef erTarget, const Vec3 vecTarget, unsigned int invokeID, int ignoreLocationRestrictions )
{
	static CharacterList listTargets;
	
	EffectApplyContext ctx;
	CharacterIter iter;
	CharacterRef *pref;
	Character *pRealTarget = NULL;
	Power **globalBoostActivatedThisFrame = NULL;
	int *piAttackBits;
	char *pchAttackFX;
	float fPrimaryAttackDelay = 0;
	float fPrimaryHitDelay = 0;
	float fPrimaryDistDelay = 0;
	float fTimeToBlock;
	int iTotalHit = 0;
	int iTotalMiss = 0;
	int iChainJump = 0, iChainFork = 0;
	bool bIsChain = (ppow->ppowBase->eEffectArea == kEffectArea_Chain);
	bool bAnyForkHit;
	int iAttackFxId;
	int iFXFlags = PLAYFX_NORMAL;
	int attackerHitMessagesSent = 0;
	const PowerFX* ppowFX = NULL;
	AttribMod **delayEvalAttribMods = NULL;
	Character ***eaProcsChecked = NULL;
	Character **eaChainHit = NULL;
	ChainFork **eaChainForks = NULL;

	InitEAContext(&ctx);
	ctx.delayEvalAttribMods = &delayEvalAttribMods;
	ctx.mf.pPow = ppow;

	if(!invokeID)
		invokeID = g_uiInvokeID;

	PERFINFO_AUTO_START("ApplyPower", 1);
	assert(pSrc!=NULL);
	assert(ppow!=NULL);

	PERFINFO_AUTO_START("validation", 1);
	if(ppow->ppowBase->eTargetType==kTargetType_Caster)
	{
		pRealTarget = pSrc;
	}
	else
	{
		Entity *pentTarget;

		if (ppow->ppowBase->eTargetType == kTargetType_Location
			|| ppow->ppowBase->eTargetType == kTargetType_Teleport)
		{
			pRealTarget = pSrc;

			//	Only need to do this check once when it activates (which is always for click powers but only on activate for toggle powers)
			//	This comes up more when players zone w/ summon/location toggles where the location is no longer valid
			if (ppow->ppowBase->eType != kPowerType_Toggle || !ppow->bActive)
			{
				if(ppow->ppowBase->bTargetNearGround && !ignoreLocationRestrictions)
				{
					if(HeightAtLoc(vecTarget, 0.01f, 2.0f) > 1.0f)
					{
						KillPower(pSrc, ppow);
						sendInfoBox(pSrc->entParent, INFO_COMBAT_ERROR, "PowerTargetMustBeOnGround");

						PERFINFO_AUTO_STOP();
						PERFINFO_AUTO_STOP();
						return false;
					}
				}
			}
		}
		else
		{
			// Make sure the target is still valid
			if((pentTarget=erGetEnt(erTarget))==NULL)
			{
				KillPower(pSrc, ppow);

				// AI_LOG(AI_LOG_POWERS, (pSrc->entParent, "ApplyPow:  Target is dead.\n"));
				LOG_ENT( pSrc->entParent, LOG_POWERS, LOG_LEVEL_IMPORTANT, 0, "Power:Cancelled %s Unable to find target entity.", dbg_BasePowerStr(ppow->ppowBase));

				PERFINFO_AUTO_STOP();
				PERFINFO_AUTO_STOP();
				return false;
			}

			pRealTarget = pentTarget->pchar;
			if(ppow->ppowBase->bTargetNearGround && !ignoreLocationRestrictions)
			{
				if(entHeight(pRealTarget->entParent, 2.0f)>1.0f)
				{
					KillPower(pSrc, ppow);
					sendInfoBox(pSrc->entParent, INFO_COMBAT_ERROR, "PowerTargetMustBeOnGround");

					PERFINFO_AUTO_STOP();
					PERFINFO_AUTO_STOP();
					return false;
				}
			}

			// Feh. These if expressions are getting out of control.
			if(!entAlive(pentTarget)
				&& (!team_IsMember(pSrc->entParent, pentTarget->db_id) || (ppow->ppowBase->eTargetType!=kTargetType_DeadOrAliveTeammate && ppow->ppowBase->eTargetType!=kTargetType_DeadTeammate))
				&& ((!league_IsMember(pSrc->entParent, pentTarget->db_id) && !team_IsMember(pSrc->entParent, pentTarget->db_id)) || (ppow->ppowBase->eTargetType!=kTargetType_DeadOrAliveLeaguemate && ppow->ppowBase->eTargetType!=kTargetType_DeadLeaguemate))
				&& (ppow->ppowBase->eTargetType!=kTargetType_DeadFoe           || character_TargetIsFriend(pSrc,pRealTarget))
				&& (ppow->ppowBase->eTargetType!=kTargetType_DeadPlayerFoe     || character_TargetIsFriend(pSrc,pRealTarget) || ENTTYPE(pentTarget)!=ENTTYPE_PLAYER)
				&& (ppow->ppowBase->eTargetType!=kTargetType_DeadOrAliveFoe    || character_TargetIsFriend(pSrc,pRealTarget))
				&& (ppow->ppowBase->eTargetType!=kTargetType_DeadFriend        || character_TargetIsFoe(pSrc,pRealTarget))
				&& (ppow->ppowBase->eTargetType!=kTargetType_DeadPlayerFriend  || character_TargetIsFoe(pSrc,pRealTarget) || ENTTYPE(pentTarget)!=ENTTYPE_PLAYER)
				&& (ppow->ppowBase->eTargetType!=kTargetType_DeadOrAliveFriend || character_TargetIsFoe(pSrc,pRealTarget))
				&& (ppow->ppowBase->eTargetType!=kTargetType_DeadPlayer        || ENTTYPE(pentTarget)!=ENTTYPE_PLAYER)
				&& (ppow->ppowBase->eTargetType!=kTargetType_DeadVillain       || ENTTYPE(pentTarget)!=ENTTYPE_CRITTER)
				&& (ppow->ppowBase->eTargetType!=kTargetType_DeadOrAliveMyPet  || erGetEnt(pentTarget->erOwner)!=pSrc->entParent)
				&& (ppow->ppowBase->eTargetType!=kTargetType_DeadMyPet         || erGetEnt(pentTarget->erOwner)!=pSrc->entParent)
				&& (ppow->ppowBase->eTargetType!=kTargetType_DeadMyCreation	   || erGetEnt(pentTarget->erCreator)!=pSrc->entParent)
				&& (ppow->ppowBase->eTargetType!=kTargetType_DeadOrAliveMyCreation  || erGetEnt(pentTarget->erCreator)!=pSrc->entParent))
			{
				sendInfoBox(pSrc->entParent, INFO_COMBAT_ERROR, "PowerTargetIsDead");

				KillPower(pSrc, ppow);
				// AI_LOG(AI_LOG_POWERS, (pSrc->entParent, "ApplyPow:  Target is dead.\n"));
				LOG_ENT( pSrc->entParent, LOG_POWERS, LOG_LEVEL_IMPORTANT, 0, "Power:Cancelled %s Unable to find target entity.", dbg_BasePowerStr(ppow->ppowBase));

				PERFINFO_AUTO_STOP();
				PERFINFO_AUTO_STOP();
				return false;
			}
		}
	}
	PERFINFO_AUTO_STOP();

	PERFINFO_AUTO_START("anim", 1);
	if(ppow->ppowBase->eType==kPowerType_Click && !ppow->bOnLastTick && !ppow->ppowParentInstance)
	{
		sendInfoBox(pSrc->entParent, INFO_COMBAT_SPAM, "PowerActivated", ppow->ppowBase->pchDisplayName);
	}
	
	// Count down charges if the power has them.
	if (ppow->ppowBase->bHasUseLimit)
	{
		ppow->iNumCharges--;
	}

	if(ppow->ppowBase->eEffectArea!=kEffectArea_Character
		&& ppow->ppowBase->eEffectArea!=kEffectArea_Location
		&& ppow->ppowBase->eEffectArea!=kEffectArea_Touch)
	{
		iFXFlags = PLAYFX_FROM_AOE;
	}

	ppowFX = power_GetFX(ppow);
	assert(ppowFX);

	if (ppowFX->bImportant)
		iFXFlags |= PLAYFX_IMPORTANT;

	// Handle the special entry attacks
	if(ppow->bFirstAttackInStance)
	{
		piAttackBits = ppowFX->piInitialAttackBits;
		pchAttackFX = ppowFX->pchInitialAttackFX;
		fPrimaryAttackDelay = (F32)ppowFX->iInitialAttackFXFrameDelay / 30.0;
		fPrimaryHitDelay = ppowFX->fInitialTimeToHit;
		fTimeToBlock = ppowFX->fInitialTimeToBlock;
		ppow->bFirstAttackInStance = false;
	}
	else
	{
		piAttackBits = ppowFX->piAttackBits;
		pchAttackFX = ppowFX->pchAttackFX;
		fPrimaryHitDelay = ppowFX->fTimeToHit;
		fTimeToBlock = ppowFX->fTimeToBlock;
		fPrimaryAttackDelay = 0;
	}

	// The Attack anim always starts right after the interrupt time.
	character_SetAnimBits(pSrc, piAttackBits, 0.0f, PLAYFX_NOT_ATTACHED);
	if(!ppowFX->bDelayedHit
		|| !(ppow->ppowBase->eEffectArea==kEffectArea_Character || bIsChain))
	{
		if (ppow->ppowBase->eTargetType == kTargetType_Location
			|| ppow->ppowBase->eTargetType == kTargetType_Teleport
			|| ppow->ppowBase->eTargetTypeSecondary == kTargetType_Location
			|| ppow->ppowBase->eTargetTypeSecondary == kTargetType_Teleport)
		{
			iAttackFxId = character_PlayFXLocation(pSrc, NULL, NULL, vecTarget,
				pchAttackFX, power_GetTint(ppow),
				fPrimaryAttackDelay,
				PLAYFX_NOT_ATTACHED, PLAYFX_PITCHTOTARGET | PLAYFX_TINT_FLAG(ppow) );
		}
		else
		{
			Vec3 loc;
			if(pSrc!=pRealTarget 
			   &&  (pRealTarget->entParent) 
			   && character_FindHitLoc(pSrc->entParent,pRealTarget->entParent,loc))
			{
				iAttackFxId = character_PlayFXLocation(pSrc, pSrc, pRealTarget, loc,
					pchAttackFX, power_GetTint(ppow),
					fPrimaryAttackDelay,
					PLAYFX_NOT_ATTACHED, PLAYFX_PITCHTOTARGET | PLAYFX_TINT_FLAG(ppow) );
			}
			else
			{
				iAttackFxId = character_PlayFX(pSrc, pSrc, pRealTarget,
					pchAttackFX, power_GetTint(ppow),
					fPrimaryAttackDelay,
					PLAYFX_NOT_ATTACHED, PLAYFX_PITCHTOTARGET | PLAYFX_TINT_FLAG(ppow) );
			}
		}
	}

	if (ppowFX->bDelayedHit && ppow->ppowBase->eEffectArea != kEffectArea_Cone
		&& pSrc != pRealTarget) {
		// If this is a projectile based power, go ahead and calculate the distance
		// delay for the primary target up front. The exception is cones, for which we
		// have to grandfather in abuse of ProjectileSpeed because of Flamethrower.
		fPrimaryDistDelay = distance3(ENTPOS(pSrc->entParent), ENTPOS(pRealTarget->entParent));
		fPrimaryDistDelay *= 1.0f/ppowFX->fProjectileSpeed;
	}
	PERFINFO_AUTO_STOP();

	PERFINFO_AUTO_START("DBLog", 1);
	if(ppow->ppowBase->eType!=kPowerType_Auto && ppow->ppowBase->eType!=kPowerType_Toggle)
		LOG_ENT( pSrc->entParent, LOG_POWERS, LOG_LEVEL_IMPORTANT, 0, "Power:Activate id:%u  %s targeting %s %s ", invokeID, dbg_BasePowerStr(ppow->ppowBase), dbg_NameStr(pRealTarget->entParent), dbg_LocationStr(vecTarget));
	PERFINFO_AUTO_STOP();

	if (ppow->ppowParentInstance) {
		// If this power was executed by another one, get the shared list of proc targets
		Power *ppowTemp = ppow->ppowParentInstance;
		while (ppowTemp->ppowParentInstance)
			ppowTemp = ppowTemp->ppowParentInstance;

		assert(ppowTemp->ppentProcChecked);
		eaProcsChecked = &ppowTemp->ppentProcChecked;
	} else if (ppow->ppowBase->bHasExecuteMods) {
		// Otherwise, if this is a top-level power that executes others, create the list
		assert(ppow->ppentProcChecked);
		eaProcsChecked = &ppow->ppentProcChecked;
		eaClear(eaProcsChecked);
	}

	if (bIsChain) {
		eaCreate(&eaChainHit);
		eaCreate(&eaChainForks);
		eaPush(&eaChainForks, chainfork_Create(pSrc, 1.0, 0));
	}

	// Handle chain jumps. Loop only runs once for non-chain effect areas
	for (iChainJump = 1; iChainJump <= (bIsChain ? ppow->ppowBase->iMaxTargetsHit : 1)
		&& (!bIsChain || iTotalHit < ppow->ppowBase->iMaxTargetsHit); iChainJump++)
	{
		bAnyForkHit = false;

		// Now loop through each fork, but do it backwards. That's because we might add
		// more for next time, or remove the current fork.
		for (iChainFork = (bIsChain ? eaSize(&eaChainForks) : 1) - 1; iChainFork >= 0; iChainFork--)
		{
			ChainFork *pFork = bIsChain ? eaChainForks[iChainFork] : 0;
			Character *pPrevTarget = ctx.mf.pPrevTarget = (bIsChain ? pFork->pPrevTarget : pSrc);
			float fChainDelay = bIsChain ? pFork->fChainDelay : 0;
			int iMaxTargetsHit = (iChainJump > 1) ? 1 : ppow->ppowBase->iMaxTargetsHit;
			int iNumHit = 0;

			// Reset a few things for each chain jump
			charlist_Clear(&listTargets);
			if (bIsChain && iChainJump > 1) {
				pRealTarget = pPrevTarget;

				if (ppow->ppowBase->ppchChainEff) {
					// We have a Chain Effectiveness expression, so update the current fChainEff
					combateval_StoreChainInfo(iChainJump, pFork->fChainEff);
					pFork->fChainEff = combateval_Eval(pSrc->entParent, NULL, ppow, ppow->ppowBase->ppchChainEff, ppow->ppowBase->pchSourceFile);
				}
			}

			//
			// Get all the targets in the area affected
			//
			PERFINFO_AUTO_START("GetTargetEntities", 1);
			{
				Vec3 vecRealLoc;
				Vec3 vecLocReachableOffset;

				// NOTE: pPrevTarget is used in place of pSrc for target reachability checks
				// so that chain powers jump from one target to the next. For non-chain powers,
				// pPrevTarget will always be the same as pSrc.

				// Moved the location setup out to here so geom stuff can use it
				if(ppow->ppowBase->eTargetType == kTargetType_Location
					|| ppow->ppowBase->eTargetType == kTargetType_Teleport)
				{
					copyVec3(vecTarget, vecRealLoc);
					getLocationReachableVecOffset( 0, vecLocReachableOffset );
				}
				else if(pRealTarget == pPrevTarget)
				{
					copyVec3(ENTPOS(pRealTarget->entParent), vecRealLoc);
					getLocationReachableVecOffset( pPrevTarget->entParent, vecLocReachableOffset );

					if(ppow->ppowBase->eEffectArea==kEffectArea_Cone) //this is a caster targeted cone, need to adjust real target a tad using their facing
					{
						Vec3 delta;
						Vec3 dest = {0,0,1};
						mulVecMat3(dest, ENTMAT(pRealTarget->entParent), delta);
						addVec3(ENTPOS(pRealTarget->entParent), delta, vecRealLoc);
					}
				}
				else //Two entities
				{
					if(!(character_EntUsesGeomColl(pRealTarget->entParent)
						&& character_FindHitLoc(pPrevTarget->entParent,pRealTarget->entParent,vecRealLoc)))
					{
						entGetPosAtEntTime(pRealTarget->entParent, pPrevTarget->entParent, vecRealLoc,0);
					}

					//99% of the time this will make vecRealLocVisOffset { 0, CHEST_OFFSET(5'), 0 } to preserve the old behavior
					//but if the entity is rotated in X or Z, it will rotate the vecLocReachableOffset.
					//NOTE: it really should consider capsule size, but I'm afraid to to change any existing behavior.
					//NOTE: for performance reasons this is not calced in target time, which is OK since the game doesn't support things rotated in X or Z moving
					getLocationReachableVecOffset( pPrevTarget->entParent, vecLocReachableOffset );
				}

				GetTargetEntities(pSrc, pPrevTarget, pRealTarget, vecRealLoc, ppow, &listTargets, vecLocReachableOffset, iChainJump );

				if (ppow->ppowBase->bShuffleTargetList)
				{
					charList_shuffle(&listTargets);
				}
			}
			PERFINFO_AUTO_STOP();

			//
			// Loop over every target and apply the power
			//

			PERFINFO_AUTO_START("ForEachTarget", 1);
			pref=charlist_GetFirst(&listTargets, &iter);
			while(pref!=NULL && (iMaxTargetsHit == 0 || iNumHit < iMaxTargetsHit) &&
				(!bIsChain || iTotalHit < ppow->ppowBase->iMaxTargetsHit))
			{
				Character *pTarget = pref->pchar;
				bool bAllowedToHit;
				int bFriend;
				float fRandRoll;
				float fDefense;
				int iDefenseType;
				int t_invokeId;
				U32 ulNow = timerSecondsSince2000();
				float fSecondaryDistDelay = 0;
				float fSecondaryAttackFXDelay = 0;
				float fHitFXDelay = 0;

				ctx.fEffectDelay = ctx.fEffectDelayFromProjectiles = 0;
				ctx.mf.fToHit = CalcToHit(pSrc, pTarget, ppow, &fDefense, &iDefenseType);

				// If we're chaining, skip this target if it's already been hit
				if (bIsChain && eaFind(&eaChainHit, pTarget) >= 0) {
					pref=charlist_GetNext(&iter);
					continue;
				}

				s_bSentHitMessage = 0;
				bFriend = character_TargetMatchesType(pSrc, pTarget, kTargetType_DeadOrAliveFriend, true);

				if(pSrc!=pTarget)
				{
					//if( ppow->ppowBase->eType == kPowerType_Auto )
					//	t_invokeId = invokeID;
					//else
						t_invokeId = nextInvokeId();

					pTarget->uiLastApplied = t_invokeId;

					if(bFriend)
					{
						character_RecordEvent(pSrc, kPowerEvent_Helped, ulNow);
						character_RecordEvent(pTarget, kPowerEvent_HelpedByOther, ulNow);
					}
					else
					{
						if( ppow->ppowBase->eAIReport == kAIReport_Always )
							character_RecordEvent(pSrc, kPowerEvent_Attacked, ulNow);

						if (!ppow->ppowBase->bIsEnvironmentHit)
						{
							character_RecordEvent(pTarget, kPowerEvent_AttackedByOther, ulNow);

							if(ppow->ppowBase->eType == kPowerType_Click)
							{
								character_RecordEvent(pTarget, kPowerEvent_AttackedByOtherClick, ulNow);
							}
						}

						character_RecordEvent(pSrc, kPowerEvent_AttackedNoException, ulNow);
					}
				}
				else
					t_invokeId = invokeID;

				PERFINFO_AUTO_START("TargetAnim", 1);
				// Set up per-target animation parameters in one place rather than all over

				if (ppowFX->bDelayedHit && ppow->ppowBase->eEffectArea == kEffectArea_Cone) {
					// stupid hack for flamethrower until I can think of something better
					fPrimaryDistDelay = distance3(ENTPOS(pSrc->entParent), ENTPOS(pTarget->entParent));
					fPrimaryDistDelay *= 1.0f/ppowFX->fProjectileSpeed;
				}

				if (pTarget == pRealTarget)
				{	// primary target
					if (ppowFX->bDelayedHit) {
						ctx.fEffectDelay = fPrimaryHitDelay;
						ctx.fEffectDelayFromProjectiles = fPrimaryDistDelay;
					} else {
						ctx.fEffectDelay = fHitFXDelay = fPrimaryHitDelay;
					}
				}
				else
				{	// additional targets
					ctx.fEffectDelay = fPrimaryHitDelay + ppowFX->fTimeToSecondaryHit + fChainDelay;

					fSecondaryAttackFXDelay = fPrimaryHitDelay + fChainDelay;
					if (ppowFX->bDelayedHit) {					// Initial attack was a projectile
						fSecondaryAttackFXDelay += fPrimaryDistDelay;
						ctx.fEffectDelayFromProjectiles = fPrimaryDistDelay;
					}

					if (ppowFX->fSecondaryProjectileSpeed > 0) {
						// Secondary Attack FX is a projectile
						fSecondaryDistDelay = distance3(ENTPOS(pRealTarget->entParent), ENTPOS(pTarget->entParent));
						fSecondaryDistDelay *= 1.0f/ppowFX->fSecondaryProjectileSpeed;

						ctx.fEffectDelayFromProjectiles += fSecondaryDistDelay;
					} else {
						// If secondary FX isn't a projectile, it hits at the same time as the mod
						fHitFXDelay = ctx.fEffectDelay + ctx.fEffectDelayFromProjectiles;
					}
				}
				PERFINFO_AUTO_STOP();

				ctx.bAlwaysHit = character_PowerAlwaysHits(pSrc, pTarget, ppow);

				if(!ctx.bAlwaysHit)
					ctx.bForceHit = ForceHitDueToMisses(pSrc, ctx.mf.fToHit);

				// This is done here (instead of in GetTargetEntities) since the
				// target character is in the target zone, they just aren't hit.
				bAllowedToHit = ppow->ppowBase->bShootThroughUntouchable
								|| ((pTarget->attrCur.fUntouchable<=0.0f
									 || pSrc==pTarget)
									&& character_TargetIsInCombatPhase(pSrc, pTarget));

				fRandRoll = ctx.mf.fToHitRoll = (float)rand()/(float)RAND_MAX;

				if(ctx.bAlwaysHit || ctx.bForceHit || ctx.mf.fToHitRoll < ctx.mf.fToHit)
				{
					// It's a hit
					int i;
					Vec3 vecGeomHitLoc;
					int iAttachHitFXTo = PLAYFX_NOT_ATTACHED;
					bool bGeomHit = false;
					bool bNeedPowerConfirm = character_NeedPowerConfirm(pTarget, ppow);
					bool bPendingConfirmBlock = (bNeedPowerConfirm
						&& pTarget->entParent->pl->uiDelayedInvokeID
						&& dbSecondsSince2000()<pTarget->entParent->pl->timeDelayedExpire);
					bool bActuallyHit = 0;

					if (!ppow->ppowBase->bIsEnvironmentHit)
					{
						if( ppow->ppowBase->eAIReport == kAIReport_HitOnly )
							character_RecordEvent(pSrc, kPowerEvent_Attacked, ulNow);
					}

					if(pSrc!=pTarget && character_EntUsesGeomColl(pTarget->entParent))
					{
						bGeomHit = character_FindHitLoc(pSrc->entParent,pTarget->entParent,vecGeomHitLoc);
					}

					PERFINFO_AUTO_START("Hit", 1);

					// For single target powers, projectiles are deferred until now because they might miss
					if(ppowFX->bDelayedHit && (ppow->ppowBase->eEffectArea==kEffectArea_Character || bIsChain)
						&& pTarget == pRealTarget)
					{
						if(bGeomHit)
						{
							iAttackFxId = character_PlayFXLocation(pSrc, pSrc, NULL, vecGeomHitLoc,
								pchAttackFX, power_GetTint(ppow),
								(F32)1/30.0f,
								PLAYFX_NOT_ATTACHED, PLAYFX_PITCHTOTARGET | PLAYFX_TINT_FLAG(ppow) );
						}
						else
						{
							iAttackFxId = character_PlayFX(pSrc, pSrc, pRealTarget,
								pchAttackFX, power_GetTint(ppow),
								fPrimaryAttackDelay,
								PLAYFX_NOT_ATTACHED, PLAYFX_PITCHTOTARGET | PLAYFX_TINT_FLAG(ppow) );
						}
					}

					// If we have a secondary attack FX and this isn't the primary target, play it
					if(ppowFX->pchSecondaryAttackFX && pTarget!=pRealTarget)
					{
						if(bGeomHit)
						{
							iAttackFxId = character_PlayFXLocation(pSrc, bIsChain ? pPrevTarget : pRealTarget, pTarget, vecGeomHitLoc,
								ppowFX->pchSecondaryAttackFX, power_GetTint(ppow),
								fSecondaryAttackFXDelay, PLAYFX_NOT_ATTACHED,
								iFXFlags|PLAYFX_DONTPITCHTOTARGET | PLAYFX_AT_GEO_LOC | PLAYFX_TINT_FLAG(ppow) );
						}
						else
						{
							iAttackFxId = character_PlayFX(pSrc, bIsChain ? pPrevTarget : pRealTarget, pTarget,
								ppowFX->pchSecondaryAttackFX, power_GetTint(ppow),
								fSecondaryAttackFXDelay, PLAYFX_NOT_ATTACHED,
								iFXFlags|PLAYFX_DONTPITCHTOTARGET | PLAYFX_TINT_FLAG(ppow) );
						}
					}
					if(ppow->ppowBase->eType!=kPowerType_Click)
					{
						// toggles apply their effects immediately
						ctx.fEffectDelay = ctx.fEffectDelayFromProjectiles = fHitFXDelay = 0;
					}

					// For projectiles, attach the hit FX to the appropriate attack
					if((ppowFX->bDelayedHit && pTarget == pRealTarget) ||
						(ppowFX->fSecondaryProjectileSpeed > 0 && pTarget != pRealTarget))
					{
						iAttachHitFXTo = iAttackFxId;
					}

					// Finally play hit FX on each target
					if(ppow->ppowBase->eEffectArea!=kEffectArea_Location
						&& (bAllowedToHit || pTarget->attrCur.fIntangible<=0.0f)
						&& !bNeedPowerConfirm
						&& (!ppow->ppowBase->bMainTargetOnly || pTarget==pRealTarget))
					{
						if(bGeomHit)
						{
							// Might actually not want to play these at all
							character_PlayFXLocation(pTarget, bIsChain ? pPrevTarget : pRealTarget, pSrc, vecGeomHitLoc,
								ppowFX->pchHitFX, power_GetTint(ppow),
								fHitFXDelay, iAttachHitFXTo,
								iFXFlags|PLAYFX_DONTPITCHTOTARGET | PLAYFX_AT_GEO_LOC | PLAYFX_TINT_FLAG(ppow) );
						}
						else
						{
							character_PlayFX(pTarget, bIsChain ? pPrevTarget : pRealTarget, pSrc, ppowFX->pchHitFX,
								power_GetTint(ppow), fHitFXDelay, iAttachHitFXTo,
								iFXFlags|PLAYFX_DONTPITCHTOTARGET | PLAYFX_TINT_FLAG(ppow) );
						}
						character_SetAnimBits(pTarget, ppowFX->piBlockBits, fTimeToBlock, PLAYFX_NOT_ATTACHED);
						character_SetAnimBits(pTarget, ppowFX->piHitBits, fHitFXDelay, iAttachHitFXTo);
					}

					if(!bAllowedToHit || bPendingConfirmBlock)
					{
						character_RecordEvent(pSrc, kPowerEvent_Miss, ulNow);
						if(pSrc!=pTarget)
						{
							character_RecordEvent(pTarget, kPowerEvent_MissByOther, ulNow);
							if(bFriend)
								character_RecordEvent(pTarget, kPowerEvent_MissByFriend, ulNow);
							else
								character_RecordEvent(pTarget, kPowerEvent_MissByFoe, ulNow);
						}

						if(ppow->ppowBase->eType!=kPowerType_Auto && ppow->ppowBase->eType!=kPowerType_Toggle)
						{
							if (bPendingConfirmBlock)
							{
								if (pTarget && pTarget->entParent)
								{
									sendInfoBox(pSrc->entParent, INFO_COMBAT_SPAM, "PowerTargetConfirming", pTarget->entParent->name);
								}
							}
							else
							{
								sendInfoBox(pSrc->entParent, INFO_COMBAT_ERROR, "PowerTargetUntouchable", ppow->ppowBase->pchDisplayName);
							}

							if(!ppow->ppowBase->bMainTargetOnly || pTarget==pRealTarget)
							{
								Character *pOwner = pSrc;
								while (pOwner->entParent && pOwner->entParent->erOwner)
								{
									Entity *e = erGetEnt(pOwner->entParent->erOwner);
									if (e && e->pchar)
									{
										pOwner = e->pchar;
									}
									else
									{
										break;
									}
								}

								serveFloatingInfoDelayed(pOwner->entParent, pTarget->entParent, "FloatUnaffected", ctx.fEffectDelay + ctx.fEffectDelayFromProjectiles);
							}

							LOG_ENT( pSrc->entParent, LOG_POWERS, LOG_LEVEL_IMPORTANT, 0, "Power:Hit id:%u target %s%s%s",
								t_invokeId,
								dbg_NameStr(pTarget->entParent),
								ctx.bAlwaysHit ? " (always)" : (ctx.bForceHit ? " (forced)" : ""),
								bPendingConfirmBlock ? " (confirm blocked)" : " (untouchable)");

							// Notify AI.
							{
								AIEventNotifyParams params = {0};

								params.notifyType = AI_EVENT_NOTIFY_POWER_FAILED;
								params.source = pSrc->entParent;
								params.target = pTarget->entParent;
								params.powerFailed.revealSource =	ppow->ppowBase->eAIReport==kAIReport_Always
																	|| ppow->ppowBase->eAIReport==kAIReport_MissOnly;
								params.powerFailed.ppow = ppow;

								aiEventNotify(&params);
							}
						}
					}
					else
					{
						ctx.bVecFloatLoc = bGeomHit
							&& !(ppow->ppowBase->eTargetType == kTargetType_Location
								 || ppow->ppowBase->eTargetType == kTargetType_Teleport
								 || ppow->ppowBase->eTargetType == kTargetType_Position);

						if (!ppow->ppowBase->bIsEnvironmentHit)
						{
							character_RecordEvent(pSrc, kPowerEvent_Hit, ulNow);
							if(pSrc!=pTarget)
							{
								character_RecordEvent(pTarget, kPowerEvent_HitByOther, ulNow);
								if(bFriend)
									character_RecordEvent(pTarget, kPowerEvent_HitByFriend, ulNow);
								else
								{
									//	if attrib mod doesn't count as a hit
									//	don't record it as one
									character_RecordEvent(pTarget, kPowerEvent_HitByFoe, ulNow);

									// travel power suppression if required
									if (server_state.enableTravelSuppressionPower)
									{
										ApplyTravelSuppression(pTarget, 2.0f);
									}
								}
							}
						}

						if(bNeedPowerConfirm)
						{
							pTarget->entParent->pl->uiDelayedInvokeID = t_invokeId;
							pTarget->entParent->pl->timeDelayedExpire = dbSecondsSince2000()+ppow->ppowBase->iTimeToConfirm+5;

							ctx.fEffectDelay += ppow->ppowBase->iTimeToConfirm+5;

							sendDialogPowerRequest(pTarget->entParent,
								ppow->ppowBase->pchDisplayConfirm,
								ENTTYPE(pSrc->entParent)!=ENTTYPE_PLAYER, pSrc->entParent->name,
								t_invokeId, ppow->ppowBase->iTimeToConfirm);
						}

						if (bIsChain) {
							ctx.mf.iChainJump = iChainJump;
							ctx.mf.fChainEff = pFork->fChainEff;
						}

						// Set up context for applying effects
						ctx.bIsProc = false;			// previous loops don't reset the context
						ctx.mf.uiInvokeID = t_invokeId;
						ctx.mf.fTimeBeforeHit = fHitFXDelay;
						ctx.mf.fEffectiveness = bIsChain ? ctx.mf.fChainEff : 1.0;
						copyVec3(ctx.bVecFloatLoc?vecGeomHitLoc:vecTarget, ctx.mf.vec);

						PERFINFO_AUTO_START("AttachAttribMods", 1);
						eaiPush(&ctx.eApplicationFilter, kModApplicationType_OnTick);
						bActuallyHit |= ApplyEffectGroups(&ctx, ppow->ppowBase->ppEffects, pSrc, pTarget, pRealTarget);
						eaiClear(&ctx.eApplicationFilter);
						PERFINFO_AUTO_STOP();

						// Slightly modified context for bonus mods
						PERFINFO_AUTO_START("AttachBonusAttribMods", 1);
						if(ppow->bBonusAttributes)
						{
							// Can only include boosts that are proper level
							int iLevelEff = character_CalcExperienceLevel(pSrc);
							bool bProcAllowedThisTarget = !pSrc->bIgnoreBoosts;

							// If power is marked to not proc except on the main target,
							// then don't let it.
							if ( ppow->ppowBase->bUseNonBoostTemplatesOnMainTarget &&
								(pTarget != pRealTarget) )
								bProcAllowedThisTarget = false;

							ctx.bIsProc = true;
							ctx.mf.fEffectiveness = 1.0;	// procs always apply at full strength

							// If we're tracking procs by target, check them and update the list
							if (eaProcsChecked && eaFind(eaProcsChecked, pTarget) >= 0)
								bProcAllowedThisTarget = false;			// already done!
							else if (eaProcsChecked && bProcAllowedThisTarget)
								eaPush(eaProcsChecked, pTarget);

							if (bProcAllowedThisTarget && (ppow->ppowBase->eProcAllowed == kProcAllowed_All ||
								ppow->ppowBase->eProcAllowed == kProcAllowed_PowerOnly))
							{
								// Bonus attributes from generic boosts
								for(i=eaSize(&ppow->ppBoosts)-1; i>=0; i--)
								{
									float fEff = ppow->ppBoosts[i] ? boost_Effectiveness(ppow->ppBoosts[i], pSrc->entParent, iLevelEff, pSrc->entParent->erOwner != 0) : 0.0f;

									if(fEff > 0.0f
										&& ppow->ppBoosts[i]->ppowBase
										&& ppow->ppBoosts[i]->ppowBase->bHasNonBoostTemplates
										&& ppow->ppBoosts[i]->fTimer <= 0.0f)
									{
										bActuallyHit |= ApplyEffectGroups(&ctx, ppow->ppBoosts[i]->ppowBase->ppEffects, pSrc, pTarget, pRealTarget);
									}
								}
							}

							if (bProcAllowedThisTarget && (ppow->ppowBase->eProcAllowed == kProcAllowed_All ||
								ppow->ppowBase->eProcAllowed == kProcAllowed_GlobalOnly))
							{
								// Bonus attributes from global boosts
								for(i=eaSize(&ppow->ppGlobalBoosts)-1; i>=0; i--)
								{
									float fEff = ppow->ppGlobalBoosts[i] ? boost_Effectiveness(ppow->ppGlobalBoosts[i], pSrc->entParent, iLevelEff, pSrc->entParent->erOwner != 0) : 0.0f;
									Power *globalBoost = NULL;
									if (pSrc && pSrc->entParent)
									{
										Entity *ent = pSrc->entParent;
										if (ent->erOwner)
										{
											ent = erGetEnt(ent->erOwner);
										}
										if (ent)
											globalBoost = character_OwnsPower(ent->pchar, ppow->ppGlobalBoosts[i]->ppowBase);
									}

									if(globalBoost && (globalBoost->ppowBase->bBoostIgnoreEffectiveness || fEff > 0.0f)
										&& ppow->ppGlobalBoosts[i]->ppowBase
										&& ppow->ppGlobalBoosts[i]->ppowBase->bHasNonBoostTemplates
										&& ppow->ppGlobalBoosts[i]->fTimer <= 0.0f)
									{
										int canFire = 0;

										//	Global boosts proc a bit differently than normal powers
										//	For auto and toggle powers, since there is only 1 global boost (instead of a boost per power)
										//	there is only 1 timer per global boost. As such, the only sensible way to do this is to have
										//	global boosts only proc once for all autos and toggles. Once any power activates the proc
										//	reset the timer. The global_boost tick will dec the 1 timer for that boost.
										//	Click powers can always proc.
										if (ppow->ppowBase->eType == kPowerType_Click)
										{
											canFire = 1;
										}
										else
										{
											if (canFire = (globalBoost->fTimer <= 0.0f))
											{
												eaPushUnique(&globalBoostActivatedThisFrame, globalBoost);
											}
										}

										if (canFire)
										{
											bActuallyHit |= ApplyEffectGroups(&ctx, ppow->ppGlobalBoosts[i]->ppowBase->ppEffects, pSrc, pTarget, pRealTarget);
										}
									}
								}
							}
						}

						// Bonus attributes from set bonus powers
						for(i=eaSize(&ppow->ppBonuses)-1;i>=0;i--)
						{
							if(ppow->ppBonuses[i]->pBonusPower
								&& ppow->ppBonuses[i]->pBonusPower->bHasNonBoostTemplates)
							{
								// TODO: Currently there are no sets that use the bonus power. If we
								// ever add one, this needs to be changed to be more like the boost
								// proc section above and to have a power proc allowed setting added
								// for it, which will mean changing it from a 4-way variable to flags,
								// and likely adding a power instance so that its timer can work.

								// The old code had bIsProc as false for these, but there doesn't
								// seem to be a good reason why, so now it's true.
								ctx.bIsProc = true;
								ctx.mf.fEffectiveness = 1.0;	// procs always apply at full strength

								bActuallyHit |= ApplyEffectGroups(&ctx, ppow->ppBonuses[i]->pBonusPower->ppEffects, pSrc, pTarget, pRealTarget);
							}
						}
						PERFINFO_AUTO_STOP();

						{
							const char* pchDest = pTarget->entParent->name;
							const char *pchSrc = pSrc->entParent->name;
							int translateSrcName = 0;
							int translateDestName = 0;

							if(ENTTYPE(pTarget->entParent)==ENTTYPE_CRITTER && (pchDest==NULL || pchDest[0]=='\0'))
							{
								pchDest = npcDefList.npcDefs[pTarget->entParent->npcIndex]->displayName;
								translateDestName	 = 1;
							}
							if(ENTTYPE(pSrc->entParent)==ENTTYPE_CRITTER && (pchDest==NULL || pchSrc[0]=='\0'))
							{
								pchSrc = npcDefList.npcDefs[pSrc->entParent->npcIndex]->displayName;
								translateSrcName = 1;
							}

							if(!ppow->bOnLastTick)
							{
								char message[1024] = {0};
								// Only send messages to players
								Entity * sendToMe = erGetEnt(pSrc->entParent->erOwner);
								if( ENTTYPE(pSrc->entParent) == ENTTYPE_PLAYER || (sendToMe && (ENTTYPE(sendToMe) == ENTTYPE_PLAYER )))
								{
									sendCombatMessage(pTarget->entParent, INFO_COMBAT_SPAM, CombatMessageType_PowerActivation, 1, ppow->ppowBase->crcFullName, 0, pchSrc, 0.f, 0.f);
								}
							}

							if(!ppow->bOnLastTick &&
								(attackerHitMessagesSent == 0 || !character_TargetIsFriend(pSrc, pTarget) || ppow->ppowBase->eEffectArea == kEffectArea_Character)) // only send one victim hit message for friendly area-of-effect powers.
							{
								char message[1024] = {0};
								Entity * sendToMe = erGetEnt(pSrc->entParent->erOwner);
								if( ENTTYPE(pSrc->entParent) == ENTTYPE_PLAYER || (sendToMe && (ENTTYPE(sendToMe) == ENTTYPE_PLAYER )))
								{
									sendCombatMessage(pSrc->entParent, INFO_COMBAT_SPAM, CombatMessageType_PowerActivation, 0, ppow->ppowBase->crcFullName, 0, pchDest, 0.f, 0.f);
									attackerHitMessagesSent++;
								}
							}
						}

						if(ppow->ppowBase->eType!=kPowerType_Auto && ppow->ppowBase->eType!=kPowerType_Toggle)
							LOG_ENT( pSrc->entParent, LOG_POWERS, LOG_LEVEL_IMPORTANT, 0, "Power:Hit id:%u target %s%s",
								t_invokeId,	dbg_NameStr(pTarget->entParent), ctx.bAlwaysHit ? " (always)" : (ctx.bForceHit ? " (forced)" : ""));
					}

					AI_LOG(AI_LOG_POWERS, (pSrc->entParent, "ApplyPow:    Power hits! (%6.3f < %6.3f) (%sforced) (%saffected)\n", ctx.mf.fToHitRoll, ctx.mf.fToHit,ctx.bAlwaysHit||ctx.bForceHit?"":"not ",bAllowedToHit?"":"un"));

					// Clear the miss count
					if(!ctx.bAlwaysHit)
					{
						pSrc->iSequentialMisses=0;
						pSrc->fSequentialMinToHit=100.0f;
					}

					if(!ppow->bOnLastTick && ppow->ppowBase->eType!=kPowerType_Auto && bActuallyHit) // Don't let iCntHits blow up.
					{
						pSrc->stats.iCntHits++; // for balancing
						pTarget->stats.iCntBeenHit++; // for balancing
						ppow->stats.iCntHits++; // for balancing
						if (isDevelopmentMode() && !isSharedMemory(ppow->ppowBase))
							(cpp_const_cast(BasePower*)(ppow->ppowBase))->stats.iCntHits++; // for balancing
					}

					if(bActuallyHit) {
						iNumHit++;
						iTotalHit++;

						if (bIsChain) {
							// Store this target so that we don't chain to it again
							eaPush(&eaChainHit, pTarget);
							// And use it as the origin point for the next jump on this fork
							pFork->pPrevTarget = pTarget;

							// Accumulate total chain delay so far
							pFork->fChainDelay += fSecondaryDistDelay + ppow->ppowBase->fChainDelay;

							// See if we're forking on this jump
							for (i = 0; i < eaiSize(&ppow->ppowBase->piChainFork); i++) {
								if (ppow->ppowBase->piChainFork[i] == iChainJump) {
									eaPush(&eaChainForks, chainfork_Create(pTarget, pFork->fChainEff, pFork->fChainDelay));
								}
							}
						}
					}
					PERFINFO_AUTO_STOP();
				}
				else if (iChainJump == 1)		// power missed
				{								// but don't spam miss stuff for chain jumps
					iTotalMiss++;

					PERFINFO_AUTO_START("Miss", 1);

					if (!ppow->ppowBase->bIsEnvironmentHit)
					{
						if( ppow->ppowBase->eAIReport == kAIReport_MissOnly )
							character_RecordEvent(pSrc, kPowerEvent_Attacked, ulNow);

						character_RecordEvent(pSrc, kPowerEvent_Miss, ulNow);
						if(pSrc!=pTarget)
						{
							character_RecordEvent(pTarget, kPowerEvent_MissByOther, ulNow);
							if(bFriend)
								character_RecordEvent(pTarget, kPowerEvent_MissByFriend, ulNow);
							else
								character_RecordEvent(pTarget, kPowerEvent_MissByFoe, ulNow);
						}
					}

					// Handle wildly missing projectiles
					if(ppowFX->bDelayedHit && (ppow->ppowBase->eEffectArea==kEffectArea_Character || bIsChain) &&
						pTarget == pRealTarget)
					{
						if(ppow->ppowBase->eType!=kPowerType_Toggle)
						{
							Entity *e = pRealTarget->entParent;
							Vec3 vecTarget;
							Vec3 vecDir;
							Vec3 vecFoo;
							float fRand;

							if(!character_FindHitLoc(pSrc->entParent,e,vecTarget))
								copyVec3(ENTPOS(pRealTarget->entParent), vecTarget);
							vecTarget[1]+=3.0*pRealTarget->entParent->seq->currgeomscale[1];
							subVec3(vecTarget, ENTPOS(pSrc->entParent), vecDir);

							normalVec3(vecDir);

							vecFoo[1] = (rand()>(RAND_MAX/2) ? -1 : 2.5) * ((float)rand()/(float)RAND_MAX);

							if(rand()<(RAND_MAX/2))
							{
								vecFoo[0] = -vecDir[2];
								vecFoo[2] = vecDir[0];
							}
							else
							{
								vecFoo[0] = vecDir[2];
								vecFoo[2] = -vecDir[0];
							}
							fRand = (float)rand()/(float)RAND_MAX;
							scaleVec3(vecFoo,
								(2+4*fRand)*pRealTarget->entParent->seq->currgeomscale[1],
								vecFoo);
							scaleVec3(vecDir, 10+8*fRand, vecDir);

							addVec3(vecFoo, vecTarget, vecTarget);
							addVec3(vecDir, vecTarget, vecTarget);

							iAttackFxId = character_PlayFXLocation(pSrc, pSrc, NULL, vecTarget,
								pchAttackFX, power_GetTint(ppow),
								fPrimaryAttackDelay, PLAYFX_NOT_ATTACHED, iFXFlags|PLAYFX_PITCHTOTARGET | PLAYFX_TINT_FLAG(ppow) );
						}
						else
						{
							iAttackFxId = character_PlayMaintainedFX(pSrc, pSrc, pRealTarget,
								pchAttackFX, power_GetTint(ppow),
								fPrimaryAttackDelay,
								PLAYFX_NOT_ATTACHED, PLAYFX_NO_TIMEOUT,
								iFXFlags|PLAYFX_DONTPITCHTOTARGET | PLAYFX_TINT_FLAG(ppow) );
						}
					}

					// The block anim starts at the same time as the attack anim (now).
					if(bAllowedToHit || !pTarget->attrCur.fIntangible<=0.0f
						&& (!ppow->ppowBase->bMainTargetOnly || pTarget==pRealTarget))
					{
						character_SetAnimBits(pTarget, ppowFX->piBlockBits, fTimeToBlock, PLAYFX_NOT_ATTACHED);
						character_PlayFX(pTarget, pTarget, pTarget, ppowFX->pchBlockFX,
							power_GetTint(ppow), 0.0f, PLAYFX_NOT_ATTACHED,
							iFXFlags|PLAYFX_DONTPITCHTOTARGET | PLAYFX_TINT_FLAG(ppow));
					}

					// Add deferred miss so that the chat spam and AI notify happen at the right time
					AddDeferredCombatEvent(ctx.fEffectDelay + ctx.fEffectDelayFromProjectiles,
						pSrc->entParent, pTarget->entParent, ppow, ctx.mf.fToHit,
						ctx.mf.fToHitRoll, 0, 0, !ppow->ppowBase->bMainTargetOnly || pTarget==pRealTarget,
						false);

					if(!ppow->ppowBase->bMainTargetOnly || pTarget==pRealTarget)
					{
						float fMissedBy = fRandRoll - ctx.mf.fToHit;
						float fMsgDelay = ctx.fEffectDelay + ctx.fEffectDelayFromProjectiles;

						if( ppow->ppowBase->eType == kPowerType_Click )
							pSrc->fLastChance = ctx.mf.fToHit;

						if(fMissedBy < fDefense)
						{
							// Missed due to defense
							AttribMod *pmod = modlist_BlameDefenseMod(&pTarget->listMod, fMissedBy/fDefense, iDefenseType);
							if(pmod && TemplateGetSub(pmod->ptemplate, pMessages, pchDisplayDefenseFloat))
							{
								serveFloatingInfoDelayed(pSrc->entParent, pTarget->entParent, TemplateGetSub(pmod->ptemplate, pMessages, pchDisplayDefenseFloat), fMsgDelay);
								serveFloatingInfoDelayed(pTarget->entParent, pTarget->entParent, TemplateGetSub(pmod->ptemplate, pMessages, pchDisplayDefenseFloat), fMsgDelay);
							}
							else if(pmod && pmod->ptemplate->ppowBase->pchDisplayDefenseFloat)
							{
								serveFloatingInfoDelayed(pSrc->entParent, pTarget->entParent, pmod->ptemplate->ppowBase->pchDisplayDefenseFloat, fMsgDelay);
								serveFloatingInfoDelayed(pTarget->entParent, pTarget->entParent, pmod->ptemplate->ppowBase->pchDisplayDefenseFloat, fMsgDelay);
							}
							else
							{
								serveFloatingInfoDelayed(pSrc->entParent, pTarget->entParent, "FloatDodge", fMsgDelay);
								serveFloatingInfoDelayed(pTarget->entParent, pTarget->entParent, "FloatDodge", fMsgDelay);
							}
						}
						else
						{
							serveFloatingInfoDelayed(pSrc->entParent, pTarget->entParent, "FloatMiss", fMsgDelay);
						}
					}

					// Remember that they missed
					pSrc->iSequentialMisses++;

					if(ppow->ppowBase->eType!=kPowerType_Auto && ppow->ppowBase->eType!=kPowerType_Toggle)
						LOG_ENT( pSrc->entParent, LOG_POWERS, LOG_LEVEL_VERBOSE, 0, "Power:Miss id:%u target %s",	t_invokeId,	dbg_NameStr(pTarget->entParent));

					pSrc->stats.iCntMisses++; // for balancing
					ppow->stats.iCntMisses++; // for balancing
					if (isDevelopmentMode() && !isSharedMemory(ppow->ppowBase))
						(cpp_const_cast(BasePower*)(ppow->ppowBase))->stats.iCntMisses++; // for balancing
					pTarget->stats.iCntBeenMissed++; // for balancing

					PERFINFO_AUTO_STOP();
				}

				pref=charlist_GetNext(&iter);

			} // End foreach target
			PERFINFO_AUTO_STOP();

			if (bIsChain && iNumHit > 0) {
				bAnyForkHit = true;
			} else if (bIsChain) {
				// This fork failed to hit any targets, so it goes away
				eaRemoveAndDestroy(&eaChainForks, iChainFork, NULL);
			}
		}

		if (bIsChain && !bAnyForkHit) {
			// If chaining but no targets in range were hit, break out of the loop early
			break;
		}
	}

	if (bIsChain) {
		eaDestroy(&eaChainHit);
		eaDestroyEx(&eaChainForks, NULL);
	}

	// Update all of the delayEval attribmods with the number of targets hit
	if (delayEvalAttribMods)
	{
		int i;
		for (i = eaSize(&delayEvalAttribMods) - 1; i >= 0; i--)
		{
			AttribMod *toUpdate = delayEvalAttribMods[i];
			if(toUpdate->evalInfo)
				toUpdate->evalInfo->targetsHit = iTotalHit;
		}
		eaDestroy(&delayEvalAttribMods);
	}

	if (globalBoostActivatedThisFrame)
	{
		int i;
		for (i = 0; i < eaSize(&globalBoostActivatedThisFrame); ++i)
		{
			Power *globalBoost = globalBoostActivatedThisFrame[i];
			globalBoost->fTimer = globalBoost->ppowBase->fActivatePeriod;
		}
		eaDestroy(&globalBoostActivatedThisFrame);
	}
	// travel power suppression if required
	if (server_state.enableTravelSuppressionPower && ppow->ppowBase->fTravelSuppression > 0.0f && !ppow->bOnLastTick)
	{
		ApplyTravelSuppression(pSrc, ppow->ppowBase->fTravelSuppression + ppow->ppowBase->fTimeToActivate);
	}

	// Clean up HitLocation array
	eaClearEx(&pSrc->entParent->ppHitLocs,NULL);

	// Increment the ID
	nextInvokeId();

	PERFINFO_AUTO_STOP();
	return true;
}

/*
 * A lot of things from character_ApplyPower have been stripped from this function because
 * they're assumed to be unused.
 */
bool character_ApplyPowerAtActivation(Character *pSrc, Power *ppow, EntityRef erTarget, const Vec3 vecTarget, unsigned int invokeID )
{
	EffectApplyContext ctx;
	int i;
	const PowerFX* ppowFX = power_GetFX(ppow);
	bool doesPowerHaveCasterAutoHit = false;

	if(!invokeID)
		invokeID = g_uiInvokeID;

	InitEAContext(&ctx);
	ctx.mf.uiInvokeID = invokeID;
	ctx.mf.pPow = ppow;

	PERFINFO_AUTO_START("ApplyPowerAtActivation", 1);
	assert(pSrc!=NULL);
	assert(ppow!=NULL);

	for (i = 0; i < eaiSize(&(int*)ppow->ppowBase->pAutoHit); i++)
	{
		if (ppow->ppowBase->pAutoHit[i] == kTargetType_Caster)
		{
			doesPowerHaveCasterAutoHit = true;
		}
	}

	// Has to have auto-hit and be caster-only because we don't do targeting checks here.
	//   Also, it might be weird if it does another target check at deactivate that returns
	//   different targets than activate/tick.  Also, people expect onDeactivate to be
	//   the exact opposite of onActivate, and that's not really true if we do another target
	//   check.  And the powers system doesn't track targets in any useful way to know to whom
	//   to apply the onDeactivate mods.
	if (!doesPowerHaveCasterAutoHit ||
		!strcmp(ppow->ppowBase->psetParent->pcatParent->pchName, "Set_Bonus") ||
		ppow->ppowBase->eType == kPowerType_Boost ||
		ppow->ppowBase->eType == kPowerType_GlobalBoost)
	{
		// These powers aren't allowed to have onActivate attribs,
		// Therefore there's no reason to continue.
		PERFINFO_AUTO_STOP();
		return true;
	}

	assert(ppowFX);

	// Handle the special entry attacks
	if(ppow->bFirstAttackInStance)
	{
		ctx.mf.fTimeBeforeHit = ppowFX->fInitialTimeToHit;
	}
	else
	{
		ctx.mf.fTimeBeforeHit = ppowFX->fTimeToHit;
	}

	{
		ctx.bAlwaysHit = true; // Assume auto-hit, if the power isn't, it should have returned at top.
		ctx.bForceHit = false; // Assumed from bAlwaysHit
		ctx.mf.fToHit = 1.0f; // assume

		// This does a to-hit roll for... the fun of it?
		ctx.mf.fToHitRoll = (float)rand()/(float)RAND_MAX;

		if(ppow->ppowBase->eType==kPowerType_Click)
		{
			ctx.fEffectDelay = ctx.mf.fTimeBeforeHit;
		}

		copyVec3(vecTarget, ctx.mf.vec);

		PERFINFO_AUTO_START("AttachAttribMods", 1);
		eaiPush(&ctx.eApplicationFilter, kModApplicationType_OnActivate);
		ApplyEffectGroups(&ctx, ppow->ppowBase->ppEffects, pSrc, pSrc, pSrc);
		eaiDestroy(&ctx.eApplicationFilter);
		PERFINFO_AUTO_STOP();
	}

	// Increment the ID
	nextInvokeId();

	PERFINFO_AUTO_STOP();
	return true;
}

/*
* A lot of things from character_ApplyPower have been stripped from this function because
* they're assumed to be unused.
*/
bool character_ApplyPowerAtDeactivation(Character *pSrc, Power *ppow, EntityRef erTarget, unsigned int invokeID)
{
	EffectApplyContext ctx;
	int i;
	const PowerFX* ppowFX = power_GetFX(ppow);
	bool doesPowerHaveCasterAutoHit = false;

	if(!invokeID)
		invokeID = g_uiInvokeID;

	InitEAContext(&ctx);
	ctx.mf.uiInvokeID = invokeID;
	ctx.mf.pPow = ppow;

	PERFINFO_AUTO_START("ApplyPowerAtDeactivation", 1);
	assert(pSrc!=NULL);
	assert(ppow!=NULL);

	for (i = 0; i < eaiSize(&(int*)ppow->ppowBase->pAutoHit); i++)
	{
		if (ppow->ppowBase->pAutoHit[i] == kTargetType_Caster)
		{
			doesPowerHaveCasterAutoHit = true;
		}
	}

	// Has to have auto-hit and be caster-only because we don't do targeting checks here.
	//   Also, it might be weird if it does another target check at deactivate that returns
	//   different targets than activate/tick.  Also, people expect onDeactivate to be
	//   the exact opposite of onActivate, and that's not really true if we do another target
	//   check.  And the powers system doesn't track targets in any useful way to know who
	//   the onActivate mods were applied to.
	// Cannot be kTargetType_Location, kTargetType_Teleport, or kTargetType_Position because I
	//   don't have the vector for the location right now.  After further research, it appears
	//   that we do have this information:  click powers are instantaneous, so
	//   vecLocationCurrent is correct in that case.  Auto and toggle powers have PowerListItem
	//   items in the lists on the Character that contain the vector.  I'm not bothering
	//   implementing the plumbing right now to get that information, but it shouldn't be
	//   extremely difficult if we need to do it.
	if (!doesPowerHaveCasterAutoHit
		|| !strcmp(ppow->ppowBase->psetParent->pcatParent->pchName, "Set_Bonus")
		|| ppow->ppowBase->eType == kPowerType_Boost
		|| ppow->ppowBase->eType == kPowerType_GlobalBoost
		|| ppow->ppowBase->eTargetType == kTargetType_Location
		|| ppow->ppowBase->eTargetType == kTargetType_Teleport
		|| ppow->ppowBase->eTargetType == kTargetType_Position)
	{
		// These powers aren't allowed to have onDeactivate attribs,
		// Therefore there's no reason to continue.
		PERFINFO_AUTO_STOP();
		return true;
	}

	assert(ppowFX);

	// Handle the special entry attacks
	if(ppow->bFirstAttackInStance)
	{
		ctx.mf.fTimeBeforeHit = ppowFX->fInitialTimeToHit;
	}
	else
	{
		ctx.mf.fTimeBeforeHit = ppowFX->fTimeToHit;
	}

	{
		bool bExpired = power_CheckUsageLimits(ppow);
		ctx.bAlwaysHit = true; // Assume auto-hit, if the power isn't, it should have returned at top.
		ctx.bForceHit = false; // Assumed from bAlwaysHit
		ctx.mf.fToHit = 1.0f; // assume

		// This does a to-hit roll for... the fun of it?
		ctx.mf.fToHitRoll = (float)rand()/(float)RAND_MAX;

		if(ppow->ppowBase->eType==kPowerType_Click)
		{
			ctx.fEffectDelay = ctx.mf.fTimeBeforeHit;
		}

		PERFINFO_AUTO_START("AttachAttribMods", 1);
		eaiPush(&ctx.eApplicationFilter, kModApplicationType_OnDeactivate);
		if (bExpired)
			eaiPush(&ctx.eApplicationFilter, kModApplicationType_OnExpire);
		ApplyEffectGroups(&ctx, ppow->ppowBase->ppEffects, pSrc, pSrc, pSrc);
		eaiDestroy(&ctx.eApplicationFilter);
		PERFINFO_AUTO_STOP();
	}

	// Increment the ID
	nextInvokeId();
	PERFINFO_AUTO_STOP();
	return true;
}

static void character_ApplyPowerInternal(Character *p, Power *ppow, ModApplicationType eType)
{
	assert(p!=NULL);
	assert(ppow);
	if (p && ppow)
	{
		EffectApplyContext ctx;
		const PowerFX *ppowFX = power_GetFX(ppow);

		InitEAContext(&ctx);
		ctx.bAlwaysHit = true;
		ctx.mf.fTimeBeforeHit = (ppow->bFirstAttackInStance) ? ppowFX->fInitialTimeToHit :ppowFX->fTimeToHit;
		ctx.mf.fToHit = 1.0;
		ctx.mf.uiInvokeID = g_uiInvokeID;
		ctx.mf.pPow = ppow;
		copyVec3(ENTPOS(p->entParent), ctx.mf.vec);

		eaiPush(&ctx.eApplicationFilter, eType);
		ApplyEffectGroups(&ctx, ppow->ppowBase->ppEffects, p, p, p);
		eaiDestroy(&ctx.eApplicationFilter);
	}
}

static void character_ChangePowerEnabledStatus(Character *pchar, Power *ppow, int enabled)
{
	if ((!!enabled) == ppow->bEnabled)
	{
		return;
	}

	ppow->bEnabled = !!enabled;

	eaPush(&pchar->entParent->enabledStatusChange, powerRef_CreateFromPower(pchar->iCurBuild, ppow));

	if (ppow->ppowBase->eType == kPowerType_GlobalBoost)
	{
		pchar->bRecalcStrengths = true;
	}
}

static void character_EnablePower(Character *p, Power *ppow)
{
	if (!ppow)
		return;

	// I've got this in the entity log so I can compare it to when I received respec information
	LOG_ENT(p->entParent, LOG_ENTITY, LOG_LEVEL_DEBUG, 0, "Enabling power %s", ppow->ppowBase->pchFullName);

	character_ChangePowerEnabledStatus(p, ppow, true);
	character_ApplyPowerInternal(p, ppow, kModApplicationType_OnEnable);
	if (ppow->ppowBase->eType == kPowerType_Auto
		|| ppow->ppowBase->eType == kPowerType_GlobalBoost)
	{
		character_ActivatePower(p, erGetRef(p->entParent), ENTPOS(p->entParent), ppow);
	}
}

static void character_DisablePower(Character *p, Power *ppow)
{
	PowerListIter iter;
	PowerListItem *ppowListItem;

	if (!ppow)
		return;

	// I've got this in the entity log so I can compare it to when I received respec information
	LOG_ENT(p->entParent, LOG_ENTITY, LOG_LEVEL_DEBUG, 0, "Disabling power %s", ppow->ppowBase->pchFullName);

	if (p->ppowTried == ppow)
	{
		character_KillRetryPower(p);
	}

	if (p->ppowQueued == ppow)
	{
		character_KillQueuedPower(p);
	}

	if (p->ppowCurrent == ppow)
	{
		character_KillCurrentPower(p);
	}

	if (ppow->ppowBase->eType == kPowerType_Toggle)
	{
		for (ppowListItem = powlist_GetFirst(&p->listTogglePowers, &iter);
			ppowListItem != NULL;
			ppowListItem = powlist_GetNext(&iter))
		{
			if (ppowListItem->ppow == ppow)
			{
				character_ShutOffTogglePower(p, ppowListItem->ppow);
				powlist_RemoveCurrent(&iter);
			}
		}
	}

	if (ppow->ppowBase->eType == kPowerType_Auto)
	{
		for (ppowListItem = powlist_GetFirst(&p->listAutoPowers, &iter);
			ppowListItem != NULL;
			ppowListItem = powlist_GetNext(&iter))
		{
			if (ppowListItem->ppow == ppow)
			{
				character_ShutOffContinuingPower(p, ppowListItem->ppow);
				powlist_RemoveCurrent(&iter);
			}
		}
	}

	character_ChangePowerEnabledStatus(p, ppow, false);
	character_ApplyPowerInternal(p, ppow, kModApplicationType_OnDisable);
}

// only call this when you're about to switch builds or something like that.
// As soon as character_UpdateEnableState() gets called again, it will re-enable mods!
// THIS TRIGGERS ONDISABLED MODS!
void character_DisableAllPowers(Character *p)
{
	int setIndex, powIndex;
	PowerSet *pset;
	Power *ppow;

	character_KillRetryPower(p);
	character_KillQueuedPower(p);
	character_KillCurrentPower(p);

	for (setIndex = eaSize(&p->ppPowerSets) - 1; setIndex >= 0; setIndex--)
	{
		pset = p->ppPowerSets[setIndex];

		for (powIndex = eaSize(&pset->ppPowers) - 1; powIndex >= 0; powIndex--)
		{
			ppow = pset->ppPowers[powIndex];
			character_DisablePower(p, ppow);
		}
	}
}

// only call this when doing a character_Reset() or something very similar!
// This hacks everything up pretty raw.
// THIS DOES NOT TRIGGER ONDISABLED MODS!
void character_OverrideAllPowersToDisabled(Character *p)
{
	int setIndex, powIndex;
	PowerSet *pset;
	Power *ppow;

	for (setIndex = eaSize(&p->ppPowerSets) - 1; setIndex >= 0; setIndex--)
	{
		pset = p->ppPowerSets[setIndex];

		for (powIndex = eaSize(&pset->ppPowers) - 1; powIndex >= 0; powIndex--)
		{
			ppow = pset->ppPowers[powIndex];

			if (ppow)
				ppow->bEnabled = false;
		}
	}
}

// This function should be called only when an event has occurred that may cause a trigger
//   to be pulled.  See comments inside the function about the specific triggers.
// This function is also called on occasion after something that resets mods on a character.
void character_UpdateEnabledState(Character *p, Power *ppow)
{
	bool modesAllowPower;
	bool passesActivateRequires = true;
	int instanceID;
	bool isInstanceLocked;
	bool deathAllowsPower;
	bool shouldBeEnabled;
	
	if (!ppow)
		return;

	// This function is called directly (instead of through character_UpdateAllEnabledState()):
	// - whenever a power has been bought or is about to be revoked.
	// - whenever a power is manually enabled or disabled by the player.
	// Both of these cases alter ppow->bDisabledManually, which isn't calculated here.
	// Make sure that you always call this function with powers in the current build!
	
	// Called (via All) by character_TickPhaseTwo() if Character::bRecalcEnabledState is true.
	// Character::bRecalcEnabledState is set to true when a power mode changes.
	modesAllowPower = character_ModesAllowPower(p, ppow->ppowBase);

	// ARM NOTE:  Until we figure out a way to only call this on ticks we must,
	//   we won't be able to use it as an enable/disable trigger!
//	passesActivateRequires = !ppow->ppowBase->ppchActivateRequires 
//								|| chareval_Eval(p, ppow->ppowBase->ppchActivateRequires, ppow->ppowBase->pchSourceFile);
	
	// Called (via All) by resumeCharacter(), which catches mapmoves, which is the event that
	//   could pull this trigger.
	turnstileMapserver_getMapInfo(&instanceID, NULL);
	isInstanceLocked = ppow->ppowBase->bInstanceLocked
						&& ppow->instanceBought != instanceID;

	// Called (via All) by character_HandleDeathAndResurrection() and dieNow() when resurrection
	//   and death events happen, respectively.
	deathAllowsPower = character_DeathAllowsPower(p, ppow->ppowBase);

	shouldBeEnabled = !ppow->bDisabledManually 
						&& modesAllowPower
						&& passesActivateRequires
						&& !isInstanceLocked
						&& deathAllowsPower;

	if (!ppow->bEnabled && shouldBeEnabled)
	{
		character_EnablePower(p, ppow);
		p->entParent->team_buff_update = true;
	}
	else if (ppow->bEnabled && !shouldBeEnabled)
	{
		character_DisablePower(p, ppow);
		p->entParent->team_buff_update = true;
	}
}

// Only updates powers in the currently active build!
void character_UpdateAllEnabledState(Character *p)
{
	int setIndex, powIndex;
	PowerSet *pset;
	Power *ppow;

	for (setIndex = eaSize(&p->ppPowerSets) - 1; setIndex >= 0; setIndex--)
	{
		pset = p->ppPowerSets[setIndex];

		for (powIndex = eaSize(&pset->ppPowers) - 1; powIndex >= 0; powIndex--)
		{
			ppow = pset->ppPowers[powIndex];
			character_UpdateEnabledState(p, ppow);
		}
	}
}

// Make sure you only call this on powers in the current build!
void character_ManuallyEnablePower(Character *p, Power *ppow)
{
	if (!ppow)
		return;

	ppow->bDisabledManually = false;
	eaPush(&p->entParent->enabledStatusChange, powerRef_CreateFromPower(p->iCurBuild, ppow));
	character_UpdateEnabledState(p, ppow);
}

// Make sure you only call this on powers in the current build!
void character_ManuallyDisablePower(Character *p, Power *ppow)
{
	if (!ppow)
		return;

	ppow->bDisabledManually = true;
	eaPush(&p->entParent->enabledStatusChange, powerRef_CreateFromPower(p->iCurBuild, ppow));
	character_UpdateEnabledState(p, ppow);
}

/**********************************************************************func*
 * AllowedToActivateBasePower
 *
 */
static bool AllowedToActivateBasePower(Character *p, const BasePower *ppowBase, bool bReport)
{
	if (ppowBase->ppchActivateRequires && !chareval_Eval(p, ppowBase->ppchActivateRequires, ppowBase->pchSourceFile))
	{
		return false;
	}

	if(ppowBase->eType != kPowerType_Auto)
	{
		if(p->attrCur.fHeld>0 && !ppowBase->bCastThroughHold)
		{
			if(bReport)
			{
				sendInfoBox(p->entParent, INFO_COMBAT_ERROR, "YouAreHeld");
				serveFloatingInfo(p->entParent, p->entParent, "FloatHeld");
			}
			return false;
		}
		if(p->attrCur.fStunned>0 && !ppowBase->bCastThroughStun)
		{
			if(bReport)
			{
				sendInfoBox(p->entParent, INFO_COMBAT_ERROR, "YouCannotUsePowers");
				serveFloatingInfo(p->entParent, p->entParent, "FloatStunned");
			}
			return false;
		}
		if(p->attrCur.fSleep>0 && !ppowBase->bCastThroughSleep)
		{
			if(bReport)
			{
				sendInfoBox(p->entParent, INFO_COMBAT_ERROR, "YouAreAsleep");
				serveFloatingInfo(p->entParent, p->entParent, "FloatAsleep");
			}
			return false;
		}
		// Terrorize players
		if(ENTTYPE(p->entParent) == ENTTYPE_PLAYER
			&& p->attrCur.fTerrorized>0
			&& p->fTerrorizePowerTimer>=0.0
			&& !ppowBase->bCastThroughTerrorize)
		{
			if(bReport)
			{
				sendInfoBox(p->entParent, INFO_COMBAT_ERROR, "YouAreTerrorized");
				serveFloatingInfo(p->entParent, p->entParent, "FloatTerrorized");
			}
			return false;
		}
	}
	if (p->attrCur.fHitPoints<=0 && ppowBase->eType == kPowerType_Inspiration 
		&& ENTTYPE(p->entParent) != ENTTYPE_PLAYER)
	{
		return false;
	}

	// special cases
	if (stricmp(ppowBase->pchName, "prestige_Mission_Teleport") == 0)
	{
		StoryTaskInfo * pSTI = MissionGetCurrentMission(p->entParent);
		if (!pSTI)
		{
			if (bReport)
			{
				sendInfoBox(p->entParent, INFO_USER_ERROR, "TeleportNoMission");
			}
			return false;
		}

		if (TaskIsZoneTransfer(pSTI))
		{
			if (bReport)
			{
				sendInfoBox(p->entParent, INFO_USER_ERROR, "TeleportNotAvailable");
			}
			return false;
		}

		if (!MissionCanGetToMissionMap(p->entParent, pSTI))
		{
			if (bReport)
			{
				sendInfoBox(p->entParent, INFO_USER_ERROR, "TeleportBadMap");
			}
			return false;
		}
	}

	return true;
}

/**********************************************************************func*
 * character_AllowedToActivatePower
 *
 */
bool character_AllowedToActivatePower(Character *p, Power *ppow, bool bReport)
{
	if (!ppow)
	{
		return false;
	}

	if (!ppow->bEnabled)
	{
		return false;
	}

	if (!AllowedToActivateBasePower(p, ppow->ppowBase, bReport))
	{
		return false;
	}

	if (ppow->ppowBase->eType != kPowerType_Auto
		&& p->attrCur.fOnlyAffectsSelf > 0.0f)
	{
		bool bOK;

		bOK = character_PowerAffectsTarget(p, p, ppow);

		// We still want to be able to turn off all toggles, and turn on
		//  toggles that don't target foes
		if (!bOK && ppow->ppowBase->eType == kPowerType_Toggle)
		{
			if (ppow->ppowBase->eTargetType != kTargetType_Foe)
			{
				bOK = true;
			}
			else
			{
				// Allow a foe-targeted power to be toggled if it's already on
				PowerListItem *ppowListItem;
				PowerListIter iter;

				for (ppowListItem = powlist_GetFirst(&p->listTogglePowers, &iter);
					ppowListItem != NULL && !bOK;
					ppowListItem = powlist_GetNext(&iter))
				{
					if (ppow->ppowBase == ppowListItem->ppow->ppowOriginal)
						bOK = true;
				}
			}
		}

		// Iterate over the attrib mods and see if any of them are
		// EntCreates. If they are, then this power is disallowed.
		if (bOK && basepower_HasAttrib(ppow->ppowBase, kSpecialAttrib_EntCreate))
		{
			bOK = false;
		}

		if (!bOK)
		{
			if (bReport)
			{
				sendInfoBox(p->entParent, INFO_COMBAT_ERROR, "YouCanOnlyAffectYourself");
			}
			return false;
		}
	}

	if(ppow->ppowBase->bHasLifetime)
	{
		// ARM NOTE:  We're explicitly letting auto powers activate even if their lifetime has expired!
		// power_CheckUsageLimits explicitly does NOT let auto powers pass, and that's used in ExpirePowers_Tick.
		// So the lifetime-expired auto power will activate, tick (probably), and then deactivate all
		// within one character tick.
		if (ppow->ppowBase->eType != kPowerType_Auto && ppow->ppowBase->fLifetime>0)
		{
			if(ppow->ulCreationTime + ((int) (ppow->ppowBase->fLifetime + 0.5f)) < timerSecondsSince2000())
			{
				if(bReport)
				{
					sendInfoBox(p->entParent, INFO_COMBAT_ERROR, "PowerHasRunOutTime", ppow->ppowBase->pchDisplayName);
				}
				return false;
			}
		}
		if(ppow->ppowBase->fLifetimeInGame>0)
		{
			if(ppow->fAvailableTime > ppow->ppowBase->fLifetimeInGame)
			{
				if(bReport)
				{
					sendInfoBox(p->entParent, INFO_COMBAT_ERROR, "PowerHasRunOutTime", ppow->ppowBase->pchDisplayName);
				}
				return false;
			}
		}
	}

	if(ppow->ppowBase->bHasUseLimit)
	{
		if(ppow->ppowBase->fUsageTime>0)
		{
			if(ppow->fUsageTime<=0)
			{
				if(bReport)
				{
					sendInfoBox(p->entParent, INFO_COMBAT_ERROR, "PowerHasRunOutTime", ppow->ppowBase->pchDisplayName);
				}
				return false;
			}
		}

		if(ppow->ppowBase->iNumCharges>0)
		{
			if(ppow->iNumCharges<=0)
			{
				if(bReport)
				{
					sendInfoBox(p->entParent, INFO_COMBAT_ERROR, "PowerHasRunOutCharges", ppow->ppowBase->pchDisplayName);
				}
				return false;
			}
		}
	}

	// Only check the level limit for non-temporary, non-bonus powers
	// Do not check this for critters, they run check levels when the power is granted and we don't want to accidentally disable their powers later
	if( ENTTYPE(p->entParent) == ENTTYPE_PLAYER &&  stricmp(ppow->ppowBase->psetParent->pcatParent->pchName,"Set_Bonus") != 0 && !character_AtLevelCanUsePower( p, ppow))
	{
		if(bReport) 
		{
			sendInfoBox(p->entParent, INFO_COMBAT_ERROR, "PowerCannotUseLevel", ppow->ppowBase->pchDisplayName, p->iCombatLevel+1);
		}
		return false;
	}

	if( power_IsMarkedForTrade(p->entParent, ppow) )
	{
		if(bReport)
		{
			sendInfoBox(p->entParent, INFO_COMBAT_ERROR, "PowerCannotUseTrade", ppow->ppowBase->pchDisplayName );
		}
	}
	
	return true;
}

/**********************************************************************func*
 * character_IsInspirationSlotInUse
 *
 */
bool character_IsNamedTogglePowerActive(Character *p, const char *name)
{
	if(p && name && name[0])
	{
		PowerListItem *ppowListItem = p->listTogglePowers;

		while(ppowListItem)
		{
			if(ppowListItem->ppow && ppowListItem->ppow->ppowBase && ppowListItem->ppow->ppowBase->pchName &&
				!stricmp(name, ppowListItem->ppow->ppowBase->pchName))
			{
				return true;
			}

			ppowListItem = ppowListItem->next;
		}
	}

	return false;
}

/**********************************************************************func*
 * character_IsInspirationSlotInUse
 *
 */
bool character_IsInspirationSlotInUse(Character *p, int iCol, int iRow)
{
	int iMagic = iCol*CHAR_INSP_MAX_ROWS+iRow;

    if(p->ppowCurrent && p->ppowCurrent->ppowBase->eType == kPowerType_Inspiration)
	{
		if(p->ppowCurrent->idx == iMagic)
			return true;
	}
	if(p->ppowQueued && p->ppowQueued->ppowBase->eType == kPowerType_Inspiration)
	{
		if(p->ppowQueued->idx == iMagic)
			return true;
	}
	if(p->ppowTried && p->ppowTried->ppowBase->eType == kPowerType_Inspiration)
	{
		if(p->ppowTried->idx == iMagic)
			return true;
	}

	return false;
}

static bool character_assignChainedPowers(Character *p, Power *ppow)
{
	if (ppow->ppowBase->pchChainIntoPowerName)
	{
		//	find the power that matches its base
		int i;
		const BasePower *bChainPower;
		Power *chainPower = NULL;

		bChainPower = powerdict_GetBasePowerByFullName(&g_PowerDictionary, ppow->ppowBase->pchChainIntoPowerName);

		for (i = 0; (i < eaSize(&p->ppPowerSets)) && !chainPower; ++i)
		{
			int j;
			for (j = 0; (j < eaSize(&p->ppPowerSets[i]->ppPowers)) && !chainPower; ++j)
			{
				Power *currentPower = p->ppPowerSets[i]->ppPowers[j];
				if (currentPower && currentPower->ppowOriginal == bChainPower)
				{
					chainPower = currentPower;
				}
			}
		}
		assertmsg(chainPower, "Could not find power match for basepower");
		if (chainPower && (ppow->chainsIntoPower == NULL))
		{
			bool returnMe;

			ppow->chainsIntoPower = chainPower;
			returnMe = character_assignChainedPowers(p, chainPower);	//	continue along the chain assigning powers

			if (!returnMe)
				ppow->chainsIntoPower = NULL; // Clean up after yourself to ensure that we don't get stuck in an infinite loop

			return returnMe;
		}
		else
		{
			//	we failed somehow, don't queue this power.
			//	it chained into another power, but we're already locked into a power
			//	possible loop detected, abort the queue
			ppow->chainsIntoPower = NULL; // Clean up after yourself to ensure that we don't get stuck in an infinite loop
			p->ppowQueued = NULL;

			return false;
		}
	}

	return true;
}
/**********************************************************************func*
 * character_ActivateInspiration
 *
 */
void character_ActivateInspiration(Character *pSrc, Character *pTarget, int iCol, int iRow)
{
	assert(pSrc!=NULL);
	assert(pTarget!=NULL);

	if (pSrc->bIgnoreInspirations)
	{
		return;
	}

	if(iCol<pSrc->iNumInspirationCols
		&& iRow<pSrc->iNumInspirationRows
		&& pSrc->aInspirations[iCol][iRow]!=NULL
		&& pSrc->aInspirations[iCol][iRow]->eType==kPowerType_Inspiration)
	{
		Power *ppow;

		if (!AllowedToActivateBasePower(pTarget, pSrc->aInspirations[iCol][iRow], true)
			|| !character_ModesAllowPower(pTarget, pSrc->aInspirations[iCol][iRow])
			|| !character_DeathAllowsPower(pTarget, pSrc->aInspirations[iCol][iRow]))
		{
			// Tell the client that this thing didn't actually go away.
			eaiPush(&pSrc->entParent->inspirationStatusChange, iCol*CHAR_INSP_MAX_ROWS+iRow);
			return;
		}

		ppow = inspiration_Create(pSrc->aInspirations[iCol][iRow], pSrc, iCol*CHAR_INSP_MAX_ROWS+iRow);

		// OK, stuff it in the queue
		// AI_LOG(AI_LOG_POWERS, (p->entParent, "Activate: Adding inspiration to queue (%s).\n", ppow->ppowBase->pchName));

		character_AccrueBoosts(pTarget, ppow, 0.0f);

		if (pSrc == pTarget)
		{
			character_KillQueuedPower(pTarget);
			character_KillRetryPower(pTarget);

			pTarget->ppowQueued = ppow;
			//commenting this function because player power should not chain and all
			//inspirations are player powers and only inspirations go through this function
			//character_assignChainedPowers(pTarget, ppow);

			pTarget->erTargetQueued = erGetRef(pTarget->entParent);
			copyVec3(ENTPOS(pTarget->entParent), pSrc->vecLocationQueued);		
		}
		else
		{
			//places inspiration in alternate queue if player feeds inspiration to pet.  This was done
			//to prevent inspirations from overwriting a queued chained power, which would interrupt
			//the pet's chaining power and cause the mapserver to crash.  Now, if the pet is using
			//a chaining power while it receives an inspiration, it won't activate the inspiration
			//until after the chaining power finishes.				
			pTarget->ppowAlternateQueue = ppow;

			pTarget->erTargetAlternateQueued = erGetRef(pTarget->entParent);
			copyVec3(ENTPOS(pTarget->entParent), pTarget->vecLocationAlternateQueued);
		}
		MissionItemInterruptInteraction(pSrc->entParent);
		RandomFameMoved(pSrc->entParent);

		if(ppow)
		{
			LOG_ENT( pTarget->entParent, LOG_POWERS, LOG_LEVEL_IMPORTANT, 0, "Power:UseInspiration %s", ppow->ppowBase->pchName);
		}
	}
	else
	{
		// Remove it: it was bad.
		character_RemoveInspiration(pSrc, iCol, iRow, NULL);
	}
	character_CompactInspirations(pSrc);
}

/**********************************************************************func*
 * character_ActivatePowerAtLocation
 *
 */
void character_ActivatePowerAtLocation(Character *p, Character *pTarget, Vec3 loc, Power *ppow)
{
	assert(p!=NULL);
	assert(ppow!=NULL);

	if(ppow->ppowBase->bTargetNearGround)
	{
		if(HeightAtLoc(loc, 0.01f, 2.0f) > 1.0f)
		{
			sendInfoBox(p->entParent, INFO_COMBAT_ERROR, "PowerTargetMustBeOnGround");
			serveFloatingInfo(p->entParent, p->entParent, "FloatTargetMustBeOnGround");
			return;
		}
	}

	if(pTarget)
	{
		character_ActivatePower(p, erGetRef(pTarget->entParent), loc, ppow);
	}
	else
	{
		character_ActivatePower(p, 0, loc, ppow);
	}
}

/**********************************************************************func*
 * character_ActivatePowerOnCharacter
 *
 * returns 0 if power wasn't activated, 1 if it was.
 */
int character_ActivatePowerOnCharacter(Character *p, Character *pTarget, Power *ppow)
{
	assert(p!=NULL);
	assert(ppow!=NULL);

	if(pTarget!=NULL)
	{
		if(ppow->ppowBase->bTargetNearGround)
		{
			Character *pGroundTarget = pTarget;

			// Do we try autoassist?  This early check is a bit more loose than a later one
			//  regarding assisting, but that should be fine.
			if(ppow->ppowBase->eTargetType != kTargetType_Location
				&& ppow->ppowBase->eTargetType != kTargetType_Teleport
				&& ppow->ppowBase->eTargetType != kTargetType_Position
				&& ppow->ppowBase->eTargetType != kTargetType_None
				&& pTarget->entParent != NULL
				&& character_IsInitialized(pTarget)
				&& !character_TargetMatchesType(p, pTarget, ppow->ppowBase->eTargetType, ppow->ppowBase->bTargetsThroughVisionPhase))
			{
				Entity *eAssistTarget = erGetEnt(pTarget->erTargetInstantaneous);
				if(eAssistTarget!=NULL
					&& character_TargetMatchesType(p, eAssistTarget->pchar, ppow->ppowBase->eTargetType, ppow->ppowBase->bTargetsThroughVisionPhase))
				{
					pGroundTarget = eAssistTarget->pchar;
				}
			}

			if(entHeight(pGroundTarget->entParent, 2.0f)>1.0f)
			{
				sendInfoBox(p->entParent, INFO_COMBAT_ERROR, "PowerTargetMustBeOnGround");
				serveFloatingInfo(p->entParent, p->entParent, "FloatTargetMustBeOnGround");

				// Notify AI.
				{
					AIEventNotifyParams params = {0};

					params.notifyType = AI_EVENT_NOTIFY_POWER_FAILED;
					params.source = p->entParent;
					params.target = pGroundTarget->entParent;
					params.powerFailed.ppow = ppow;
					params.powerFailed.targetNotOnGround = 1;

					aiEventNotify(&params);
				}

				return 0;
			}
		}

		return character_ActivatePower(p, erGetRef(pTarget->entParent), ENTPOS(pTarget->entParent), ppow);
	}
	else
	{
		return character_ActivatePower(p, 0, ENTPOS(p->entParent), ppow);
	}

}

/**********************************************************************func*
 * character_ActivatePower
 *
 * returns 0 if power wasn't activated, 1 if it was.
 */
int character_ActivatePower(Character *p, EntityRef erTarget, const Vec3 loc, Power *ppow)
{
	int returnMe = -1;
	Vec3 vecLocation;
	PowerListItem *ppowListItem;
	PowerListIter iter;

	assert(p!=NULL);
	assert(loc!=NULL);
	assert(ppow!=NULL);

	copyVec3(loc, vecLocation);

	p->stats.ppowLastAttempted = ppow; // for balancing

	if(!character_AllowedToActivatePower(p, ppow, true))
	{
		return 0;
	}
	
	if (p->ppowCurrent && p->ppowCurrent->chainsIntoPower &&
		p->ppowCurrent->chainsIntoPower != ppow && (ppow->ppowBase->eType != kPowerType_Auto) &&
		(ppow->ppowBase->eType != kPowerType_GlobalBoost) )
	{
		return 0;
	}

	if (ppow->ppowBase->eTargetType == kTargetType_Position
		|| ppow->ppowBase->eTargetTypeSecondary == kTargetType_Position)
	{
		int i;
		bool isTeleport = false;

		if (ppow->ppowBase->ePositionCenter == kModTarget_Caster)
		{
			copyVec3(ENTPOS(p->entParent), vecLocation);
		}
		else if (ppow->ppowBase->ePositionCenter == kModTarget_Focus)
		{
			// leave vecLocation alone
		}
		else
		{
			assertmsg(0, "Illegal position center value!");
		}

		if (ppow->ppowBase->fPositionDistance
			|| ppow->ppowBase->fPositionYaw)
		{
			Vec3 vecOrigDirection;
			Vec3 tempVec = {0,0,1};
			Mat3 tempMat;

			// determine base unit vector
			if (ppow->ppowBase->ePositionCenter == kModTarget_Caster)
			{
				mulVecMat3(tempVec, ENTMAT(p->entParent), vecOrigDirection);
			}
			else if (ppow->ppowBase->ePositionCenter == kModTarget_Focus)
			{
				subVec3(vecLocation, ENTPOS(p->entParent), vecOrigDirection);
			}
			vecOrigDirection[1] = 0.0f; // base unit vector is always horizontal
			normalVec3(vecOrigDirection);

			// create yaw matrix
			tempVec[0] = 0;
			tempVec[1] = ppow->ppowBase->fPositionYaw;
			tempVec[2] = 0;
			createMat3YPR(tempMat, tempVec);

			// apply yaw to unit vector
			copyVec3(vecOrigDirection, tempVec);
			mulVecMat3(tempVec, tempMat, vecOrigDirection);

			// apply distance to unit vector
			scaleByVec3(vecOrigDirection, ppow->ppowBase->fPositionDistance);

			// apply distance vector to location
			addToVec3(vecOrigDirection, vecLocation);
		}

		// apply height
		vecLocation[1] += ppow->ppowBase->fPositionHeight;

		// SUPER HACK to get the position to not collide with the ground.
		// ARM NOTE:  Someone who knows matrix/vector math should redo this?
		vecLocation[1] += 0.1f;

		// SUPER DUPER HACK to try find a valid above-ground spot if this
		// is a teleport power
		for( i = eaSize(&ppow->ppowBase->ppTemplateIdx)-1; i>=0; i-- )
		{
			if (TemplateHasAttrib(ppow->ppowBase->ppTemplateIdx[i], offsetof(CharacterAttributes, fTeleport)))
			{
				isTeleport = true;
				break;
			}
		}
		if (isTeleport) {
			float off, orig;
			orig = vecLocation[1];
			for (off = 0; off <= 3.0f; off += 0.25f) {
				vecLocation[1] = orig + off;
				if (IsTeleportTargetValid(vecLocation))
					break;
			}
		}
		// if a suitable spot is not found, it will evenutally fail in PowerQueue_Tick
	}

	// OK, on with the show
	switch(ppow->ppowBase->eType)
	{
		case kPowerType_Auto:
			for (ppowListItem = powlist_GetFirst(&p->listAutoPowers, &iter);
				ppowListItem != NULL;
				ppowListItem = powlist_GetNext(&iter))
			{
				if (ppowListItem->ppow == ppow)
				{
					// It's already in the Auto list, do nothing.
					returnMe = 0;
					break;
				}
			}
			if (ppowListItem == NULL)
			{
				// This auto power isn't in the list yet. Get it there.
				// AI_LOG(AI_LOG_POWERS, (p->entParent, "Activate: Adding auto power to list (%s).\n", ppow->ppowBase->pchName));
				powlist_Add(&p->listAutoPowers, ppow, nextInvokeId());
				if (ppow->ppowBase->bShowBuffIcon)
					p->entParent->team_buff_update = true;

				// This auto power just got activated when it wasn't before,
				character_ApplyPowerAtActivation(p, ppow, erTarget, vecLocation, 0);
			}
			returnMe = 1;
			break;

		case kPowerType_Toggle:
			// Remove it from the list if it is there
			for (ppowListItem = powlist_GetFirst(&p->listTogglePowers, &iter);
				ppowListItem != NULL;
				ppowListItem = powlist_GetNext(&iter))
			{
				if (ppowListItem->ppow == ppow)
				{
					if (!p->ppowCurrent || !basepower_HasAttrib(ppow->ppowBase, kSpecialAttrib_SetCostume))
					{
						character_ShutOffTogglePower(p, ppow);

						// AI_LOG(AI_LOG_POWERS, (p->entParent, "Activate: Removing toggle power from list (%s).\n", ppow->ppowBase->pchName));
						sendInfoBox(p->entParent, INFO_COMBAT_SPAM, "ShuttingOffToggle", ppow->ppowBase->pchDisplayName);

						powlist_RemoveCurrent(&iter);
					}

					returnMe = 1;
					break;
				}
			}
			if (ppowListItem == NULL)
			{
				// Didn't find it in the list, add to the queue.

				// If the toggle power is being charged up, cancel it.
				if(p->ppowCurrent == ppow)
				{
					// AI_LOG(AI_LOG_POWERS, (p->entParent, "Activate: Cancelling toggle power (%s).\n", ppow->ppowBase->pchName));
					sendInfoBox(p->entParent, INFO_COMBAT_SPAM, "ShuttingOffToggle", ppow->ppowBase->pchDisplayName);
					character_KillCurrentPower(p);

					returnMe = 0;
					break;
				}

				if(p->ppowQueued == NULL)
				{
					// AI_LOG(AI_LOG_POWERS, (p->entParent, "Activate: Adding toggle power to queue (%s).\n", ppow->ppowBase->pchName));
					if(p->ppowCurrent != NULL)
					{
						sendInfoBox(p->entParent, INFO_COMBAT_SPAM, "PowerQueueing", ppow->ppowBase->pchDisplayName);
					}
				}
				else
				{
					// AI_LOG(AI_LOG_POWERS, (p->entParent, "Activate: There is already a power (%s) queued.\n", p->ppowQueued->ppowBase->pchName));
					sendInfoBox(p->entParent, INFO_COMBAT_SPAM, "PowerReplacingQueue", ppow->ppowBase->pchDisplayName, p->ppowQueued->ppowBase->pchDisplayName);
				}

				character_KillQueuedPower(p);
				character_KillRetryPower(p);
				MissionItemInterruptInteraction(p->entParent);
				RandomFameMoved(p->entParent);

				p->ppowQueued = ppow;
				character_assignChainedPowers(p, ppow);

				p->erTargetQueued = erTarget;
				copyVec3(vecLocation, p->vecLocationQueued);
				power_ChangeActiveStatus(ppow, ppow->bActive|ACTIVE_STATUS_QUEUE_CHANGED);

				if(p->ppowCurrent!=NULL)
				{
					LOG_ENT( p->entParent, LOG_POWERS, LOG_LEVEL_VERBOSE, 0, "Power:Queue %s targeting %s %s ", dbg_BasePowerStr(ppow->ppowBase), dbg_NameStr(erGetEnt(erTarget)), dbg_LocationStr(vecLocation));
				}
			}
			returnMe = 1;
			break;

		case kPowerType_Click:
			// OK, is there space in the queue?
			if(p->ppowQueued == NULL)
			{
				// OK, stuff it in the queue
				// AI_LOG(AI_LOG_POWERS, (p->entParent, "Activate: Adding click power to queue (%s).\n", ppow->ppowBase->pchName));
				if(p->ppowCurrent != NULL)
				{
					sendInfoBox(p->entParent, INFO_COMBAT_SPAM, "PowerQueueing", ppow->ppowBase->pchDisplayName);
					LOG_ENT( p->entParent, LOG_POWERS, LOG_LEVEL_VERBOSE, 0, "Power:Queue %s targeting %s %s ", dbg_BasePowerStr(ppow->ppowBase), dbg_NameStr(erGetEnt(erTarget)), dbg_LocationStr(vecLocation));
				}
			}
			else
			{
				// AI_LOG(AI_LOG_POWERS, (p->entParent, "Activate: There is already a power (%s) queued.\n", p->ppowQueued->ppowBase->pchName));
				if (ppow->ppowBase != p->ppowQueued->ppowBase)
				{
					sendInfoBox(p->entParent, INFO_COMBAT_SPAM, "PowerReplacingQueue", ppow->ppowBase->pchDisplayName, p->ppowQueued->ppowBase->pchDisplayName);
					LOG_ENT( p->entParent, LOG_POWERS, LOG_LEVEL_VERBOSE, 0, " Power:Queue %s targeting %s %s ", dbg_BasePowerStr(ppow->ppowBase), dbg_NameStr(erGetEnt(erTarget)), dbg_LocationStr(vecLocation));
				}
			}

			character_KillQueuedPower(p);
			character_KillRetryPower(p);
			MissionItemInterruptInteraction(p->entParent);
			RandomFameMoved(p->entParent);

			p->ppowQueued = ppow;
			character_assignChainedPowers(p, ppow);

			p->erTargetQueued = erTarget;
			copyVec3(vecLocation, p->vecLocationQueued);
			power_ChangeActiveStatus(ppow, ppow->bActive|ACTIVE_STATUS_QUEUE_CHANGED);

			returnMe = 1;
			break;

		case kPowerType_GlobalBoost:
		{
			for(ppowListItem = powlist_GetFirst(&p->listGlobalBoosts, &iter);
				ppowListItem != NULL;
				ppowListItem = powlist_GetNext(&iter))
			{
				if (ppowListItem->ppow == ppow)
				{
					// It's already in the Global Boost list, do nothing.
					returnMe = 0;
					break;
				}
			}
			if (ppowListItem == NULL)
			{
				// Add Global Boost to all powers
				int powerSetCount = eaSize(&p->ppPowerSets);
				int powerSetIndex;
				for (powerSetIndex = 0; powerSetIndex < powerSetCount; powerSetIndex++)
				{
					PowerSet *powerSet = p->ppPowerSets[powerSetIndex];
					int powerCount = eaSize(&powerSet->ppPowers);
					int powerIndex;
					for (powerIndex = 0; powerIndex < powerCount; powerIndex++)
					{
						Power *power = powerSet->ppPowers[powerIndex];
						const BasePower *basePower;
						if (!power)
						{
							continue;
						}

						basePower = power->ppowBase;
						if (basePower->eType != kPowerType_Boost && basePower->eType != kPowerType_GlobalBoost) // if power isn't a boost (recursion isn't allowed)
						{
							Boost *globalBoost = boost_Create(ppow->ppowBase, 0, power, character_CalcExperienceLevel(p), 0, NULL, 0, NULL, 0);
							if (power_IsValidBoost(power, globalBoost))
							{
								int globalBoostCount = eaSize(&power->ppGlobalBoosts);
								int globalBoostIndex;
								for (globalBoostIndex = 0; globalBoostIndex < globalBoostCount; globalBoostIndex++)
								{
									if (power->ppGlobalBoosts[globalBoostIndex]->ppowBase == globalBoost->ppowBase)
									{
										break;
									}
								}
								if (globalBoostIndex == globalBoostCount) // not in list
								{
									eaPush(&power->ppGlobalBoosts, globalBoost);
									power->bBoostsCalcuated = false;
									eaPush(&p->entParent->powerDefChange, powerRef_CreateFromPower(p->iCurBuild, power));

								}
								else
								{
									boost_Destroy(globalBoost);
								}
							}
							else
							{
								boost_Destroy(globalBoost);
							}
						}
					}
				}

				// This global boost isn't in the list yet. Get it there.
				powlist_Add(&p->listGlobalBoosts, ppow, nextInvokeId());
			}

			break;
		}
	}

	p->stats.iCntUsed++; // for balancing
	ppow->stats.iCntUsed++; // for balancing
	if (isDevelopmentMode() && !isSharedMemory(ppow->ppowBase))
		(cpp_const_cast(BasePower*)(ppow->ppowBase))->stats.iCntUsed++; // for balancing

	return returnMe;
}

/**********************************************************************func*
 * character_ActivateAllAutoPowers
 *
 * Also activates all Global Boosts.
 */
void character_ActivateAllAutoPowers(Character *p)
{
	int iset;

	assert(p!=NULL);

	// This function assumes that ActivatePower won't activate the same power
	// more than once.

	for(iset=eaSize(&p->ppPowerSets)-1; iset>=0; iset--)
	{
		int ipow;
		for(ipow=eaSize(&p->ppPowerSets[iset]->ppPowers)-1; ipow>=0; ipow--)
		{
			Power *ppow = p->ppPowerSets[iset]->ppPowers[ipow];
			if(ppow
				&& (ppow->ppowBase->eType == kPowerType_Auto || ppow->ppowBase->eType == kPowerType_GlobalBoost)
				&& character_AllowedToActivatePower(p, ppow, false))
			{
				character_EnterStance(p, ppow, false);
				character_ActivatePowerOnCharacter(p, p, ppow);
				if (ppow->ppowBase->bShowBuffIcon)
					p->entParent->team_buff_update = true;
			}
		}
	}
}

/**********************************************************************func*
 * character_PowerAlwaysHits
 *
 */
bool character_PowerAlwaysHits(Character *pSrc, Character *pTarget, const Power *ppow)
{
	assert(pSrc!=NULL);
	assert(pTarget!=NULL);
	assert(ppow!=NULL);

	return pTarget!=NULL
		&& (pTarget->entParent->alwaysGetHit
			|| pSrc->entParent->alwaysHit
			|| pSrc->entParent->cutSceneActor
			|| MatchesTypeList(pSrc, pTarget, ppow->ppowBase->pAutoHit, ppow->ppowBase->bTargetsThroughVisionPhase));
}

/**********************************************************************func*
 * character_KillCurrentPower
 *
 */
void character_KillCurrentPower(Character *p)
{
	if(p->ppowCurrent!=NULL)
	{
		if(p->bCurrentPowerActivated)
		{
			if(p->ppowCurrent->ppowBase->eType == kPowerType_Toggle)
			{
				character_ShutOffContinuingPower(p, p->ppowCurrent);
			}
			else
			{
				// Put this in the recharge list (with leftover activation time)
				power_ChangeActiveStatus(p->ppowCurrent, false);
				power_ChangeRechargeTimer(p->ppowCurrent, p->ppowCurrent->fTimer + p->ppowCurrent->ppowBase->fRechargeTime);
				powlist_Add(&p->listRechargingPowers, p->ppowCurrent,0);
				p->ppowCurrent = NULL;
				return;
			}
		}
		else if(p->ppowCurrent->ppowBase->eType == kPowerType_Inspiration)
		{
			// Put this in the recharge list to get it freed later on.
			power_ChangeRechargeTimer(p->ppowCurrent, p->ppowCurrent->ppowBase->fRechargeTime);
			powlist_Add(&p->listRechargingPowers, p->ppowCurrent,0);
		}

		power_ChangeActiveStatus(p->ppowCurrent, false);

		p->ppowCurrent->fTimer = 0.0f;
		p->ppowCurrent = NULL;

	}
}

/**********************************************************************func*
 * character_KillQueuedPower
 *
 */
void character_KillQueuedPower(Character *p)
{
	if(p->ppowQueued!=NULL)
	{
		character_ReleasePowerChain(p, p->ppowQueued);
		if(p->ppowQueued->ppowBase->eType == kPowerType_Inspiration)
		{
			Entity * pent = p->entParent;
			if( ENTTYPE(pent)==ENTTYPE_CRITTER && pent->erCreator ) // pet trying to use inspiration
			{
				pent = erGetEnt(pent->erCreator);
				if (pent) // JP: pent==NULL if creator state is ENTITY_BEING_FREED
				{
					eaiPush(&pent->inspirationStatusChange, p->ppowQueued->idx);
					character_CompactInspirations(pent->pchar);
				}
			}
			else
			{
				eaiPush(&pent->inspirationStatusChange, p->ppowQueued->idx);
			}

			if(p->ppowTried!=p->ppowQueued)
				inspiration_Destroy(p->ppowQueued);
		}
		else
			power_ChangeActiveStatus(p->ppowQueued, p->ppowQueued->bActive|ACTIVE_STATUS_QUEUE_CHANGED); // Will send down queue info

		p->ppowQueued = NULL;
	}
}

/**********************************************************************func*
 * character_KillRetryPower
 *
 */
void character_KillRetryPower(Character *p)
{
	if(p->ppowTried!=NULL)
	{
		if(p->ppowTried->ppowBase->eType == kPowerType_Inspiration)
		{
			eaiPush(&p->entParent->inspirationStatusChange, p->ppowTried->idx);
			if(p->ppowTried!=p->ppowQueued)
			{
				inspiration_Destroy(p->ppowTried);
			}
		}
		else
		{
			power_ChangeActiveStatus(p->ppowTried, p->ppowTried->bActive|ACTIVE_STATUS_QUEUE_CHANGED); // Will send down queue info
		}

		p->ppowTried = NULL;
	}
}

/*****************************************************
 *  character_ReleasePowerChain
 *	Runs the length of the a power chaining into another power
 *	clears the power chain
 */
void character_ReleasePowerChain(Character *p, Power*ppowCurrentLink)
{
	assert(p);
	assert(ppowCurrentLink);
	while (ppowCurrentLink->chainsIntoPower)
	{
		//	free the length of the chain
		Power *ppowNextChainPower = ppowCurrentLink->chainsIntoPower;
		ppowCurrentLink->chainsIntoPower = NULL;	//	free the chaining power
		ppowCurrentLink = ppowNextChainPower;
	}
}
/**********************************************************************func*
 * character_InterruptPower
 *
 */
void character_InterruptPower(Character *p, Character *pSrc, float fDelay, bool bAwaken)
{
	assert(p!=NULL);

	// Can the supposed interrupting entity interrupt this character?
	if(pSrc==NULL // used for movement and world interruption
		|| (pSrc!=p && character_TargetIsFoe(p,pSrc)))  // They aren't myself and they're on the other team
	{
		// Also abort object interaction.
		MissionItemInterruptInteraction(p->entParent);
		RandomFameMoved(p->entParent);
		if(p->entParent && p->entParent->pl && p->entParent->pl->usingTeleportPower)
		{
			START_PACKET(pak, p->entParent, SERVER_MAP_XFER_LIST_CLOSE);
			END_PACKET
			p->entParent->pl->usingTeleportPower = false;
		}

		if(p->ppowCurrent!=NULL)
		{
			// The power is interrupted!
			if((pSrc==NULL // moves and clickies
				|| bAwaken // strong interrupts (damage/knockback)
				|| !p->ppowCurrent->ppowBase->bInterruptLikeSleep) // easily-interrupted powers
					&& (p->ppowCurrent->fTimer+fDelay) <
					   (p->ppowCurrent->ppowBase->fInterruptTime/(p->ppowCurrent->pattrStrength->fInterruptTime+0.0001f)))
			{
				// Shut off the WindUp FX
				character_KillMaintainedFX(p, p, power_GetFX(p->ppowCurrent)->pchWindUpFX);

				//	free up the build up power
				if (p->ppowCurrent->chainsIntoPower)
				{
					character_ReleasePowerChain(p, p->ppowCurrent);
					p->ppowQueued = NULL;	//	kill the chain into power
				}

				// TODO: This message is potentially a little early.
				sendInfoBox(p->entParent, INFO_COMBAT_ERROR, "PowerInterrupted", p->ppowCurrent->ppowBase->pchDisplayName);
				serveFloatingInfo(p->entParent, p->entParent, "FloatInterrupted");
				AI_LOG(AI_LOG_POWERS, (p->entParent, "IntPower: Power Interrupted (%s).\n", p->ppowCurrent->ppowBase->pchName));

				LOG_ENT( pSrc->entParent, LOG_POWERS, LOG_LEVEL_VERBOSE, 0, "Power:Interrupted %s", dbg_BasePowerStr(p->ppowCurrent->ppowBase));

				character_KillCurrentPower(p);
			}
		}
	}
}

void character_InterruptMissionTeleport(Character *pchar)
{
	int i;
	const BasePower *bPower = pchar && pchar->ppowCurrent && pchar->ppowCurrent->ppowBase ?
		pchar->ppowCurrent->ppowBase : NULL;
	if (bPower)
	{
		for (i = 0; i < eaSize(&bPower->ppTemplateIdx); ++i)
		{
			const AttribModTemplate *ptemplate = bPower->ppTemplateIdx[i];
			size_t offAspect = ptemplate->offAspect; // common subexpression

			//	Am I teleporting?
			if(TemplateHasAttrib(ptemplate, offsetof(CharacterAttributes, fTeleport))
				&& (offAspect==offsetof(CharacterAttribSet, pattrMod)
				|| offAspect==offsetof(CharacterAttribSet, pattrAbsolute)))
			{
				if (TemplateGetParam(ptemplate, Teleport, pchDestination) && (stricmp(TemplateGetParam(ptemplate, Teleport, pchDestination), "mission") == 0))
				{
					character_InterruptPower(pchar, NULL, 0, 0);
					return;
				}
			}
		}
	}
}

//	Check if this has any OnActivateMods, and grab the invokeID from it
int character_GetOnactivateInvokeID(Character *p, const BasePower *bpow)
{
	int i;
	int invokeID = 0;
	for(i=eaSize(&bpow->ppTemplateIdx)-1;i>=0;i--)
	{
		const AttribModTemplate *modTemplate = bpow->ppTemplateIdx[i];

		if (modTemplate->eApplicationType == kModApplicationType_OnActivate
			&& (modTemplate->eTarget == kModTarget_Caster
			|| modTemplate->eTarget == kModTarget_CastersOwnerAndAllPets)) 
		{
			AttribModListIter iter;
			AttribMod *pmod = modlist_GetFirstMod(&iter, &p->listMod);
			while(pmod!=NULL)
			{
				if(pmod->ptemplate == modTemplate)
				{
					invokeID = pmod->uiInvokeID;
					break;
				}
				pmod = modlist_GetNextMod(&iter);
			}
		}
	}
	return invokeID;
}

/**********************************************************************func*
 * character_TurnOnActiveTogglePowers
 *
 */
void character_TurnOnActiveTogglePowers(Character *p)
{
	int iset;

	assert(p!=NULL);

	for(iset=eaSize(&p->ppPowerSets)-1; iset>=0; iset--)
	{
		int ipow;
		for(ipow=eaSize(&p->ppPowerSets[iset]->ppPowers)-1; ipow>=0; ipow--)
		{
			Power *ppow = p->ppPowerSets[iset]->ppPowers[ipow];
			if(ppow && ppow->bActive
				&& ppow->ppowBase->eType == kPowerType_Toggle)
			{
				PowerListItem *ppowListItem;
				PowerListIter iter;
				bool donotadd = false;
				int invokeID;

				// Make sure this toggle isn't already in the active toggle list.
				// Needed because of an otherwise unavoidable conflict between
				// character_Reset and resumeCharacter, which both want to be the
				// one to turn on toggles when resetting powers after being offline.
				for (ppowListItem = powlist_GetFirst(&p->listTogglePowers, &iter);
					ppowListItem != NULL;
					ppowListItem = powlist_GetNext(&iter))
				{
					// If it's already in the list, don't activate it again
					if (ppowListItem->ppow->ppowBase == ppow->ppowBase) {
						donotadd = true;
						break;
					}
				}

				invokeID = character_GetOnactivateInvokeID(p, ppow->ppowBase);

				character_EnterStance(p, ppow, false);

				// The power is already marked as active when it is load out of the database
				// Turn it off, then back on to force the mutator to acknowledge the status change.
				power_ChangeActiveStatus(ppow, false);
				power_ChangeActiveStatus(ppow, true);

				if (!donotadd)
					powlist_Add(&p->listTogglePowers, ppow, invokeID ? invokeID : nextInvokeId());
				if (ppow->ppowBase->bShowBuffIcon)
					p->entParent->team_buff_update = true;
				ppow->fTimer = 0.0f;
			}
		}
	}
}

/**********************************************************************func*
 * character_ShutOffAllAutoPowers
 *
 * also shuts off all global boosts (mirroring character_ActivateAllAutoPowers)
 */
void character_ShutOffAllAutoPowers(Character *p)
{
	PowerListItem *ppowListItem;
	PowerListIter iter;

	for (ppowListItem = powlist_GetFirst(&p->listAutoPowers, &iter);
		ppowListItem != NULL;
		ppowListItem = powlist_GetNext(&iter))
	{
		character_ShutOffContinuingPower(p, ppowListItem->ppow);
		powlist_RemoveCurrent(&iter);
	}

	for (ppowListItem = powlist_GetFirst(&p->listGlobalBoosts, &iter);
		ppowListItem != NULL;
		ppowListItem = powlist_GetNext(&iter))
	{
		character_ShutOffGlobalBoost(p, ppowListItem->ppow);
		powlist_RemoveCurrent(&iter);
	}
}

/**********************************************************************func*
 * character_ShutOffAllTogglePowers
 *
 */
void character_ShutOffAllTogglePowers(Character *p)
{
	PowerListItem *ppowListItem;
	PowerListIter iter;

	// Loop over all the Toggle powers and get them processed
	for (ppowListItem = powlist_GetFirst(&p->listTogglePowers, &iter);
		ppowListItem != NULL;
		ppowListItem = powlist_GetNext(&iter))
	{
		character_ShutOffTogglePower(p, ppowListItem->ppow);
		powlist_RemoveCurrent(&iter);
	}
}

/**********************************************************************func*
 * character_ShutOffTogglePowers_Hold
 *
 */
void character_ShutOffTogglePowers_Hold(Character *p)
{
	PowerListItem *ppowListItem;
	PowerListIter iter;

	// Loop over all the Toggle powers and get them processed
	for (ppowListItem = powlist_GetFirst(&p->listTogglePowers, &iter);
		ppowListItem != NULL;
		ppowListItem = powlist_GetNext(&iter))
	{
		if(ppowListItem->ppow && ppowListItem->ppow->ppowBase && ppowListItem->ppow->ppowBase->bToggleIgnoreHold)
			continue;

		character_ShutOffTogglePower(p, ppowListItem->ppow);
		powlist_RemoveCurrent(&iter);
	}
}

/**********************************************************************func*
 * character_ShutOffTogglePowers_Sleep
 *
 */
void character_ShutOffTogglePowers_Sleep(Character *p)
{
	PowerListItem *ppowListItem;
	PowerListIter iter;

	// Loop over all the Toggle powers and get them processed
	for (ppowListItem = powlist_GetFirst(&p->listTogglePowers, &iter);
		ppowListItem != NULL;
		ppowListItem = powlist_GetNext(&iter))
	{
		if(ppowListItem->ppow && ppowListItem->ppow->ppowBase && ppowListItem->ppow->ppowBase->bToggleIgnoreSleep)
			continue;

		character_ShutOffTogglePower(p, ppowListItem->ppow);
		powlist_RemoveCurrent(&iter);
	}
}

/**********************************************************************func*
 * character_ShutOffTogglePowers_Stun
 *
 */
void character_ShutOffTogglePowers_Stun(Character *p)
{
	PowerListItem *ppowListItem;
	PowerListIter iter;

	// Loop over all the Toggle powers and get them processed
	for (ppowListItem = powlist_GetFirst(&p->listTogglePowers, &iter);
		ppowListItem != NULL;
		ppowListItem = powlist_GetNext(&iter))
	{
		if (ppowListItem->ppow && ppowListItem->ppow->ppowBase && ppowListItem->ppow->ppowBase->bToggleIgnoreStun)
			continue;

		character_ShutOffTogglePower(p, ppowListItem->ppow);
		powlist_RemoveCurrent(&iter);
	}
}

/**********************************************************************func*
 * character_ShutOffAllTogglePowersInSameGroup
 *
 */
void character_ShutOffAllTogglePowersInSameGroup(Character *p, const BasePower *ppowBase)
{
	PowerListItem *ppowListItem;
	PowerListIter iter;

	// Loop over all the Toggle powers
	for (ppowListItem = powlist_GetFirst(&p->listTogglePowers, &iter);
		ppowListItem != NULL;
		ppowListItem = powlist_GetNext(&iter))
	{
		int i;
		int bRemoved = false;
		const BasePower *ppowBaseCheck = ppowListItem->ppow->ppowBase;

		// TODO: This could be done more efficiently, but the lists are
		//       usually short or empty.
		for(i=eaiSize(&ppowBase->pGroupMembership)-1; !bRemoved && i>=0; i--)
		{
			int j;
			for(j=eaiSize(&ppowBaseCheck->pGroupMembership)-1; !bRemoved && j>=0; j--)
			{
				if(ppowBase->pGroupMembership[i]==ppowBaseCheck->pGroupMembership[j])
				{
					sendInfoBox(p->entParent, INFO_COMBAT_SPAM, "ShuttingOffToggle", ppowBaseCheck->pchDisplayName);
					character_ShutOffTogglePower(p, ppowListItem->ppow);

					powlist_RemoveCurrent(&iter);
					bRemoved = true;
				}
			}
		}
	}
}

static void character_CancelMods(Character *p, Power *ppow, int petInherit)
{
	int i;
	AttribMod *pmod;
	AttribModListIter iter;

	// Go through and nuke the modifiers.
	pmod = modlist_GetFirstMod(&iter, &p->listMod);
	while(pmod!=NULL)
	{

		if(pmod->parentPowerInstance == ppow
			&& pmod->ptemplate
			&& pmod->ptemplate->eApplicationType != kModApplicationType_OnEnable
			&& pmod->ptemplate->eApplicationType != kModApplicationType_OnDisable)
		{
			if (pmod->ptemplate->ppowBase && pmod->ptemplate->ppowBase->bShowBuffIcon)
				p->entParent->team_buff_update = true;

			mod_CancelFX(pmod, p);
			mod_Cancel(pmod, p);

			// Mark this attribmod to be removed next time through and don't
			//   let it get turned on again.
			pmod->fDuration = 0;
			pmod->fTimer = 100; // arbitrary, must be bigger than the likely real tick time.
		}
		pmod = modlist_GetNextMod(&iter);
	}

	if (petInherit)
	{
		for(i = 0; i < p->entParent->petList_size; i++)
		{
			Entity *pet = erGetEnt(p->entParent->petList[i]);
			if (pet && !pet->petByScript)
			{
				Character *pNewTarget = pet->pchar;
				character_CancelMods(pNewTarget, ppow, true);
			}
		}
	}
}

void character_CancelModsByBasePower(Character *p, const BasePower *ppowBase)
{
	int i, modIndex;
	AttribMod *pmod;
	AttribModListIter iter;
	bool petInherit = false;

	pmod = modlist_GetFirstMod(&iter, &p->listMod);
	while(pmod!=NULL)
	{
		if(pmod->ptemplate)
		{
			if (pmod->ptemplate->ppowBase == ppowBase)
			{
				if (ppowBase->bShowBuffIcon)
					p->entParent->team_buff_update = true;

				pmod->fDuration = 0;
			}
		}
		pmod = modlist_GetNextMod(&iter);
	}

	for (modIndex = eaSize(&ppowBase->ppTemplateIdx) - 1; modIndex >= 0; modIndex--)
	{
		if (ppowBase->ppTemplateIdx[modIndex]->eTarget == kModTarget_CastersOwnerAndAllPets
			|| ppowBase->ppTemplateIdx[modIndex]->eTarget == kModTarget_FocusOwnerAndAllPets
			|| ppowBase->ppTemplateIdx[modIndex]->eTarget == kModTarget_AffectedsOwnerAndAllPets)
		{
			petInherit = true;
			break;
		}
	}

	if (petInherit)
	{
		for(i = 0; i < p->entParent->petList_size; i++)
		{
			Entity *pet = erGetEnt(p->entParent->petList[i]);
			if (pet && !pet->petByScript)
			{
				Character *pNewTarget = pet->pchar;
				character_CancelModsByBasePower(pNewTarget, ppowBase);
			}
		}
	}
}

void character_CancelModsFiltered(Character *p, const AttribModParam_EffectFilter *pfilter)
{
	int i;
	AttribMod *pmod;
	AttribModListIter iter;
	bool petInherit = false;

	pmod = modlist_GetFirstMod(&iter, &p->listMod);
	while(pmod!=NULL)
	{
		if(pmod->ptemplate)
		{
			if (effectfilter_Test(pmod->ptemplate->peffParent, pfilter))
			{
				if (pmod->ptemplate->ppowBase->bShowBuffIcon)
					p->entParent->team_buff_update = true;

				pmod->fDuration = 0;

				if (pmod->ptemplate->eTarget == kModTarget_CastersOwnerAndAllPets
					|| pmod->ptemplate->eTarget == kModTarget_FocusOwnerAndAllPets
					|| pmod->ptemplate->eTarget == kModTarget_AffectedsOwnerAndAllPets)
					petInherit = true;
			}
		}
		pmod = modlist_GetNextMod(&iter);
	}

	if (petInherit)
	{
		for(i = 0; i < p->entParent->petList_size; i++)
		{
			Entity *pet = erGetEnt(p->entParent->petList[i]);
			if (pet && !pet->petByScript)
			{
				character_CancelModsFiltered(pet->pchar, pfilter);
			}
		}
	}
}

/**********************************************************************func*
 * character_ShutOffContinuingPower
 *
 */
void character_ShutOffContinuingPower(Character *p, Power *ppow)
{
	EntityRef er = erGetRef(p->entParent);
	const PowerFX* powerFX = power_GetFX(ppow);
	int modIndex, petInherit = false;
	Character *pTopLevelTarget = p;

	character_KillMaintainedFX(p, p, powerFX->pchActivationFX);
	// Set off deactivation FX
	character_PlayFX(p, NULL, p, powerFX->pchDeactivationFX,
		power_GetTint(ppow), 0, 0, 
		PLAYFX_DONTPITCHTOTARGET | PLAYFX_TINT_FLAG(ppow));
	character_SetAnimBits(p, powerFX->piDeactivationBits, 0.0f, PLAYFX_NOT_ATTACHED);

	// Clear out any boost timers so they go off the next time the power is
	// used.
	power_ClearBoostTimers(ppow);

	for (modIndex = eaSize(&ppow->ppowBase->ppTemplateIdx) - 1; modIndex >= 0; modIndex--)
	{
		if (ppow->ppowBase->ppTemplateIdx[modIndex]->eTarget == kModTarget_CastersOwnerAndAllPets
			|| ppow->ppowBase->ppTemplateIdx[modIndex]->eTarget == kModTarget_FocusOwnerAndAllPets
			|| ppow->ppowBase->ppTemplateIdx[modIndex]->eTarget == kModTarget_AffectedsOwnerAndAllPets)
		{
			petInherit = true;
			break;
		}
	}

	if (petInherit)
	{
		Entity *temp;

		if (pTopLevelTarget->entParent->erOwner &&
			(temp = erGetEnt(pTopLevelTarget->entParent->erOwner)))
		{
			pTopLevelTarget = temp->pchar;
		}
	}

	character_CancelMods(pTopLevelTarget, ppow, petInherit);

	character_ApplyPowerAtDeactivation(p, ppow, er, 0);
}

void character_ShutOffAndRemoveAutoPower(Character *p, Power *ppow)
{
	PowerListItem *ppowListItem;
	PowerListIter iter;

	for (ppowListItem = powlist_GetFirst(&p->listAutoPowers, &iter);
		ppowListItem != NULL;
		ppowListItem = powlist_GetNext(&iter))
	{
		if (ppow == ppowListItem->ppow)
		{
			character_ShutOffContinuingPower(p, ppow);
			powlist_RemoveCurrent(&iter);
		}
	}
}

/**********************************************************************func*
 * character_ShutOffTogglePower
 *
 */
void character_ShutOffTogglePower(Character *p, Power *ppow)
{
	EntityRef er = erGetRef(p->entParent);

	power_ChangeActiveStatus(ppow, false);
	power_ChangeRechargeTimer(ppow, ppow->ppowBase->fRechargeTime);
	powlist_Add(&p->listRechargingPowers, ppow,0);

	if(p->ppowStance==ppow)
	{
		character_EnterStance(p, NULL, 1);
		p->ppowStance=NULL;
	}

	LOG_ENT( p->entParent, LOG_POWERS, LOG_LEVEL_IMPORTANT, 0, "Power:ToggleOff %s", dbg_BasePowerStr(ppow->ppowBase));


	character_ShutOffContinuingPower(p, ppow);
}

/**********************************************************************func*
 * character_ShutOffAndRemoveTogglePower
 *
 */
void character_ShutOffAndRemoveTogglePower(Character *p, Power *ppow)
{
	PowerListItem *ppowListItem;
	PowerListIter iter;

	for (ppowListItem = powlist_GetFirst(&p->listTogglePowers, &iter);
		ppowListItem != NULL;
		ppowListItem = powlist_GetNext(&iter))
	{
		if(ppow == ppowListItem->ppow)
		{
			powlist_RemoveCurrent(&iter);
			character_ShutOffTogglePower(p, ppow);
		}
	}
}

void character_ShutOffGlobalBoost(Character *p, Power *ppow)
{
	// don't call character_ShutOffContinuingPower(), that's for autos and toggles.
	// it cancels the effects of character_ApplyPower(), which doesn't get called for global boosts.

	int powerSetCount = eaSize(&p->ppPowerSets);
	int powerSetIndex;
	for (powerSetIndex = 0; powerSetIndex < powerSetCount; powerSetIndex++)
	{
		PowerSet *powerSet = p->ppPowerSets[powerSetIndex];
		int powerCount = eaSize(&powerSet->ppPowers);
		int powerIndex;
		for (powerIndex = 0; powerIndex < powerCount; powerIndex++)
		{
			Power *power = powerSet->ppPowers[powerIndex];
			if (power)
			{
				const BasePower *basePower = power->ppowBase;
				if (basePower->eType != kPowerType_Boost && basePower->eType != kPowerType_GlobalBoost) // if power isn't a boost (recursion isn't allowed)
				{
					int globalBoostCount = eaSize(&power->ppGlobalBoosts);
					int globalBoostIndex;
					for (globalBoostIndex = 0; globalBoostIndex < globalBoostCount; globalBoostIndex++)
					{
						if (power->ppGlobalBoosts[globalBoostIndex]->ppowBase == ppow->ppowBase)
						{
							eaRemoveAndDestroy(&power->ppGlobalBoosts, globalBoostIndex, boost_Destroy);
							power->bBoostsCalcuated = false;
							eaPush(&p->entParent->powerDefChange, powerRef_CreateFromPower(p->iCurBuild, power));
							break;
						}
					}
				}
			}
		}
	}
}

void character_ShutOffAndRemoveGlobalBoost(Character *p, Power *ppow)
{
	PowerListItem *ppowListItem;
	PowerListIter iter;

	for (ppowListItem = powlist_GetFirst(&p->listGlobalBoosts, &iter);
		ppowListItem != NULL;
		ppowListItem = powlist_GetNext(&iter))
	{
		if (ppow == ppowListItem->ppow)
		{
			powlist_RemoveCurrent(&iter);
			character_ShutOffGlobalBoost(p, ppow);
		}
	}
}

/**********************************************************************func*
 * character_RemoveAllMods
 *
 */
void character_RemoveAllMods(Character *p)
{
	AttribModListIter iter;
	AttribMod *pmod = modlist_GetFirstMod(&iter, &p->listMod);
	while(pmod!=NULL)
	{
		if (pmod->ptemplate && pmod->ptemplate->ppowBase && pmod->ptemplate->ppowBase->bShowBuffIcon)
			p->entParent->team_buff_update = true;
		mod_CancelFX(pmod, p);
		mod_Cancel(pmod, p);

		pmod = modlist_GetNextMod(&iter);
	}

	modlist_RemoveAll(&p->listMod);
}

/**********************************************************************func*
 * character_RemoveModsAtDeath
 *
 */
void character_RemoveModsAtDeath(Character *p)
{
	AttribModListIter iter;
	AttribMod *pmod = modlist_GetFirstMod(&iter, &p->listMod);
	while(pmod!=NULL)
	{
		if(!TemplateHasFlag(pmod->ptemplate, KeepThroughDeath))
		{
			if (pmod->ptemplate->ppowBase && pmod->ptemplate->ppowBase->bShowBuffIcon)
			{
				p->entParent->team_buff_update = true;
			}
			mod_CancelFX(pmod, p);
			mod_Cancel(pmod, p);
			modlist_RemoveAndDestroyCurrentMod(&iter);
		}

		pmod = modlist_GetNextMod(&iter);
	}
}

/**********************************************************************func*
 * character_AssistByName
 *
 */
void character_AssistByName(Character *p, char *pchName)
{
	int db_id = dbPlayerIdFromName(pchName);
	Entity *e = entFromDbId(db_id);

	if(e!=NULL)
	{
		p->erAssist = erGetRef(e);
		p->entParent->target_update = 1;
	}
}

/**********************************************************************func*
 * character_Unassist
 *
 */
void character_Unassist(Character *p)
{
	p->erAssist = 0;
	p->entParent->target_update = 1;
}

/**********************************************************************func*
 * character_ConfirmPower
 *
 * NOTE: If this is run before the casting character's animation is over,
 * the power will have an effect immediately.  Nonconfirmable powers cannot have an effect
 * until after the casting character's animation is completed.
 * This is due to the reset of fCheckTimer I added.
 */
bool character_ConfirmPower(Character *p, unsigned int ui, bool bAccept)
{
	if(ui==p->entParent->pl->uiDelayedInvokeID)
	{
		if(dbSecondsSince2000()<p->entParent->pl->timeDelayedExpire)
		{
			Entity *e = NULL;
			Character *pSrc;
			const BasePower *ppowBase = NULL;
			const PowerFX *fx = NULL;
			AttribModListIter iter;

			AttribMod *pmod = modlist_GetFirstMod(&iter, &p->listMod);
			while(pmod!=NULL)
			{
				if(pmod->uiInvokeID == ui)
				{
					e = erGetEnt(pmod->erSource);
					ppowBase = pmod->ptemplate->ppowBase;
					fx = pmod->fx;
					break;
				}
				pmod = modlist_GetNextMod(&iter);
			}

			pSrc = e ? e->pchar : NULL;

			if(bAccept)
			{
				ColorPair uiTint;
				AttribModListIter iter;

				// OK, The power is confirmed. Loop over the attribmods
				// and set their timers to the ones defined. This will make them
				// go off more-or-less right away.
				AttribMod *pmod = modlist_GetFirstMod(&iter, &p->listMod);

				uiTint.primary.integer = 0;
				uiTint.secondary.integer = 0;

				while(pmod!=NULL)
				{
					if(pmod->uiInvokeID == ui)
					{
						pmod->fTimer = pmod->ptemplate->fDelay;
						pmod->fDuration = pmod->ptemplate->fDuration+pmod->ptemplate->fDelay;
						pmod->fCheckTimer = pmod->ptemplate->fDelay;
						uiTint = pmod->uiTint;
					}
					pmod = modlist_GetNextMod(&iter);
				}

				if(ppowBase!=NULL)
				{
					int iFXFlags = PLAYFX_NORMAL;

					if(ppowBase->eEffectArea!=kEffectArea_Character
						&& ppowBase->eEffectArea!=kEffectArea_Location
						&& ppowBase->eEffectArea!=kEffectArea_Touch)
					{
						iFXFlags = PLAYFX_FROM_AOE;
					}

					if(fx->bImportant)
						iFXFlags |= PLAYFX_IMPORTANT;

					character_PlayFX(p, NULL, pSrc, fx->pchHitFX, uiTint, 0.0f, PLAYFX_NOT_ATTACHED, iFXFlags|PLAYFX_DONTPITCHTOTARGET);
					character_SetAnimBits(p, fx->piBlockBits, 0.0f, PLAYFX_NOT_ATTACHED);
					character_SetAnimBits(p, fx->piHitBits, 0.0f, PLAYFX_NOT_ATTACHED);
				}

				if(pSrc!=NULL && ppowBase!=NULL)
				{
					sendInfoBox(pSrc->entParent, INFO_COMBAT_SPAM, "PowerConfirmed", p->entParent->name, ppowBase->pchDisplayName);
				}
			}
			else
			{
				// OK, The power is declined. Loop over the attribmods
				// and set them to time out.
				AttribModListIter iter;
				AttribMod *pmod = modlist_GetFirstMod(&iter, &p->listMod);
				while(pmod!=NULL)
				{
					if(pmod->uiInvokeID == ui)
					{
						pmod->fTimer = 10;   // make sure they don't go off.
						pmod->fDuration = 0; // get them to time out.
					}
					pmod = modlist_GetNextMod(&iter);
				}

				if(pSrc!=NULL && ppowBase!=NULL)
				{
					sendInfoBox(pSrc->entParent, INFO_COMBAT_SPAM, "PowerNotConfirmed", p->entParent->name, ppowBase->pchDisplayName);
				}
			}

			p->entParent->pl->uiDelayedInvokeID = 0;
		}
	}

	return true;
}


/**********************************************************************func*
 * character_NeedPowerConfirm
 *
 */
bool character_NeedPowerConfirm(Character *p, Power *ppow)
{
	bool bNeedConfirm = true;

	if (ENTTYPE(p->entParent) != ENTTYPE_PLAYER)
	{
		return false; // return immediately, because later code assumes e->pl != NULL
	}

	if(ppow->ppowBase->iTimeToConfirm==0
		|| (ppow->psetParent->pcharParent==p && !ppow->ppowBase->iSelfConfirm))
	{
		bNeedConfirm = false;
	}

	if (ppow->ppowBase->ppchConfirmRequires
		&& !chareval_Eval(p, ppow->ppowBase->ppchConfirmRequires, ppow->ppowBase->pchSourceFile))
	{
		bNeedConfirm = false;
	}

	if(!p->entParent->pl->promptTeamTeleport
		&& ppow->ppowBase->eTargetTypeSecondary == kTargetType_Teleport)
	{
		bNeedConfirm = false;
	}

	// More checks can go here.


	return bNeedConfirm;
}

/**********************************************************************func*
 * character_BecomeCamera
 *
 */
void character_BecomeCamera(Entity* e)
{
	const BasePower *ppowBase = powerdict_GetBasePowerByName(&g_PowerDictionary, 
		"Temporary_Powers", "Temporary_Powers", "Remote_Camera");

	assert(e && e->pl && e->pchar);

	character_Reset(e->pchar, false);

	if(ppowBase!=NULL)
	{
		Power *ppow;
		PowerSet *pset;

		pset = character_BuyPowerSet(e->pchar, ppowBase->psetParent);
		ppow = character_BuyPower(e->pchar, pset, ppowBase, 0); // checks for duplicates, etc.

		if(ppow!=NULL)
		{
			eaPush(&e->powerDefChange, powerRef_CreateFromPower(e->pchar->iCurBuild, ppow));

			character_EnterStance(e->pchar, ppow, false);
			character_ActivatePowerOnCharacter(e->pchar, e->pchar, ppow);
			if (ppow->ppowBase && ppow->ppowBase->bShowBuffIcon)
				e->team_buff_update = true;
		}
	}
	else
	{
		dbLog("Arena:Camera", e, "Unable to find camera power %s", dbg_BasePowerStr(ppowBase));
	}

	character_SetCostumeByName(e->pchar, "ARENA_CAMERA", 9);

	//e->pl->disablePowers = 1;
}

/**********************************************************************func*
 * character_UnCamera
 *
 */
void character_UnCamera(Entity* e)
{
	const BasePower *ppowBase = powerdict_GetBasePowerByName(&g_PowerDictionary, 
		"Temporary_Powers", "Temporary_Powers", "Remote_Camera");

	assert(e && e->pl && e->pchar);

	//e->pl->disablePowers = 0;

	if(ppowBase!=NULL)
	{
		character_RemoveBasePower(e->pchar, ppowBase);
	}
	else
	{
		dbLog("Arena:Camera", e, "Unable to find camera power %s", dbg_BasePowerStr(ppowBase));
	}

	character_ActivateAllAutoPowers(e->pchar);
	character_RestoreCostume(e->pchar);
}

int character_LastCombatActivity(Entity *e)
{
	int now = timerSecondsSince2000();
	int attackedTime, attackTime, onTime;

	if (!e)
		return 0;

	attackedTime = MAX(0, now - e->pchar->aulEventTimes[kPowerEvent_AttackedByOther]);
	attackTime = MAX(0, now - e->pchar->aulEventTimes[kPowerEvent_ActivateAttackClick]);
	onTime = MAX(0, now - e->on_since);

	return MIN(onTime, MIN(attackedTime, attackTime));
}

/* End of File */
