#include "HSRSaveSubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Party/HSRPartySubsystem.h"
#include "HSRSaveGame.h"
#include "HSRSaveVersion.h"
#include "Kismet/GameplayStatics.h"
#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "../Data/Definitions/HSRItemEquipmentMappingCatalog.h"
#if WITH_EDITOR
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "../Battle/HSRBattleGameMode.h"
#include "../Battle/HSRBattleCoordinator.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"
#include "AbilitySystemComponent.h"

namespace { void RunHSRSavePIEAudit(){UWorld* W=nullptr;if(GEngine)for(const FWorldContext& C:GEngine->GetWorldContexts())if(C.World()&&C.World()->IsPlayInEditor()){W=C.World();break;}AHSRBattleGameMode* GM=W?Cast<AHSRBattleGameMode>(W->GetAuthGameMode()):nullptr;UGameInstance* GI=W?W->GetGameInstance():nullptr;UHSRSaveSubsystem* S=GI?GI->GetSubsystem<UHSRSaveSubsystem>():nullptr;UHSRCharacterProfileSubsystem* P=GI?GI->GetSubsystem<UHSRCharacterProfileSubsystem>():nullptr;UHSRBattleCoordinator* BC=GM?GM->GetCoordinator():nullptr;if(!S||!P||!BC){UE_LOG(LogTemp,Error,TEXT("HSR.SaveTest FAILED MissingPIEChain"));return;}const FString Slot=FString::Printf(TEXT("HSR_SaveTest_%s"),*FGuid::NewGuid().ToString(EGuidFormats::Digits));FHSRSaveData Baseline;if(S->SaveSnapshot(Baseline)!=EHSRSaveResult::Success||S->SaveToSlot(Slot)!=EHSRSaveResult::Success){UE_LOG(LogTemp,Error,TEXT("HSR.SaveTest FAILED Baseline"));return;}P->GrantExperience(TEXT("Character.A"),100);S->SaveToSlot(Slot);S->LoadSnapshot(Baseline);const FString Old=BC->GetProgressionPrimaryHandleForDevelopmentTest(TEXT("Player"));const int32 RefreshBefore=BC->GetProgressionRefreshCountForDevelopmentTest();const EHSRSaveResult First=S->LoadFromSlot(Slot);const FString New=BC->GetProgressionPrimaryHandleForDevelopmentTest(TEXT("Player"));float HP=0,MaxHP=0;for(const FHSRBattleParticipant& BP:BC->GetParticipants())if(BP.ParticipantId==TEXT("Player")&&BP.AbilitySystemComponent.IsValid()){HP=BP.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());MaxHP=BP.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetMaxHealthAttribute());}const int32 RefreshAfter=BC->GetProgressionRefreshCountForDevelopmentTest();const EHSRSaveResult Repeat=S->LoadFromSlot(Slot);const FString RepeatHandle=BC->GetProgressionPrimaryHandleForDevelopmentTest(TEXT("Player"));UE_LOG(LogTemp,Log,TEXT("HSR.SaveTest TxRefresh=%d->%d First=%d Repeat=%d Old=%s New=%s RepeatHandle=%s Secondary=%d Matching=%d Fingerprint=%s Health=%.3f MaxHealth=%.3f RefreshResult=%d"),RefreshBefore,RefreshAfter,static_cast<int32>(First),static_cast<int32>(Repeat),*Old,*New,*RepeatHandle,BC->GetProgressionSecondaryCountForDevelopmentTest(TEXT("Player")),BC->GetProgressionActiveHandleCountForDevelopmentTest(TEXT("Player")),*BC->GetProgressionFingerprintForDevelopmentTest(TEXT("Player")),HP,MaxHP,BC->GetLastProgressionRefreshResultForDevelopmentTest()?1:0);UGameplayStatics::DeleteGameInSlot(Slot,0);UE_LOG(LogTemp,Log,TEXT("HSR.SaveTest COMPLETE Cleanup=%d NoOpHandleStable=%d"),UGameplayStatics::DoesSaveGameExist(Slot,0)?0:1,New==RepeatHandle?1:0);}

