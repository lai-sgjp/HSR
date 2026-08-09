#include "HSRUIManagerSubsystem.h"
#include "HSRInputModeCoordinator.h"
#include "HSRScreenStack.h"
#include "HSRScreenWidget.h"
#include "HSRCharacterDetailWidget.h"
#include "HSRInventoryRewardWidget.h"
#include "HSRInventoryRewardViewModel.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Reward/HSRRewardSubsystem.h"
#include "../Map/HSRMapSubsystem.h"
#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "HSRUserWidget.h"
#include "HSRHUD.h"
#include "Frontend/HSRFrontendRouter.h"
#include "Frontend/HSRFrontendShellWidget.h"
#include "Frontend/HSRFrontendModuleRootWidget.h"
#include "../Player/HSRPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const FName ExplorationRootId(TEXT("UI.Screen.ExplorationRoot"));
	const FName PauseScreenId(TEXT("UI.Screen.Pause"));
	const FName PauseFocusToken(TEXT("UI.Focus.Pause.Primary"));
	const FName CharacterDetailScreenId(TEXT("UI.Screen.CharacterDetail"));
	const FName CharacterDetailFocusToken(TEXT("UI.Focus.CharacterDetail.Back"));
	const FName InventoryScreenId(TEXT("UI.Screen.Inventory"));
	const FName InventoryFocusToken(TEXT("UI.Focus.Inventory.Back"));
}

void UHSRUIManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ScreenStack = NewObject<UHSRScreenStack>(this);
	InputModeCoordinator = NewObject<UHSRInputModeCoordinator>(this);
	FrontendRouter = NewObject<UHSRFrontendRouter>(this);
	NextRequestToken = 1;
	NextFrontendRequestToken = 1;
	bInitialized = true;
	bInconsistent = false;
	bInconsistencyIsTravelRecoverable = false;
	if (UGameInstance* GameInstance = GetLocalPlayer() ? GetLocalPlayer()->GetGameInstance() : nullptr)
	{
		if (UHSRMapSubsystem* Maps = GameInstance->GetSubsystem<UHSRMapSubsystem>())
		{
			ArrivalCommittedHandle = Maps->OnArrivalCommitted().AddUObject(this, &ThisClass::HandleArrivalCommitted);
		}
	}
}

void UHSRUIManagerSubsystem::Deinitialize()
{
	if (UGameInstance* GameInstance = GetLocalPlayer() ? GetLocalPlayer()->GetGameInstance() : nullptr)
	{
		if (UHSRMapSubsystem* Maps = GameInstance->GetSubsystem<UHSRMapSubsystem>())
		{
			Maps->OnArrivalCommitted().Remove(ArrivalCommittedHandle);
		}
	}
	ArrivalCommittedHandle.Reset();
	bTravelRestorePending = false;
	bTravelArrivalObserved = false;
	TravelRestoreScreenId = NAME_None;
	if (InventoryWidgetInstance)
	{
		InventoryWidgetInstance->SetViewModel(nullptr);
		InventoryWidgetInstance->RemoveFromParent();
		InventoryWidgetInstance = nullptr;
	}
	if (InventoryViewModelInstance)
	{
		InventoryViewModelInstance->Shutdown();
		InventoryViewModelInstance = nullptr;
	}
	if (CharacterDetailWidgetInstance)
	{
		CharacterDetailWidgetInstance->RemoveFromParent();
		CharacterDetailWidgetInstance = nullptr;
	}
	if (FrontendShellInstance)
	{
		FrontendShellInstance->RemoveFromParent();
		FrontendShellInstance = nullptr;
	}
	if (FrontendModuleRootInstance)
	{
		ReleaseFrontendModuleContent();
		FrontendModuleRootInstance->RemoveFromParent();
		FrontendModuleRootInstance = nullptr;
	}
	if (PauseOwnerToken.IsValid())
	{
		if (AHSRPlayerController* PC = RegisteredPlayerController.Get())
		{
			if (UWorld* World = PC->GetWorld(); World && World->IsPaused())
			{
				UGameplayStatics::SetGamePaused(World, false);
			}
		}
	}
	PauseOwnerToken.Invalidate();
	ClearHostReferences();
	InputModeCoordinator = nullptr;
	FrontendRouter = nullptr;
	ScreenStack = nullptr;
	bInitialized = false;
	Super::Deinitialize();
}

EHSRScreenStackResult UHSRUIManagerSubsystem::SubmitScreenRequest(const FHSRScreenRequest& Request)
{
	return ScreenStack ? ScreenStack->SubmitRequest(Request) : EHSRScreenStackResult::InvalidRequest;
}

FHSRInputModePolicy UHSRUIManagerSubsystem::GetResolvedInputPolicy() const
{
	return InputModeCoordinator ? InputModeCoordinator->ResolvePolicy(ScreenStack) : FHSRInputModePolicy{};
}

int32 UHSRUIManagerSubsystem::GetLogicalScreenCount() const
{
	return ScreenStack ? ScreenStack->GetSnapshot().Entries.Num() : 0;
}

EHSRUIScreenResult UHSRUIManagerSubsystem::RegisterExplorationHost(AHSRHUD* HUD,
	AHSRPlayerController* PlayerController, UHSRUserWidget* RootWidget,
	TSubclassOf<UHSRFrontendShellWidget> InFrontendShellClass,
	TSubclassOf<UHSRFrontendModuleRootWidget> InFrontendModuleRootClass,
	TSubclassOf<UHSRScreenWidget> InCharacterDetailWidgetClass,
	TSubclassOf<UHSRInventoryWidget> InInventoryWidgetClass,
	const TMap<EHSRFrontendModule, TSubclassOf<UUserWidget>>& InModuleWidgetClasses)
{
	if (!bInitialized || !ScreenStack || !InputModeCoordinator)
	{
		return EHSRUIScreenResult::NotInitialized;
	}
	if (!HUD || !PlayerController || !PlayerController->IsLocalPlayerController() || !RootWidget)
	{
		return EHSRUIScreenResult::InvalidHost;
	}
	if (HasInventoryOwnershipMismatch())
	{
		bInconsistent = true;
		bInconsistencyIsTravelRecoverable = false;
		return EHSRUIScreenResult::Inconsistent;
	}
	if (RegisteredHUD.Get() == HUD && RegisteredPlayerController.Get() == PlayerController
		&& RegisteredRootWidget.Get() == RootWidget)
	{
		FrontendShellClass = InFrontendShellClass;
		FrontendModuleRootClass = InFrontendModuleRootClass;
		CharacterDetailWidgetClass = InCharacterDetailWidgetClass;
		InventoryWidgetClass = InInventoryWidgetClass;
		FrontendModuleWidgetClasses = InModuleWidgetClasses;
		return EHSRUIScreenResult::NoOp;
	}
	if (RegisteredHUD.IsValid() || FrontendShellInstance || FrontendModuleContentInstance
		|| CharacterDetailWidgetInstance || InventoryWidgetInstance || InventoryViewModelInstance)
	{
		return EHSRUIScreenResult::InvalidHost;
	}

	if (ScreenStack->GetSnapshot().Entries.IsEmpty())
	{
		if (ScreenStack->SubmitRequest(MakeRootRequest(AllocateRequestToken())) != EHSRScreenStackResult::Success)
		{
			return EHSRUIScreenResult::StackRejected;
		}
	}
	else if (ScreenStack->GetSnapshot().Entries[0].ScreenId != ExplorationRootId)
	{
		return EHSRUIScreenResult::InvalidHost;
	}

	RegisteredHUD = HUD;
	RegisteredPlayerController = PlayerController;
	RegisteredRootWidget = RootWidget;
	ActiveHostGeneration = NextHostGeneration++;
	FrontendShellClass = InFrontendShellClass;
	FrontendModuleRootClass = InFrontendModuleRootClass;
	CharacterDetailWidgetClass = InCharacterDetailWidgetClass;
	InventoryWidgetClass = InInventoryWidgetClass;
	FrontendModuleWidgetClasses = InModuleWidgetClasses;
	// Clear before restoring: a travel-scoped inconsistency would otherwise reject the restore
	// and every later OpenFrontendModule call on this otherwise-healthy host.
	TryClearRecoverableInconsistency();
	TryRestoreTravelDescriptor();
	return EHSRUIScreenResult::Success;
}

EHSRUIScreenResult UHSRUIManagerSubsystem::UnregisterExplorationHost(AHSRHUD* HUD, AHSRPlayerController* PlayerController)
{
	if (RegisteredHUD.Get() != HUD || RegisteredPlayerController.Get() != PlayerController)
	{
		return EHSRUIScreenResult::InvalidHost;
	}
	return TeardownCurrentHost();
}

EHSRUIScreenResult UHSRUIManagerSubsystem::TeardownExplorationHostForTravel(AHSRHUD* HUD,
	AHSRPlayerController* PlayerController)
{
	if (RegisteredHUD.Get() != HUD || RegisteredPlayerController.Get() != PlayerController || ActiveHostGeneration == 0)
		return EHSRUIScreenResult::InvalidHost;
	return CaptureAndTeardownTravelHost();
}

EHSRUIScreenResult UHSRUIManagerSubsystem::PrepareExplorationTravel()
{
	if (!bInitialized || bInconsistent)
	{
		return EHSRUIScreenResult::Inconsistent;
	}
	if (ActiveHostGeneration == 0)
	{
		return EHSRUIScreenResult::Success;
	}
	if (bTravelRestorePending)
	{
		return EHSRUIScreenResult::InvalidHost;
	}
	return CaptureAndTeardownTravelHost();
}

EHSRUIScreenResult UHSRUIManagerSubsystem::TeardownCurrentHost()
{
	bool bRecovered = true;
	AHSRPlayerController* PC = RegisteredPlayerController.Get();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (FrontendShellInstance)
	{
		bRecovered &= CloseFrontendToRoot() == EHSRUIScreenResult::Success;
	}
	if (InventoryWidgetInstance || InventoryViewModelInstance)
	{
		const EHSRUIScreenResult CloseResult = CloseInventoryScreen();
		bRecovered &= CloseResult == EHSRUIScreenResult::Success;
		if (InventoryWidgetInstance || InventoryViewModelInstance)
		{
			if (InventoryWidgetInstance)
			{
				InventoryWidgetInstance->SetViewModel(nullptr);
				InventoryWidgetInstance->RemoveFromParent();
				InventoryWidgetInstance = nullptr;
			}
			if (InventoryViewModelInstance)
			{
				InventoryViewModelInstance->Shutdown();
				InventoryViewModelInstance = nullptr;
			}
			bRecovered &= ApplyInventoryPolicyBackend(PC, GetResolvedInputPolicy(), EHSRPlayerControlMode::Exploration);
		}
	}
	if (CharacterDetailWidgetInstance)
	{
		const EHSRUIScreenResult CloseResult = CloseCharacterDetailScreen();
		bRecovered &= CloseResult == EHSRUIScreenResult::Success;
		if (CharacterDetailWidgetInstance)
		{
			CharacterDetailWidgetInstance->RemoveFromParent();
			CharacterDetailWidgetInstance = nullptr;
			bRecovered &= ApplyCharacterDetailPolicyBackend(PC, GetResolvedInputPolicy(), EHSRPlayerControlMode::Exploration);
		}
	}
	if (FrontendShellInstance)
	{
		const EHSRUIScreenResult CloseResult = RequestBack();
		bRecovered &= CloseResult == EHSRUIScreenResult::Success;
		if (FrontendShellInstance)
		{
			FrontendShellInstance->RemoveFromParent();
			FrontendShellInstance = nullptr;
			// The shell owns the module root; CloseFrontendToRoot() only reaches its own
			// clear on the success path, so a forced shell teardown must release it here or
			// the retired module root outlives the host and blocks every later recovery.
			if (FrontendModuleRootInstance)
			{
				ReleaseFrontendModuleContent();
				FrontendModuleRootInstance->RemoveFromParent();
				FrontendModuleRootInstance = nullptr;
			}
			if (FrontendRouter)
			{
				FHSRFrontendRouteRequest ForcedClose;
				ForcedClose.RequestToken = AllocateFrontendRequestToken();
				ForcedClose.Operation = EHSRFrontendRouteOperation::CloseToRoot;
				FrontendRouter->Submit(ForcedClose);
			}
			if (ScreenStack && ScreenStack->GetSnapshot().Entries.Num() > 1)
			{
				bRecovered &= ScreenStack->SubmitRequest(MakePopRequest(AllocateRequestToken())) == EHSRScreenStackResult::Success;
			}
			bRecovered &= ApplyPolicyBackend(PC, GetResolvedInputPolicy(), EHSRPlayerControlMode::Exploration);
			if (PauseOwnerToken.IsValid() && IsBackendPaused(World))
			{
				bRecovered &= ApplyPauseBackend(World, false);
			}
		}
	}
	PauseOwnerToken.Invalidate();
	// Evaluate containment while the retired host is still observable: ClearHostReferences()
	// zeroes ActiveHostGeneration, and IsAtCleanExplorationRoot() would then read a torn state.
	const bool bContainedToRetiredHost = IsAtCleanExplorationRoot();
	ClearHostReferences();
#if WITH_DEV_AUTOMATION_TESTS
	AutomationHostIdentity = 0;
	bAutomationHostValid = false;
#endif
	if (!bRecovered)
	{
		bInconsistent = true;
		// Host references and module instances are cleared above, so if the stack is back at a
		// clean root the damage is contained to the retired host and a fresh one can recover.
		bInconsistencyIsTravelRecoverable = bContainedToRetiredHost;
		UE_LOG(LogTemp, Error,
			TEXT("HSRUI P17 Host teardown required forced cleanup; host references cleared Recoverable=%s Stack=%d"),
			bContainedToRetiredHost ? TEXT("true") : TEXT("false"), GetLogicalScreenCount());
		return EHSRUIScreenResult::Inconsistent;
	}
	return EHSRUIScreenResult::Success;
}

