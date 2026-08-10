#include "HSRPlayerController.h"
#include "../UI/HSRHUD.h"
#include "../Character/HSRExplorationCharacter.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "../UI/HSRScreenStackTypes.h"
#include "../UI/HSRUIManagerSubsystem.h"
#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "../Battle/HSRBattleGameMode.h"
#include "../Battle/HSRExplorationReturnConsumer.h"
#include "../Party/HSRPartySubsystem.h"
#include "../Party/HSRPartyTypes.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "EngineUtils.h"
#include "Framework/Application/NavigationConfig.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"

AHSRPlayerController::AHSRPlayerController()
{
	// PlayerController input, including Enhanced Input action evaluation, is
	// processed through the controller's per-frame player-input tick.
	PrimaryActorTick.bCanEverTick = true;
	CurrentControlMode = EHSRPlayerControlMode::Exploration;
	bControlModeApplied = false;
	bExplorationContextAdded = false;
	bFrontendNavigationContextAdded = false;
	bInputSystemReady = false;
	AppliedInputIntent = EHSRUIInputIntent::GameOnly;
}

void AHSRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	bInputSystemReady = true;
	if (InputComponent)
	{
		InputComponent->Priority = FrontendInputPriority;
		InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ThisClass::HandlePartySlot1);
		InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ThisClass::HandlePartySlot2);
		InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ThisClass::HandlePartySlot3);
		InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ThisClass::HandlePartySlot4);
	}
	AddFrontendNavigationContext();
	if (UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(InputComponent);
		Enhanced && ShouldBindFrontendInputComponent(FrontendBindingsInputComponent, InputComponent))
	{
		const auto BindStarted = [Enhanced, this](UInputAction* Action, void (AHSRPlayerController::*Handler)())
		{
			if (Action) Enhanced->BindAction(Action, ETriggerEvent::Started, this, Handler);
			else UE_LOG(LogTemp, Warning, TEXT("HSRUI Frontend action is not configured; binding skipped"));
		};
		BindStarted(PauseBackAction, &ThisClass::HandlePauseBack);
		BindStarted(InventoryAction, &ThisClass::HandleInventory);
		BindStarted(PartyAction, &ThisClass::HandleParty);
		BindStarted(MapAction, &ThisClass::HandleMap);
		BindStarted(ChallengeAction, &ThisClass::HandleChallenge);
		BindStarted(CloseToRootAction, &ThisClass::HandleCloseToRoot);
		FrontendBindingsInputComponent = InputComponent;
		const bool bPauseMapped = FrontendNavigationMappingContext && PauseBackAction
			&& FrontendNavigationMappingContext->GetMappings().ContainsByPredicate([this](const FEnhancedActionKeyMapping& Mapping)
			{
				return Mapping.Action == PauseBackAction;
			});
		if (FrontendNavigationMappingContext && PauseBackAction)
		{
			for (const FEnhancedActionKeyMapping& Mapping : FrontendNavigationMappingContext->GetMappings())
			{
				if (Mapping.Action == PauseBackAction)
				{
					UE_LOG(LogTemp, Log, TEXT("HSRUI Frontend PauseBack key=%s"), *Mapping.Key.ToString());
				}
			}
		}
		UE_LOG(LogTemp, Log, TEXT("HSRUI Frontend input bound Component=%s Priority=%d Context=%s PauseAction=%s PauseMapped=%s"),
			*InputComponent->GetName(), InputComponent->Priority,
			FrontendNavigationMappingContext ? *FrontendNavigationMappingContext->GetPathName() : TEXT("None"),
			PauseBackAction ? *PauseBackAction->GetPathName() : TEXT("None"), bPauseMapped ? TEXT("true") : TEXT("false"));
	}

	UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::SetupInputComponent - PlayerInput=%s InputComponent=%s"),
		PlayerInput ? *PlayerInput->GetClass()->GetName() : TEXT("None"),
		InputComponent ? *InputComponent->GetClass()->GetName() : TEXT("None"));

	if (IsLocalPlayerController() && CurrentControlMode == EHSRPlayerControlMode::Exploration)
	{
		AddExplorationContext();
	}
}

