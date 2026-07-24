# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

All commands use UE5.6 UBT via the `.uproject` file association:

```bash
# Build (Development Editor)
"E:/Program Files/Epic Games/UE_5.6/Engine/Build/BatchFiles/Build.bat" HREditor Win64 Development -Project="E:/work/unreal_projects/HSR/HSR.uproject"

# Build verbosely (for diagnosing UHT/link errors)
"E:/Program Files/Epic Games/UE_5.6/Engine/Build/BatchFiles/Build.bat" HREditor Win64 Development -Project="E:/work/unreal_projects/HSR/HSR.uproject" -verbose

# Generate project files
"E:/Program Files/Epic Games/UE_5.6/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" -ProjectFiles -Project="E:/work/unreal_projects/HSR/HSR.uproject" -Game

# Run the editor
start "" "E:/Program Files/Epic Games/UE_5.6/Engine/Binaries/Win64/UnrealEditor.exe" "E:/work/unreal_projects/HSR/HSR.uproject"
```

Project is a single `Runtime` module named `HSR` built with C++20 (confirmed via `_MSVC_LANG`). No Editor or other modules exist.

## High-Level Architecture

### Game Flow
```
Exploration (AHSRExplorationCharacter, AHSRGameModeBase)
  → Encounter (AHSREnemyCharacter overlap, UHSREncounterDefinition DataAsset)
    → Transition (UHSRBattleTransitionSubsystem — travel to battle map)
      → Battle (AHSRBattleGameMode, UHSRBattleCoordinator)
        → Return (UHSRBattleTransitionSubsystem — travel back to exploration)
```

### Battle System (core)

Turn-based command battle with a **pure-value DTO** pattern: cross-system communication uses structs with no UObject/AActor references, making interactions stateless and testable.

| Layer | Class | Role |
|---|---|---|
| **GameMode** | `AHSRBattleGameMode` | Entry point for the battle world. Owns skill/status definitions, test harness toggles. |
| **Coordinator** | `UHSRBattleCoordinator` | State machine (Idle→Consuming→Spawned→Finished). Spawns participants, handles victory/defeat, owns skill definitions. |
| **Turn Manager** | `UHSRTurnManager` | Speed-sorted turn order, action resolution, break delays. Event-driven, never ticks. |
| **Targeting** | `FHSRTargetingPolicy` | Static pure policy: builds candidate target lists, validates submitted target IDs against current participants. |

### Participant Model

`FHSRBattleParticipant` is a **struct** (not Actor or Component) containing:
- `ParticipantId` (FName), `Team`, `InitiativeSpeed`
- Weakness tags, defeated flag
- `TWeakObjectPtr<AActor>` and `TWeakObjectPtr<UAbilitySystemComponent>`

### GAS Integration

| Component | File(s) | Notes |
|---|---|---|
| **ASC** | `UHSRAbilitySystemComponent` | Minimal subclass of `UAbilitySystemComponent` |
| **Attribute Set** | `UHSRCoreAttributeSet` | Health/MaxHealth, Energy/MaxEnergy, Speed, Attack, Defense, CritRate/CritDamage, Toughness/MaxToughness, IncomingDamage (meta), IncomingToughnessDamage (meta) |
| **Abilities** | `UHSRGameplayAbilityBase` → `HSRBasicAttackAbility`, `HSRSkillAbility`, `HSRUltimateAbility`, `HSRHealAbility` | Target handoff + formal damage prepare/apply pattern |
| **Damage** | `UHSRDamageExecutionCalculation` (ExecutionCalculation), `FHSRDamageContext` (pure input), `FHSRDamageBreakdown` (formula trace) | Formal damage pipeline: prepare (capture → compute → spec) → apply (cost commit → GE apply → post) |
| **Damage Rules** | `UHSRDamageRuleDefinition` | DataAsset holding `DefenseCoefficient` and `MinDamage` |
| **Damage Types** | `EHSRDamageResultType` enum, `FHSRDamageResult` struct | Pure result DTOs |

### Status System (P9)