EHSRUIScreenResult UHSRUIManagerSubsystem::OpenPauseScreen()
{
	if (!bInitialized || !ScreenStack || !InputModeCoordinator)
	{
		return EHSRUIScreenResult::NotInitialized;
	}
	if (bInconsistent)
	{
		return EHSRUIScreenResult::Inconsistent;
	}
	if (IsTravelPending()) return EHSRUIScreenResult::InvalidHost;
	if (HasInventoryOwnershipMismatch())
	{
		bInconsistent = true;
		return EHSRUIScreenResult::Inconsistent;
	}
	if (FrontendShellInstance || CharacterDetailWidgetInstance || InventoryWidgetInstance)
	{
		return EHSRUIScreenResult::AlreadyOpen;
	}
	AHSRPlayerController* PC = RegisteredPlayerController.Get();
	UHSRUserWidget* RootWidget = RegisteredRootWidget.Get();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!IsBackendHostValid(PC, RootWidget, World))
	{
		return EHSRUIScreenResult::InvalidHost;
	}
	if (!IsBackendExploration(PC))
	{
		return EHSRUIScreenResult::NotExploration;
	}
	const FHSRScreenStackSnapshot PreflightSnapshot = ScreenStack->GetSnapshot();
	if (PreflightSnapshot.Entries.Num() != 1 || PreflightSnapshot.Entries[0].ScreenId != ExplorationRootId)
	{
		bInconsistent = true;
		return EHSRUIScreenResult::Inconsistent;
	}
	if (IsBackendPaused(World) && !PauseOwnerToken.IsValid())
	{
		return EHSRUIScreenResult::ExternalPause;
	}
	if (!FrontendShellClass
#if WITH_DEV_AUTOMATION_TESTS
		&& !(bUseAutomationBackend && bAutomationHasPauseClass)
#endif
	)
	{
		return EHSRUIScreenResult::MissingWidgetClass;
	}

	UHSRFrontendShellWidget* Candidate = CreatePauseCandidate(PC);
	if (!Candidate)
	{
		return EHSRUIScreenResult::WidgetCreationFailed;
	}
	Candidate->SetOwningUIManager(this);
	const FHSRInputModePolicy OldPolicy = GetResolvedInputPolicy();
	const FHSRScreenStackSnapshot OldStack = ScreenStack->GetSnapshot();
	const FHSRFrontendRouteSnapshot OldRoute = FrontendRouter ? FrontendRouter->GetSnapshot() : FHSRFrontendRouteSnapshot{};
	const int64 OpenToken = AllocateRequestToken();
	if (ScreenStack->SubmitRequest(MakePauseRequest(OpenToken)) != EHSRScreenStackResult::Success)
	{
		return EHSRUIScreenResult::StackRejected;
	}

	if (!AttachPauseCandidate(Candidate))
	{
		return CompensatePop(OldPolicy, PC, Candidate) ? EHSRUIScreenResult::ViewportAttachFailed
			: EHSRUIScreenResult::CompensationFailed;
	}
	const FHSRInputModePolicy PausePolicy = GetResolvedInputPolicy();
	if (!ApplyPolicyBackend(PC, PausePolicy, EHSRPlayerControlMode::UIOnly))
	{
		return CompensatePop(OldPolicy, PC, Candidate) ? EHSRUIScreenResult::PolicyApplyFailed
			: EHSRUIScreenResult::CompensationFailed;
	}
	if (!ApplyPauseBackend(World, true))
	{
		return CompensatePop(OldPolicy, PC, Candidate) ? EHSRUIScreenResult::PauseApplyFailed
			: EHSRUIScreenResult::CompensationFailed;
	}

	PauseOwnerToken = FGuid::NewGuid();
	const EHSRFocusApplyResult FocusResult = ApplyFocusBackend(PC, Candidate->GetPreferredFocusWidget(), Candidate);
	if (FocusResult == EHSRFocusApplyResult::Unavailable)
	{
		const bool bPauseRestored = ApplyPauseBackend(World, false);
		const bool bPolicyRestored = ApplyPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::Exploration);
		Candidate->RemoveFromParent();
		ScreenStack->RestoreSnapshotForTransaction(OldStack);
		if (FrontendRouter) FrontendRouter->RestoreSnapshotForTransaction(OldRoute);
		PauseOwnerToken.Invalidate();
		return ResolveCompensation(bPauseRestored && bPolicyRestored, EHSRUIScreenResult::FocusApplyFailed);
	}
	FHSRFrontendRouteRequest RouteRequest;
	RouteRequest.RequestToken = AllocateFrontendRequestToken();
	RouteRequest.Route.Module = EHSRFrontendModule::PauseHub;
	if (!FrontendRouter || FrontendRouter->Submit(RouteRequest) != EHSRFrontendRouteResult::Success)
	{
		const bool bPauseRestored = ApplyPauseBackend(World, false);
		const bool bPolicyRestored = ApplyPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::Exploration);
		Candidate->RemoveFromParent();
		ScreenStack->RestoreSnapshotForTransaction(OldStack);
		if (FrontendRouter) FrontendRouter->RestoreSnapshotForTransaction(OldRoute);
		PauseOwnerToken.Invalidate();
		return ResolveCompensation(bPauseRestored && bPolicyRestored, EHSRUIScreenResult::StackRejected);
	}
	FrontendShellInstance = Candidate;
	Candidate->PresentRoute(FrontendRouter->GetSnapshot());
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 OpenPause Success Token=%lld Stack=%d FocusResult=%d"),
		OpenToken, GetLogicalScreenCount(), static_cast<uint8>(FocusResult));
	return EHSRUIScreenResult::Success;
}

EHSRUIScreenResult UHSRUIManagerSubsystem::OpenCharacterDetailScreen()
{
	return OpenFrontendModule(EHSRFrontendModule::Character);
}

EHSRUIScreenResult UHSRUIManagerSubsystem::OpenCharacterDetailInternal()
{
	if (!bInitialized || !ScreenStack || !InputModeCoordinator)
	{
		return EHSRUIScreenResult::NotInitialized;
	}
	if (bInconsistent)
	{
		return EHSRUIScreenResult::Inconsistent;
	}
	if (HasInventoryOwnershipMismatch())
	{
		bInconsistent = true;
		return EHSRUIScreenResult::Inconsistent;
	}
	if (CharacterDetailWidgetInstance)
	{
		return EHSRUIScreenResult::AlreadyOpen;
	}
	AHSRPlayerController* PC = RegisteredPlayerController.Get();
	UHSRUserWidget* RootWidget = RegisteredRootWidget.Get();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!IsBackendHostValid(PC, RootWidget, World))
	{
		return EHSRUIScreenResult::InvalidHost;
	}
	if (!IsBackendExploration(PC) && !PauseOwnerToken.IsValid())
	{
		return EHSRUIScreenResult::NotExploration;
	}
	const FHSRScreenStackSnapshot PreflightSnapshot = ScreenStack->GetSnapshot();
	if (PreflightSnapshot.Entries.Num() != 2
		|| PreflightSnapshot.Entries[0].ScreenId != ExplorationRootId
		|| PreflightSnapshot.Entries.Last().ScreenId != PauseScreenId)
	{
		bInconsistent = true;
		return EHSRUIScreenResult::Inconsistent;
	}
	if (!HasModuleRootClass() || !HasCharacterDetailClass())
	{
		return EHSRUIScreenResult::MissingWidgetClass;
	}
	UHSRScreenWidget* Candidate = CreateCharacterDetailCandidate(PC);
	if (!Candidate)
	{
		return EHSRUIScreenResult::WidgetCreationFailed;
	}
	Candidate->SetOwningUIManager(this);
	UHSRFrontendModuleRootWidget* RootCandidate = CreateFrontendModuleRootCandidate(PC);
	if (!RootCandidate)
	{
		return EHSRUIScreenResult::WidgetCreationFailed;
	}
	RootCandidate->SetOwningUIManager(this);
	RootCandidate->PresentModule(EHSRFrontendModule::Character);
	const FHSRInputModePolicy OldPolicy = GetResolvedInputPolicy();
	const FHSRFrontendRouteSnapshot OldRoute = FrontendRouter->GetSnapshot();
	bool bContentAttached = false;
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		bContentAttached = bAutomationDetailAttachSucceeds;
	}
	else
#endif
	{
		bContentAttached = RootCandidate->SetModuleContent(Candidate);
	}
	if (!bContentAttached || !AttachFrontendModuleRootCandidate(RootCandidate))
	{
		RootCandidate->ClearModuleContent();
		RootCandidate->RemoveFromParent();
		Candidate->RemoveFromParent();
		const bool bRestore = ApplyCharacterDetailPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		if (!bRestore) { bInconsistent = true; return EHSRUIScreenResult::CompensationFailed; }
		return EHSRUIScreenResult::ViewportAttachFailed;
	}
	if (!ApplyCharacterDetailPolicyBackend(PC, GetResolvedInputPolicy(), EHSRPlayerControlMode::UIOnly))
	{
		RootCandidate->ClearModuleContent();
		RootCandidate->RemoveFromParent();
		Candidate->RemoveFromParent();
		const bool bRestore = ApplyCharacterDetailPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		if (!bRestore) { bInconsistent = true; return EHSRUIScreenResult::CompensationFailed; }
		return EHSRUIScreenResult::PolicyApplyFailed;
	}
	const EHSRFocusApplyResult FocusResult = ApplyCharacterDetailFocusBackend(PC, Candidate->GetPreferredFocusWidget(), Candidate);
	if (FocusResult == EHSRFocusApplyResult::Unavailable)
	{
		RootCandidate->ClearModuleContent();
		RootCandidate->RemoveFromParent();
		Candidate->RemoveFromParent();
		const bool bPolicyRestored = ApplyCharacterDetailPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		return ResolveCompensation(bPolicyRestored, EHSRUIScreenResult::FocusApplyFailed);
	}
	if (FrontendRouter && FrontendShellInstance)
	{
		FHSRFrontendRouteRequest RouteRequest; RouteRequest.RequestToken = AllocateFrontendRequestToken(); RouteRequest.Route.Module = EHSRFrontendModule::Character;
		if (SubmitFrontendRoute(RouteRequest) != EHSRFrontendRouteResult::Success)
		{
			RootCandidate->ClearModuleContent();
			RootCandidate->RemoveFromParent();
			Candidate->RemoveFromParent();
			FrontendRouter->RestoreSnapshotForTransaction(OldRoute);
			const bool bPolicyRestored = ApplyCharacterDetailPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
			const bool bFocusRestored = RestoreFrontendModuleFocus(PC, OldRoute.GetActiveRoute().Module);
			return ResolveCompensation(bPolicyRestored && bFocusRestored, EHSRUIScreenResult::StackRejected);
		}
		FrontendShellInstance->PresentRoute(FrontendRouter->GetSnapshot());
	}
	if (FrontendModuleRootInstance)
	{
		ReleaseFrontendModuleContent();
		FrontendModuleRootInstance->RemoveFromParent();
		FrontendModuleRootInstance = nullptr;
	}
	if (InventoryWidgetInstance)
	{
		InventoryWidgetInstance->SetViewModel(nullptr); InventoryWidgetInstance->RemoveFromParent(); InventoryWidgetInstance = nullptr;
		UHSRInventoryRewardViewModel* OldVM = InventoryViewModelInstance; InventoryViewModelInstance = nullptr; ShutdownInventoryViewModelCandidate(OldVM);
	}
	FrontendModuleRootInstance = RootCandidate;
	CharacterDetailWidgetInstance = Candidate;
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 CharacterDetail Open Success Stack=%d FocusResult=%d"),
		GetLogicalScreenCount(), static_cast<uint8>(FocusResult));
	return EHSRUIScreenResult::Success;
}

