#ifndef ENTAIBEHAVIORCOH_H
#define ENTAIBEHAVIORCOH_H

#include "aiBehaviorInterface.h"

typedef struct AIBehavior			AIBehavior;
typedef struct AIPriorityManager	AIPriorityManager;
typedef struct Entity				Entity;

typedef enum AIBCustomEntryType
{
	AIB_ANIMLIST = AIB_CUSTOM,
}AIBCustomEntryType;

int aiBehaviorGetActivity(Entity* e);
AIPriorityManager* aiBehaviorGetPriorityManager(Entity* e, int add, int clearFinish);
void aiBehaviorSetActivity(Entity* e, int activity);
void aiBehaviorAddPLFlag(Entity* e, char* string);
AIBehavior* aiBehaviorGetPLBehavior(Entity* e, int add);

void aiBehaviorCohInitSystem(void);
int aiBFMoveToPosDo(Entity* e, AIVarsBase* aibase, Vec3 target, F32 radius);

//////////////////////////////////////////////////////////////////////////
// AlertTeam
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_RUN_FUNC(AlertTeam);

//////////////////////////////////////////////////////////////////////////
// AttackTarget
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_FLAG_FUNC(AttackTarget);
BEHAVIOR_START_FUNC(AttackTarget);
BEHAVIOR_RUN_FUNC(AttackTarget);
BEHAVIOR_SET_FUNC(AttackTarget);
BEHAVIOR_UNSET_FUNC(AttackTarget);

//////////////////////////////////////////////////////////////////////////
// AttackVolumes
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_DEBUG_FUNC(AttackVolumes);
BEHAVIOR_FLAG_FUNC(AttackVolumes);
BEHAVIOR_RUN_FUNC(AttackVolumes);
BEHAVIOR_SET_FUNC(AttackVolumes);
BEHAVIOR_FINISH_FUNC(AttackVolumes);

//////////////////////////////////////////////////////////////////////////
// BallTurretBaseCheckDeath
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_RUN_FUNC(BallTurretBaseCheckDeath);

//////////////////////////////////////////////////////////////////////////
// BallTurretSlideOut
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_RUN_FUNC(BallTurretSlideOut);

//////////////////////////////////////////////////////////////////////////
// CallForHelp
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_RUN_FUNC(CallForHelp);
BEHAVIOR_STRING_FUNC(CallForHelp);

//////////////////////////////////////////////////////////////////////////
// Chat
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_RUN_FUNC(Chat);

//////////////////////////////////////////////////////////////////////////
// Combat
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_DEBUG_FUNC(Combat);
BEHAVIOR_RUN_FUNC(Combat);
BEHAVIOR_SET_FUNC(Combat);
BEHAVIOR_START_FUNC(Combat);
BEHAVIOR_STRING_FUNC(Combat);
BEHAVIOR_UNSET_FUNC(Combat);

//////////////////////////////////////////////////////////////////////////
// Camera
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_DEBUG_FUNC(CameraScan);
BEHAVIOR_FLAG_FUNC(CameraScan);
BEHAVIOR_RUN_FUNC(CameraScan);

//////////////////////////////////////////////////////////////////////////
// CutScene
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_STRING_FUNC(CutScene); //(Uses Alias for all other stuff)

//////////////////////////////////////////////////////////////////////////
// DestroyMe
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_RUN_FUNC(DestroyMe);

//////////////////////////////////////////////////////////////////////////
// DisappearInStandardDoor
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_RUN_FUNC(DisappearInStandardDoor);

//////////////////////////////////////////////////////////////////////////
// DoNothing
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_RUN_FUNC(DoNothing);
BEHAVIOR_STRING_FUNC(DoNothing);

//////////////////////////////////////////////////////////////////////////
// HideBehindEnt
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_RUN_FUNC(HideBehindEnt);
BEHAVIOR_SET_FUNC(HideBehindEnt);

//////////////////////////////////////////////////////////////////////////
// HostageScared
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_RUN_FUNC(HostageScared);

//////////////////////////////////////////////////////////////////////////
// Loop
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_DEBUG_FUNC(Loop);
BEHAVIOR_FINISH_FUNC(Loop);
BEHAVIOR_RUN_FUNC(Loop);
BEHAVIOR_STRING_FUNC(Loop);

//////////////////////////////////////////////////////////////////////////
// MoveToPos
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_DEBUG_FUNC(MoveToPos);
BEHAVIOR_FLAG_FUNC(MoveToPos);
BEHAVIOR_RUN_FUNC(MoveToPos);
BEHAVIOR_SET_FUNC(MoveToPos);
BEHAVIOR_UNSET_FUNC(MoveToPos);

//////////////////////////////////////////////////////////////////////////
// MoveToRandPoint
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_RUN_FUNC(MoveToRandPoint);
BEHAVIOR_STRING_FUNC(MoveToRandPoint);

//////////////////////////////////////////////////////////////////////////
// ReturnToSpawn
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_DEBUG_FUNC(ReturnToSpawn);
BEHAVIOR_FLAG_FUNC(ReturnToSpawn);
BEHAVIOR_RUN_FUNC(ReturnToSpawn);
BEHAVIOR_SET_FUNC(ReturnToSpawn);
BEHAVIOR_UNSET_FUNC(ReturnToSpawn);

