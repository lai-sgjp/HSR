#include "HSRGameModeBase.h"

#include "../Character/HSRCharacterBase.h"
#include "../Data/Definitions/HSRCharacterCatalog.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Party/HSRPartySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "Curves/CurveFloat.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"

AHSRGameModeBase::AHSRGameModeBase()
{
}

void AHSRGameModeBase::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);
	if (NewPlayer && NewPlayer->GetPawn())
	{
#if WITH_DEV_AUTOMATION_TESTS
		AutomationController = NewPlayer;
#endif
		BootstrapCharacterIdentity(CharacterBootstrapMode);
	}
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
#endif