void RunHSRProgressionFailureTest(){UWorld* W=nullptr;if(GEngine)for(const FWorldContext& C:GEngine->GetWorldContexts())if(C.World()&&C.World()->IsPlayInEditor()){W=C.World();break;}AHSRBattleGameMode* GM=W?Cast<AHSRBattleGameMode>(W->GetAuthGameMode()):nullptr;UHSRBattleCoordinator* BC=GM?GM->GetCoordinator():nullptr;UHSRCharacterProfileSubsystem* P=W&&W->GetGameInstance()?W->GetGameInstance()->GetSubsystem<UHSRCharacterProfileSubsystem>():nullptr;if(!BC||!P){UE_LOG(LogTemp,Error,TEXT("HSR.ProgressionFailureTest Result=FAIL Reason=MissingPIEChain"));return;}FHSRCharacterProgressionContext Base;if(!P->GetProgressionContext(TEXT("Character.A"),Base)){UE_LOG(LogTemp,Error,TEXT("HSR.ProgressionFailureTest Result=FAIL Reason=MissingProfile"));return;}float Health=0;for(const FHSRBattleParticipant& X:BC->GetParticipants())if(X.ParticipantId==TEXT("Player")&&X.AbilitySystemComponent.IsValid())Health=X.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());const FString Old=BC->GetProgressionPrimaryHandleForDevelopmentTest(TEXT("Player"));FHSRCharacterProgressionContext First=Base;++First.RuntimeRevision;First.ProgressionBonuses.MaxHealth+=10.0f;BC->SetProgressionApplyFailureForDevelopmentTest(true);const bool ApplyFail=BC->RefreshCharacterProgression(TEXT("Player"),First);BC->SetProgressionApplyFailureForDevelopmentTest(false);const bool ApplyPreserved=!ApplyFail&&Old==BC->GetProgressionPrimaryHandleForDevelopmentTest(TEXT("Player"))&&BC->GetProgressionActiveHandleCountForDevelopmentTest(TEXT("Player"))==1&&BC->GetProgressionSecondaryCountForDevelopmentTest(TEXT("Player"))==0;const bool FirstRetry=BC->RefreshCharacterProgression(TEXT("Player"),First);const FString New=BC->GetProgressionPrimaryHandleForDevelopmentTest(TEXT("Player"));FHSRCharacterProgressionContext Second=First;++Second.RuntimeRevision;Second.ProgressionBonuses.MaxHealth+=10.0f;BC->SetProgressionOldRemoveFailureForDevelopmentTest(true);const bool RemoveFail=BC->RefreshCharacterProgression(TEXT("Player"),Second);BC->SetProgressionOldRemoveFailureForDevelopmentTest(false);const bool RemovePreserved=!RemoveFail&&New==BC->GetProgressionPrimaryHandleForDevelopmentTest(TEXT("Player"))&&BC->GetProgressionActiveHandleCountForDevelopmentTest(TEXT("Player"))==1&&BC->GetProgressionSecondaryCountForDevelopmentTest(TEXT("Player"))==0;const bool SecondRetry=BC->RefreshCharacterProgression(TEXT("Player"),Second);const FString Final=BC->GetProgressionPrimaryHandleForDevelopmentTest(TEXT("Player"));const bool FinalUnique=SecondRetry&&Final!=New&&BC->GetProgressionActiveHandleCountForDevelopmentTest(TEXT("Player"))==1&&BC->GetProgressionSecondaryCountForDevelopmentTest(TEXT("Player"))==0;const bool bPassed=ApplyPreserved&&FirstRetry&&RemovePreserved&&FinalUnique;if(bPassed){UE_LOG(LogTemp,Log,TEXT("HSR.ProgressionFailureTest Result=PASS ApplyFail=%d ApplyPreserved=%d FirstRetry=%d RemoveFail=%d RemovePreserved=%d FinalUnique=%d Health=%.3f Old=%s New=%s Final=%s Matching=%d Secondary=%d Fingerprint=%s"),ApplyFail?1:0,ApplyPreserved?1:0,FirstRetry?1:0,RemoveFail?1:0,RemovePreserved?1:0,FinalUnique?1:0,Health,*Old,*New,*Final,BC->GetProgressionActiveHandleCountForDevelopmentTest(TEXT("Player")),BC->GetProgressionSecondaryCountForDevelopmentTest(TEXT("Player")),*BC->GetProgressionFingerprintForDevelopmentTest(TEXT("Player")));}else{UE_LOG(LogTemp,Error,TEXT("HSR.ProgressionFailureTest Result=FAIL ApplyFail=%d ApplyPreserved=%d FirstRetry=%d RemoveFail=%d RemovePreserved=%d FinalUnique=%d Health=%.3f Old=%s New=%s Final=%s Matching=%d Secondary=%d Fingerprint=%s"),ApplyFail?1:0,ApplyPreserved?1:0,FirstRetry?1:0,RemoveFail?1:0,RemovePreserved?1:0,FinalUnique?1:0,Health,*Old,*New,*Final,BC->GetProgressionActiveHandleCountForDevelopmentTest(TEXT("Player")),BC->GetProgressionSecondaryCountForDevelopmentTest(TEXT("Player")),*BC->GetProgressionFingerprintForDevelopmentTest(TEXT("Player")));}}
UHSRSaveSubsystem* GetCloseoutSave(){if(!GEngine)return nullptr;for(const FWorldContext& C:GEngine->GetWorldContexts())if(C.World()&&C.World()->IsPlayInEditor())return C.World()->GetGameInstance()?C.World()->GetGameInstance()->GetSubsystem<UHSRSaveSubsystem>():nullptr;return nullptr;}
void RunCloseoutSave(){UHSRSaveSubsystem* S=GetCloseoutSave();if(!S){UE_LOG(LogTemp,Error,TEXT("HSR.CloseoutSave Result=FAIL Reason=MissingPIESave"));return;}const EHSRSaveResult R=S->SaveToSlot(TEXT("HSR_P11_Closeout"));const FHSRSaveData& D=S->GetSnapshot();UE_LOG(LogTemp,Log,TEXT("HSR.CloseoutSave Result=%d ProfileCount=%d PartyRevision=%lld"),static_cast<int32>(R),D.Profiles.Num(),D.PartyRevision);}
void RunCloseoutLoad(){UHSRSaveSubsystem* S=GetCloseoutSave();if(!S){UE_LOG(LogTemp,Error,TEXT("HSR.CloseoutLoad Result=FAIL Reason=MissingPIESave"));return;}const EHSRSaveResult R=S->LoadFromSlot(TEXT("HSR_P11_Closeout"));FHSRCharacterProfileSnapshot A;UHSRCharacterProfileSubsystem* P=S->GetGameInstance()?S->GetGameInstance()->GetSubsystem<UHSRCharacterProfileSubsystem>():nullptr;UHSRPartySubsystem* Party=S->GetGameInstance()?S->GetGameInstance()->GetSubsystem<UHSRPartySubsystem>():nullptr;FHSRPartySnapshot PS;if(Party)Party->GetSnapshot(PS);UE_LOG(LogTemp,Log,TEXT("HSR.CloseoutLoad Result=%d RestoreTx=%lld CharacterARevision=%lld PartyRevision=%lld"),static_cast<int32>(R),S->GetRestoreTransactionRevisionForDevelopmentTest(),P&&P->GetProfileSnapshot(TEXT("Character.A"),A)?A.RuntimeRevision:-1,PS.Revision);}
void RunCloseoutCleanup(){const bool bExisted=UGameplayStatics::DoesSaveGameExist(TEXT("HSR_P11_Closeout"),0);if(bExisted)UGameplayStatics::DeleteGameInSlot(TEXT("HSR_P11_Closeout"),0);const bool bGone=!UGameplayStatics::DoesSaveGameExist(TEXT("HSR_P11_Closeout"),0);if(bGone){UE_LOG(LogTemp,Log,TEXT("HSR.CloseoutCleanup Result=SUCCESS Existed=%d Absent=1"),bExisted?1:0);}else{UE_LOG(LogTemp,Error,TEXT("HSR.CloseoutCleanup Result=FAIL Existed=%d Absent=0"),bExisted?1:0);}}
void LogP13State(const TCHAR* Operation,EHSRSaveResult Result,UHSRSaveSubsystem* Save){const FHSRSaveData* Data=Save?&Save->GetSnapshot():nullptr;UE_LOG(LogTemp,Log,TEXT("P13-004 %s Result=%d Schema=%d Stacks=%d Unique=%d Claims=%d InventoryRevision=%lld RewardRevision=%lld"),Operation,static_cast<int32>(Result),Data?Data->SchemaVersion:-1,Data?Data->Inventory.Stacks.Num():-1,Data?Data->Inventory.UniqueItems.Num():-1,Data?Data->Rewards.Receipts.Num():-1,Data?Data->Inventory.Revision:-1,Data?Data->Rewards.Revision:-1);}
void RunP13Save(){UHSRSaveSubsystem* S=GetCloseoutSave();const EHSRSaveResult R=S?S->SaveToSlot(TEXT("HSR_P13_Closeout")):EHSRSaveResult::InvalidData;LogP13State(TEXT("Save"),R,S);}
void RunP13Clear(){UHSRSaveSubsystem* S=GetCloseoutSave();if(!S){LogP13State(TEXT("Clear"),EHSRSaveResult::InvalidData,S);return;}FHSRSaveData Data;const EHSRSaveResult Capture=S->SaveSnapshot(Data);if(Capture==EHSRSaveResult::Success){Data.Inventory=FHSRInventorySaveData();Data.Rewards=FHSRRewardSaveData();}const EHSRSaveResult R=Capture==EHSRSaveResult::Success?S->LoadSnapshot(Data):Capture;LogP13State(TEXT("Clear"),R,S);}
void RunP13Load(){UHSRSaveSubsystem* S=GetCloseoutSave();const EHSRSaveResult R=S?S->LoadFromSlot(TEXT("HSR_P13_Closeout")):EHSRSaveResult::InvalidData;LogP13State(TEXT("Load"),R,S);}
void RunP13Cleanup(){const bool bDeleted=!UGameplayStatics::DoesSaveGameExist(TEXT("HSR_P13_Closeout"),0)||UGameplayStatics::DeleteGameInSlot(TEXT("HSR_P13_Closeout"),0);UE_LOG(LogTemp,Log,TEXT("P13-004 Cleanup Result=%s"),bDeleted?TEXT("SUCCESS"):TEXT("FAILED"));}
FAutoConsoleCommand Cmd(TEXT("HSR.SaveTest"),TEXT("Runs the P11-005 Save/Battle PIE audit and cleans its temporary slot."),FConsoleCommandDelegate::CreateStatic(&RunHSRSavePIEAudit)); FAutoConsoleCommand FailureCmd(TEXT("HSR.ProgressionFailureTest"),TEXT("Runs P11-006 progression apply/remove failure audit in PIE."),FConsoleCommandDelegate::CreateStatic(&RunHSRProgressionFailureTest)); FAutoConsoleCommand CloseoutSaveCmd(TEXT("HSR.CloseoutSave"),TEXT("Writes the P11 closeout save slot."),FConsoleCommandDelegate::CreateStatic(&RunCloseoutSave)); FAutoConsoleCommand CloseoutLoadCmd(TEXT("HSR.CloseoutLoad"),TEXT("Loads the P11 closeout save slot."),FConsoleCommandDelegate::CreateStatic(&RunCloseoutLoad)); FAutoConsoleCommand CloseoutCleanupCmd(TEXT("HSR.CloseoutCleanup"),TEXT("Deletes the P11 closeout save slot."),FConsoleCommandDelegate::CreateStatic(&RunCloseoutCleanup)); FAutoConsoleCommand P13SaveCmd(TEXT("HSR.P13Save"),TEXT("Saves the Phase 13 closeout slot."),FConsoleCommandDelegate::CreateStatic(&RunP13Save)); FAutoConsoleCommand P13ClearCmd(TEXT("HSR.P13Clear"),TEXT("Clears Phase 13 Inventory and Reward runtime through Save v3 restore."),FConsoleCommandDelegate::CreateStatic(&RunP13Clear)); FAutoConsoleCommand P13LoadCmd(TEXT("HSR.P13Load"),TEXT("Loads the Phase 13 closeout slot."),FConsoleCommandDelegate::CreateStatic(&RunP13Load)); FAutoConsoleCommand P13CleanupCmd(TEXT("HSR.P13Cleanup"),TEXT("Deletes the Phase 13 closeout slot."),FConsoleCommandDelegate::CreateStatic(&RunP13Cleanup)); }
#endif