//////////////////////////////////////////////////////////////////////////
// Patrol
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_DEBUG_FUNC(Patrol);
BEHAVIOR_FLAG_FUNC(Patrol);
BEHAVIOR_RUN_FUNC(Patrol);
BEHAVIOR_SET_FUNC(Patrol);
BEHAVIOR_START_FUNC(Patrol);
BEHAVIOR_STRING_FUNC(Patrol);
BEHAVIOR_UNSET_FUNC(Patrol);

//////////////////////////////////////////////////////////////////////////
// PatrolRiktiDropship
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_RUN_FUNC(PatrolRiktiDropship);

//////////////////////////////////////////////////////////////////////////
// PERMVAR
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Pet
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_DEBUG_FUNC(Pet);
BEHAVIOR_FINISH_FUNC(Pet);
BEHAVIOR_RUN_FUNC(Pet);
BEHAVIOR_SET_FUNC(Pet);
BEHAVIOR_START_FUNC(Pet);
BEHAVIOR_STRING_FUNC(Pet);
BEHAVIOR_UNSET_FUNC(Pet);

//////////////////////////////////////////////////////////////////////////
// Pet.Follow
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_RUN_FUNC(PetFollow);
BEHAVIOR_SET_FUNC(PetFollow);

//////////////////////////////////////////////////////////////////////////
// PickRandomWeapon
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_RUN_FUNC(PickRandomWeapon);

//////////////////////////////////////////////////////////////////////////
// PlayFX
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_FINISH_FUNC(PlayFX);
BEHAVIOR_RUN_FUNC(PlayFX);
BEHAVIOR_STRING_FUNC(PlayFX);

//////////////////////////////////////////////////////////////////////////
// PriorityList
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_DEBUG_FUNC(PriorityList);
BEHAVIOR_FINISH_FUNC(PriorityList);
BEHAVIOR_FLAG_FUNC(PriorityList);
BEHAVIOR_RUN_FUNC(PriorityList);
BEHAVIOR_SET_FUNC(PriorityList);
BEHAVIOR_START_FUNC(PriorityList);
BEHAVIOR_STRING_FUNC(PriorityList);
BEHAVIOR_UNSET_FUNC(PriorityList);

//////////////////////////////////////////////////////////////////////////
// RunAwayFar
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_RUN_FUNC(RunAwayFar);
BEHAVIOR_SET_FUNC(RunAwayFar);
BEHAVIOR_STRING_FUNC(RunAwayFar);

//////////////////////////////////////////////////////////////////////////
// RunIntoDoor
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_RUN_FUNC(RunIntoDoor);
BEHAVIOR_SET_FUNC(RunIntoDoor);

//////////////////////////////////////////////////////////////////////////
// RunOutOfDoor
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_OFFTICK_FUNC(RunOutOfDoor);
BEHAVIOR_RUN_FUNC(RunOutOfDoor);
BEHAVIOR_SET_FUNC(RunOutOfDoor);
BEHAVIOR_START_FUNC(RunOutOfDoor);

//////////////////////////////////////////////////////////////////////////
// RunThroughDoor
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_RUN_FUNC(RunThroughDoor);
BEHAVIOR_SET_FUNC(RunThroughDoor);
BEHAVIOR_STRING_FUNC(RunThroughDoor);
BEHAVIOR_START_FUNC(RunThroughDoor);
BEHAVIOR_FINISH_FUNC(RunThroughDoor);

//////////////////////////////////////////////////////////////////////////
// SCOPEVAR
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_RUN_FUNC(SCOPEVAR);

//////////////////////////////////////////////////////////////////////////
// Say
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_FLAG_FUNC(Say);
BEHAVIOR_RUN_FUNC(Say);
BEHAVIOR_STRING_FUNC(Say);
BEHAVIOR_FINISH_FUNC(Say);

//////////////////////////////////////////////////////////////////////////
// SetHealth
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_RUN_FUNC(SetHealth);
BEHAVIOR_STRING_FUNC(SetHealth);

//////////////////////////////////////////////////////////////////////////
// SetObjective
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_RUN_FUNC(SetObjective);
BEHAVIOR_STRING_FUNC(SetObjective);

//////////////////////////////////////////////////////////////////////////
// Test
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_DEBUG_FUNC(Test);
BEHAVIOR_FINISH_FUNC(Test);
BEHAVIOR_FLAG_FUNC(Test);
BEHAVIOR_OFFTICK_FUNC(Test);
BEHAVIOR_RUN_FUNC(Test);
BEHAVIOR_SET_FUNC(Test);
BEHAVIOR_START_FUNC(Test);
BEHAVIOR_STRING_FUNC(Test);
BEHAVIOR_UNSET_FUNC(Test);

//////////////////////////////////////////////////////////////////////////
// ThankHero
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_RUN_FUNC(ThankHero);

//////////////////////////////////////////////////////////////////////////
// UsePower
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_RUN_FUNC(UsePower);
BEHAVIOR_SET_FUNC(UsePower);
BEHAVIOR_START_FUNC(UsePower);
BEHAVIOR_STRING_FUNC(UsePower);
BEHAVIOR_UNSET_FUNC(UsePower);

//////////////////////////////////////////////////////////////////////////
// WanderAround
//////////////////////////////////////////////////////////////////////////
BEHAVIOR_DEBUG_FUNC(WanderAround);
BEHAVIOR_RUN_FUNC(WanderAround);
BEHAVIOR_SET_FUNC(WanderAround);
BEHAVIOR_STRING_FUNC(WanderAround);

#endif