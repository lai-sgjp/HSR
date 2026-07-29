#include "HSREquipmentDevelopmentHarness.h"

#include "HSREquipmentSubsystem.h"
#include "../Data/Definitions/HSREquipmentDefinition.h"
#include "../Data/Definitions/HSRRelicDefinition.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Data/Definitions/HSRItemEquipmentMappingCatalog.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Save/HSRSaveSubsystem.h"
#include "../UI/HSREquipmentDetailViewModel.h"
#include "../UI/HSREquipmentDetailWidget.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const FName CharacterName(TEXT("Character.A"));
	const FGuid WeaponInstance(0x12004001,0,0,1);
	const FGuid HeadInstance(0x12004002,0,0,1);
	const FGuid HandsInstance(0x12004003,0,0,1);
	const FString SaveSlot(TEXT("HSR_P12_Development"));
	TWeakObjectPtr<UHSREquipmentDetailWidget> DetailWidget;
	TMap<TObjectKey<UHSREquipmentSubsystem>,int32> LastHarnessRevisions;

	UGameInstance* FindPIEGameInstance()
	{
		if(!GEngine)return nullptr;
		for(const FWorldContext& Context:GEngine->GetWorldContexts())
			if(UWorld* World=Context.World();World&&World->IsPlayInEditor())return World->GetGameInstance();
		return nullptr;
	}

	bool RegisterDefinitions(UHSREquipmentSubsystem* Equipment,FName& OutHeadId,FName& OutHandsId,FName& OutSetId)
	{
		if(!Equipment){UE_LOG(LogTemp,Error,TEXT("HSR.EquipmentHarness RegisterDefinitions FAIL MissingSubsystem"));return false;}
		UHSREquipmentDefinition* Weapon=NewObject<UHSREquipmentDefinition>(GetTransientPackage());
		Weapon->DefinitionId=TEXT("Equipment.P12.FixedWeapon");Weapon->Slot=EHSREquipmentSlot::Weapon;Weapon->EnhancementCap=15;
		const EHSREquipmentOperationResult WeaponResult=Equipment->RegisterDefinition(*Weapon);
		if(WeaponResult!=EHSREquipmentOperationResult::Success&&WeaponResult!=EHSREquipmentOperationResult::DuplicateDefinitionId){UE_LOG(LogTemp,Error,TEXT("HSR.EquipmentHarness RegisterDefinitions FAIL WeaponResult=%d"),static_cast<int32>(WeaponResult));return false;}
		UHSRRelicDefinition* Head=LoadObject<UHSRRelicDefinition>(nullptr,TEXT("/Game/Data/Relics/DA_Relic_Head.DA_Relic_Head"));
		UHSRRelicDefinition* Hands=LoadObject<UHSRRelicDefinition>(nullptr,TEXT("/Game/Data/Relics/DA_Relic_Hands.DA_Relic_Hands"));
		if(!Head||!Hands||Head->DefinitionId.IsNone()||Hands->DefinitionId.IsNone()||Head->SetId.IsNone()||Head->SetId!=Hands->SetId){UE_LOG(LogTemp,Error,TEXT("HSR.EquipmentHarness RegisterDefinitions FAIL Head=%s HeadId=%s HeadSet=%s Hands=%s HandsId=%s HandsSet=%s"),Head?TEXT("Loaded"):TEXT("Missing"),Head?*Head->DefinitionId.ToString():TEXT("None"),Head?*Head->SetId.ToString():TEXT("None"),Hands?TEXT("Loaded"):TEXT("Missing"),Hands?*Hands->DefinitionId.ToString():TEXT("None"),Hands?*Hands->SetId.ToString():TEXT("None"));return false;}
		const EHSREquipmentOperationResult HeadResult=Equipment->RegisterDefinition(*Head);
		const EHSREquipmentOperationResult HandsResult=Equipment->RegisterDefinition(*Hands);
		if((HeadResult!=EHSREquipmentOperationResult::Success&&HeadResult!=EHSREquipmentOperationResult::DuplicateDefinitionId)||(HandsResult!=EHSREquipmentOperationResult::Success&&HandsResult!=EHSREquipmentOperationResult::DuplicateDefinitionId)){UE_LOG(LogTemp,Error,TEXT("HSR.EquipmentHarness RegisterDefinitions FAIL HeadResult=%d HandsResult=%d"),static_cast<int32>(HeadResult),static_cast<int32>(HandsResult));return false;}
		OutHeadId=Head->DefinitionId;OutHandsId=Hands->DefinitionId;OutSetId=Head->SetId;return true;
	}

	bool Commit(UGameInstance* GameInstance,const FHSREquipmentRestoreState* Desired)
	{
		UHSREquipmentSubsystem* Equipment=GameInstance?GameInstance->GetSubsystem<UHSREquipmentSubsystem>():nullptr;
		if(!Equipment){UE_LOG(LogTemp,Error,TEXT("HSR.EquipmentHarness Commit FAIL MissingEquipmentSubsystem"));return false;}
		FHSREquipmentRestoreMap Candidate;
		const FGuid CharacterId=HSRCharacterGuidFromProfileName(CharacterName);
		if(Desired)Candidate.Add(CharacterId,*Desired);
		if(!Equipment->ProjectRestore(Candidate)){UE_LOG(LogTemp,Error,TEXT("HSR.EquipmentHarness Commit FAIL ProjectRestore CandidateCharacters=%d Desired=%d"),Candidate.Num(),Desired?1:0);return false;}
		Equipment->CommitRestore(Candidate);Equipment->NotifyRestored({CharacterId});if(Desired)LastHarnessRevisions.Add(Equipment,Desired->Revision);return true;
	}

	bool Commit(UHSREquipmentSubsystem* Equipment,const FHSREquipmentRestoreState* Desired)
	{
		if(!Equipment){UE_LOG(LogTemp,Error,TEXT("HSR.EquipmentHarness CommitForTest FAIL MissingEquipmentSubsystem"));return false;}
		FHSREquipmentRestoreMap Candidate;
		const FGuid CharacterId=HSRCharacterGuidFromProfileName(CharacterName);
		if(Desired)Candidate.Add(CharacterId,*Desired);
		if(!Equipment->ProjectRestore(Candidate)){UE_LOG(LogTemp,Error,TEXT("HSR.EquipmentHarness CommitForTest FAIL ProjectRestore CandidateCharacters=%d Desired=%d"),Candidate.Num(),Desired?1:0);return false;}
		Equipment->CommitRestore(Candidate);Equipment->NotifyRestored({CharacterId});if(Desired)LastHarnessRevisions.Add(Equipment,Desired->Revision);return true;
	}

	FHSREquipmentRestoreState BuildFixedState(UHSREquipmentSubsystem* Equipment,bool bIncludeHands)
	{
		FName HeadId,HandsId,SetId;FHSREquipmentRestoreState State;
		if(!RegisterDefinitions(Equipment,HeadId,HandsId,SetId)){UE_LOG(LogTemp,Error,TEXT("HSR.EquipmentHarness BuildFixedState FAIL RegisterDefinitions"));return State;}
		FHSREquipmentInstance Weapon;Weapon.InstanceId=WeaponInstance;Weapon.DefinitionId=TEXT("Equipment.P12.FixedWeapon");Weapon.Kind=EHSREquipmentKind::Equipment;Weapon.EnhancementLevel=3;Weapon.Modifiers.Add({EHSREquipmentStat::Attack,20.0f});State.Loadout.Equipment.Add(EHSREquipmentSlot::Weapon,Weapon);
		FHSREquipmentInstance Head;Head.InstanceId=HeadInstance;Head.DefinitionId=HeadId;Head.Kind=EHSREquipmentKind::Relic;Head.EnhancementLevel=3;Head.Modifiers.Add({EHSREquipmentStat::Defense,12.0f});State.Loadout.Relics.Add(EHSRRelicSlot::Head,Head);
		if(bIncludeHands){FHSREquipmentInstance Hands;Hands.InstanceId=HandsInstance;Hands.DefinitionId=HandsId;Hands.Kind=EHSREquipmentKind::Relic;Hands.EnhancementLevel=3;Hands.Modifiers.Add({EHSREquipmentStat::Speed,4.0f});State.Loadout.Relics.Add(EHSRRelicSlot::Hands,Hands);}
		State.RelicSetCounts.Add(SetId,bIncludeHands?2:1);FHSREquipmentLoadout Existing;int32 ExistingRevision=0;Equipment->GetLoadout(HSRCharacterGuidFromProfileName(CharacterName),Existing,ExistingRevision);State.Revision=FMath::Max(FMath::Max(ExistingRevision,LastHarnessRevisions.FindRef(Equipment))+1,1);return State;
	}

	void LogResult(const TCHAR* Command,bool bSuccess){if(bSuccess){UE_LOG(LogTemp,Log,TEXT("HSR.EquipmentHarness Command=%s Result=SUCCESS"),Command);}else{UE_LOG(LogTemp,Error,TEXT("HSR.EquipmentHarness Command=%s Result=FAIL"),Command);}}
	void RunSetup(){UGameInstance* GI=FindPIEGameInstance();LogResult(TEXT("Setup"),FHSREquipmentDevelopmentHarness::SetupFixedLoadout(GI));}
	void RunRemove(){UGameInstance* GI=FindPIEGameInstance();LogResult(TEXT("RemoveSecondRelic"),FHSREquipmentDevelopmentHarness::RemoveSecondRelic(GI));}
	void RunRestore(){UGameInstance* GI=FindPIEGameInstance();LogResult(TEXT("RestoreSecondRelic"),FHSREquipmentDevelopmentHarness::RestoreSecondRelic(GI));}
	void RunClear(){UGameInstance* GI=FindPIEGameInstance();LogResult(TEXT("Clear"),FHSREquipmentDevelopmentHarness::ClearLoadout(GI));}
	void RunSave(){UGameInstance* GI=FindPIEGameInstance();LogResult(TEXT("Save"),FHSREquipmentDevelopmentHarness::Save(GI));}
	void RunLoad(){UGameInstance* GI=FindPIEGameInstance();LogResult(TEXT("Load"),FHSREquipmentDevelopmentHarness::Load(GI));}
	void RunCleanup(){LogResult(TEXT("Cleanup"),FHSREquipmentDevelopmentHarness::CleanupSave());}
	void RunP17Audit(){UGameInstance* GI=FindPIEGameInstance();LogResult(TEXT("P17MovementAudit"),FHSREquipmentDevelopmentHarness::RunP17MovementAudit(GI));}
	void RunHide(){if(DetailWidget.IsValid()){DetailWidget->RemoveFromParent();DetailWidget.Reset();}LogResult(TEXT("HideDetail"),true);}
	void RunShow()
	{
		UGameInstance* GI=FindPIEGameInstance();UWorld* World=GI?GI->GetWorld():nullptr;UHSREquipmentSubsystem* Equipment=GI?GI->GetSubsystem<UHSREquipmentSubsystem>():nullptr;
		TSubclassOf<UHSREquipmentDetailWidget> WidgetClass=LoadClass<UHSREquipmentDetailWidget>(nullptr,TEXT("/Game/UI/WBP_EquipmentDetail_P12.WBP_EquipmentDetail_P12_C"));
		if(!World||!Equipment||!WidgetClass){LogResult(TEXT("ShowDetail"),false);return;}
		RunHide();UHSREquipmentDetailWidget* Widget=CreateWidget<UHSREquipmentDetailWidget>(World,WidgetClass);if(!Widget){LogResult(TEXT("ShowDetail"),false);return;}UHSREquipmentDetailViewModel* ViewModel=NewObject<UHSREquipmentDetailViewModel>(Widget);if(!ViewModel){LogResult(TEXT("ShowDetail"),false);return;}
		Widget->AddToViewport(100);Widget->SetViewModel(ViewModel);ViewModel->Initialize(Equipment,HSRCharacterGuidFromProfileName(CharacterName));DetailWidget=Widget;LogResult(TEXT("ShowDetail"),true);
	}

	FAutoConsoleCommand SetupCommand(TEXT("HSR.Equipment.Setup"),TEXT("Build the fixed Phase 12 loadout."),FConsoleCommandDelegate::CreateStatic(&RunSetup));
	FAutoConsoleCommand RemoveCommand(TEXT("HSR.Equipment.RemoveSecondRelic"),TEXT("Transition the fixed set from 2 to 1."),FConsoleCommandDelegate::CreateStatic(&RunRemove));
	FAutoConsoleCommand RestoreCommand(TEXT("HSR.Equipment.RestoreSecondRelic"),TEXT("Transition the fixed set from 1 to 2."),FConsoleCommandDelegate::CreateStatic(&RunRestore));
	FAutoConsoleCommand ClearCommand(TEXT("HSR.Equipment.Clear"),TEXT("Clear the fixed loadout."),FConsoleCommandDelegate::CreateStatic(&RunClear));
	FAutoConsoleCommand ShowCommand(TEXT("HSR.Equipment.ShowDetail"),TEXT("Create and show the read-only Phase 12 detail widget."),FConsoleCommandDelegate::CreateStatic(&RunShow));
	FAutoConsoleCommand HideCommand(TEXT("HSR.Equipment.HideDetail"),TEXT("Destroy the Phase 12 detail widget."),FConsoleCommandDelegate::CreateStatic(&RunHide));
	FAutoConsoleCommand SaveCommand(TEXT("HSR.Equipment.Save"),TEXT("Save the Phase 12 development slot."),FConsoleCommandDelegate::CreateStatic(&RunSave));
	FAutoConsoleCommand LoadCommand(TEXT("HSR.Equipment.Load"),TEXT("Load the Phase 12 development slot."),FConsoleCommandDelegate::CreateStatic(&RunLoad));
	FAutoConsoleCommand CleanupCommand(TEXT("HSR.Equipment.Cleanup"),TEXT("Delete the Phase 12 development slot."),FConsoleCommandDelegate::CreateStatic(&RunCleanup));
	FAutoConsoleCommand P17AuditCommand(TEXT("HSR.Equipment.P17Audit"),TEXT("Run the P17 equip, replace, and unequip transaction audit."),FConsoleCommandDelegate::CreateStatic(&RunP17Audit));
}