#if WITH_EDITOR
namespace
{
constexpr const TCHAR* P15MapSlot = TEXT("HSR_P15_Map_Closeout");

UHSRSaveSubsystem* GetP15SaveSubsystem()
{
	if (!GEngine)
	{
		return nullptr;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (UWorld* World = Context.World(); World && World->IsPlayInEditor())
		{
			return World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UHSRSaveSubsystem>() : nullptr;
		}
	}
	return nullptr;
}

void LogP15MapSave(const TCHAR* Operation, const EHSRSaveResult Result, const UHSRSaveSubsystem* Save)
{
	const FHSRSaveData* Data = Save ? &Save->GetSnapshot() : nullptr;
	UE_LOG(LogTemp, Log, TEXT("P15 MapSave %s Result=%d Schema=%d Map=%s Arrival=%s Location=%s Regions=%d Teleports=%d Flags=%d Revision=%lld RestoreTx=%lld"),
		Operation, static_cast<int32>(Result), Data ? Data->SchemaVersion : -1,
		Data ? *Data->Map.CurrentLocation.MapId.ToString() : TEXT("None"),
		Data ? *Data->Map.CurrentLocation.ArrivalId.ToString() : TEXT("None"),
		Data ? *Data->Map.CurrentLocation.WorldTransform.GetLocation().ToString() : TEXT("None"),
		Data ? Data->Map.UnlockedRegionIds.Num() : -1, Data ? Data->Map.UnlockedTeleportIds.Num() : -1,
		Data ? Data->Map.ExplorationFlags.Num() : -1, Data ? Data->Map.Revision : -1,
		Save ? Save->GetRestoreTransactionRevisionForDevelopmentTest() : -1);
}

void RunP15MapSave()
{
	UHSRSaveSubsystem* Save = GetP15SaveSubsystem();
	const EHSRSaveResult Result = Save ? Save->SaveToSlot(P15MapSlot) : EHSRSaveResult::InvalidData;
	LogP15MapSave(TEXT("Save"), Result, Save);
}

void RunP15MapSetFlag()
{
	UHSRSaveSubsystem* Save = GetP15SaveSubsystem();
	UHSRMapSubsystem* Maps = Save && Save->GetGameInstance()
		? Save->GetGameInstance()->GetSubsystem<UHSRMapSubsystem>() : nullptr;
	const EHSRMapOperationResult Result = Maps
		? Maps->SetExplorationFlag(TEXT("Exploration.P15.EditorGate"))
		: EHSRMapOperationResult::InvalidWorld;
	UE_LOG(LogTemp, Log, TEXT("P15 MapSave SetFlag Result=%d Flag=Exploration.P15.EditorGate"),
		static_cast<int32>(Result));
}

void RunP15MapClear()
{
	UHSRSaveSubsystem* Save = GetP15SaveSubsystem();
	FHSRSaveData Data;
	EHSRSaveResult Result = Save ? Save->SaveSnapshot(Data) : EHSRSaveResult::InvalidData;
	if (Result == EHSRSaveResult::Success)
	{
		Data.Map = FHSRMapSaveData();
		Result = Save->LoadSnapshot(Data);
	}
	LogP15MapSave(TEXT("ClearRuntime"), Result, Save);
}

void RunP15MapLoad()
{
	UHSRSaveSubsystem* Save = GetP15SaveSubsystem();
	const EHSRSaveResult Result = Save ? Save->LoadFromSlot(P15MapSlot) : EHSRSaveResult::InvalidData;
	LogP15MapSave(TEXT("Load"), Result, Save);
}

void RunP15MapCleanup()
{
	const bool bDeleted = !UGameplayStatics::DoesSaveGameExist(P15MapSlot, 0)
		|| UGameplayStatics::DeleteGameInSlot(P15MapSlot, 0);
	UE_LOG(LogTemp, Log, TEXT("P15 MapSave Cleanup Result=%s"), bDeleted ? TEXT("SUCCESS") : TEXT("FAILED"));
}

FAutoConsoleCommand P15MapSaveCommand(TEXT("HSR.P15MapSave"), TEXT("Saves the Phase 15 map closeout slot."),
	FConsoleCommandDelegate::CreateStatic(&RunP15MapSave));
FAutoConsoleCommand P15MapSetFlagCommand(TEXT("HSR.P15MapSetFlag"), TEXT("Sets the Phase 15 Editor Gate exploration flag."),
	FConsoleCommandDelegate::CreateStatic(&RunP15MapSetFlag));
FAutoConsoleCommand P15MapClearCommand(TEXT("HSR.P15MapClear"), TEXT("Clears runtime map state without touching the disk slot."),
	FConsoleCommandDelegate::CreateStatic(&RunP15MapClear));
FAutoConsoleCommand P15MapLoadCommand(TEXT("HSR.P15MapLoad"), TEXT("Loads the Phase 15 map closeout slot."),
	FConsoleCommandDelegate::CreateStatic(&RunP15MapLoad));
FAutoConsoleCommand P15MapCleanupCommand(TEXT("HSR.P15MapCleanup"), TEXT("Deletes the Phase 15 map closeout slot."),
	FConsoleCommandDelegate::CreateStatic(&RunP15MapCleanup));
}
#endif

void UHSRSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Super::Initialize(Collection); Profiles=GetGameInstance()?GetGameInstance()->GetSubsystem<UHSRCharacterProfileSubsystem>():nullptr; Party=GetGameInstance()?GetGameInstance()->GetSubsystem<UHSRPartySubsystem>():nullptr; Equipment=GetGameInstance()?GetGameInstance()->GetSubsystem<UHSREquipmentSubsystem>():nullptr; Inventory=GetGameInstance()?GetGameInstance()->GetSubsystem<UHSRInventorySubsystem>():nullptr; Reward=GetGameInstance()?GetGameInstance()->GetSubsystem<UHSRRewardSubsystem>():nullptr; Quest=GetGameInstance()?GetGameInstance()->GetSubsystem<UHSRQuestSubsystem>():nullptr; Map=GetGameInstance()?GetGameInstance()->GetSubsystem<UHSRMapSubsystem>():nullptr; MappingCatalog=LoadObject<UHSRItemEquipmentMappingCatalog>(nullptr,TEXT("/Game/Data/Items/DA_ItemEquipmentMappingCatalog_P17.DA_ItemEquipmentMappingCatalog_P17")); if(!MappingCatalog)UE_LOG(LogTemp,Error,TEXT("HSR Save missing production ItemEquipmentMappingCatalog")); }

