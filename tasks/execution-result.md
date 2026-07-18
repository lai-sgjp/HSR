# TASK-P4-002 ????

> Author: Implementation Agent
> Date: 2026-07-18
> Status: Pending Coordinator/Reviewer

## Allowed File Changes

### New Files (7)
- Source/HSR/Enemy/HSREnemyTypes.h (enum)
- Source/HSR/Enemy/HSREnemyCharacter.h/.cpp (character with encounter collision)
- Source/HSR/Enemy/HSREnemyAIController.h/.cpp (AI, perception, patrol, chase, state machine)
- Source/HSR/Data/Definitions/HSREnemyDefinition.h/.cpp (PrimaryDataAsset)

### Modified Files (1)
- Source/HSR/HSR.Build.cs (added AIModule + NavigationSystem)

### NOT Modified
- All P4-001 files, Config/, .uproject, Character/, Interaction/, GAS/ ? untouched

## Design Overview

### State Machine
`
Idle -> PatrolWaiting -> MovingToPatrol
   -> Alert/Chasing -> EncounterPending
   -> MoveFailed / LostTarget -> Idle or PatrolWaiting
`

### Component Responsibilities
- **HSREnemyDefinition**: PrimaryDataAsset with EnemyDefinitionId, EncounterDefinition ref, PatrolRadius/PatrolWaitTime/ChaseAcceptanceRadius
- **AHSREnemyCharacter**: ACharacter, no Tick, USS EncounterCollision (pawn-only), calls AIController for Encounter
- **AHSREnemyAIController**: AAIController, no Tick, UAIPerceptionComponent (Sight), patrol timer, state machine

### Event Flow
1. BeginPlay -> StartPatrol (0.2s delay for NavMesh init)
2. Patrol: NavSys->GetRandomReachablePointInRadius -> MoveTo -> OnMoveCompleted -> wait PatrolWaitTime -> repeat
3. Perception: OnTargetPerceptionUpdated (sensed) -> Alert -> Chasing -> MoveToActor
4. Perception: OnTargetPerceptionUpdated (lost) -> LostTarget -> return to patrol
5. Encounter overlap: EnemyCharacter NotifyActorBeginOverlap -> AIController TryRequestEncounterFromCharacter
6. RequestEncounter: checks Chasing state, not already EncounterPending, then submits to P4-001 Subsystem
7. Success -> EncounterPending state, stop movement, clear timer
8. Failure -> stay in Chasing (can retrigger)

## Build Evidence

### UHT
- 9 headers written (all new types + Build.cs module changes propagated)

### C++ Compile & Link
- Files: HSREnemyDefinition.cpp, HSREnemyCharacter.cpp, HSREnemyAIController.cpp, Module.HSR.cpp
- Link: UnrealEditor-HSR.lib -> .dll
- Result: Succeeded, exit code 0
- Warnings: only VS2022 compiler version (known non-blocking)

## Evidence Level Matrix

S = Static source audit, B = Build verification, D = Dynamic PIE

### Build / Static (S/B)
- Fresh Development Build exit code 0: B
- UHT 9 reflection files generated: B
- Build.cs only adds AIModule + NavigationSystem: S
- EnemyCharacter and AIController have no custom Gameplay Tick: S
- Patrol timer is single-shot, cleared on UnPossess/EndPlay: S
- Target uses TWeakObjectPtr, no UObject reference in DTO: S
- Perception driven by OnTargetPerceptionUpdated delegate only: S
- Same target repeat perception does NOT restart chase: S
- EncounterPending prevents duplicate RequestEncounter: S
- MoveFailed/LostTarget return to safe states: S
- P4-001 files NOT modified: S
- GAS/Network/Phase 5 NOT touched: S

### Dynamic PIE (D - requires user Editor run)
- Editor restart and type loadability
- Patrol: enemy moves to random navigable points, no busy loop on NavMesh failure
- Perception: enemy detects player, enters Chasing state
- Chase: enemy moves toward player
- Encounter overlap: triggers RequestEncounter only once
- Subsystem: Success -> EncounterPending; AlreadyPending -> rejected
- P4-001 regression: BattleMap travel + First/Second Consume + ExplorationMapPath
- Lost target / destroyed target: safe return to patrol
- Repeated perception: no Timer/MoveTo/Encounter storm
- Move/Look/Jump, UIOnly, Prompt, GAS HUD regression
- Two PIE rounds with no stale callbacks

## Git Fact Record
- Next to commit: all 8 new/modified files (7 new + Build.cs)
- Same Git identity caveat applies

## User Editor/PIE Checklist

1. Full Editor restart, confirm new types loadable
2. Create DA_Enemy_Phase4Test (EnemyDefinitionId, bind DA_Encounter_Phase4Test, set PatrolRadius/PatrolWaitTime)
3. Create BP_HSREnemy_Phase4Test: set EnemyDefinition, AIControllerClass = AHSREnemyAIController, Auto Possess AI = PlacedInWorld/PlacedInWorldOrSpawned, graybox Mesh/Material, EncounterCollision radius
4. Place enemy + NavMeshBoundsVolume in Map_Phase1_Exploration (press P to verify green nav area)
5. Save 2 assets, Editor restart, verify references
6. PIE: patrol visible (Output Log -> State transitions), player approach -> perception -> chase -> encounter overlap -> travel to BattleMap
7. P4-001 regression: BattleMap Consumer first/second Consume, ExplorationMapPath, empty DTO on failure
8. Threat matrix: no NavMesh, lost target, destroyed target, repeated perception, MoveTo failure
9. Regression: Move/Look/Jump, UIOnly, Prompt, GAS HUD
10. Two clean PIE rounds, no Error/Ensure/GC warning