void AHSRPlayerController::BeginPlay()
{
	Super::BeginPlay();

	bControlModeApplied = false;
	InstallFrontendSlateNavigation();

	UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::BeginPlay - Controller=%s Local=%s Pawn=%s"),
		*GetName(),
		IsLocalController() ? TEXT("true") : TEXT("false"),
		GetPawn() ? *GetPawn()->GetName() : TEXT("None"));
	UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::BeginPlay - PlayerInput=%s InputComponent=%s"),
		PlayerInput ? *PlayerInput->GetClass()->GetName() : TEXT("None"),
		InputComponent ? *InputComponent->GetClass()->GetName() : TEXT("None"));

	// Battle return is a GameInstance transaction. Exploration maps may not
	// contain a placed consumer, so provide a runtime fallback at the map's
	// PlayerController lifecycle boundary without duplicating the transaction.
	if (UHSRBattleTransitionSubsystem* BattleTravel = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UHSRBattleTransitionSubsystem>() : nullptr;
		BattleTravel && BattleTravel->HasReturnPending() && GetWorld())
	{
		bool bConsumerPresent = false;
		for (TActorIterator<AHSRExplorationReturnConsumer> It(GetWorld()); It; ++It)
		{
			bConsumerPresent = true;
			break;
		}
		if (ShouldEnsureBattleReturnConsumer(true, bConsumerPresent))
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AHSRExplorationReturnConsumer* Consumer = GetWorld()->SpawnActor<AHSRExplorationReturnConsumer>(
				AHSRExplorationReturnConsumer::StaticClass(), FTransform::Identity, SpawnParameters))
			{
				UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::BeginPlay - Spawned battle return consumer %s"),
					*Consumer->GetName());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("AHSRPlayerController::BeginPlay - Failed to spawn battle return consumer"));
			}
		}
	}
	// Pick the mode from the world we actually landed in. Deciding here rather than from the
	// battle GameMode avoids the PlayerController-lookup timing problem entirely: inside our
	// own BeginPlay the controller is `this`, so there is nothing to find and nothing to defer.
	SetControlMode(ResolveControlModeForCurrentWorld());
}

void AHSRPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RestoreFrontendSlateNavigation();
	Super::EndPlay(EndPlayReason);
}

void AHSRPlayerController::ConfigureFrontendNavigation(FNavigationConfig& NavigationConfig)
{
	NavigationConfig.bTabNavigation = false;
}

void AHSRPlayerController::InstallFrontendSlateNavigation()
{
	if (!IsLocalPlayerController() || !FSlateApplication::IsInitialized() || FrontendSlateNavigationConfig)
	{
		return;
	}

	FSlateApplication& SlateApplication = FSlateApplication::Get();
	FrontendSlateUserIndex = SlateApplication.GetUserIndexForKeyboard();
	TSharedPtr<FSlateUser> SlateUser = SlateApplication.GetUser(FrontendSlateUserIndex);
	if (!SlateUser)
	{
		UE_LOG(LogTemp, Warning, TEXT("HSRUI Slate navigation unavailable User=%d"), FrontendSlateUserIndex);
		FrontendSlateUserIndex = INDEX_NONE;
		return;
	}
	PreviousSlateNavigationConfig = SlateUser->GetUserNavigationConfig();
	FrontendSlateNavigationConfig = MakeShared<FNavigationConfig>();
	ConfigureFrontendNavigation(*FrontendSlateNavigationConfig);
	SlateUser->SetUserNavigationConfig(FrontendSlateNavigationConfig);

	UE_LOG(LogTemp, Log, TEXT("HSRUI Slate navigation installed User=%d TabNavigation=%s"),
		FrontendSlateUserIndex, FrontendSlateNavigationConfig->bTabNavigation ? TEXT("true") : TEXT("false"));
}