EHSRUIScreenResult UHSRUIManagerSubsystem::OpenInventoryScreen()
{
	return OpenFrontendModule(EHSRFrontendModule::Inventory);
}

EHSRUIScreenResult UHSRUIManagerSubsystem::OpenInventoryInternal()
{
	if (!bInitialized || !ScreenStack || !InputModeCoordinator) return EHSRUIScreenResult::NotInitialized;
	if (bInconsistent) return EHSRUIScreenResult::Inconsistent;
	if (HasInventoryOwnershipMismatch())
	{
		bInconsistent = true;
		return EHSRUIScreenResult::Inconsistent;
	}
	if (InventoryWidgetInstance) return EHSRUIScreenResult::AlreadyOpen;
	AHSRPlayerController* PC = RegisteredPlayerController.Get();
	UHSRUserWidget* RootWidget = RegisteredRootWidget.Get();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!IsBackendHostValid(PC, RootWidget, World)) return EHSRUIScreenResult::InvalidHost;
	if (!IsBackendExploration(PC) && !PauseOwnerToken.IsValid()) return EHSRUIScreenResult::NotExploration;
	const FHSRScreenStackSnapshot Preflight = ScreenStack->GetSnapshot();
	if (Preflight.Entries.Num() != 2
		|| Preflight.Entries[0].ScreenId != ExplorationRootId
		|| Preflight.Entries.Last().ScreenId != PauseScreenId)
	{
		bInconsistent = true;
		return EHSRUIScreenResult::Inconsistent;
	}
	if (!HasModuleRootClass() || !HasInventoryClass())
	{
		return EHSRUIScreenResult::MissingWidgetClass;
	}

#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend && !bAutomationInventoryDependenciesSucceed)
		return EHSRUIScreenResult::ViewModelInitializationFailed;
#endif
	UHSRInventorySubsystem* Inventory = nullptr;
	UHSRRewardSubsystem* Reward = nullptr;
#if WITH_DEV_AUTOMATION_TESTS
	if (!bUseAutomationBackend)
#endif
	{
		UGameInstance* GameInstance = GetLocalPlayer() ? GetLocalPlayer()->GetGameInstance() : nullptr;
		Inventory = GameInstance ? GameInstance->GetSubsystem<UHSRInventorySubsystem>() : nullptr;
		Reward = GameInstance ? GameInstance->GetSubsystem<UHSRRewardSubsystem>() : nullptr;
		if (!Inventory || !Reward)
			return EHSRUIScreenResult::ViewModelInitializationFailed;
	}
	UHSRInventoryRewardViewModel* ViewModelCandidate = CreateInventoryViewModelCandidate();
	if (!ViewModelCandidate) return EHSRUIScreenResult::ViewModelInitializationFailed;
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend && !bAutomationInventorySnapshotSucceeds)
	{
		ShutdownInventoryViewModelCandidate(ViewModelCandidate);
		return EHSRUIScreenResult::ViewModelInitializationFailed;
	}
	if (!bUseAutomationBackend)
#endif
	{
		ViewModelCandidate->Initialize(Inventory, Reward);
		FHSRInventoryRewardSnapshot Snapshot;
		if (!ViewModelCandidate->GetSnapshot(Snapshot))
		{
			ShutdownInventoryViewModelCandidate(ViewModelCandidate);
			return EHSRUIScreenResult::ViewModelInitializationFailed;
		}
	}
	UHSRInventoryWidget* Candidate = CreateInventoryCandidate(PC);
	if (!Candidate)
	{
		ShutdownInventoryViewModelCandidate(ViewModelCandidate);
		return EHSRUIScreenResult::WidgetCreationFailed;
	}
	Candidate->SetOwningUIManager(this);
	Candidate->SetViewModel(ViewModelCandidate);
#if WITH_DEV_AUTOMATION_TESTS
	// Automation widgets are NewObject'd and never enter the UMG construct path, so
	// SetViewModel's IsConstructed() guard skips the bind. Real attach reaches it via
	// NativeConstruct; mirror that here so bind/unbind accounting matches production.
	if (bUseAutomationBackend)
	{
		Candidate->AttachForAutomation();
	}
#endif
	UHSRFrontendModuleRootWidget* RootCandidate = CreateFrontendModuleRootCandidate(PC);
	if (!RootCandidate)
	{
		ReleaseInventoryCandidates(Candidate, ViewModelCandidate);
		return EHSRUIScreenResult::WidgetCreationFailed;
	}
	RootCandidate->SetOwningUIManager(this);
	RootCandidate->PresentModule(EHSRFrontendModule::Inventory);
	const FHSRInputModePolicy OldPolicy = GetResolvedInputPolicy();
	const FHSRFrontendRouteSnapshot OldRoute = FrontendRouter->GetSnapshot();
	bool bContentAttached = false;
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		bContentAttached = bAutomationInventoryAttachSucceeds;
	}
	else
#endif
	{
		bContentAttached = RootCandidate->SetModuleContent(Candidate);
	}
	if (!bContentAttached || !AttachFrontendModuleRootCandidate(RootCandidate))
	{
		RootCandidate->ClearModuleContent();
		RootCandidate->RemoveFromParent();
		ReleaseInventoryCandidates(Candidate, ViewModelCandidate);
		const bool bRestore = ApplyInventoryPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		if (!bRestore) { bInconsistent = true; return EHSRUIScreenResult::CompensationFailed; }
		return EHSRUIScreenResult::ViewportAttachFailed;
	}
	if (!ApplyInventoryPolicyBackend(PC, GetResolvedInputPolicy(), EHSRPlayerControlMode::UIOnly))
	{
		RootCandidate->ClearModuleContent();
		RootCandidate->RemoveFromParent();
		ReleaseInventoryCandidates(Candidate, ViewModelCandidate);
		const bool bRestore = ApplyInventoryPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		if (!bRestore) { bInconsistent = true; return EHSRUIScreenResult::CompensationFailed; }
		return EHSRUIScreenResult::PolicyApplyFailed;
	}
	const EHSRFocusApplyResult FocusResult = ApplyInventoryFocusBackend(PC, Candidate->GetPreferredFocusWidget(), Candidate);
	if (FocusResult == EHSRFocusApplyResult::Unavailable)
	{
		RootCandidate->ClearModuleContent();
		RootCandidate->RemoveFromParent();
		ReleaseInventoryCandidates(Candidate, ViewModelCandidate);
		const bool bPolicyRestored = ApplyInventoryPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		return ResolveCompensation(bPolicyRestored, EHSRUIScreenResult::FocusApplyFailed);
	}
	if (FrontendRouter && FrontendShellInstance)
	{
		FHSRFrontendRouteRequest RouteRequest; RouteRequest.RequestToken = AllocateFrontendRequestToken(); RouteRequest.Route.Module = EHSRFrontendModule::Inventory;
		if (SubmitFrontendRoute(RouteRequest) != EHSRFrontendRouteResult::Success)
		{
			RootCandidate->ClearModuleContent();
			RootCandidate->RemoveFromParent();
			ReleaseInventoryCandidates(Candidate, ViewModelCandidate);
			FrontendRouter->RestoreSnapshotForTransaction(OldRoute);
			const bool bPolicyRestored = ApplyInventoryPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
			const bool bFocusRestored = RestoreFrontendModuleFocus(PC, OldRoute.GetActiveRoute().Module);
			return ResolveCompensation(bPolicyRestored && bFocusRestored, EHSRUIScreenResult::StackRejected);
		}
		FrontendShellInstance->PresentRoute(FrontendRouter->GetSnapshot());
	}
	if (FrontendModuleRootInstance)
	{
		ReleaseFrontendModuleContent();
		FrontendModuleRootInstance->RemoveFromParent();
		FrontendModuleRootInstance = nullptr;
	}
	if (CharacterDetailWidgetInstance) { CharacterDetailWidgetInstance->RemoveFromParent(); CharacterDetailWidgetInstance = nullptr; }
	FrontendModuleRootInstance = RootCandidate;
	InventoryWidgetInstance = Candidate;
	InventoryViewModelInstance = ViewModelCandidate;
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 Inventory Open Success Stack=%d FocusResult=%d"),
		GetLogicalScreenCount(), static_cast<uint8>(FocusResult));
	return EHSRUIScreenResult::Success;
}

