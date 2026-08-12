#include "HSRGameModeBase.h"

#include "../Character/HSRCharacterBase.h"
#include "../Data/Definitions/HSRCharacterCatalog.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Data/Definitions/HSRMapCatalog.h"
#include "../Data/Definitions/HSRMapDefinition.h"
#include "../Data/Definitions/HSRTeleportDefinition.h"
#include "../Map/HSRMapSubsystem.h"
#include "../Party/HSRPartySubsystem.h"
#include "../Player/HSRPlayerController.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "Curves/CurveFloat.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"

AHSRGameModeBase::AHSRGameModeBase()
{
	// BP_HSRGameMode already sets this in the asset, so exploration works today. Pinning the
	// same default in C++ costs nothing and stops a newly authored GameMode Blueprint from
	// silently inheriting the engine controller -- exactly how the battle GameMode lost its
	// input handling.
	PlayerControllerClass = AHSRPlayerController::StaticClass();
}

void AHSRGameModeBase::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);
	if (NewPlayer && NewPlayer->GetPawn())
	{
#if WITH_DEV_AUTOMATION_TESTS
		AutomationController = NewPlayer;
#endif
		BootstrapMapDefinitions();
		BootstrapCharacterIdentity(CharacterBootstrapMode);
	}
}

EHSRMapBootstrapResult AHSRGameModeBase::BootstrapMapDefinitions()
{
	if (!MapCatalog)
	{
		LastMapBootstrapResult = EHSRMapBootstrapResult::MissingCatalog;
		return LastMapBootstrapResult;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UHSRMapSubsystem* Maps = GameInstance ? GameInstance->GetSubsystem<UHSRMapSubsystem>() : nullptr;
	if (!Maps)
	{
		LastMapBootstrapResult = EHSRMapBootstrapResult::MissingCatalog;
		return LastMapBootstrapResult;
	}

	for (const TObjectPtr<UHSRMapDefinition>& MapEntry : MapCatalog->Maps)
	{
		if (!MapEntry)
		{
			LastMapBootstrapResult = EHSRMapBootstrapResult::MapRegistrationFailed;
			return LastMapBootstrapResult;
		}
		const EHSRMapOperationResult RegisterResult = Maps->RegisterMapAsset(MapEntry);
		if (RegisterResult != EHSRMapOperationResult::Success && RegisterResult != EHSRMapOperationResult::NoOp)
		{
			UE_LOG(LogTemp, Warning, TEXT("P18 MapBootstrap map '%s' register failed result=%d"),
				*MapEntry->MapId.ToString(), static_cast<int32>(RegisterResult));
			LastMapBootstrapResult = EHSRMapBootstrapResult::MapRegistrationFailed;
			return LastMapBootstrapResult;
		}
	}

	for (const TObjectPtr<UHSRTeleportDefinition>& TeleportEntry : MapCatalog->Teleports)
	{
		if (!TeleportEntry)
		{
			LastMapBootstrapResult = EHSRMapBootstrapResult::TeleportRegistrationFailed;
			return LastMapBootstrapResult;
		}
		const EHSRMapOperationResult RegisterResult = Maps->RegisterTeleportAsset(TeleportEntry);
		if (RegisterResult != EHSRMapOperationResult::Success && RegisterResult != EHSRMapOperationResult::NoOp)
		{
			UE_LOG(LogTemp, Warning, TEXT("P18 MapBootstrap teleport '%s' register failed result=%d"),
				*TeleportEntry->TeleportId.ToString(), static_cast<int32>(RegisterResult));
			LastMapBootstrapResult = EHSRMapBootstrapResult::TeleportRegistrationFailed;
			return LastMapBootstrapResult;
		}
	}

	// Unlock every catalog region so all authored teleports are reachable from their source maps.
	for (const TObjectPtr<UHSRMapDefinition>& MapEntry : MapCatalog->Maps)
	{
		if (!MapEntry)
		{
			LastMapBootstrapResult = EHSRMapBootstrapResult::MapRegistrationFailed;
			return LastMapBootstrapResult;
		}
		const EHSRMapOperationResult UnlockResult = Maps->UnlockRegion(MapEntry->RegionId);
		if (UnlockResult != EHSRMapOperationResult::Success && UnlockResult != EHSRMapOperationResult::NoOp)
		{
			UE_LOG(LogTemp, Warning, TEXT("P18 MapBootstrap region '%s' unlock failed result=%d"),
				*MapEntry->RegionId.ToString(), static_cast<int32>(UnlockResult));
			LastMapBootstrapResult = EHSRMapBootstrapResult::MapRegistrationFailed;
			return LastMapBootstrapResult;
		}
	}

	if (!InitialMapId.IsNone())
	{
		const EHSRMapOperationResult LocationResult = Maps->SetCurrentLocation(InitialMapId);
		if (LocationResult != EHSRMapOperationResult::Success && LocationResult != EHSRMapOperationResult::NoOp)
		{
			UE_LOG(LogTemp, Warning, TEXT("P18 MapBootstrap initial location '%s' failed result=%d"),
				*InitialMapId.ToString(), static_cast<int32>(LocationResult));
			LastMapBootstrapResult = EHSRMapBootstrapResult::InitialLocationFailed;
			return LastMapBootstrapResult;
		}
	}

	LastMapBootstrapResult = EHSRMapBootstrapResult::Success;
	UE_LOG(LogTemp, Log, TEXT("P18 MapBootstrap registered %d maps %d teleports regions unlocked initial=%s"),
		MapCatalog->Maps.Num(), MapCatalog->Teleports.Num(), *InitialMapId.ToString());
	return LastMapBootstrapResult;
}

EHSRCharacterBootstrapResult AHSRGameModeBase::BootstrapCharacterIdentity(const EHSRCharacterBootstrapMode Mode)
{
	AController* Controller = ResolveBootstrapController();
	AHSRCharacterBase* Character = Controller ? Cast<AHSRCharacterBase>(Controller->GetPawn()) : nullptr;
	if (!Character)
	{
		return FinishBootstrap(EHSRCharacterBootstrapResult::PawnProjectionFailed);
	}
	if (!CharacterCatalog)
	{
		return FinishBootstrap(EHSRCharacterBootstrapResult::MissingCatalog);
	}

	UGameInstance* GameInstance = GetGameInstance();
	UHSRCharacterProfileSubsystem* Profiles = GameInstance ? GameInstance->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
	UHSRPartySubsystem* Party = GameInstance ? GameInstance->GetSubsystem<UHSRPartySubsystem>() : nullptr;
	if (!Profiles || !Party)
	{
		return FinishBootstrap(EHSRCharacterBootstrapResult::PartyUnavailable);
	}

	TSet<FName> CatalogIds;
	bool bContainsInitialCharacter = false;
	int32 RegisteredDefinitionCount = 0;
	for (const TSubclassOf<UHSRCharacterDefinition>& Entry : CharacterCatalog->Characters)
	{
		const UHSRCharacterDefinition* Definition = Entry ? Entry->GetDefaultObject<UHSRCharacterDefinition>() : nullptr;
		if (!Definition || Definition->CharacterId.IsNone() || CatalogIds.Contains(Definition->CharacterId)
			|| Definition->CumulativeExperienceCurve.IsNull()
			|| !Definition->CumulativeExperienceCurve.LoadSynchronous())
		{
			return FinishBootstrap(EHSRCharacterBootstrapResult::CatalogConflict);
		}

		CatalogIds.Add(Definition->CharacterId);
		bContainsInitialCharacter |= Definition->CharacterId == InitialCharacterId;
		const UHSRCharacterDefinition* RegisteredDefinition = nullptr;
		if (Profiles->GetDefinition(Definition->CharacterId, RegisteredDefinition))
		{
			if (RegisteredDefinition != Definition)
			{
				return FinishBootstrap(EHSRCharacterBootstrapResult::CatalogConflict);
			}
			++RegisteredDefinitionCount;
		}
	}

	if (InitialCharacterId.IsNone() || !bContainsInitialCharacter)
	{
		return FinishBootstrap(EHSRCharacterBootstrapResult::InvalidInitialCharacter);
	}
	if (RegisteredDefinitionCount != 0 && RegisteredDefinitionCount != CatalogIds.Num())
	{
		return FinishBootstrap(EHSRCharacterBootstrapResult::CatalogConflict);
	}
	if (RegisteredDefinitionCount == 0
		&& Profiles->RegisterLoadedCatalog(CharacterCatalog) != EHSRCharacterProfileResult::Success)
	{
		return FinishBootstrap(EHSRCharacterBootstrapResult::ProfileRegistrationFailed);
	}

	FHSRPartySnapshot PartySnapshot;
	if (!Party->GetSnapshot(PartySnapshot) || PartySnapshot.Slots.IsEmpty())
	{
		return FinishBootstrap(EHSRCharacterBootstrapResult::PartyUnavailable);
	}

	bool bSeededParty = false;
	if (PartySnapshot.Slots[0].IsEmpty())
	{
		const bool bHasCommittedPartyMember = PartySnapshot.Slots.ContainsByPredicate(
			[](const FHSRPartySlot& Slot) { return !Slot.IsEmpty(); });
		if (bHasCommittedPartyMember)
		{
			return FinishBootstrap(EHSRCharacterBootstrapResult::NoCommittedSelection);
		}
		if (Mode == EHSRCharacterBootstrapMode::UseCommittedRuntime)
		{
			return FinishBootstrap(EHSRCharacterBootstrapResult::NoCommittedSelection);
		}
		if (Party->AddCharacter(InitialCharacterId, 0) != EHSRPartyResult::Success)
		{
			return FinishBootstrap(EHSRCharacterBootstrapResult::PartyUnavailable);
		}
		bSeededParty = true;
		Party->GetSnapshot(PartySnapshot);
	}

	const FName SelectedCharacterId = PartySnapshot.Slots[0].CharacterId;
	FHSRCharacterProfileSnapshot SelectedProfile;
	const UHSRCharacterDefinition* SelectedDefinition = nullptr;
	if (SelectedCharacterId.IsNone() || !Profiles->GetProfileSnapshot(SelectedCharacterId, SelectedProfile)
		|| !Profiles->GetDefinition(SelectedCharacterId, SelectedDefinition) || !SelectedDefinition)
	{
		return FinishBootstrap(EHSRCharacterBootstrapResult::NoCommittedSelection);
	}
	if (!Character->SetProjectedCharacterId(SelectedCharacterId))
	{
		return FinishBootstrap(EHSRCharacterBootstrapResult::PawnProjectionFailed);
	}

	return FinishBootstrap(bSeededParty ? EHSRCharacterBootstrapResult::Success : EHSRCharacterBootstrapResult::NoOp,
		SelectedCharacterId);
}

EHSRCharacterBootstrapResult AHSRGameModeBase::FinishBootstrap(const EHSRCharacterBootstrapResult Result,
	const FName CharacterId)
{
	LastCharacterBootstrapResult = Result;
	const bool bSucceeded = Result == EHSRCharacterBootstrapResult::Success
		|| Result == EHSRCharacterBootstrapResult::NoOp;
	if (bSucceeded)
	{
		ResolvedCharacterId = CharacterId;
	}
	if (bSucceeded)
	{
		UE_LOG(LogTemp, Log, TEXT("P17-PATCH-03B Bootstrap Result=%d CharacterId=%s"),
			static_cast<int32>(Result), *CharacterId.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("P17-PATCH-03B Bootstrap Result=%d CharacterId=%s"),
			static_cast<int32>(Result), *CharacterId.ToString());
	}
	return Result;
}

AController* AHSRGameModeBase::ResolveBootstrapController() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (AutomationController.IsValid())
	{
		return AutomationController.Get();
	}
#endif
	return GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
}

#if WITH_DEV_AUTOMATION_TESTS
void AHSRGameModeBase::ConfigureCharacterBootstrapForAutomation(UHSRCharacterCatalog* InCatalog,
	const FName InInitialCharacterId, AController* InController)
{
	CharacterCatalog = InCatalog;
	InitialCharacterId = InInitialCharacterId;
	AutomationController = InController;
}

void AHSRGameModeBase::ConfigureMapBootstrapForAutomation(UHSRMapCatalog* InCatalog, const FName InInitialMapId)
{
	MapCatalog = InCatalog;
	InitialMapId = InInitialMapId;
}
#endif