bool FHSREquipmentDevelopmentHarness::SetupFixedLoadout(UGameInstance* GI){UHSREquipmentSubsystem* E=GI?GI->GetSubsystem<UHSREquipmentSubsystem>():nullptr;if(!E)return false;const FHSREquipmentRestoreState State=BuildFixedState(E,true);return !State.Loadout.Relics.IsEmpty()&&Commit(GI,&State);}
bool FHSREquipmentDevelopmentHarness::RemoveSecondRelic(UGameInstance* GI){UHSREquipmentSubsystem* E=GI?GI->GetSubsystem<UHSREquipmentSubsystem>():nullptr;if(!E)return false;const FHSREquipmentRestoreState State=BuildFixedState(E,false);return !State.Loadout.Relics.IsEmpty()&&Commit(GI,&State);}
bool FHSREquipmentDevelopmentHarness::RestoreSecondRelic(UGameInstance* GI){return SetupFixedLoadout(GI);}
bool FHSREquipmentDevelopmentHarness::ClearLoadout(UGameInstance* GI){UHSREquipmentSubsystem* E=GI?GI->GetSubsystem<UHSREquipmentSubsystem>():nullptr;if(!E)return false;FHSREquipmentLoadout Existing;int32 Revision=0;E->GetLoadout(HSRCharacterGuidFromProfileName(CharacterName),Existing,Revision);const int32 ClearRevision=FMath::Max(FMath::Max(Revision,LastHarnessRevisions.FindRef(E))+1,1);if(!Commit(GI,nullptr))return false;LastHarnessRevisions.Add(E,ClearRevision);return true;}
bool FHSREquipmentDevelopmentHarness::Save(UGameInstance* GI){UHSRSaveSubsystem* S=GI?GI->GetSubsystem<UHSRSaveSubsystem>():nullptr;return S&&S->SaveToSlot(SaveSlot)==EHSRSaveResult::Success;}
bool FHSREquipmentDevelopmentHarness::Load(UGameInstance* GI){UHSREquipmentSubsystem* E=GI?GI->GetSubsystem<UHSREquipmentSubsystem>():nullptr;UHSRSaveSubsystem* S=GI?GI->GetSubsystem<UHSRSaveSubsystem>():nullptr;FName HeadId,HandsId,SetId;return E&&S&&RegisterDefinitions(E,HeadId,HandsId,SetId)&&S->LoadFromSlot(SaveSlot)==EHSRSaveResult::Success;}
bool FHSREquipmentDevelopmentHarness::CleanupSave(){return !UGameplayStatics::DoesSaveGameExist(SaveSlot,0)||UGameplayStatics::DeleteGameInSlot(SaveSlot,0);}
bool FHSREquipmentDevelopmentHarness::SetupFixedLoadoutForTest(UHSREquipmentSubsystem* E){if(!E)return false;const FHSREquipmentRestoreState State=BuildFixedState(E,true);return !State.Loadout.Relics.IsEmpty()&&Commit(E,&State);}
bool FHSREquipmentDevelopmentHarness::RemoveSecondRelicForTest(UHSREquipmentSubsystem* E){if(!E)return false;const FHSREquipmentRestoreState State=BuildFixedState(E,false);return !State.Loadout.Relics.IsEmpty()&&Commit(E,&State);}
bool FHSREquipmentDevelopmentHarness::ClearLoadoutForTest(UHSREquipmentSubsystem* E){if(!E)return false;FHSREquipmentLoadout Existing;int32 Revision=0;E->GetLoadout(HSRCharacterGuidFromProfileName(CharacterName),Existing,Revision);const int32 ClearRevision=FMath::Max(FMath::Max(Revision,LastHarnessRevisions.FindRef(E))+1,1);if(!Commit(E,nullptr))return false;LastHarnessRevisions.Add(E,ClearRevision);return true;}
bool FHSREquipmentDevelopmentHarness::RunP17MovementAudit(UGameInstance* GI){return RunP17MovementAuditForTest(GI?GI->GetSubsystem<UHSREquipmentSubsystem>():nullptr,GI?GI->GetSubsystem<UHSRInventorySubsystem>():nullptr);}
bool FHSREquipmentDevelopmentHarness::RunP17MovementAuditForTest(UHSREquipmentSubsystem* E,UHSRInventorySubsystem* I)
{
	if(!E||!I)return false;
	auto* Item=NewObject<UHSRItemDefinition>(GetTransientPackage());Item->ItemId=TEXT("Item.P17.Console");Item->StorageKind=EHSRItemStorageKind::Unique;Item->MaxStack=1;
	const EHSRInventoryOperationResult ItemResult=I->RegisterDefinition(*Item);if(ItemResult!=EHSRInventoryOperationResult::Success&&ItemResult!=EHSRInventoryOperationResult::DuplicateDefinitionId)return false;
	auto* Definition=NewObject<UHSREquipmentDefinition>(GetTransientPackage());Definition->DefinitionId=TEXT("Equipment.P17.Console");Definition->Slot=EHSREquipmentSlot::Body;Definition->EnhancementCap=1;
	const EHSREquipmentOperationResult DefinitionResult=E->RegisterDefinition(*Definition);if(DefinitionResult!=EHSREquipmentOperationResult::Success&&DefinitionResult!=EHSREquipmentOperationResult::DuplicateDefinitionId)return false;
	auto* Catalog=NewObject<UHSRItemEquipmentMappingCatalog>(GetTransientPackage());FHSRItemEquipmentMappingEntry Mapping;Mapping.ItemId=Item->ItemId;Mapping.EquipmentDefinitionId=Definition->DefinitionId;Mapping.Kind=EHSREquipmentKind::Equipment;Mapping.Slot=static_cast<int32>(EHSREquipmentSlot::Body);if(!Catalog->AddMapping(Mapping))return false;
	const FGuid CharacterId=HSRCharacterGuidFromProfileName(CharacterName);const FGuid First(0x17003001,0,0,1),Second(0x17003002,0,0,1);for(const FGuid Id:{First,Second}){FHSRItemInstance Bag;Bag.InstanceId=Id;Bag.DefinitionId=Item->ItemId;if(I->AddUnique(Bag)!=EHSRInventoryOperationResult::Success)return false;FHSREquipmentInstance Registry;Registry.InstanceId=Id;Registry.DefinitionId=Definition->DefinitionId;Registry.Kind=EHSREquipmentKind::Equipment;if(E->RegisterInstance(Registry)!=EHSREquipmentOperationResult::Success)return false;}
	auto Execute=[&](FGuid OperationId,FGuid InstanceId,EHSREquipmentMovementIntent Intent,int64 InventoryRevision,int64 EquipmentRevision){FHSREquipmentMovementRequest Request;Request.OperationId=OperationId;Request.CharacterId=CharacterId;Request.InstanceId=InstanceId;Request.Intent=Intent;Request.Kind=EHSREquipmentKind::Equipment;Request.Slot=static_cast<int32>(EHSREquipmentSlot::Body);Request.ExpectedInventoryRevision=InventoryRevision;Request.ExpectedEquipmentRevision=EquipmentRevision;return E->ExecuteMovement(Request,*I,*Catalog);};
	FHSRInventorySnapshot Inventory;FHSREquipmentLoadout InitialLoadout;int32 InitialEquipmentRevision=0;E->GetLoadout(CharacterId,InitialLoadout,InitialEquipmentRevision);I->GetSnapshot(Inventory);const FHSREquipmentMovementResult Equip=Execute(FGuid(0x17004001,0,0,1),First,EHSREquipmentMovementIntent::Equip,Inventory.Revision,InitialEquipmentRevision);if(Equip.Code!=EHSREquipmentMovementResultCode::Success)return false;I->GetSnapshot(Inventory);const FHSREquipmentMovementResult Replace=Execute(FGuid(0x17004002,0,0,1),Second,EHSREquipmentMovementIntent::Replace,Inventory.Revision,Equip.NewEquipmentRevision);if(Replace.Code!=EHSREquipmentMovementResultCode::Success)return false;I->GetSnapshot(Inventory);const FHSREquipmentMovementResult Unequip=Execute(FGuid(0x17004003,0,0,1),Second,EHSREquipmentMovementIntent::Unequip,Inventory.Revision,Replace.NewEquipmentRevision);FHSREquipmentLoadout FinalLoadout;int32 FinalRevision=0;E->GetLoadout(CharacterId,FinalLoadout,FinalRevision);return Unequip.Code==EHSREquipmentMovementResultCode::Success&&FinalLoadout.Equipment.Num()==InitialLoadout.Equipment.Num();
}