EHSRUIScreenResult UHSRUIManagerSubsystem::RequestBack()
{
	if (!bInitialized || !ScreenStack || !InputModeCoordinator)
	{
		return EHSRUIScreenResult::NotInitialized;
	}
	if (bInconsistent)
	{
		return EHSRUIScreenResult::Inconsistent;
	}
	if (HasInventoryOwnershipMismatch())
	{
		bInconsistent = true;
		return EHSRUIScreenResult::Inconsistent;
	}
	const FHSRScreenStackSnapshot Snapshot = ScreenStack->GetSnapshot();
	if (Snapshot.Entries.Num() <= 1)
	{
		return EHSRUIScreenResult::NothingOpen;
	}
	const EHSRFrontendModule ActiveFrontendModule = FrontendRouter
		? FrontendRouter->GetSnapshot().GetActiveRoute().Module : EHSRFrontendModule::None;
	if (FrontendModuleRootInstance || HSRFrontendModule::UsesSharedModuleRoot(ActiveFrontendModule))
	{
		const FHSRFrontendRouteSnapshot OldRoute = FrontendRouter->GetSnapshot();
		const FHSRInputModePolicy OldPolicy = GetResolvedInputPolicy();
		const auto ApplyActiveModulePolicy = [this, ActiveFrontendModule](const FHSRInputModePolicy& Policy)
		{
			switch (ActiveFrontendModule)
			{
			case EHSRFrontendModule::Character:
				return ApplyCharacterDetailPolicyBackend(RegisteredPlayerController.Get(), Policy, EHSRPlayerControlMode::UIOnly);
			case EHSRFrontendModule::Inventory:
				return ApplyInventoryPolicyBackend(RegisteredPlayerController.Get(), Policy, EHSRPlayerControlMode::UIOnly);
			default:
				return ApplyPolicyBackend(RegisteredPlayerController.Get(), Policy, EHSRPlayerControlMode::UIOnly);
			}
		};
		const auto ApplyActiveModuleFocus = [this, ActiveFrontendModule]()
		{
			switch (ActiveFrontendModule)
			{
			case EHSRFrontendModule::Character:
				return ApplyCharacterDetailFocusBackend(RegisteredPlayerController.Get(), FrontendShellInstance, FrontendShellInstance);
			case EHSRFrontendModule::Inventory:
				return ApplyInventoryFocusBackend(RegisteredPlayerController.Get(), FrontendShellInstance, FrontendShellInstance);
			default:
				return ApplyFocusBackend(RegisteredPlayerController.Get(), FrontendShellInstance->GetPreferredFocusWidget(), FrontendShellInstance);
			}
		};
		if (!ApplyActiveModulePolicy(GetResolvedInputPolicy()))
		{
			const bool bPolicyRestored = ApplyActiveModulePolicy(OldPolicy);
			return ResolveCompensation(bPolicyRestored, EHSRUIScreenResult::PolicyApplyFailed);
		}
		if (ApplyActiveModuleFocus() == EHSRFocusApplyResult::Unavailable)
		{
			const bool bPolicyRestored = ApplyActiveModulePolicy(OldPolicy);
			return ResolveCompensation(bPolicyRestored, EHSRUIScreenResult::FocusApplyFailed);
		}
		FHSRFrontendRouteRequest RouteRequest;
		RouteRequest.RequestToken = AllocateFrontendRequestToken();
		RouteRequest.Operation = EHSRFrontendRouteOperation::Back;
		if (!FrontendRouter || SubmitFrontendRoute(RouteRequest) != EHSRFrontendRouteResult::Success)
		{
			if (FrontendRouter) FrontendRouter->RestoreSnapshotForTransaction(OldRoute);
			const bool bPolicyRestored = ApplyActiveModulePolicy(OldPolicy);
			const bool bFocusRestored = RestoreFrontendModuleFocus(RegisteredPlayerController.Get(), ActiveFrontendModule);
			return ResolveCompensation(bPolicyRestored && bFocusRestored, EHSRUIScreenResult::StackRejected);
		}
		if (FrontendModuleRootInstance)
		{
			ReleaseFrontendModuleContent();
			FrontendModuleRootInstance->RemoveFromParent();
		}
		FrontendModuleRootInstance = nullptr;
		FrontendModuleContentModule = EHSRFrontendModule::None;
		if (ActiveFrontendModule == EHSRFrontendModule::Character)
		{
			CharacterDetailWidgetInstance = nullptr;
		}
		else if (ActiveFrontendModule == EHSRFrontendModule::Inventory)
		{
			if (InventoryWidgetInstance)
			{
				InventoryWidgetInstance->SetViewModel(nullptr);
#if WITH_DEV_AUTOMATION_TESTS
				LastReleasedInventoryBindCount = InventoryWidgetInstance->GetBindCountForAutomation();
				LastReleasedInventoryUnbindCount = InventoryWidgetInstance->GetUnbindCountForAutomation();
#endif
				InventoryWidgetInstance = nullptr;
			}
			UHSRInventoryRewardViewModel* ViewModelToShutdown = InventoryViewModelInstance;
			InventoryViewModelInstance = nullptr;
			ShutdownInventoryViewModelCandidate(ViewModelToShutdown);
		}
		FrontendShellInstance->PresentRoute(FrontendRouter->GetSnapshot());
		// A module reopened by travel restore has no shell layer beneath it to fall back to, so
		// backing out of it must land on the root rather than leaving the shell on the stack.
		if (bTravelRestoredModule)
		{
			bTravelRestoredModule = false;
			return CloseFrontendToRoot();
		}
		return EHSRUIScreenResult::Success;
	}
	if (ActiveFrontendModule == EHSRFrontendModule::Inventory)
	{
		if (!InventoryWidgetInstance || !InventoryViewModelInstance || CharacterDetailWidgetInstance)
		{
			bInconsistent = true;
			return EHSRUIScreenResult::Inconsistent;
		}
		const EHSRUIScreenResult Result = CloseInventoryScreen();
		if (Result == EHSRUIScreenResult::Success && bTravelRestoredModule)
		{
			bTravelRestoredModule = false;
			return CloseFrontendToRoot();
		}
		return Result;
	}
	if (ActiveFrontendModule == EHSRFrontendModule::Character)
	{
		if (!CharacterDetailWidgetInstance || InventoryWidgetInstance)
		{
			bInconsistent = true;
			return EHSRUIScreenResult::Inconsistent;
		}
		const EHSRUIScreenResult Result = CloseCharacterDetailScreen();
		if (Result == EHSRUIScreenResult::Success && bTravelRestoredModule)
		{
			bTravelRestoredModule = false;
			return CloseFrontendToRoot();
		}
		return Result;
	}
	const FName TopId = Snapshot.Entries.Last().ScreenId;
	if (TopId != PauseScreenId || ActiveFrontendModule != EHSRFrontendModule::PauseHub
		|| !FrontendShellInstance || !PauseOwnerToken.IsValid()
		|| CharacterDetailWidgetInstance || InventoryWidgetInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("HSRUI Frontend hub close invariant Top=%s Shell=%s PauseOwner=%s Character=%s Inventory=%s"),
			*TopId.ToString(), FrontendShellInstance ? TEXT("true") : TEXT("false"),
			PauseOwnerToken.IsValid() ? TEXT("true") : TEXT("false"),
			CharacterDetailWidgetInstance ? TEXT("true") : TEXT("false"), InventoryWidgetInstance ? TEXT("true") : TEXT("false"));
		bInconsistent = true;
		return EHSRUIScreenResult::Inconsistent;
	}
	return CloseFrontendToRoot();
}

EHSRUIScreenResult UHSRUIManagerSubsystem::OpenFrontendModule(const EHSRFrontendModule Module)
{
	if (Module == EHSRFrontendModule::None) return EHSRUIScreenResult::StackRejected;
	bool bOpenedShell = false;
	if (!FrontendShellInstance)
	{
		const EHSRUIScreenResult ShellResult = OpenPauseScreen();
		if (ShellResult != EHSRUIScreenResult::Success) return ShellResult;
		bOpenedShell = true;
	}
	if (Module == EHSRFrontendModule::PauseHub)
	{
		if (bOpenedShell) return EHSRUIScreenResult::Success;
		return FrontendRouter && FrontendRouter->GetSnapshot().GetActiveRoute().Module == EHSRFrontendModule::PauseHub
			? EHSRUIScreenResult::NoOp : RequestBack();
	}
	if (FrontendRouter && FrontendRouter->GetSnapshot().GetActiveRoute().Module == Module)
	{
		FHSRFrontendRouteRequest NoOpRequest; NoOpRequest.RequestToken = AllocateFrontendRequestToken(); NoOpRequest.Route.Module = Module;
		return FrontendRouter->Submit(NoOpRequest) == EHSRFrontendRouteResult::NoOp
			? EHSRUIScreenResult::NoOp : EHSRUIScreenResult::StackRejected;
	}
	const auto CompleteModuleAttempt = [this, bOpenedShell](const EHSRUIScreenResult Result)
	{
		if (bOpenedShell && Result != EHSRUIScreenResult::Success && Result != EHSRUIScreenResult::NoOp)
		{
			if (CloseFrontendToRoot() != EHSRUIScreenResult::Success)
			{
				bInconsistent = true;
				return EHSRUIScreenResult::CompensationFailed;
			}
		}
		return Result;
	};
	switch (Module)
	{
	case EHSRFrontendModule::Character:
		return CompleteModuleAttempt(OpenCharacterDetailInternal());
	case EHSRFrontendModule::Inventory:
		return CompleteModuleAttempt(OpenInventoryInternal());
	case EHSRFrontendModule::Party:
	case EHSRFrontendModule::Map:
	case EHSRFrontendModule::Challenge:
	case EHSRFrontendModule::Quest:
	case EHSRFrontendModule::Save:
	{
#if WITH_DEV_AUTOMATION_TESTS
		if ((!bUseAutomationBackend && (!FrontendModuleRootClass || !GetFrontendModuleWidgetClass(Module)))
			|| (bUseAutomationBackend && !bAutomationHasFrontendModuleClass))
#else
		if (!FrontendModuleRootClass || !GetFrontendModuleWidgetClass(Module))
#endif
		{
			return CompleteModuleAttempt(EHSRUIScreenResult::MissingWidgetClass);
		}
		UHSRFrontendModuleRootWidget* RootCandidate = CreateFrontendModuleRootCandidate(RegisteredPlayerController.Get());
		if (!RootCandidate) return CompleteModuleAttempt(EHSRUIScreenResult::WidgetCreationFailed);
		UUserWidget* ContentCandidate = CreateFrontendModuleContentCandidate(RegisteredPlayerController.Get(), Module);
		if (!ContentCandidate)
		{
			RootCandidate->RemoveFromParent();
			return CompleteModuleAttempt(EHSRUIScreenResult::WidgetCreationFailed);
		}
		RootCandidate->SetOwningUIManager(this);
		if (UHSRScreenWidget* ScreenContent = Cast<UHSRScreenWidget>(ContentCandidate))
		{
			ScreenContent->SetOwningUIManager(this);
		}
		RootCandidate->PresentModule(Module);
		const FHSRFrontendRouteSnapshot OldRoute = FrontendRouter->GetSnapshot();
		const FHSRInputModePolicy OldPolicy = GetResolvedInputPolicy();
		if (!AttachFrontendModuleContentCandidate(RootCandidate, ContentCandidate)
			|| !AttachFrontendModuleRootCandidate(RootCandidate))
		{
			RootCandidate->ClearModuleContent();
			RootCandidate->RemoveFromParent();
			ContentCandidate->RemoveFromParent();
			return CompleteModuleAttempt(EHSRUIScreenResult::ViewportAttachFailed);
		}
		if (!ApplyPolicyBackend(RegisteredPlayerController.Get(), GetResolvedInputPolicy(), EHSRPlayerControlMode::UIOnly))
		{
			RootCandidate->ClearModuleContent();
			RootCandidate->RemoveFromParent();
			ContentCandidate->RemoveFromParent();
			const bool bPolicyRestored = ApplyPolicyBackend(RegisteredPlayerController.Get(), OldPolicy, EHSRPlayerControlMode::UIOnly);
			return CompleteModuleAttempt(ResolveCompensation(bPolicyRestored, EHSRUIScreenResult::PolicyApplyFailed));
		}
		if (ApplyFocusBackend(RegisteredPlayerController.Get(), RootCandidate->GetPreferredFocusWidget(), RootCandidate) == EHSRFocusApplyResult::Unavailable)
		{
			RootCandidate->ClearModuleContent();
			RootCandidate->RemoveFromParent();
			ContentCandidate->RemoveFromParent();
			const bool bPolicyRestored = ApplyPolicyBackend(RegisteredPlayerController.Get(), OldPolicy, EHSRPlayerControlMode::UIOnly);
			return CompleteModuleAttempt(ResolveCompensation(bPolicyRestored, EHSRUIScreenResult::FocusApplyFailed));
		}
		FHSRFrontendRouteRequest RouteRequest;
		RouteRequest.RequestToken = AllocateFrontendRequestToken();
		RouteRequest.Route.Module = Module;
		const EHSRFrontendRouteResult RouteResult = SubmitFrontendRoute(RouteRequest);
		if (RouteResult != EHSRFrontendRouteResult::Success && RouteResult != EHSRFrontendRouteResult::NoOp)
		{
			RootCandidate->ClearModuleContent();
			RootCandidate->RemoveFromParent();
			ContentCandidate->RemoveFromParent();
			FrontendRouter->RestoreSnapshotForTransaction(OldRoute);
			const bool bPolicyRestored = ApplyPolicyBackend(RegisteredPlayerController.Get(), OldPolicy, EHSRPlayerControlMode::UIOnly);
			const bool bFocusRestored = RestoreFrontendModuleFocus(
				RegisteredPlayerController.Get(), OldRoute.GetActiveRoute().Module);
			return CompleteModuleAttempt(ResolveCompensation(
				bPolicyRestored && bFocusRestored, EHSRUIScreenResult::StackRejected));
		}
		ReleaseFrontendModuleContent();
		if (FrontendModuleRootInstance)
		{
			FrontendModuleRootInstance->RemoveFromParent();
			FrontendModuleRootInstance = nullptr;
		}
		if (CharacterDetailWidgetInstance) { CharacterDetailWidgetInstance->RemoveFromParent(); CharacterDetailWidgetInstance = nullptr; }
		if (InventoryWidgetInstance)
		{
			InventoryWidgetInstance->SetViewModel(nullptr); InventoryWidgetInstance->RemoveFromParent(); InventoryWidgetInstance = nullptr;
			UHSRInventoryRewardViewModel* OldVM = InventoryViewModelInstance; InventoryViewModelInstance = nullptr; ShutdownInventoryViewModelCandidate(OldVM);
		}
		FrontendModuleRootInstance = RootCandidate;
		FrontendModuleContentInstance = ContentCandidate;
		FrontendModuleContentModule = Module;
		FrontendShellInstance->PresentRoute(FrontendRouter->GetSnapshot());
		return RouteResult == EHSRFrontendRouteResult::NoOp ? EHSRUIScreenResult::NoOp : EHSRUIScreenResult::Success;
	}
	case EHSRFrontendModule::None:
	default:
		return EHSRUIScreenResult::StackRejected;
	}
}