void AHSRPlayerController::RestoreFrontendSlateNavigation()
{
	if (FrontendSlateUserIndex != INDEX_NONE && FrontendSlateNavigationConfig && FSlateApplication::IsInitialized())
	{
		if (TSharedPtr<FSlateUser> SlateUser = FSlateApplication::Get().GetUser(FrontendSlateUserIndex);
			SlateUser && SlateUser->GetUserNavigationConfig() == FrontendSlateNavigationConfig)
		{
			SlateUser->SetUserNavigationConfig(PreviousSlateNavigationConfig);
			UE_LOG(LogTemp, Log, TEXT("HSRUI Slate navigation restored User=%d"), FrontendSlateUserIndex);
		}
	}

	FrontendSlateUserIndex = INDEX_NONE;
	PreviousSlateNavigationConfig.Reset();
	FrontendSlateNavigationConfig.Reset();
}

EHSRPlayerControlMode AHSRPlayerController::ResolveControlModeForCurrentWorld() const
{
	return Cast<AHSRBattleGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr)
		? EHSRPlayerControlMode::Battle
		: EHSRPlayerControlMode::Exploration;
}

bool AHSRPlayerController::SwitchExplorationCharacter(int32 PartySlot)
{
	if (CurrentControlMode != EHSRPlayerControlMode::Exploration || !GetWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("HSR Exploration switch REJECTED Mode=%d World=%d"), static_cast<int32>(CurrentControlMode), GetWorld() ? 1 : 0);
		return false;
	}
	UGameInstance* GI = GetGameInstance();
	UHSRPartySubsystem* Party = GI ? GI->GetSubsystem<UHSRPartySubsystem>() : nullptr;
	UHSRCharacterProfileSubsystem* Profiles = GI ? GI->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
	FHSRPartySnapshot PartySnapshot;
	if (!Party || !Profiles || !Party->GetSnapshot(PartySnapshot) || !PartySnapshot.Slots.IsValidIndex(PartySlot)
		|| PartySnapshot.Slots[PartySlot].IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("HSR Exploration switch REJECTED PartyMissingOrSlotEmpty Slot=%d Party=%d Profiles=%d Slots=%d"),
			PartySlot, Party ? 1 : 0, Profiles ? 1 : 0, Party ? PartySnapshot.Slots.Num() : -1);
		return false;
	}
	const FName CharacterId = PartySnapshot.Slots[PartySlot].CharacterId;
	const UHSRCharacterDefinition* Definition = nullptr;
	if (!Profiles->GetDefinition(CharacterId, Definition) || !Definition || Definition->CharacterClass.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("HSR Exploration switch REJECTED DefinitionMissing Char=%s"), *CharacterId.ToString());
		return false;
	}
	UClass* CharacterClass = Definition->CharacterClass.LoadSynchronous();
	if (!CharacterClass || !CharacterClass->IsChildOf<APawn>())
	{
		UE_LOG(LogTemp, Warning, TEXT("HSR Exploration switch REJECTED ClassInvalid Char=%s Class=%s"), *CharacterId.ToString(), CharacterClass ? *CharacterClass->GetName() : TEXT("None"));
		return false;
	}
	APawn* PreviousPawn = GetPawn();
	const FTransform SpawnTransform = PreviousPawn ? PreviousPawn->GetActorTransform() : FTransform::Identity;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	APawn* NewPawn = GetWorld()->SpawnActor<APawn>(CharacterClass, SpawnTransform, SpawnParameters);
	if (!NewPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("HSR Exploration switch REJECTED SpawnFailed Char=%s Slot=%d"), *CharacterId.ToString(), PartySlot);
		return false;
	}
	Possess(NewPawn);
	if (GetPawn() != NewPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("HSR Exploration switch REJECTED PossessFailed Char=%s Slot=%d"), *CharacterId.ToString(), PartySlot);
		NewPawn->Destroy();
		return false;
	}
	if (Party->SetActiveSlot(PartySlot) != EHSRPartyResult::Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("HSR Exploration switch REJECTED SetActiveSlotFailed Slot=%d"), PartySlot);
		if (PreviousPawn) Possess(PreviousPawn);
		NewPawn->Destroy();
		return false;
	}
	if (PreviousPawn && PreviousPawn != NewPawn) PreviousPawn->Destroy();
	UE_LOG(LogTemp, Log, TEXT("HSR Exploration character switched Slot=%d CharacterId=%s Pawn=%s"), PartySlot, *CharacterId.ToString(), *NewPawn->GetName());
	return true;
}