#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
void UHSRSaveSubsystem::InitializeForDevelopmentTest(UHSRCharacterProfileSubsystem* InProfiles, UHSRPartySubsystem* InParty, UHSREquipmentSubsystem* InEquipment, UHSRInventorySubsystem* InInventory, UHSRRewardSubsystem* InReward, UHSRQuestSubsystem* InQuest, UHSRMapSubsystem* InMap, UHSRItemEquipmentMappingCatalog* InMappingCatalog)
{
	Profiles = InProfiles;
	Party = InParty;
	Equipment = InEquipment;
	UGameInstance* OwnerGameInstance = GetGameInstance() ? GetGameInstance() : Cast<UGameInstance>(GetOuter());
	DevelopmentInventory = InInventory ? InInventory : NewObject<UHSRInventorySubsystem>(OwnerGameInstance);
	DevelopmentReward = InReward ? InReward : NewObject<UHSRRewardSubsystem>(OwnerGameInstance);
	DevelopmentQuest = InQuest ? InQuest : NewObject<UHSRQuestSubsystem>(OwnerGameInstance);
	DevelopmentMap = InMap ? InMap : NewObject<UHSRMapSubsystem>(OwnerGameInstance);
	Inventory = DevelopmentInventory;
	Reward = DevelopmentReward;
	Quest = DevelopmentQuest;
	Map = DevelopmentMap;
	MappingCatalog = InMappingCatalog;
	DevelopmentReward->InitializeForDevelopmentTest(DevelopmentInventory);
	DevelopmentQuest->InitializeForDevelopmentTest(DevelopmentReward);
}
#endif