EHSRUIScreenResult UHSRUIManagerSubsystem::CloseFrontendToRoot()
{
	if (!FrontendShellInstance) return EHSRUIScreenResult::NothingOpen;
	AHSRPlayerController* PC = RegisteredPlayerController.Get();
	UHSRUserWidget* RootWidget = RegisteredRootWidget.Get();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!IsBackendHostValid(PC, RootWidget, World)) return EHSRUIScreenResult::InvalidHost;
	const FHSRScreenStackSnapshot OldStack = ScreenStack->GetSnapshot();
	const FHSRFrontendRouteSnapshot OldRoute = FrontendRouter ? FrontendRouter->GetSnapshot() : FHSRFrontendRouteSnapshot{};
	const FHSRInputModePolicy OldPolicy = GetResolvedInputPolicy();
	const FHSRScreenRequest CloseRequest{AllocateRequestToken(), EHSRScreenStackOperation::CloseToRoot};
	const EHSRScreenStackResult StackResult = ScreenStack->SubmitRequest(CloseRequest);
	if (StackResult != EHSRScreenStackResult::Success && StackResult != EHSRScreenStackResult::NoOp)
		return EHSRUIScreenResult::StackRejected;
	if (!ApplyPolicyBackend(PC, GetResolvedInputPolicy(), EHSRPlayerControlMode::Exploration))
	{
		ScreenStack->RestoreSnapshotForTransaction(OldStack);
		const bool bPolicyRestored = ApplyPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		return ResolveCompensation(bPolicyRestored, EHSRUIScreenResult::PolicyApplyFailed);
	}
	if (PauseOwnerToken.IsValid() && IsBackendPaused(World) && !ApplyPauseBackend(World, false))
	{
		ScreenStack->RestoreSnapshotForTransaction(OldStack);
		const bool bPolicyRestored = ApplyPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		return ResolveCompensation(bPolicyRestored, EHSRUIScreenResult::PauseApplyFailed);
	}
	if (ApplyFocusBackend(PC, RootWidget, RootWidget) == EHSRFocusApplyResult::Unavailable)
	{
		const bool bPauseRestored = ApplyPauseBackend(World, true); ScreenStack->RestoreSnapshotForTransaction(OldStack);
		const bool bPolicyRestored = ApplyPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		return ResolveCompensation(bPauseRestored && bPolicyRestored, EHSRUIScreenResult::FocusApplyFailed);
	}
	FHSRFrontendRouteRequest RouteRequest;
	RouteRequest.RequestToken = AllocateFrontendRequestToken();
	RouteRequest.Operation = EHSRFrontendRouteOperation::CloseToRoot;
	if (!FrontendRouter || (FrontendRouter->Submit(RouteRequest) != EHSRFrontendRouteResult::Success
		&& FrontendRouter->GetSnapshot().IsOpen()))
	{
		const bool bPauseRestored = ApplyPauseBackend(World, true); ScreenStack->RestoreSnapshotForTransaction(OldStack);
		if (FrontendRouter) FrontendRouter->RestoreSnapshotForTransaction(OldRoute);
		const bool bPolicyRestored = ApplyPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		return ResolveCompensation(bPauseRestored && bPolicyRestored, EHSRUIScreenResult::StackRejected);
	}
	if (FrontendModuleRootInstance)
	{
		ReleaseFrontendModuleContent();
		FrontendModuleRootInstance->RemoveFromParent();
		FrontendModuleRootInstance = nullptr;
	}
	if (CharacterDetailWidgetInstance) { CharacterDetailWidgetInstance->RemoveFromParent(); CharacterDetailWidgetInstance = nullptr; }
	if (InventoryWidgetInstance)
	{
		InventoryWidgetInstance->SetViewModel(nullptr); InventoryWidgetInstance->RemoveFromParent(); InventoryWidgetInstance = nullptr;
		UHSRInventoryRewardViewModel* OldVM = InventoryViewModelInstance; InventoryViewModelInstance = nullptr; ShutdownInventoryViewModelCandidate(OldVM);
	}
	FrontendShellInstance->RemoveFromParent(); FrontendShellInstance = nullptr;
	PauseOwnerToken.Invalidate();
	return EHSRUIScreenResult::Success;
}

EHSRUIScreenResult UHSRUIManagerSubsystem::CloseCharacterDetailScreen()
{
	AHSRPlayerController* PC = RegisteredPlayerController.Get();
	UHSRUserWidget* RootWidget = RegisteredRootWidget.Get();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!CharacterDetailWidgetInstance || !IsBackendHostValid(PC, RootWidget, World))
	{
		return EHSRUIScreenResult::InvalidHost;
	}
	const FHSRFrontendRouteSnapshot OldRoute = FrontendRouter->GetSnapshot();
	const FHSRInputModePolicy OldPolicy = GetResolvedInputPolicy();
	if (!ApplyCharacterDetailPolicyBackend(PC, GetResolvedInputPolicy(), FrontendShellInstance ? EHSRPlayerControlMode::UIOnly : EHSRPlayerControlMode::Exploration))
	{
		if (!ApplyCharacterDetailPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly)) { bInconsistent = true; return EHSRUIScreenResult::CompensationFailed; }
		return EHSRUIScreenResult::PolicyApplyFailed;
	}
	const EHSRFocusApplyResult FocusResult = ApplyCharacterDetailFocusBackend(PC, FrontendShellInstance, FrontendShellInstance);
	if (FocusResult == EHSRFocusApplyResult::Unavailable)
	{
		if (!ApplyCharacterDetailPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly)) { bInconsistent = true; return EHSRUIScreenResult::CompensationFailed; }
		return EHSRUIScreenResult::FocusApplyFailed;
	}
	FHSRFrontendRouteRequest RouteRequest; RouteRequest.RequestToken = AllocateFrontendRequestToken(); RouteRequest.Operation = EHSRFrontendRouteOperation::Back;
	if (SubmitFrontendRoute(RouteRequest) != EHSRFrontendRouteResult::Success)
	{
		FrontendRouter->RestoreSnapshotForTransaction(OldRoute);
		const bool bPolicyRestored = ApplyCharacterDetailPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		const bool bFocusRestored = ApplyCharacterDetailFocusBackend(PC,
			CharacterDetailWidgetInstance->GetPreferredFocusWidget(), CharacterDetailWidgetInstance) != EHSRFocusApplyResult::Unavailable;
		return ResolveCompensation(bPolicyRestored && bFocusRestored, EHSRUIScreenResult::StackRejected);
	}
	CharacterDetailWidgetInstance->RemoveFromParent(); CharacterDetailWidgetInstance = nullptr;
	FrontendShellInstance->PresentRoute(FrontendRouter->GetSnapshot());
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 CharacterDetail Close Success Stack=%d FocusResult=%d"),
		GetLogicalScreenCount(), static_cast<uint8>(FocusResult));
	return EHSRUIScreenResult::Success;
}

EHSRUIScreenResult UHSRUIManagerSubsystem::CloseInventoryScreen()
{
	AHSRPlayerController* PC = RegisteredPlayerController.Get();
	UHSRUserWidget* RootWidget = RegisteredRootWidget.Get();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!InventoryWidgetInstance || !InventoryViewModelInstance || !IsBackendHostValid(PC, RootWidget, World))
		return EHSRUIScreenResult::InvalidHost;
	const FHSRFrontendRouteSnapshot OldRoute = FrontendRouter->GetSnapshot();
	const FHSRInputModePolicy OldPolicy = GetResolvedInputPolicy();
	if (!ApplyInventoryPolicyBackend(PC, GetResolvedInputPolicy(), FrontendShellInstance ? EHSRPlayerControlMode::UIOnly : EHSRPlayerControlMode::Exploration))
	{
		if (!ApplyInventoryPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly)) { bInconsistent = true; return EHSRUIScreenResult::CompensationFailed; }
		return EHSRUIScreenResult::PolicyApplyFailed;
	}
	const EHSRFocusApplyResult FocusResult = ApplyInventoryFocusBackend(PC, FrontendShellInstance, FrontendShellInstance);
	if (FocusResult == EHSRFocusApplyResult::Unavailable)
	{
		if (!ApplyInventoryPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly)) { bInconsistent = true; return EHSRUIScreenResult::CompensationFailed; }
		return EHSRUIScreenResult::FocusApplyFailed;
	}
	FHSRFrontendRouteRequest RouteRequest; RouteRequest.RequestToken = AllocateFrontendRequestToken(); RouteRequest.Operation = EHSRFrontendRouteOperation::Back;
	if (SubmitFrontendRoute(RouteRequest) != EHSRFrontendRouteResult::Success)
	{
		FrontendRouter->RestoreSnapshotForTransaction(OldRoute);
		const bool bPolicyRestored = ApplyInventoryPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		const bool bFocusRestored = ApplyInventoryFocusBackend(PC,
			InventoryWidgetInstance->GetPreferredFocusWidget(), InventoryWidgetInstance) != EHSRFocusApplyResult::Unavailable;
		return ResolveCompensation(bPolicyRestored && bFocusRestored, EHSRUIScreenResult::StackRejected);
	}
	InventoryWidgetInstance->SetViewModel(nullptr);
#if WITH_DEV_AUTOMATION_TESTS
	LastReleasedInventoryBindCount = InventoryWidgetInstance->GetBindCountForAutomation();
	LastReleasedInventoryUnbindCount = InventoryWidgetInstance->GetUnbindCountForAutomation();
#endif
	InventoryWidgetInstance->RemoveFromParent();
	InventoryWidgetInstance = nullptr;
	UHSRInventoryRewardViewModel* ViewModelToShutdown = InventoryViewModelInstance;
	InventoryViewModelInstance = nullptr;
	ShutdownInventoryViewModelCandidate(ViewModelToShutdown);
	FrontendShellInstance->PresentRoute(FrontendRouter->GetSnapshot());
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 Inventory Close Success Stack=%d FocusResult=%d"),
		GetLogicalScreenCount(), static_cast<uint8>(FocusResult));
	return EHSRUIScreenResult::Success;
}

int64 UHSRUIManagerSubsystem::AllocateRequestToken()
{
	if (ScreenStack)
	{
		NextRequestToken = FMath::Max(NextRequestToken, ScreenStack->GetSnapshot().LastProcessedRequestToken + 1);
	}
	return NextRequestToken++;
}

bool UHSRUIManagerSubsystem::RestoreFrontendModuleFocus(AHSRPlayerController* PlayerController,
	const EHSRFrontendModule Module)
{
	switch (Module)
	{
	case EHSRFrontendModule::Character:
		return CharacterDetailWidgetInstance
			&& ApplyCharacterDetailFocusBackend(PlayerController,
				CharacterDetailWidgetInstance->GetPreferredFocusWidget(), CharacterDetailWidgetInstance)
				!= EHSRFocusApplyResult::Unavailable;
	case EHSRFrontendModule::Inventory:
		return InventoryWidgetInstance
			&& ApplyInventoryFocusBackend(PlayerController,
				InventoryWidgetInstance->GetPreferredFocusWidget(), InventoryWidgetInstance)
				!= EHSRFocusApplyResult::Unavailable;
	case EHSRFrontendModule::Party:
	case EHSRFrontendModule::Map:
	case EHSRFrontendModule::Challenge:
	case EHSRFrontendModule::Quest:
	case EHSRFrontendModule::Save:
		return FrontendModuleRootInstance
			&& ApplyFocusBackend(PlayerController, FrontendModuleRootInstance->GetPreferredFocusWidget(),
				FrontendModuleRootInstance) != EHSRFocusApplyResult::Unavailable;
	case EHSRFrontendModule::PauseHub:
		return FrontendShellInstance
			&& ApplyFocusBackend(PlayerController, FrontendShellInstance->GetPreferredFocusWidget(),
				FrontendShellInstance) != EHSRFocusApplyResult::Unavailable;
	case EHSRFrontendModule::None:
	default:
		return false;
	}
}