- `UHSRStatusDefinition` (UPrimaryDataAsset) — authored status config (duration, stacks, trigger timing, refresh policy, classification Buff/Debuff, dispel rules, damage linkage)
- `UHSRStatusComponent` (ActorComponent) — per-participant runtime; managed by `UHSRBattleCoordinator`
- `FHSRStatusInstance` / `FHSRStatusRuntimeSnapshot` — pure state structs
- Bind to `UHSRTurnManager` lifecycle events for turn-based duration consumption

### Break / Toughness System (P8)

- `FHSRToughnessResult` — pure toughness damage result (weakness matching, before/after)
- `FHSRBreakResult` — break trigger publication
- `FHSRTurnDelayRequest` — break causes delay; consumed by `UHSRTurnManager`
- Element/Weakness tags via GameplayTags hierarchy (`Element.*`, `Weakness.*`)
- `FHSRToughnessConfiguration` — static validation helpers for DataAsset integrity

### UI (MVVM-like)

| Component | Role |
|---|---|
| **ViewState** | `FHSRBattleCommandViewState` — pure-value snapshot of current actor, SP, Energy, skill availability, last resolution |
| **ViewModel** | `UHSRBattleCommandViewModel` — owned by GameMode, broadcasts `OnChanged()`, holds display-ready `FText` properties |
| **Widget** | `UHSRBattleCommandWidget` — UMG widget base, subscribes to ViewModel |
| **Coordinator** | Publishes `OnCommandStateReady()` for ViewModel to consume |

The ViewState is deliberately pointer-free: UI never holds references to Actors or ASCs.

### Data Assets

All authored via UPrimaryDataAsset / UDataAsset Blueprint child assets in `Content/Data/`:
- `UHSREncounterDefinition` — encounter ID, enemy definition ref, battle map
- `UHSREnemyDefinition` — enemy stats, weakness tags, toughness, patrol config
- `UHSRSkillDefinition` — skill config (category, target type, damage type, element tag, toughness damage, costs, ability class, damage rule)
- `UHSRDamageRuleDefinition` — damage formula constants
- `UHSRStatusDefinition` — status effect config

### Exploration Layer

- `AHSRExplorationCharacter` — third-person character with Enhanced Input (Move/Look/Jump/Interact)
- `UHSRInteractionComponent` — detects interactable actors via `IHSRInteractableInterface`
- `AHSREnemyCharacter` — encounter trigger on overlap; references `UHSREnemyDefinition`
- `AHSRHUD` — exploration HUD with optional `UHSRInteractionViewModel`
- `UHSRBattleTransitionSubsystem` (UGameInstanceSubsystem) — encounter request/response, world travel, return

### Key Architectural Patterns

1. **Pure-value DTOs for cross-system boundaries**: `FHSRBattleParticipant`, `FHSRBattleCommandViewState`, `FHSRAbilityResolution`, `FHSRDamageResult`, `FHSRBattleResult` — never pass runtime objects between subsystems. This makes replays, snapshots, and test harnesses safe and simple.

2. **Coordinator authority**: `UHSRBattleCoordinator` is the single source of truth for battle state — TurnManager, StatusComponents, participants are all owned/coordinated through it.

3. **Development test harnesses**: Editor-only boolean toggles on `AHSRBattleGameMode` (`bRunP7DamageHarness`, `bRunP8ContractHarness`, `bRunP9TurnLifecycleHarness`, etc.) and `#if WITH_EDITOR` accessors on Coordinators for isolated contract tests without PIE.

4. **Phase-based incremental development**: Features progress through numbered phases (P0–P20), each with execution plans in `docs/phase-*-execution-plan.md`. Current: P9-004 (Immunity, Dispel, Source Invalid, Battle Cleanup).

5. **Gameplay Tags hierarchy** (defined in `Config/DefaultGameplayTags.ini`):
   - `Ability.*`, `Cooldown.*`, `Damage.*`, `Damage.Type.*`, `Element.*`, `Event.*`, `State.*`, `Weakness.*`

6. **C++20 features**: Designated initializers, `constexpr`, `std::optional`-like patterns via `TOptional`.