void AHSRPlayerController::HandlePartySlot1() { SwitchExplorationCharacter(0); }
void AHSRPlayerController::HandlePartySlot2() { SwitchExplorationCharacter(1); }
void AHSRPlayerController::HandlePartySlot3() { SwitchExplorationCharacter(2); }
void AHSRPlayerController::HandlePartySlot4() { SwitchExplorationCharacter(3); }

void AHSRPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!InPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRPlayerController::OnPossess - InPawn is nullptr"));
		return;
	}

	AHSRExplorationCharacter* ExplorationChar = Cast<AHSRExplorationCharacter>(InPawn);
	if (!ExplorationChar)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRPlayerController::OnPossess - Possessed Pawn is not AHSRExplorationCharacter: %s"), *InPawn->GetName());
	}

	if (ExplorationChar && ExplorationMappingContext)
	{
		for (const UInputAction* BoundAction : { ExplorationChar->GetMoveAction(), ExplorationChar->GetLookAction(),
			ExplorationChar->GetJumpAction(), ExplorationChar->GetInteractAction() })
		{
			const bool bFoundInContext = ExplorationMappingContext->GetMappings().ContainsByPredicate(
				[BoundAction](const FEnhancedActionKeyMapping& Mapping) { return Mapping.Action == BoundAction; });
			UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::OnPossess - BoundAction=%s FoundInContext=%s"),
				BoundAction ? *BoundAction->GetPathName() : TEXT("None"),
				bFoundInContext ? TEXT("true") : TEXT("false"));
		}
	}

	// Possession can land after BeginPlay in the battle world, so re-resolve rather than
	// trusting the mode set earlier. Without this the exploration context would be added
	// back on top of battle mode and the camera would orbit again.
	const EHSRPlayerControlMode ResolvedMode = ResolveControlModeForCurrentWorld();
	if (ResolvedMode != CurrentControlMode)
	{
		SetControlMode(ResolvedMode);
	}

	// SetupInputComponent owns the initial add, matching the UE 5.6 templates.
	if (bInputSystemReady && CurrentControlMode == EHSRPlayerControlMode::Exploration)
	{
		AddExplorationContext();
	}

	// Refresh HUD interaction observation if HUD already exists
	if (AHSRHUD* HSRHUD = Cast<AHSRHUD>(GetHUD()))
	{
		HSRHUD->RefreshInteractionObserver();
	}

	UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::OnPossess - Controller=%s Pawn=%s"),
		*GetName(), *InPawn->GetName());
}

void AHSRPlayerController::OnUnPossess()
{
	RemoveExplorationContext();

	// Clear HUD interaction observation before Super clears pawn reference
	if (AHSRHUD* HSRHUD = Cast<AHSRHUD>(GetHUD()))
	{
		HSRHUD->ClearInteractionObserverInstance();
	}

	Super::OnUnPossess();
}

void AHSRPlayerController::BuildPolicyForControlMode(EHSRPlayerControlMode Mode, FHSRInputModePolicy& OutPolicy)
{
	switch (Mode)
	{
	case EHSRPlayerControlMode::UIOnly:
		// Menus and the result panel: the pawn is not meant to receive anything.
		OutPolicy.InputIntent = EHSRUIInputIntent::UIOnly;
		OutPolicy.bShowMouseCursor = true;
		break;
	case EHSRPlayerControlMode::Battle:
		// The command panel needs clicks, but the battle world still runs game input
		// (abilities, camera framing), so this is GameAndUI rather than UIOnly. The
		// pawn keeps ticking; ApplyUIInputPolicy suppresses look/move separately,
		// because the exploration pawn is what the battle world possesses.
		OutPolicy.InputIntent = EHSRUIInputIntent::GameAndUI;
		OutPolicy.bShowMouseCursor = true;
		break;
	case EHSRPlayerControlMode::Exploration:
	default:
		OutPolicy.InputIntent = EHSRUIInputIntent::GameOnly;
		OutPolicy.bShowMouseCursor = false;
		break;
	}
}