int64 UHSRUIManagerSubsystem::AllocateFrontendRequestToken()
{
	if (FrontendRouter)
		NextFrontendRequestToken = FMath::Max(NextFrontendRequestToken,
			FrontendRouter->GetSnapshot().LastProcessedRequestToken + 1);
	return NextFrontendRequestToken++;
}

EHSRFrontendRouteResult UHSRUIManagerSubsystem::SubmitFrontendRoute(const FHSRFrontendRouteRequest& Request)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend && bAutomationFailNextRouteSubmit)
	{
		bAutomationFailNextRouteSubmit = false;
		return EHSRFrontendRouteResult::InvalidRequest;
	}
#endif
	return FrontendRouter ? FrontendRouter->Submit(Request) : EHSRFrontendRouteResult::InvalidRequest;
}

EHSRUIScreenResult UHSRUIManagerSubsystem::ResolveCompensation(const bool bRecovered,
	const EHSRUIScreenResult OriginalFailure)
{
	if (bRecovered) return OriginalFailure;
	// A failed compensation left the backend in an unknown state; never auto-clear this.
	bInconsistent = true;
	bInconsistencyIsTravelRecoverable = false;
	return EHSRUIScreenResult::CompensationFailed;
}

FHSRScreenRequest UHSRUIManagerSubsystem::MakeRootRequest(const int64 Token) const
{
	FHSRScreenRequest Request;
	Request.RequestToken = Token;
	Request.ScreenId = ExplorationRootId;
	Request.Layer = EHSRUIScreenLayer::HUD;
	Request.InputIntent = EHSRUIInputIntent::GameOnly;
	return Request;
}

FHSRScreenRequest UHSRUIManagerSubsystem::MakePauseRequest(const int64 Token) const
{
	FHSRScreenRequest Request;
	Request.RequestToken = Token;
	Request.ScreenId = PauseScreenId;
	Request.Layer = EHSRUIScreenLayer::Modal;
	Request.InputIntent = EHSRUIInputIntent::UIOnly;
	Request.FocusToken = PauseFocusToken;
	return Request;
}

FHSRScreenRequest UHSRUIManagerSubsystem::MakePopRequest(const int64 Token) const
{
	FHSRScreenRequest Request;
	Request.RequestToken = Token;
	Request.Operation = EHSRScreenStackOperation::Pop;
	return Request;
}

bool UHSRUIManagerSubsystem::CompensatePop(const FHSRInputModePolicy& RestorePolicy,
	AHSRPlayerController* PlayerController, UHSRScreenWidget* CandidateWidget)
{
	if (CandidateWidget)
	{
		CandidateWidget->RemoveFromParent();
	}
	const EHSRScreenStackResult PopResult = ScreenStack->SubmitRequest(MakePopRequest(AllocateRequestToken()));
	const bool bPolicyRestored = ApplyPolicyBackend(PlayerController, RestorePolicy,
		EHSRPlayerControlMode::Exploration);
	if (PopResult != EHSRScreenStackResult::Success || !bPolicyRestored)
	{
		bInconsistent = true;
		return false;
	}
	return true;
}

bool UHSRUIManagerSubsystem::CompensatePausePush(AHSRPlayerController* PlayerController)
{
	const EHSRScreenStackResult PushResult = ScreenStack->SubmitRequest(MakePauseRequest(AllocateRequestToken()));
	const bool bPolicyRestored = ApplyPolicyBackend(PlayerController, GetResolvedInputPolicy(),
		EHSRPlayerControlMode::UIOnly);
	if (PushResult != EHSRScreenStackResult::Success || !bPolicyRestored)
	{
		bInconsistent = true;
		return false;
	}
	return true;
}

void UHSRUIManagerSubsystem::ReleaseInventoryCandidates(UHSRInventoryWidget*& Widget,
	UHSRInventoryRewardViewModel*& ViewModel)
{
	if (Widget)
	{
		Widget->SetViewModel(nullptr);
#if WITH_DEV_AUTOMATION_TESTS
		LastReleasedInventoryBindCount = Widget->GetBindCountForAutomation();
		LastReleasedInventoryUnbindCount = Widget->GetUnbindCountForAutomation();
#endif
		Widget->RemoveFromParent();
		Widget = nullptr;
	}
	ShutdownInventoryViewModelCandidate(ViewModel);
}

void UHSRUIManagerSubsystem::ShutdownInventoryViewModelCandidate(UHSRInventoryRewardViewModel*& ViewModel)
{
	if (!ViewModel) return;
	ViewModel->Shutdown();
	ViewModel = nullptr;
#if WITH_DEV_AUTOMATION_TESTS
	++InventoryCandidateShutdownCount;
#endif
}

bool UHSRUIManagerSubsystem::HasInventoryOwnershipMismatch() const
{
	return (InventoryWidgetInstance != nullptr) != (InventoryViewModelInstance != nullptr);
}

void UHSRUIManagerSubsystem::ClearHostReferences()
{
	RegisteredHUD.Reset();
	RegisteredPlayerController.Reset();
	RegisteredRootWidget.Reset();
	FrontendShellClass = nullptr;
	FrontendModuleRootClass = nullptr;
	CharacterDetailWidgetClass = nullptr;
	InventoryWidgetClass = nullptr;
	FrontendModuleWidgetClasses.Reset();
	FrontendModuleContentInstance = nullptr;
	FrontendModuleContentModule = EHSRFrontendModule::None;
	ActiveHostGeneration = 0;
}

FName UHSRUIManagerSubsystem::SelectRestorableScreenId() const
{
	if (!ScreenStack || bInconsistent || HasInventoryOwnershipMismatch()) return NAME_None;
	const FHSRScreenStackSnapshot Snapshot = ScreenStack->GetSnapshot();
	if (Snapshot.Entries.Num() < 2 || Snapshot.Entries[0].ScreenId != ExplorationRootId)
	{
		return NAME_None;
	}
	const EHSRFrontendModule ActiveModule = FrontendRouter
		? FrontendRouter->GetSnapshot().GetActiveRoute().Module : EHSRFrontendModule::None;
	if (ActiveModule == EHSRFrontendModule::Character && CharacterDetailWidgetInstance
		&& !InventoryWidgetInstance && !InventoryViewModelInstance) return CharacterDetailScreenId;
	if (ActiveModule == EHSRFrontendModule::Inventory && InventoryWidgetInstance && InventoryViewModelInstance
		&& !CharacterDetailWidgetInstance) return InventoryScreenId;
	return NAME_None;
}

bool UHSRUIManagerSubsystem::IsAtCleanExplorationRoot() const
{
	if (!ScreenStack || HasInventoryOwnershipMismatch()) return false;
	if (FrontendShellInstance || FrontendModuleRootInstance || FrontendModuleContentInstance
		|| FrontendModuleContentModule != EHSRFrontendModule::None || CharacterDetailWidgetInstance
		|| InventoryWidgetInstance || InventoryViewModelInstance) return false;
	const FHSRScreenStackSnapshot Snapshot = ScreenStack->GetSnapshot();
	return Snapshot.Entries.Num() == 1 && Snapshot.Entries[0].ScreenId == ExplorationRootId;
}

void UHSRUIManagerSubsystem::TryClearRecoverableInconsistency()
{
	if (!bInconsistent || !bInconsistencyIsTravelRecoverable) return;
	// Only a live registered host plus an exact, unowned root proves the UI is coherent again.
	AHSRPlayerController* PC = RegisteredPlayerController.Get();
	if (ActiveHostGeneration == 0
		|| !IsBackendHostValid(PC, RegisteredRootWidget.Get(), PC ? PC->GetWorld() : nullptr)
		|| !IsAtCleanExplorationRoot()) return;
	bInconsistent = false;
	bInconsistencyIsTravelRecoverable = false;
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 Inconsistency cleared by fresh host HostGeneration=%lld Stack=%d"),
		ActiveHostGeneration, GetLogicalScreenCount());
}

EHSRUIScreenResult UHSRUIManagerSubsystem::CaptureAndTeardownTravelHost()
{
	const int64 CapturedHost = ActiveHostGeneration;
	// Capture ownership while the old host is still alive; teardown releases the
	// widget/view-model pair that SelectRestorableScreenId validates.
	const FName Restorable = SelectRestorableScreenId();
	AHSRPlayerController* CapturedPC = RegisteredPlayerController.Get();
	int64 ArrivalBaseline = LastObservedArrivalCommitGeneration;
	if (UGameInstance* GameInstance = GetLocalPlayer() ? GetLocalPlayer()->GetGameInstance() : nullptr)
		if (const UHSRMapSubsystem* Maps = GameInstance->GetSubsystem<UHSRMapSubsystem>())
			ArrivalBaseline = FMath::Max(ArrivalBaseline, Maps->GetArrivalCommitGeneration());

	EHSRUIScreenResult Result = TeardownCurrentHost();
	bool bForcedRootCleanup = false;
	while (ScreenStack && ScreenStack->GetSnapshot().Entries.Num() > 1)
	{
		bForcedRootCleanup = true;
		if (ScreenStack->SubmitRequest(MakePopRequest(AllocateRequestToken())) != EHSRScreenStackResult::Success)
		{
			Result = EHSRUIScreenResult::Inconsistent;
			break;
		}
	}
	const FHSRScreenStackSnapshot PostTeardown = ScreenStack ? ScreenStack->GetSnapshot() : FHSRScreenStackSnapshot{};
	const bool bAtExactRoot = PostTeardown.Entries.Num() == 1
		&& PostTeardown.Entries[0].ScreenId == ExplorationRootId;
	if (!bAtExactRoot || (bForcedRootCleanup
		&& !ApplyPolicyBackend(CapturedPC, GetResolvedInputPolicy(), EHSRPlayerControlMode::Exploration)))
	{
		Result = EHSRUIScreenResult::Inconsistent;
	}
	if (bForcedRootCleanup)
	{
		bInconsistent = true;
		// Travel cleanup that still lands on an exact root is recoverable: the next host
		// registration re-validates the stack and clears the flag. Anything else is real corruption.
		bInconsistencyIsTravelRecoverable = bAtExactRoot;
		Result = EHSRUIScreenResult::Inconsistent;
		UE_LOG(LogTemp, Warning, TEXT("HSRUI P17 TravelFreeze forced non-owned stack cleanup to root Recoverable=%s"),
			bInconsistencyIsTravelRecoverable ? TEXT("true") : TEXT("false"));
	}
	TravelRestoreGeneration = NextTravelRestoreGeneration++;
	TravelCapturedHostGeneration = CapturedHost;
	MinimumArrivalCommitGeneration = ArrivalBaseline + 1;
	TravelRestoreScreenId = Result == EHSRUIScreenResult::Success ? Restorable : NAME_None;
	bTravelRestorePending = true;
	bTravelArrivalObserved = false;
	LatchedArrivalCommitGeneration = 0;
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 TravelFreeze Generation=%lld HostGeneration=%lld Screen=%s MinArrival=%lld Teardown=%d"),
		TravelRestoreGeneration, TravelCapturedHostGeneration, *TravelRestoreScreenId.ToString(),
		MinimumArrivalCommitGeneration, static_cast<int32>(Result));
	return Result;
}