bool UHSRSaveSubsystem::Validate(const FHSRSaveData& C) const {
	if((C.SchemaVersion<1 || C.SchemaVersion>HSRSaveVersion::CurrentSchema) || C.PartyRevision<0 || C.PartySlots.Num()!=UHSRPartySubsystem::Capacity) return false;
	TSet<FName> Seen;
	TMap<FGuid,FName> GuidOwners;
	for(const FHSRSaveProfileDto& P:C.Profiles){ const auto& S=P.State; if(S.CharacterId.IsNone()||S.Level<1||S.Experience<0||S.Ascension<0)return false; if(Seen.Contains(S.CharacterId))return false; Seen.Add(S.CharacterId); const FGuid Guid=HSRCharacterGuidFromProfileName(S.CharacterId); if(const FName* Owner=GuidOwners.Find(Guid)){if(*Owner!=S.CharacterId)return false;}else GuidOwners.Add(Guid,S.CharacterId); for(const auto& K:S.SkillLevels) if(K.Key.IsNone()||K.Value<0)return false; }
	TSet<FName> PartySeen; for(const FHSRPartySlot& Slot:C.PartySlots){ if(Slot.CharacterId.IsNone())continue; if(PartySeen.Contains(Slot.CharacterId))return false; PartySeen.Add(Slot.CharacterId); if(!Seen.Contains(Slot.CharacterId))return false; }
	if(C.SchemaVersion==1 && !C.Equipment.IsEmpty())return false;
	if(C.SchemaVersion>=7 && !C.Equipment.IsEmpty())return false;
	if(C.SchemaVersion<3 && (!C.Inventory.Stacks.IsEmpty() || !C.Inventory.UniqueItems.IsEmpty() || C.Inventory.Revision!=0 || !C.Rewards.Receipts.IsEmpty() || C.Rewards.Revision!=0))return false;
	if(C.SchemaVersion<4 && (!C.Quests.States.IsEmpty() || C.Quests.Revision!=0))return false;
	if(C.SchemaVersion<5 && (!C.Map.CurrentLocation.MapId.IsNone() || !C.Map.CurrentLocation.ArrivalId.IsNone()
		|| !C.Map.UnlockedRegionIds.IsEmpty() || !C.Map.UnlockedTeleportIds.IsEmpty()
		|| !C.Map.ExplorationFlags.IsEmpty() || C.Map.Revision!=0))return false;
	TSet<FGuid> OwnedInstances;
	TMap<FGuid, const FHSREquipmentRegistryDto*> RegistryInstances;
	TSet<FGuid> PlacedInstances;
	if(C.SchemaVersion>=7){for(const auto& D:C.EquipmentRegistry){if(!D.InstanceId.IsValid()||OwnedInstances.Contains(D.InstanceId))return false;OwnedInstances.Add(D.InstanceId);RegistryInstances.Add(D.InstanceId,&D);}TSet<FString> Slots;for(const auto& D:C.EquipmentPlacements){const FName* Owner=GuidOwners.Find(D.CharacterId);const FString Slot=FString::Printf(TEXT("%s:%d:%d"),*D.CharacterId.ToString(),D.Kind,D.Slot);if(!Owner||HSRCharacterGuidFromProfileName(*Owner)!=D.CharacterId||!OwnedInstances.Contains(D.InstanceId)||PlacedInstances.Contains(D.InstanceId)||Slots.Contains(Slot))return false;PlacedInstances.Add(D.InstanceId);Slots.Add(Slot);}}
	else for(const FHSREquipmentSaveDto& D:C.Equipment){const FName* Owner=GuidOwners.Find(D.CharacterId);if(!Owner||HSRCharacterGuidFromProfileName(*Owner)!=D.CharacterId||!D.InstanceId.IsValid()||OwnedInstances.Contains(D.InstanceId))return false;OwnedInstances.Add(D.InstanceId);}
	TSet<FGuid> InventoryInstances;
	for(const FHSRItemInstance& I:C.Inventory.UniqueItems){if(!I.InstanceId.IsValid()||InventoryInstances.Contains(I.InstanceId)||(C.SchemaVersion>=7?PlacedInstances.Contains(I.InstanceId):OwnedInstances.Contains(I.InstanceId)))return false;InventoryInstances.Add(I.InstanceId);if(const FHSREquipmentRegistryDto* const* Registry=RegistryInstances.Find(I.InstanceId)){FHSRItemEquipmentMappingEntry Mapping;if(!MappingCatalog||!MappingCatalog->Resolve(I.DefinitionId,Mapping)||Mapping.EquipmentDefinitionId!=(*Registry)->DefinitionId||static_cast<int32>(Mapping.Kind)!=(*Registry)->Kind||!Equipment.IsValid()||!Equipment->IsDefinitionCompatible(Mapping.EquipmentDefinitionId,Mapping.Kind,Mapping.Slot))return false;}}
	// Read-only cross-domain preflight: this must complete before any domain PrepareRestore or equipment projection.
	if(!Profiles.IsValid()||!Equipment.IsValid()||!Inventory.IsValid()||!Reward.IsValid()||!Quest.IsValid()||!Map.IsValid())return false;
	for(const FHSRSaveProfileDto& P:C.Profiles)if(!Profiles->HasDefinition(P.State.CharacterId))return false;
	if(C.SchemaVersion>=7){for(const auto& D:C.EquipmentRegistry)if(!Equipment->HasDefinition(D.DefinitionId))return false;}else for(const FHSREquipmentSaveDto& D:C.Equipment)if(!Equipment->HasDefinition(D.DefinitionId))return false;
	for(const FHSRItemStackSnapshot& S:C.Inventory.Stacks)if(!Inventory->HasDefinition(S.ItemId))return false;
	for(const FHSRItemInstance& I:C.Inventory.UniqueItems)if(!Inventory->HasDefinition(I.DefinitionId))return false;
	for(const FHSRRewardReceipt& R:C.Rewards.Receipts){if(!Reward->HasDefinition(R.Request.RewardDefinitionId))return false;for(const FHSRInventoryGrant& G:R.Grants)if(!Inventory->HasDefinition(G.ItemId))return false;}
	for(const FHSRQuestRuntimeState& Q:C.Quests.States)if(!Quest->HasDefinition(Q.QuestId))return false;
	if(!C.Map.CurrentLocation.MapId.IsNone()&&!Map->HasMapDefinition(C.Map.CurrentLocation.MapId))return false;
	for(const FName& R:C.Map.UnlockedRegionIds)if(!Map->HasRegionDefinition(R))return false;
	for(const FName& T:C.Map.UnlockedTeleportIds)if(!Map->HasTeleportDefinition(T))return false;
	return true;
}
bool UHSRSaveSubsystem::CanPrepareSnapshot(const FHSRSaveData& Candidate) const
{
	if(Candidate.SchemaVersion<1||Candidate.SchemaVersion>HSRSaveVersion::CurrentSchema||!Profiles.IsValid()||!Party.IsValid()||!Equipment.IsValid()||!Inventory.IsValid()||!Reward.IsValid()||!Quest.IsValid()||!Map.IsValid()||!Validate(Candidate))
	{
#if WITH_DEV_AUTOMATION_TESTS
		UE_LOG(LogTemp,Warning,TEXT("HSR Save prepare rejected by prerequisites/validation"));
#endif
		return false;
	}
	TArray<FHSRCharacterProfileSnapshot> SavedProfiles;for(const FHSRSaveProfileDto& D:Candidate.Profiles){FHSRCharacterProfileSnapshot P;P.RuntimeState=D.State;P.RuntimeRevision=D.RuntimeRevision;SavedProfiles.Add(MoveTemp(P));}
	TMap<FName,FHSRCharacterProfileSnapshot> ProfileCandidate;FHSRPartySnapshot PartySaved;PartySaved.Slots=Candidate.PartySlots;PartySaved.Revision=Candidate.PartyRevision;FHSRPartySnapshot PartyCandidate;FHSREquipmentRestoreMap EquipmentCandidate;FHSREquipmentRegistryRestoreState RegistryCandidate;FHSRInventoryRestoreState InventoryCandidate;FHSRRewardRestoreState RewardCandidate;FHSRQuestRestoreState QuestCandidate;FHSRMapRuntimeSnapshot MapCandidate;
	const TArray<FHSREquipmentSaveDto> EmptyEquipment;const FHSRInventorySaveData EmptyInventory;const FHSRRewardSaveData EmptyRewards;const FHSRQuestSaveData EmptyQuests;const FHSRMapSaveData EmptyMap;
	const bool bProfiles=Profiles->PrepareRestore(SavedProfiles,ProfileCandidate);const bool bParty=Party->PrepareRestore(PartySaved,PartyCandidate);const bool bEquipment=Candidate.SchemaVersion>=7?Equipment->PrepareRestore(Candidate.EquipmentRegistry,Candidate.EquipmentPlacements,RegistryCandidate):Equipment->PrepareRestore(Candidate.SchemaVersion==1?EmptyEquipment:Candidate.Equipment,EquipmentCandidate);const bool bInventory=Inventory->PrepareRestore(Candidate.SchemaVersion<3?EmptyInventory:Candidate.Inventory,InventoryCandidate);const bool bReward=Reward->PrepareRestore(Candidate.SchemaVersion<3?EmptyRewards:Candidate.Rewards,RewardCandidate);const bool bQuest=Quest->PrepareRestore(Candidate.SchemaVersion<4?EmptyQuests:Candidate.Quests,QuestCandidate);const bool bMap=Map->PrepareRestore(Candidate.SchemaVersion<5?EmptyMap:Candidate.Map,MapCandidate);
#if WITH_DEV_AUTOMATION_TESTS
	if(!(bProfiles&&bParty&&bEquipment&&bInventory&&bReward&&bQuest&&bMap))UE_LOG(LogTemp,Warning,TEXT("HSR Save prepare rejected Profiles=%d Party=%d Equipment=%d Inventory=%d Reward=%d Quest=%d Map=%d"),bProfiles,bParty,bEquipment,bInventory,bReward,bQuest,bMap);
#endif
	return bProfiles&&bParty&&bEquipment&&bInventory&&bReward&&bQuest&&bMap;
}
EHSRSaveResult UHSRSaveSubsystem::SaveSnapshot(FHSRSaveData& Out) { if(!Party.IsValid()||!Profiles.IsValid()||!Equipment.IsValid()||!Inventory.IsValid()||!Reward.IsValid()||!Quest.IsValid()||!Map.IsValid()) return EHSRSaveResult::InvalidData; FHSRSaveData Captured; Captured.SchemaVersion=HSRSaveVersion::CurrentSchema; TArray<FHSRCharacterProfileSnapshot> P; Profiles->ExportProfiles(P); for(const auto& Entry:P){ FHSRSaveProfileDto D;D.State=Entry.RuntimeState;D.RuntimeRevision=Entry.RuntimeRevision;Captured.Profiles.Add(MoveTemp(D)); } FHSRPartySnapshot PS;Party->GetSnapshot(PS);Captured.PartySlots=PS.Slots;Captured.PartyRevision=PS.Revision;Equipment->ExportSaveData(Captured.EquipmentRegistry,Captured.EquipmentPlacements);Inventory->ExportSaveData(Captured.Inventory);Reward->ExportSaveData(Captured.Rewards);Quest->ExportSaveData(Captured.Quests);Map->ExportSaveData(Captured.Map);if(!Validate(Captured))return EHSRSaveResult::InvalidData;Current=Captured;Out=Captured;return EHSRSaveResult::Success; }
EHSRSaveResult UHSRSaveSubsystem::LoadSnapshot(const FHSRSaveData& Candidate)
{
	if(Candidate.SchemaVersion<1 || Candidate.SchemaVersion>HSRSaveVersion::CurrentSchema || Candidate.SchemaVersion==6)return EHSRSaveResult::UnsupportedSchema; if(!Profiles.IsValid()||!Party.IsValid()||!Equipment.IsValid()||!Inventory.IsValid()||!Reward.IsValid()||!Quest.IsValid()||!Map.IsValid()||!Validate(Candidate))return EHSRSaveResult::InvalidData;
	TArray<FHSRCharacterProfileSnapshot> SavedProfiles;for(const auto& D:Candidate.Profiles){FHSRCharacterProfileSnapshot P;P.RuntimeState=D.State;P.RuntimeRevision=D.RuntimeRevision;SavedProfiles.Add(MoveTemp(P));}
	TMap<FName,FHSRCharacterProfileSnapshot> ProfileCandidate;FHSRPartySnapshot PartySaved;PartySaved.Slots=Candidate.PartySlots;PartySaved.Revision=Candidate.PartyRevision;FHSRPartySnapshot PartyCandidate;
	TMap<FGuid,FHSREquipmentRestoreState> EquipmentCandidate;FHSREquipmentRegistryRestoreState RegistryCandidate;const TArray<FHSREquipmentSaveDto> EmptyEquipment;
	FHSRInventoryRestoreState InventoryCandidate;FHSRRewardRestoreState RewardCandidate;FHSRQuestRestoreState QuestCandidate;FHSRMapRuntimeSnapshot MapCandidate;const FHSRInventorySaveData EmptyInventory;const FHSRRewardSaveData EmptyRewards;const FHSRQuestSaveData EmptyQuests;const FHSRMapSaveData EmptyMap;
	const bool bEquipmentPrepared=Candidate.SchemaVersion>=7?Equipment->PrepareRestore(Candidate.EquipmentRegistry,Candidate.EquipmentPlacements,RegistryCandidate):Equipment->PrepareRestore(Candidate.SchemaVersion==1?EmptyEquipment:Candidate.Equipment,EquipmentCandidate);
	if(Candidate.SchemaVersion>=7&&bEquipmentPrepared)EquipmentCandidate=RegistryCandidate.Loadouts;
	if(!Profiles->PrepareRestore(SavedProfiles,ProfileCandidate)||!Party->PrepareRestore(PartySaved,PartyCandidate)||!bEquipmentPrepared||!Inventory->PrepareRestore(Candidate.SchemaVersion<3?EmptyInventory:Candidate.Inventory,InventoryCandidate)||!Reward->PrepareRestore(Candidate.SchemaVersion<3?EmptyRewards:Candidate.Rewards,RewardCandidate)||!Quest->PrepareRestore(Candidate.SchemaVersion<4?EmptyQuests:Candidate.Quests,QuestCandidate)||!Map->PrepareRestore(Candidate.SchemaVersion<5?EmptyMap:Candidate.Map,MapCandidate))return EHSRSaveResult::InvalidData;
	const FHSREquipmentRestoreMap& ProjectedEquipment=Candidate.SchemaVersion>=7?RegistryCandidate.Loadouts:EquipmentCandidate;if(!Equipment->ProjectRestore(ProjectedEquipment))return EHSRSaveResult::InvalidData;
	TArray<FName> ChangedIds;for(const auto& It:ProfileCandidate){FHSRCharacterProfileSnapshot Old;if(!Profiles->GetProfileSnapshot(It.Key,Old)||Old.RuntimeRevision!=It.Value.RuntimeRevision||Old.RuntimeState.Level!=It.Value.RuntimeState.Level||Old.RuntimeState.Experience!=It.Value.RuntimeState.Experience||Old.RuntimeState.Ascension!=It.Value.RuntimeState.Ascension||!Old.RuntimeState.SkillLevels.OrderIndependentCompareEqual(It.Value.RuntimeState.SkillLevels))ChangedIds.Add(It.Key);}ChangedIds.Sort(FNameLexicalLess());FHSRPartySnapshot OldParty;Party->GetSnapshot(OldParty);bool PartyChanged=OldParty.Revision!=PartyCandidate.Revision;for(int32 I=0;!PartyChanged&&I<OldParty.Slots.Num();++I)PartyChanged=OldParty.Slots[I].CharacterId!=PartyCandidate.Slots[I].CharacterId;
	TArray<FHSREquipmentSaveDto> CandidateEquipmentRows=Candidate.Equipment;if(Candidate.SchemaVersion>=7){for(const auto& Placement:Candidate.EquipmentPlacements)for(const auto& Registry:Candidate.EquipmentRegistry)if(Registry.InstanceId==Placement.InstanceId){FHSREquipmentSaveDto D;D.DefinitionId=Registry.DefinitionId;D.InstanceId=Registry.InstanceId;D.CharacterId=Placement.CharacterId;D.Kind=Placement.Kind;D.Slot=Placement.Slot;D.EnhancementLevel=Registry.EnhancementLevel;D.Modifiers=Registry.Modifiers;D.SetId=Registry.SetId;D.AuthorityRevision=Placement.AuthorityRevision;CandidateEquipmentRows.Add(MoveTemp(D));break;}}
	TSet<FGuid> EquipmentChanged;TArray<FHSREquipmentSaveDto> ExistingEquipment;Equipment->ExportSaveData(ExistingEquipment);TSet<FGuid> ExistingCharacters;for(const FHSREquipmentSaveDto& D:ExistingEquipment)ExistingCharacters.Add(D.CharacterId);for(const FGuid& Id:ExistingCharacters)if(!EquipmentCandidate.Contains(Id))EquipmentChanged.Add(Id);for(const auto& P:EquipmentCandidate){TArray<FHSREquipmentSaveDto> OldRows;for(const FHSREquipmentSaveDto& D:ExistingEquipment)if(D.CharacterId==P.Key)OldRows.Add(D);TArray<FHSREquipmentSaveDto> NewRows;for(const FHSREquipmentSaveDto& D:CandidateEquipmentRows)if(D.CharacterId==P.Key)NewRows.Add(D);if(OldRows.Num()!=NewRows.Num()){EquipmentChanged.Add(P.Key);continue;}OldRows.Sort([](const auto& A,const auto& B){return A.InstanceId<B.InstanceId;});NewRows.Sort([](const auto& A,const auto& B){return A.InstanceId<B.InstanceId;});for(int32 I=0;I<OldRows.Num();++I){bool bDifferent=OldRows[I].InstanceId!=NewRows[I].InstanceId||OldRows[I].DefinitionId!=NewRows[I].DefinitionId||OldRows[I].Kind!=NewRows[I].Kind||OldRows[I].Slot!=NewRows[I].Slot||OldRows[I].EnhancementLevel!=NewRows[I].EnhancementLevel||OldRows[I].AuthorityRevision!=NewRows[I].AuthorityRevision||OldRows[I].SetId!=NewRows[I].SetId||OldRows[I].Modifiers.Num()!=NewRows[I].Modifiers.Num();for(int32 M=0;!bDifferent&&M<OldRows[I].Modifiers.Num();++M)bDifferent=OldRows[I].Modifiers[M].Stat!=NewRows[I].Modifiers[M].Stat||OldRows[I].Modifiers[M].Value!=NewRows[I].Modifiers[M].Value;if(bDifferent){EquipmentChanged.Add(P.Key);break;}}}
	const bool bInventoryChanged=Inventory->IsRestoreDifferent(InventoryCandidate);const bool bRewardsChanged=Reward->IsRestoreDifferent(RewardCandidate);const bool bQuestsChanged=Quest->IsRestoreDifferent(QuestCandidate);const bool bMapChanged=Map->IsRestoreDifferent(MapCandidate);
	Profiles->CommitRestoreSilent(MoveTemp(ProfileCandidate));Party->CommitRestoreSilent(MoveTemp(PartyCandidate));if(Candidate.SchemaVersion>=7)Equipment->CommitRestore(RegistryCandidate);else Equipment->CommitRestore(EquipmentCandidate);Inventory->CommitRestore(MoveTemp(InventoryCandidate),false);Reward->CommitRestore(MoveTemp(RewardCandidate),false);Quest->CommitRestore(MoveTemp(QuestCandidate),false);Map->CommitRestore(MoveTemp(MapCandidate),false);Current=Candidate;Current.SchemaVersion=HSRSaveVersion::CurrentSchema;if(Candidate.SchemaVersion<3){Current.Inventory=FHSRInventorySaveData();Current.Rewards=FHSRRewardSaveData();}if(Candidate.SchemaVersion<4){Current.Quests=FHSRQuestSaveData();}if(Candidate.SchemaVersion<5){Current.Map=FHSRMapSaveData();}Profiles->NotifyRestored(ChangedIds);if(PartyChanged)Party->NotifyRestored();Equipment->NotifyRestored(EquipmentChanged);if(bInventoryChanged)Inventory->OnInventoryChanged().Broadcast(Current.Inventory.Revision);if(bRewardsChanged)Reward->OnRewardRestored().Broadcast(Current.Rewards.Revision);if(bQuestsChanged)Quest->OnQuestRestored().Broadcast(Current.Quests.Revision);if(bMapChanged)Map->OnMapStateChanged().Broadcast(Map->GetSnapshot());if(!ChangedIds.IsEmpty()||PartyChanged||!EquipmentChanged.IsEmpty()||bInventoryChanged||bRewardsChanged||bQuestsChanged||bMapChanged){FHSRRestoreCommitInfo Info;Info.ChangedCharacterIds=ChangedIds;Info.bPartyChanged=PartyChanged;Info.bInventoryChanged=bInventoryChanged;Info.bRewardsChanged=bRewardsChanged;Info.bQuestsChanged=bQuestsChanged;Info.bMapChanged=bMapChanged;Info.TransactionRevision=++RestoreTransactionRevision;RestoreCommitted.Broadcast(Info);}return EHSRSaveResult::Success;
}