void AHSRPlayerController::SetControlMode(EHSRPlayerControlMode NewMode)
{
	FHSRInputModePolicy Policy;
	BuildPolicyForControlMode(NewMode, Policy);
	ApplyUIInputPolicy(Policy, NewMode);
	UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::SetControlMode - Applied mode %d"),
		static_cast<uint8>(CurrentControlMode));
}

bool AHSRPlayerController::ApplyUIInputPolicy(const FHSRInputModePolicy& Policy, const EHSRPlayerControlMode SemanticMode)
{
	if (!IsLocalPlayerController())
	{
		return false;
	}
	if (bControlModeApplied && CurrentControlMode == SemanticMode
		&& AppliedInputIntent == Policy.InputIntent && bShowMouseCursor == Policy.bShowMouseCursor)
	{
		return true;
	}
	// The exploration mapping context only ever belongs to exploration.  Removing it on every
	// mode switch (not just the first) keeps Mouse2D from reaching the pawn while the battle
	// command panel is up, independent of the ApplyUIInputPolicy short-circuit above.
	RemoveExplorationContext();

	switch (Policy.InputIntent)
	{
	case EHSRUIInputIntent::GameOnly:
		SetInputMode(FInputModeGameOnly());
		break;
	case EHSRUIInputIntent::UIOnly:
	{
		FInputModeUIOnly Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
		break;
	}
	case EHSRUIInputIntent::GameAndUI:
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
		break;
	}
	default:
		return false;
	}

	CurrentControlMode = SemanticMode;
	AppliedInputIntent = Policy.InputIntent;
	bShowMouseCursor = Policy.bShowMouseCursor;

	// Suppress pawn look/move outside exploration. The battle world possesses the same
	// exploration pawn, whose CameraBoom uses bUsePawnControlRotation, so a bare input-mode
	// change is not enough: GameAndUI still delivers Mouse2D to the Look action and the
	// camera would keep orbiting under the cursor. Gating here rather than in the pawn
	// keeps one owner for the rule -- the pawn cannot know why input is suppressed.
	//
	// SetIgnoreLookInput/SetIgnoreMoveInput are reference counters, not booleans: calling
	// the setter twice needs two matching releases. Reset first so repeated mode changes
	// cannot strand a permanent suppression that would make exploration unrecoverable.
	const bool bBlockPawnInput = SemanticMode != EHSRPlayerControlMode::Exploration;
	ResetIgnoreInputFlags();
	if (bBlockPawnInput)
	{
		SetIgnoreLookInput(true);
		SetIgnoreMoveInput(true);
	}

	if (SemanticMode == EHSRPlayerControlMode::Exploration && bInputSystemReady)
	{
		AddExplorationContext();
	}
	bControlModeApplied = true;
	return true;
}

void AHSRPlayerController::RequestOpenPauseScreen()
{
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UHSRUIManagerSubsystem* Manager = LP->GetSubsystem<UHSRUIManagerSubsystem>())
		{
			const EHSRUIScreenResult Result = Manager->OpenPauseScreen();
			UE_LOG(LogTemp, Log, TEXT("HSRUI P17 RequestOpen Result=%d Stack=%d HasPause=%s"),
				static_cast<int32>(Result), Manager->GetLogicalScreenCount(),
				Manager->HasOpenPauseScreen() ? TEXT("true") : TEXT("false"));
		}
	}
}

void AHSRPlayerController::RequestCloseFrontendToRoot()
{
	if (ULocalPlayer* LP = GetLocalPlayer())
		if (UHSRUIManagerSubsystem* Manager = LP->GetSubsystem<UHSRUIManagerSubsystem>())
			Manager->CloseFrontendToRoot();
}