void UHSRUIManagerSubsystem::HandleArrivalCommitted(const FHSRMapArrivalCommitInfo& Info)
{
	LastObservedArrivalCommitGeneration = FMath::Max(LastObservedArrivalCommitGeneration, Info.CommitGeneration);
	if (!bTravelRestorePending || Info.CommitGeneration < MinimumArrivalCommitGeneration) return;
	bTravelArrivalObserved = true;
	LatchedArrivalCommitGeneration = Info.CommitGeneration;
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 TravelArrival Generation=%lld ArrivalGeneration=%lld Map=%s Kind=%d HostGeneration=%lld"),
		TravelRestoreGeneration, Info.CommitGeneration, *Info.MapId.ToString(), static_cast<int32>(Info.Kind), ActiveHostGeneration);
	TryRestoreTravelDescriptor();
}

void UHSRUIManagerSubsystem::TryRestoreTravelDescriptor()
{
	AHSRPlayerController* RegisteredPC = RegisteredPlayerController.Get();
	UHSRUserWidget* RegisteredRoot = RegisteredRootWidget.Get();
	UWorld* RegisteredWorld = RegisteredPC ? RegisteredPC->GetWorld() : nullptr;
	if (!bTravelRestorePending || !bTravelArrivalObserved
		|| LatchedArrivalCommitGeneration < MinimumArrivalCommitGeneration
		|| ActiveHostGeneration <= TravelCapturedHostGeneration
		|| !IsBackendHostValid(RegisteredPC, RegisteredRoot, RegisteredWorld)) return;
	const int64 DescriptorGeneration = TravelRestoreGeneration;
	const FName ScreenId = TravelRestoreScreenId;
	bTravelRestorePending = false;
	bTravelArrivalObserved = false;
	LatchedArrivalCommitGeneration = 0;
	TravelRestoreScreenId = NAME_None;
	EHSRUIScreenResult Result = EHSRUIScreenResult::Success;
	if (!ScreenId.IsNone())
	{
		if (ScreenId == CharacterDetailScreenId)
		{
			Result = OpenFrontendModule(EHSRFrontendModule::Character);
		}
		else if (ScreenId == InventoryScreenId)
		{
			Result = OpenFrontendModule(EHSRFrontendModule::Inventory);
		}
		else
		{
			Result = EHSRUIScreenResult::Inconsistent;
		}
	}
	else
	{
		AHSRPlayerController* PC = RegisteredPlayerController.Get();
		Result = ApplyPolicyBackend(PC, GetResolvedInputPolicy(), EHSRPlayerControlMode::Exploration)
			? EHSRUIScreenResult::Success : EHSRUIScreenResult::PolicyApplyFailed;
		ApplyFocusBackend(PC, RegisteredRootWidget.Get(), RegisteredRootWidget.Get());
	}
	bTravelRestoredModule = Result == EHSRUIScreenResult::Success && !ScreenId.IsNone();
	if (Result == EHSRUIScreenResult::CompensationFailed || Result == EHSRUIScreenResult::Inconsistent)
	{
		bInconsistent = true;
	}
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 TravelRestore Consume Generation=%lld NewHostGeneration=%lld Screen=%s Result=%d Stack=%d"),
		DescriptorGeneration, ActiveHostGeneration, *ScreenId.ToString(), static_cast<int32>(Result), GetLogicalScreenCount());
}

bool UHSRUIManagerSubsystem::IsBackendHostValid(AHSRPlayerController* PlayerController,
	UHSRUserWidget* RootWidget, UWorld* World) const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return bAutomationHostValid;
	}
#endif
	return RegisteredHUD.IsValid() && PlayerController && PlayerController->IsLocalPlayerController() && RootWidget && World;
}

bool UHSRUIManagerSubsystem::IsBackendExploration(AHSRPlayerController* PlayerController) const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return bAutomationExploration;
	}
#endif
	return PlayerController && PlayerController->GetControlMode() == EHSRPlayerControlMode::Exploration;
}

bool UHSRUIManagerSubsystem::IsBackendPaused(UWorld* World) const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return bAutomationPaused;
	}
#endif
	return World && World->IsPaused();
}

bool UHSRUIManagerSubsystem::HasModuleRootClass() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return true;
	}
#endif
	return FrontendModuleRootClass != nullptr;
}

bool UHSRUIManagerSubsystem::HasCharacterDetailClass() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return bAutomationHasDetailClass;
	}
#endif
	return CharacterDetailWidgetClass != nullptr;
}

bool UHSRUIManagerSubsystem::HasInventoryClass() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return bAutomationHasInventoryClass;
	}
#endif
	return InventoryWidgetClass != nullptr;
}

bool UHSRUIManagerSubsystem::IsTravelPending() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend) return false;
#endif
	UGameInstance* GameInstance = GetLocalPlayer() ? GetLocalPlayer()->GetGameInstance() : nullptr;
	const UHSRMapSubsystem* Maps = GameInstance ? GameInstance->GetSubsystem<UHSRMapSubsystem>() : nullptr;
	const UHSRBattleTransitionSubsystem* Battle = GameInstance ? GameInstance->GetSubsystem<UHSRBattleTransitionSubsystem>() : nullptr;
	return (Maps && Maps->HasPendingTravel()) || (Battle && (Battle->HasPending() || Battle->HasReturnPending()));
}

UHSRFrontendShellWidget* UHSRUIManagerSubsystem::CreatePauseCandidate(AHSRPlayerController* PlayerController)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return bAutomationCreateSucceeds ? NewObject<UHSRFrontendShellWidget>(this) : nullptr;
	}
#endif
	return CreateWidget<UHSRFrontendShellWidget>(PlayerController, FrontendShellClass);
}

UHSRFrontendModuleRootWidget* UHSRUIManagerSubsystem::CreateFrontendModuleRootCandidate(AHSRPlayerController* PlayerController)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return bAutomationCreateSucceeds ? NewObject<UHSRFrontendModuleRootWidget>(this) : nullptr;
	}
#endif
	return CreateWidget<UHSRFrontendModuleRootWidget>(PlayerController, FrontendModuleRootClass);
}

UHSRScreenWidget* UHSRUIManagerSubsystem::CreateCharacterDetailCandidate(AHSRPlayerController* PlayerController)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return bAutomationDetailCreateSucceeds ? NewObject<UHSRCharacterDetailWidget>(this) : nullptr;
	}
#endif
	return CreateWidget<UHSRScreenWidget>(PlayerController, CharacterDetailWidgetClass);
}

UHSRInventoryWidget* UHSRUIManagerSubsystem::CreateInventoryCandidate(AHSRPlayerController* PlayerController)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
		return bAutomationInventoryCreateSucceeds ? NewObject<UHSRInventoryWidget>(this) : nullptr;
#endif
	return CreateWidget<UHSRInventoryWidget>(PlayerController, InventoryWidgetClass);
}

UHSRInventoryRewardViewModel* UHSRUIManagerSubsystem::CreateInventoryViewModelCandidate()
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend && !bAutomationInventoryViewModelSucceeds) return nullptr;
#endif
	return NewObject<UHSRInventoryRewardViewModel>(this);
}

bool UHSRUIManagerSubsystem::AttachPauseCandidate(UHSRFrontendShellWidget* Candidate)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return bAutomationAttachSucceeds;
	}
#endif
	Candidate->AddToViewport(100);
	return Candidate->IsInViewport();
}

bool UHSRUIManagerSubsystem::AttachFrontendModuleRootCandidate(UHSRFrontendModuleRootWidget* Candidate)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return bAutomationAttachSucceeds;
	}
#endif
	Candidate->AddToViewport(110);
	return Candidate->IsInViewport();
}

TSubclassOf<UUserWidget> UHSRUIManagerSubsystem::GetFrontendModuleWidgetClass(const EHSRFrontendModule Module) const
{
	const TSubclassOf<UUserWidget>* Found = FrontendModuleWidgetClasses.Find(Module);
	return Found ? *Found : nullptr;
}

UUserWidget* UHSRUIManagerSubsystem::CreateFrontendModuleContentCandidate(
	AHSRPlayerController* PlayerController, const EHSRFrontendModule Module)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return bAutomationFrontendModuleCreateSucceeds ? NewObject<UHSRUserWidget>(this) : nullptr;
	}
#endif
	const TSubclassOf<UUserWidget> WidgetClass = GetFrontendModuleWidgetClass(Module);
	return WidgetClass ? CreateWidget<UUserWidget>(PlayerController, WidgetClass) : nullptr;
}

bool UHSRUIManagerSubsystem::AttachFrontendModuleContentCandidate(
	UHSRFrontendModuleRootWidget* RootCandidate, UUserWidget* ContentCandidate)
{
	if (!RootCandidate || !ContentCandidate)
	{
		return false;
	}
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return bAutomationFrontendModuleAttachSucceeds;
	}
#endif
	return RootCandidate->SetModuleContent(ContentCandidate);
}

void UHSRUIManagerSubsystem::ReleaseFrontendModuleContent()
{
	if (FrontendModuleRootInstance)
	{
		FrontendModuleRootInstance->ClearModuleContent();
	}
	if (FrontendModuleContentInstance)
	{
		FrontendModuleContentInstance->RemoveFromParent();
		FrontendModuleContentInstance = nullptr;
	}
	FrontendModuleContentModule = EHSRFrontendModule::None;
}

bool UHSRUIManagerSubsystem::ApplyPolicyBackend(AHSRPlayerController* PlayerController,
	const FHSRInputModePolicy& Policy, const EHSRPlayerControlMode SemanticMode)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		if (AutomationPolicyCallsUntilFailure > 0 && --AutomationPolicyCallsUntilFailure == 0) return false;
		if (bAutomationFailNextPolicyApply) { bAutomationFailNextPolicyApply = false; return false; }
		return bAutomationPolicySucceeds;
	}
#endif
	return InputModeCoordinator->ApplyPolicy(PlayerController, Policy, SemanticMode);
}

bool UHSRUIManagerSubsystem::ApplyPauseBackend(UWorld* World, const bool bPaused)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		if (AutomationPauseCallsUntilFailure > 0 && --AutomationPauseCallsUntilFailure == 0) return false;
		if (bPaused && bAutomationFailPauseRestore) { bAutomationFailPauseRestore = false; return false; }
		if (bAutomationFailNextPauseApply) { bAutomationFailNextPauseApply = false; return false; }
		if (!bAutomationPauseSucceeds)
		{
			return false;
		}
		bAutomationPaused = bPaused;
		return true;
	}
#endif
	return World && UGameplayStatics::SetGamePaused(World, bPaused);
}

bool UHSRUIManagerSubsystem::ApplyCharacterDetailPolicyBackend(AHSRPlayerController* PlayerController,
	const FHSRInputModePolicy& Policy, const EHSRPlayerControlMode SemanticMode)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		if (bAutomationFailNextDetailPolicyApply)
		{
			bAutomationFailNextDetailPolicyApply = false;
			return false;
		}
		return bAutomationDetailPolicySucceeds;
	}
#endif
	return InputModeCoordinator->ApplyPolicy(PlayerController, Policy, SemanticMode);
}

bool UHSRUIManagerSubsystem::ApplyInventoryPolicyBackend(AHSRPlayerController* PlayerController,
	const FHSRInputModePolicy& Policy, const EHSRPlayerControlMode SemanticMode)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		if (bAutomationFailNextInventoryPolicyApply)
		{
			bAutomationFailNextInventoryPolicyApply = false;
			return false;
		}
		return bAutomationInventoryPolicySucceeds;
	}
#endif
	return InputModeCoordinator->ApplyPolicy(PlayerController, Policy, SemanticMode);
}

EHSRFocusApplyResult UHSRUIManagerSubsystem::ApplyFocusBackend(AHSRPlayerController* PlayerController,
	UWidget* Preferred, UWidget* Fallback)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		if (bAutomationFailNextFocusApply) { bAutomationFailNextFocusApply = false; return EHSRFocusApplyResult::Unavailable; }
		LastAutomationFocusModule = Preferred == FrontendShellInstance ? EHSRFrontendModule::PauseHub : EHSRFrontendModule::None;
		return bAutomationFocusSucceeds ? EHSRFocusApplyResult::Preferred : EHSRFocusApplyResult::Unavailable;
	}