EHSRSaveResult UHSRSaveSubsystem::SaveToSlot(const FString& SlotName,int32 UserIndex)
{
	LastWriteFailureStage=EHSRSaveFailureStage::None;bLastWriteCleanupWarning=false;LastWriteHeader=FHSRSaveEnvelopeHeader();
	if(!HSRSaveVersion::IsValidSlot(SlotName,UserIndex)||SlotName.Contains(TEXT(".__hsr_")))return EHSRSaveResult::InvalidArgument;
	if(bOperationInProgress)return EHSRSaveResult::InvalidArgument;TGuardValue<bool> OperationGuard(bOperationInProgress,true);
	const FString Staging=SlotName+TEXT(".__hsr_staging_v1"),Backup=SlotName+TEXT(".__hsr_backup_v1");const FHSRSaveData Previous=Current;
#if WITH_DEV_AUTOMATION_TESTS
	if(bInjectCreateFailure||InjectedTransactionStage==EHSRSaveFailureStage::Capture){LastWriteFailureStage=EHSRSaveFailureStage::Capture;return bInjectCreateFailure?EHSRSaveResult::CreateFailed:EHSRSaveResult::InvalidData;}
#endif
	FHSRSaveData Captured;if(SaveSnapshot(Captured)!=EHSRSaveResult::Success){LastWriteFailureStage=EHSRSaveFailureStage::Capture;return EHSRSaveResult::InvalidData;}Captured.SchemaVersion=HSRSaveVersion::CurrentSchema;
	auto Fail=[&](EHSRSaveFailureStage Stage,EHSRSaveResult Result){LastWriteFailureStage=Stage;Current=Previous;return Result;};
	auto ReadValid=[&](const FString& Physical,TArray<uint8>& Out,FHSRSaveData& Data,FHSRSaveEnvelopeHeader& Header){return UGameplayStatics::LoadDataFromSlot(Out,Physical,UserIndex)&&HSRSaveVersion::DecodeEnvelope(Out,SlotName,UserIndex,Data,&Header)==EHSRSaveDecodeResult::Success&&Validate(Data);};
	FGuid Id=FGuid::NewGuid();uint64 Gen=1;TArray<uint8> Old;FHSRSaveData OldData;FHSRSaveEnvelopeHeader OldHeader;const bool bOldPrimaryValid=UGameplayStatics::DoesSaveGameExist(SlotName,UserIndex)&&ReadValid(SlotName,Old,OldData,OldHeader);
	if(bOldPrimaryValid){if(OldHeader.Generation==MAX_uint64)return Fail(EHSRSaveFailureStage::Encode,EHSRSaveResult::InvalidEnvelope);Id=OldHeader.SaveId;Gen=OldHeader.Generation+1;}
	else{TArray<uint8> ExistingBackup;FHSRSaveData BackupData;FHSRSaveEnvelopeHeader BackupHeader;if(UGameplayStatics::DoesSaveGameExist(Backup,UserIndex)&&ReadValid(Backup,ExistingBackup,BackupData,BackupHeader)){if(BackupHeader.Generation==MAX_uint64)return Fail(EHSRSaveFailureStage::Encode,EHSRSaveResult::InvalidEnvelope);Id=BackupHeader.SaveId;Gen=BackupHeader.Generation+1;}}
#if WITH_DEV_AUTOMATION_TESTS
	if(InjectedTransactionStage==EHSRSaveFailureStage::Encode)return Fail(EHSRSaveFailureStage::Encode,EHSRSaveResult::InvalidData);
#endif
	TArray<uint8> Bytes;if(!HSRSaveVersion::EncodeEnvelope(Captured,SlotName,UserIndex,Id,Gen,Bytes))return Fail(EHSRSaveFailureStage::Encode,EHSRSaveResult::InvalidData);
#if WITH_DEV_AUTOMATION_TESTS
	if(bInjectSaveFailure||InjectedTransactionStage==EHSRSaveFailureStage::StagingWrite)return Fail(EHSRSaveFailureStage::StagingWrite,EHSRSaveResult::SaveFailed);
#endif
	if(!UGameplayStatics::SaveDataToSlot(Bytes,Staging,UserIndex))return Fail(EHSRSaveFailureStage::StagingWrite,EHSRSaveResult::SaveFailed);TArray<uint8> Check;FHSRSaveData CheckData;FHSRSaveEnvelopeHeader CheckHeader;
#if WITH_DEV_AUTOMATION_TESTS
	if(InjectedTransactionStage==EHSRSaveFailureStage::StagingReadback)return Fail(EHSRSaveFailureStage::StagingReadback,EHSRSaveResult::LoadFailed);
#endif
	const bool bStagingLoaded=UGameplayStatics::LoadDataFromSlot(Check,Staging,UserIndex);const EHSRSaveDecodeResult StagingDecode=bStagingLoaded?HSRSaveVersion::DecodeEnvelope(Check,SlotName,UserIndex,CheckData,&CheckHeader):EHSRSaveDecodeResult::TooShort;const bool bStagingValid=StagingDecode==EHSRSaveDecodeResult::Success&&Validate(CheckData);if(!bStagingValid||Check!=Bytes||CheckHeader.SaveId!=Id||CheckHeader.Generation!=Gen){UE_LOG(LogTemp,Warning,TEXT("HSR save staging validation failed Loaded=%d Decode=%d Valid=%d Bytes=%d Id=%d Gen=%d"),bStagingLoaded?1:0,static_cast<int32>(StagingDecode),bStagingValid?1:0,Check==Bytes?1:0,CheckHeader.SaveId==Id?1:0,CheckHeader.Generation==Gen?1:0);return Fail(EHSRSaveFailureStage::StagingReadback,EHSRSaveResult::LoadFailed);}
	if(bOldPrimaryValid){
#if WITH_DEV_AUTOMATION_TESTS
		if(InjectedTransactionStage==EHSRSaveFailureStage::BackupWrite)return Fail(EHSRSaveFailureStage::BackupWrite,EHSRSaveResult::SaveFailed);
#endif
		if(!UGameplayStatics::SaveDataToSlot(Old,Backup,UserIndex))return Fail(EHSRSaveFailureStage::BackupWrite,EHSRSaveResult::SaveFailed);
#if WITH_DEV_AUTOMATION_TESTS
		if(InjectedTransactionStage==EHSRSaveFailureStage::BackupReadback)return Fail(EHSRSaveFailureStage::BackupReadback,EHSRSaveResult::LoadFailed);
#endif
		TArray<uint8> BackupCheck;FHSRSaveData BackupCheckData;FHSRSaveEnvelopeHeader BackupCheckHeader;if(!ReadValid(Backup,BackupCheck,BackupCheckData,BackupCheckHeader)||BackupCheck!=Old||BackupCheckHeader.SaveId!=OldHeader.SaveId||BackupCheckHeader.Generation!=OldHeader.Generation)return Fail(EHSRSaveFailureStage::BackupReadback,EHSRSaveResult::LoadFailed);
	}
#if WITH_DEV_AUTOMATION_TESTS
	if(InjectedTransactionStage==EHSRSaveFailureStage::PrimaryWrite)return Fail(EHSRSaveFailureStage::PrimaryWrite,EHSRSaveResult::SaveFailed);
#endif
	if(!UGameplayStatics::SaveDataToSlot(Bytes,SlotName,UserIndex))return Fail(EHSRSaveFailureStage::PrimaryWrite,EHSRSaveResult::SaveFailed);
#if WITH_DEV_AUTOMATION_TESTS
	if(InjectedTransactionStage==EHSRSaveFailureStage::PrimaryReadback)return Fail(EHSRSaveFailureStage::PrimaryReadback,EHSRSaveResult::LoadFailed);
#endif
	if(!ReadValid(SlotName,Check,CheckData,CheckHeader)||Check!=Bytes||CheckHeader.SaveId!=Id||CheckHeader.Generation!=Gen)return Fail(EHSRSaveFailureStage::PrimaryReadback,EHSRSaveResult::LoadFailed);LastWriteHeader=CheckHeader;
#if WITH_DEV_AUTOMATION_TESTS
	if(InjectedTransactionStage==EHSRSaveFailureStage::Cleanup){bLastWriteCleanupWarning=true;return EHSRSaveResult::Success;}
#endif
	if(UGameplayStatics::DoesSaveGameExist(Staging,UserIndex)&&!UGameplayStatics::DeleteGameInSlot(Staging,UserIndex))bLastWriteCleanupWarning=true;return EHSRSaveResult::Success;
}
EHSRSaveResult UHSRSaveSubsystem::LoadFromSlot(const FString& SlotName,int32 UserIndex)
{
	LastLoadResult=FHSRSaveLoadResult();
	if(!HSRSaveVersion::IsValidSlot(SlotName,UserIndex)||SlotName.Contains(TEXT(".__hsr_"))){LastLoadResult.Result=EHSRSaveResult::InvalidArgument;LastLoadResult.PrimaryStageReason=EHSRSaveLoadReason::InvalidArgument;return LastLoadResult.Result;}
	if(bOperationInProgress){LastLoadResult.Result=EHSRSaveResult::InvalidArgument;LastLoadResult.PrimaryStageReason=EHSRSaveLoadReason::Busy;return LastLoadResult.Result;}
	const UHSRBattleTransitionSubsystem* Battle=GetGameInstance()?GetGameInstance()->GetSubsystem<UHSRBattleTransitionSubsystem>():nullptr;
#if WITH_DEV_AUTOMATION_TESTS
	const bool bMapBlocked=bInjectMapTravelPending||(Map.IsValid()&&Map->HasPendingTravel());const bool bBattleBlocked=bInjectBattleReturnPending||(Battle&&Battle->HasReturnPending());
#else
	const bool bMapBlocked=Map.IsValid()&&Map->HasPendingTravel();const bool bBattleBlocked=Battle&&Battle->HasReturnPending();
#endif
	if(bMapBlocked||bBattleBlocked){LastLoadResult.Result=EHSRSaveResult::InvalidData;LastLoadResult.PrimaryStageReason=EHSRSaveLoadReason::TravelPending;return LastLoadResult.Result;}
	TGuardValue<bool> OperationGuard(bOperationInProgress,true);
#if WITH_DEV_AUTOMATION_TESTS
	if(bInjectLoadFailure){LastLoadResult.Result=EHSRSaveResult::LoadFailed;return LastLoadResult.Result;}
#endif
	const FString BackupSlot=SlotName+TEXT(".__hsr_backup_v1");FHSRSaveData Selected;FHSRSaveEnvelopeHeader PrimaryHeader,BackupHeader;bool bHaveSelected=false;bool bPrimaryTrusted=false;EHSRSaveResult PrimaryFailureResult=EHSRSaveResult::InvalidEnvelope;
	if(UGameplayStatics::DoesSaveGameExist(SlotName,UserIndex))
	{
		TArray<uint8> PrimaryBytes;FHSRSaveData PrimaryData;if(UGameplayStatics::LoadDataFromSlot(PrimaryBytes,SlotName,UserIndex))
		{
			const EHSRSaveDecodeResult Reason=HSRSaveVersion::DecodeEnvelope(PrimaryBytes,SlotName,UserIndex,PrimaryData,&PrimaryHeader);LastLoadResult.PrimaryReason=static_cast<uint8>(Reason);bPrimaryTrusted=PrimaryHeader.SaveId.IsValid();LastLoadResult.bPrimaryHeaderTrusted=bPrimaryTrusted;
			if(Reason==EHSRSaveDecodeResult::Success){if(CanPrepareSnapshot(PrimaryData)){Selected=MoveTemp(PrimaryData);bHaveSelected=true;LastLoadResult.Source=EHSRSaveLoadSource::Primary;LastLoadResult.SaveId=PrimaryHeader.SaveId;LastLoadResult.Generation=PrimaryHeader.Generation;}else LastLoadResult.PrimaryStageReason=EHSRSaveLoadReason::PrepareFailed;}
			else if(Reason==EHSRSaveDecodeResult::BadMagic)
			{
				USaveGame* LegacyObject=UGameplayStatics::LoadGameFromSlot(SlotName,UserIndex);const UHSRSaveGame* Legacy=Cast<UHSRSaveGame>(LegacyObject);const bool bSupportedLegacy=Legacy&&(Legacy->Data.SchemaVersion<=5||Legacy->Data.SchemaVersion==HSRSaveVersion::CurrentSchema);if(bSupportedLegacy&&CanPrepareSnapshot(Legacy->Data)){Selected=Legacy->Data;bHaveSelected=true;LastLoadResult.Source=EHSRSaveLoadSource::LegacyPrimary;}else{LastLoadResult.PrimaryStageReason=EHSRSaveLoadReason::LegacyInvalid;PrimaryFailureResult=!LegacyObject?EHSRSaveResult::LoadFailed:!Legacy?EHSRSaveResult::ClassMismatch:!bSupportedLegacy?EHSRSaveResult::UnsupportedSchema:EHSRSaveResult::InvalidData;}
			}
			else LastLoadResult.PrimaryStageReason=EHSRSaveLoadReason::DecodeFailure;
		}
		else LastLoadResult.PrimaryStageReason=EHSRSaveLoadReason::DecodeFailure;
	}
	else{LastLoadResult.PrimaryStageReason=EHSRSaveLoadReason::Missing;LastLoadResult.PrimaryReason=static_cast<uint8>(EHSRSaveDecodeResult::TooShort);}
	if(!bHaveSelected&&UGameplayStatics::DoesSaveGameExist(BackupSlot,UserIndex))
	{
		TArray<uint8> BackupBytes;FHSRSaveData BackupData;const bool bRead=UGameplayStatics::LoadDataFromSlot(BackupBytes,BackupSlot,UserIndex);const EHSRSaveDecodeResult Reason=bRead?HSRSaveVersion::DecodeEnvelope(BackupBytes,SlotName,UserIndex,BackupData,&BackupHeader):EHSRSaveDecodeResult::TooShort;LastLoadResult.BackupReason=static_cast<uint8>(Reason);
		if(Reason==EHSRSaveDecodeResult::Success)
		{
			bool bLineageValid=true;if(bPrimaryTrusted){if(BackupHeader.SaveId!=PrimaryHeader.SaveId){bLineageValid=false;LastLoadResult.BackupStageReason=EHSRSaveLoadReason::LineageMismatch;}else if(BackupHeader.Generation>=PrimaryHeader.Generation){bLineageValid=false;LastLoadResult.BackupStageReason=EHSRSaveLoadReason::InvalidGeneration;}}
			if(bLineageValid&&CanPrepareSnapshot(BackupData)){Selected=MoveTemp(BackupData);bHaveSelected=true;LastLoadResult.Source=EHSRSaveLoadSource::Backup;LastLoadResult.SaveId=BackupHeader.SaveId;LastLoadResult.Generation=BackupHeader.Generation;LastLoadResult.bRecoveredFromBackup=true;LastLoadResult.bPrimaryUntrusted=!bPrimaryTrusted;}else if(bLineageValid)LastLoadResult.BackupStageReason=EHSRSaveLoadReason::PrepareFailed;
		}
		else LastLoadResult.BackupStageReason=EHSRSaveLoadReason::DecodeFailure;
	}
	else if(!bHaveSelected){LastLoadResult.BackupStageReason=EHSRSaveLoadReason::Missing;LastLoadResult.BackupReason=static_cast<uint8>(EHSRSaveDecodeResult::TooShort);}
	if(!bHaveSelected){LastLoadResult.Result=LastLoadResult.PrimaryStageReason==EHSRSaveLoadReason::Missing&&LastLoadResult.BackupStageReason==EHSRSaveLoadReason::Missing?EHSRSaveResult::SlotNotFound:LastLoadResult.PrimaryStageReason==EHSRSaveLoadReason::LegacyInvalid?PrimaryFailureResult:EHSRSaveResult::InvalidEnvelope;return LastLoadResult.Result;}
	const int64 BeforeRevision=RestoreTransactionRevision;LastLoadResult.Result=LoadSnapshot(Selected);LastLoadResult.bRuntimeChanged=RestoreTransactionRevision!=BeforeRevision;if(LastLoadResult.Result!=EHSRSaveResult::Success&&LastLoadResult.Source!=EHSRSaveLoadSource::None){if(LastLoadResult.Source==EHSRSaveLoadSource::Backup)LastLoadResult.BackupStageReason=EHSRSaveLoadReason::ProjectionFailed;else LastLoadResult.PrimaryStageReason=EHSRSaveLoadReason::ProjectionFailed;}return LastLoadResult.Result;
}