void AHSRPlayerController::HandlePauseBack()
{
	ULocalPlayer* LP = GetLocalPlayer();
	UHSRUIManagerSubsystem* Manager = LP ? LP->GetSubsystem<UHSRUIManagerSubsystem>() : nullptr;
	if (!Manager)
	{
		UE_LOG(LogTemp, Warning, TEXT("HSRUI P17 PauseBack ignored LocalPlayer=%s Manager=None"), LP ? TEXT("valid") : TEXT("None"));
		return;
	}
	if (Manager->HasOpenDialogueOverlay())
	{
		const EHSRUIScreenResult Result = Manager->CloseDialogueOverlay();
		UE_LOG(LogTemp, Log, TEXT("HSRUI P17 Dialogue Back Result=%d HasOverlay=%s"),
			static_cast<int32>(Result), Manager->HasOpenDialogueOverlay() ? TEXT("true") : TEXT("false"));
		return;
	}

	const int32 StackBefore = Manager->GetLogicalScreenCount();
	const EHSRUIScreenResult Result = StackBefore <= 1
		? Manager->OpenFrontendModule(EHSRFrontendModule::PauseHub) : Manager->RequestBack();
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 PauseBack handled StackBefore=%d Result=%d StackAfter=%d HasPause=%s"),
		StackBefore, static_cast<int32>(Result), Manager->GetLogicalScreenCount(),
		Manager->HasOpenPauseScreen() ? TEXT("true") : TEXT("false"));
}

void AHSRPlayerController::HandleInventory() { if (ULocalPlayer* LP = GetLocalPlayer()) if (auto* M = LP->GetSubsystem<UHSRUIManagerSubsystem>()) M->OpenFrontendModule(EHSRFrontendModule::Inventory); }
void AHSRPlayerController::HandleParty() { if (ULocalPlayer* LP = GetLocalPlayer()) if (auto* M = LP->GetSubsystem<UHSRUIManagerSubsystem>()) M->OpenFrontendModule(EHSRFrontendModule::Party); }
void AHSRPlayerController::HandleMap() { if (ULocalPlayer* LP = GetLocalPlayer()) if (auto* M = LP->GetSubsystem<UHSRUIManagerSubsystem>()) M->OpenFrontendModule(EHSRFrontendModule::Map); }
void AHSRPlayerController::HandleChallenge() { if (ULocalPlayer* LP = GetLocalPlayer()) if (auto* M = LP->GetSubsystem<UHSRUIManagerSubsystem>()) M->OpenFrontendModule(EHSRFrontendModule::Challenge); }
void AHSRPlayerController::HandleCloseToRoot()
{
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UHSRUIManagerSubsystem* Manager = LP->GetSubsystem<UHSRUIManagerSubsystem>())
		{
			if (Manager->HasOpenDialogueOverlay())
			{
				const EHSRUIScreenResult Result = Manager->CloseDialogueOverlay();
				UE_LOG(LogTemp, Log, TEXT("HSRUI P17 Dialogue CloseToRoot Result=%d HasOverlay=%s"),
					static_cast<int32>(Result), Manager->HasOpenDialogueOverlay() ? TEXT("true") : TEXT("false"));
				return;
			}
		}
	}
	RequestCloseFrontendToRoot();
}

void AHSRPlayerController::AddFrontendNavigationContext()
{
	if (!FrontendNavigationMappingContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("HSRUI Frontend context is not configured"));
		return;
	}
	ULocalPlayer* LP = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP ? LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Subsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("HSRUI Frontend context unavailable LocalPlayer=%s Subsystem=%s"),
			LP ? TEXT("valid") : TEXT("None"), Subsystem ? TEXT("valid") : TEXT("None"));
		return;
	}

	const bool bContextPresent = Subsystem->HasMappingContext(FrontendNavigationMappingContext);
	if (ShouldRestoreFrontendNavigationContext(bFrontendNavigationContextAdded, bContextPresent))
	{
		FModifyContextOptions Options;
		Options.bForceImmediately = true;
		Subsystem->AddMappingContext(FrontendNavigationMappingContext, 10, Options);
	}
	bFrontendNavigationContextAdded = true;
	UE_LOG(LogTemp, Log, TEXT("HSRUI Frontend context %s Context=%s Present=%s"),
		bContextPresent ? TEXT("verified") : TEXT("restored"), *FrontendNavigationMappingContext->GetPathName(),
		Subsystem->HasMappingContext(FrontendNavigationMappingContext) ? TEXT("true") : TEXT("false"));
}