#endif
	return InputModeCoordinator->ApplyFocus(PlayerController, Preferred, Fallback);
}

EHSRFocusApplyResult UHSRUIManagerSubsystem::ApplyCharacterDetailFocusBackend(AHSRPlayerController* PlayerController,
	UWidget* Preferred, UWidget* Fallback)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		if (bAutomationFailOldModuleFocusRestore && (Preferred == CharacterDetailWidgetInstance || Fallback == CharacterDetailWidgetInstance))
		{
			bAutomationFailOldModuleFocusRestore = false; return EHSRFocusApplyResult::Unavailable;
		}
		LastAutomationFocusModule = (Preferred == CharacterDetailWidgetInstance || Fallback == CharacterDetailWidgetInstance) ? EHSRFrontendModule::Character : EHSRFrontendModule::PauseHub;
		const bool bIsCloseFocus = FrontendShellInstance && Preferred == FrontendShellInstance;
		if (bIsCloseFocus && !bAutomationDetailCloseFocusSucceeds)
		{
			return EHSRFocusApplyResult::Unavailable;
		}
		return bAutomationDetailFocusSucceeds ? EHSRFocusApplyResult::Preferred : EHSRFocusApplyResult::Unavailable;
	}
#endif
	return InputModeCoordinator->ApplyFocus(PlayerController, Preferred, Fallback);
}

EHSRFocusApplyResult UHSRUIManagerSubsystem::ApplyInventoryFocusBackend(AHSRPlayerController* PlayerController,
	UWidget* Preferred, UWidget* Fallback)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		if (bAutomationFailOldModuleFocusRestore && (Preferred == InventoryWidgetInstance || Fallback == InventoryWidgetInstance))
		{
			bAutomationFailOldModuleFocusRestore = false; return EHSRFocusApplyResult::Unavailable;
		}
		LastAutomationFocusModule = (Preferred == InventoryWidgetInstance || Fallback == InventoryWidgetInstance) ? EHSRFrontendModule::Inventory : EHSRFrontendModule::PauseHub;
		const bool bIsCloseFocus = FrontendShellInstance && Preferred == FrontendShellInstance;
		if (bIsCloseFocus && !bAutomationInventoryCloseFocusSucceeds)
		{
			return EHSRFocusApplyResult::Unavailable;
		}
		return bAutomationInventoryFocusSucceeds ? EHSRFocusApplyResult::Preferred : EHSRFocusApplyResult::Unavailable;
	}
#endif
	return InputModeCoordinator->ApplyFocus(PlayerController, Preferred, Fallback);
}

#if WITH_DEV_AUTOMATION_TESTS
void UHSRUIManagerSubsystem::InitializeForAutomation()
{
	if (!bInitialized)
	{
		ScreenStack = NewObject<UHSRScreenStack>(this);
		InputModeCoordinator = NewObject<UHSRInputModeCoordinator>(this);
		FrontendRouter = NewObject<UHSRFrontendRouter>(this);
		NextRequestToken = 1;
		NextFrontendRequestToken = 1;
		bInitialized = true;
		bInconsistent = false;
	}
}

void UHSRUIManagerSubsystem::RegisterHostForAutomation(const bool bInExplorationMode, const bool bHasPauseClass)
{
	RegisterHostIdentityForAutomation(1, bInExplorationMode, bHasPauseClass);
}

EHSRUIScreenResult UHSRUIManagerSubsystem::RegisterHostIdentityForAutomation(const int32 HostIdentity,
	const bool bInExplorationMode, const bool bHasPauseClass)
{
	bUseAutomationBackend = true;
	if (HostIdentity <= 0)
	{
		return EHSRUIScreenResult::InvalidHost;
	}
	if (HasInventoryOwnershipMismatch())
	{
		bInconsistent = true;
		bInconsistencyIsTravelRecoverable = false;
		return EHSRUIScreenResult::Inconsistent;
	}
	if (AutomationHostIdentity == HostIdentity)
	{
		bAutomationExploration = bInExplorationMode;
		bAutomationHasPauseClass = bHasPauseClass;
		return EHSRUIScreenResult::NoOp;
	}
	if (AutomationHostIdentity != 0 || FrontendShellInstance || FrontendModuleContentInstance
		|| CharacterDetailWidgetInstance || InventoryWidgetInstance || InventoryViewModelInstance)
	{
		return EHSRUIScreenResult::InvalidHost;
	}
	if (ScreenStack && ScreenStack->GetSnapshot().Entries.IsEmpty()
		&& ScreenStack->SubmitRequest(MakeRootRequest(AllocateRequestToken())) != EHSRScreenStackResult::Success)
	{
		return EHSRUIScreenResult::StackRejected;
	}
	AutomationHostIdentity = HostIdentity;
	bAutomationHostValid = true;
	ActiveHostGeneration = NextHostGeneration++;
	bAutomationExploration = bInExplorationMode;
	bAutomationHasPauseClass = bHasPauseClass;
	bAutomationHasDetailClass = true;
	bAutomationHasInventoryClass = true;
	TryClearRecoverableInconsistency();
	TryRestoreTravelDescriptor();
	return EHSRUIScreenResult::Success;
}

EHSRUIScreenResult UHSRUIManagerSubsystem::UnregisterHostIdentityForAutomation(const int32 HostIdentity)
{
	if (HostIdentity <= 0 || AutomationHostIdentity != HostIdentity)
	{
		return EHSRUIScreenResult::InvalidHost;
	}
	return TeardownCurrentHost();
}

EHSRUIScreenResult UHSRUIManagerSubsystem::TeardownHostIdentityForTravelForAutomation(const int32 HostIdentity)
{
	if (HostIdentity <= 0 || AutomationHostIdentity != HostIdentity || ActiveHostGeneration == 0)
		return EHSRUIScreenResult::InvalidHost;
	return CaptureAndTeardownTravelHost();
}

void UHSRUIManagerSubsystem::NotifyArrivalCommittedForAutomation(const int64 CommitGeneration, const FName MapId)
{
	FHSRMapArrivalCommitInfo Info;
	Info.CommitGeneration = CommitGeneration;
	Info.MapId = MapId;
	Info.Kind = EHSRMapArrivalCommitKind::OrdinaryTravel;
	HandleArrivalCommitted(Info);
}

void UHSRUIManagerSubsystem::ConfigureAutomationBackend(const bool bCreateSucceeds, const bool bAttachSucceeds,
	const bool bPolicySucceeds, const bool bPauseSucceeds, const bool bFocusSucceeds, const bool bInitiallyPaused)
{
	bUseAutomationBackend = true;
	bAutomationCreateSucceeds = bCreateSucceeds;
	bAutomationAttachSucceeds = bAttachSucceeds;
	bAutomationPolicySucceeds = bPolicySucceeds;
	bAutomationPauseSucceeds = bPauseSucceeds;
	bAutomationFocusSucceeds = bFocusSucceeds;
	bAutomationPaused = bInitiallyPaused;
}

void UHSRUIManagerSubsystem::ConfigureAutomationFrontendModuleBackend(
	const bool bHasClass, const bool bCreateSucceeds, const bool bAttachSucceeds)
{
	bUseAutomationBackend = true;
	bAutomationHasFrontendModuleClass = bHasClass;
	bAutomationFrontendModuleCreateSucceeds = bCreateSucceeds;
	bAutomationFrontendModuleAttachSucceeds = bAttachSucceeds;
}

int32 UHSRUIManagerSubsystem::GetFrontendModuleContentCountForAutomation() const
{
	return FrontendModuleContentInstance ? 1 : 0;
}

EHSRFrontendModule UHSRUIManagerSubsystem::GetFrontendModuleContentModuleForAutomation() const
{
	return FrontendModuleContentModule;
}

void UHSRUIManagerSubsystem::ConfigureAutomationDetailBackend(const bool bHasClass, const bool bCreateSucceeds,
	const bool bAttachSucceeds, const bool bPolicySucceeds, const bool bFocusSucceeds)
{
	bUseAutomationBackend = true;
	bAutomationHasDetailClass = bHasClass;
	bAutomationDetailCreateSucceeds = bCreateSucceeds;
	bAutomationDetailAttachSucceeds = bAttachSucceeds;
	bAutomationDetailPolicySucceeds = bPolicySucceeds;
	bAutomationDetailFocusSucceeds = bFocusSucceeds;
	bAutomationDetailCloseFocusSucceeds = bFocusSucceeds;
}

void UHSRUIManagerSubsystem::ConfigureAutomationDetailCloseFocus(const bool bCloseFocusSucceeds)
{
	bUseAutomationBackend = true;
	bAutomationDetailCloseFocusSucceeds = bCloseFocusSucceeds;
}

void UHSRUIManagerSubsystem::ConfigureAutomationInventoryCloseFocus(const bool bCloseFocusSucceeds)
{
	bUseAutomationBackend = true;
	bAutomationInventoryCloseFocusSucceeds = bCloseFocusSucceeds;
}

void UHSRUIManagerSubsystem::ConfigureAutomationInventoryBackend(const bool bHasClass, const bool bCreateSucceeds,
	const bool bViewModelSucceeds, const bool bAttachSucceeds, const bool bPolicySucceeds, const bool bFocusSucceeds)
{
	bUseAutomationBackend = true;
	bAutomationHasInventoryClass = bHasClass;
	bAutomationInventoryCreateSucceeds = bCreateSucceeds;
	bAutomationInventoryViewModelSucceeds = bViewModelSucceeds;
	bAutomationInventoryDependenciesSucceed = bViewModelSucceeds;
	bAutomationInventorySnapshotSucceeds = bViewModelSucceeds;
	bAutomationInventoryAttachSucceeds = bAttachSucceeds;
	bAutomationInventoryPolicySucceeds = bPolicySucceeds;
	bAutomationInventoryFocusSucceeds = bFocusSucceeds;
	bAutomationInventoryCloseFocusSucceeds = bFocusSucceeds;
}

void UHSRUIManagerSubsystem::ConfigureAutomationInventoryViewModelStages(const bool bDependenciesSucceed,
	const bool bCreateSucceeds, const bool bSnapshotSucceeds)
{
	bUseAutomationBackend = true;
	bAutomationInventoryDependenciesSucceed = bDependenciesSucceed;
	bAutomationInventoryViewModelSucceeds = bCreateSucceeds;
	bAutomationInventorySnapshotSucceeds = bSnapshotSucceeds;
}

void UHSRUIManagerSubsystem::InjectInventoryHalfPairForAutomation(const bool bWidgetOnly)
{
	bUseAutomationBackend = true;
	InventoryWidgetInstance = bWidgetOnly ? NewObject<UHSRInventoryWidget>(this) : nullptr;
	InventoryViewModelInstance = bWidgetOnly ? nullptr : NewObject<UHSRInventoryRewardViewModel>(this);
}

int32 UHSRUIManagerSubsystem::GetInventoryBindCountForAutomation() const
{
	return InventoryWidgetInstance ? InventoryWidgetInstance->GetBindCountForAutomation() : 0;
}

void UHSRUIManagerSubsystem::DeinitializeForAutomation()
{
	FrontendShellInstance = nullptr;
	FrontendModuleRootInstance = nullptr;
	CharacterDetailWidgetInstance = nullptr;
	if (InventoryWidgetInstance) InventoryWidgetInstance->SetViewModel(nullptr);
	InventoryWidgetInstance = nullptr;
	if (InventoryViewModelInstance) InventoryViewModelInstance->Shutdown();
	InventoryViewModelInstance = nullptr;
	PauseOwnerToken.Invalidate();
	ClearHostReferences();
	InputModeCoordinator = nullptr;
	FrontendRouter = nullptr;
	ScreenStack = nullptr;
	bInitialized = false;
	bInconsistent = false;
	bInconsistencyIsTravelRecoverable = false;
	bUseAutomationBackend = false;
	bAutomationHostValid = false;
	AutomationHostIdentity = 0;
	ActiveHostGeneration = 0;
	bTravelRestorePending = false;
	bTravelArrivalObserved = false;
}
#endif