void AHSRPlayerController::RequestBackScreen()
{
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UHSRUIManagerSubsystem* Manager = LP->GetSubsystem<UHSRUIManagerSubsystem>())
		{
			const EHSRUIScreenResult Result = Manager->RequestBack();
			UE_LOG(LogTemp, Log, TEXT("HSRUI P17 RequestBack Result=%d Stack=%d HasPause=%s"),
				static_cast<int32>(Result), Manager->GetLogicalScreenCount(),
				Manager->HasOpenPauseScreen() ? TEXT("true") : TEXT("false"));
		}
	}
}

void AHSRPlayerController::RequestOpenCharacterDetailScreen()
{
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UHSRUIManagerSubsystem* Manager = LP->GetSubsystem<UHSRUIManagerSubsystem>())
		{
			const EHSRUIScreenResult Result = Manager->OpenFrontendModule(EHSRFrontendModule::Character);
			UE_LOG(LogTemp, Log, TEXT("HSRUI P17 CharacterDetail RequestOpen Result=%d Stack=%d HasDetail=%s"),
				static_cast<int32>(Result), Manager->GetLogicalScreenCount(),
				Manager->HasOpenCharacterDetailScreen() ? TEXT("true") : TEXT("false"));
		}
	}
}

void AHSRPlayerController::RequestOpenInventoryScreen()
{
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UHSRUIManagerSubsystem* Manager = LP->GetSubsystem<UHSRUIManagerSubsystem>())
		{
			const EHSRUIScreenResult Result = Manager->OpenFrontendModule(EHSRFrontendModule::Inventory);
			UE_LOG(LogTemp, Log, TEXT("HSRUI P17 Inventory RequestOpen Result=%d Stack=%d HasInventory=%s"),
				static_cast<int32>(Result), Manager->GetLogicalScreenCount(),
				Manager->HasOpenInventoryScreen() ? TEXT("true") : TEXT("false"));
		}
	}
}

void AHSRPlayerController::AddExplorationContext()
{
	if (bExplorationContextAdded)
	{
		return;
	}

	if (!ExplorationMappingContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRPlayerController::AddExplorationContext - ExplorationMappingContext is not set"));
		return;
	}

	ULocalPlayer* LP = GetLocalPlayer();
	if (!LP)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRPlayerController::AddExplorationContext - No LocalPlayer"));
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRPlayerController::AddExplorationContext - No EnhancedInputLocalPlayerSubsystem"));
		return;
	}

	FModifyContextOptions Options;
	Options.bForceImmediately = true;
	Subsystem->AddMappingContext(ExplorationMappingContext, 0, Options);
	bExplorationContextAdded = true;
	UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::AddExplorationContext - Added %s"),
		*ExplorationMappingContext->GetName());

	UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::AddExplorationContext - HasContext=%s"),
		Subsystem->HasMappingContext(ExplorationMappingContext) ? TEXT("true") : TEXT("false"));

	for (const FEnhancedActionKeyMapping& Mapping : ExplorationMappingContext->GetMappings())
	{
		UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::AddExplorationContext - Mapping Action=%s Key=%s"),
			Mapping.Action ? *Mapping.Action->GetPathName() : TEXT("None"),
			*Mapping.Key.ToString());
	}
}

void AHSRPlayerController::RemoveExplorationContext()
{
	if (!bExplorationContextAdded)
	{
		return;
	}

	if (!ExplorationMappingContext)
	{
		bExplorationContextAdded = false;
		return;
	}

	ULocalPlayer* LP = GetLocalPlayer();
	if (!LP)
	{
		bExplorationContextAdded = false;
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Subsystem)
	{
		bExplorationContextAdded = false;
		return;
	}

	Subsystem->RemoveMappingContext(ExplorationMappingContext);
	bExplorationContextAdded = false;
}
