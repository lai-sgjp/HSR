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
	if (FrontendModuleRootInstance) { FrontendModuleRootInstance->RemoveFromParent(); FrontendModuleRootInstance = nullptr; }
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
	TSubclassOf<UHSRCharacterDetailWidget> InCharacterDetailWidgetClass,
	TSubclassOf<UHSRInventoryWidget> InInventoryWidgetClass)
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
		return EHSRUIScreenResult::Inconsistent;
	}
	if (RegisteredHUD.Get() == HUD && RegisteredPlayerController.Get() == PlayerController
		&& RegisteredRootWidget.Get() == RootWidget)
	{
		FrontendShellClass = InFrontendShellClass;
		FrontendModuleRootClass = InFrontendModuleRootClass;
		CharacterDetailWidgetClass = InCharacterDetailWidgetClass;
		InventoryWidgetClass = InInventoryWidgetClass;
		return EHSRUIScreenResult::NoOp;
	}
	if (RegisteredHUD.IsValid() || FrontendShellInstance || CharacterDetailWidgetInstance || InventoryWidgetInstance
		|| InventoryViewModelInstance)
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
	ClearHostReferences();
#if WITH_DEV_AUTOMATION_TESTS
	AutomationHostIdentity = 0;
	bAutomationHostValid = false;
#endif
	if (!bRecovered)
	{
		bInconsistent = true;
		UE_LOG(LogTemp, Error, TEXT("HSRUI P17 Host teardown required forced cleanup; host references cleared"));
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
	if (!CharacterDetailWidgetClass
#if WITH_DEV_AUTOMATION_TESTS
		&& !(bUseAutomationBackend && bAutomationHasDetailClass)
#endif
	)
	{
		return EHSRUIScreenResult::MissingWidgetClass;
	}
	UHSRCharacterDetailWidget* Candidate = CreateCharacterDetailCandidate(PC);
	if (!Candidate)
	{
		return EHSRUIScreenResult::WidgetCreationFailed;
	}
	Candidate->SetOwningUIManager(this);
	const FHSRInputModePolicy OldPolicy = GetResolvedInputPolicy();
	const FHSRFrontendRouteSnapshot OldRoute = FrontendRouter->GetSnapshot();
	if (!AttachCharacterDetailCandidate(Candidate))
	{
		Candidate->RemoveFromParent();
		const bool bRestore = ApplyCharacterDetailPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		if (!bRestore) { bInconsistent = true; return EHSRUIScreenResult::CompensationFailed; }
		return EHSRUIScreenResult::ViewportAttachFailed;
	}
	if (!ApplyCharacterDetailPolicyBackend(PC, GetResolvedInputPolicy(), EHSRPlayerControlMode::UIOnly))
	{
		Candidate->RemoveFromParent();
		const bool bRestore = ApplyCharacterDetailPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		if (!bRestore) { bInconsistent = true; return EHSRUIScreenResult::CompensationFailed; }
		return EHSRUIScreenResult::PolicyApplyFailed;
	}
	const EHSRFocusApplyResult FocusResult = ApplyCharacterDetailFocusBackend(PC, Candidate->GetPreferredFocusWidget(), Candidate);
	if (FocusResult == EHSRFocusApplyResult::Unavailable)
	{
		Candidate->RemoveFromParent();
		const bool bPolicyRestored = ApplyCharacterDetailPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		return ResolveCompensation(bPolicyRestored, EHSRUIScreenResult::FocusApplyFailed);
	}
	if (FrontendRouter && FrontendShellInstance)
	{
		FHSRFrontendRouteRequest RouteRequest; RouteRequest.RequestToken = AllocateFrontendRequestToken(); RouteRequest.Route.Module = EHSRFrontendModule::Character;
		if (SubmitFrontendRoute(RouteRequest) != EHSRFrontendRouteResult::Success)
		{
			Candidate->RemoveFromParent();
			FrontendRouter->RestoreSnapshotForTransaction(OldRoute);
			const bool bPolicyRestored = ApplyCharacterDetailPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
			const bool bFocusRestored = RestoreFrontendModuleFocus(PC, OldRoute.GetActiveRoute().Module);
			return ResolveCompensation(bPolicyRestored && bFocusRestored, EHSRUIScreenResult::StackRejected);
		}
		FrontendShellInstance->PresentRoute(FrontendRouter->GetSnapshot());
	}
	if (FrontendModuleRootInstance) { FrontendModuleRootInstance->RemoveFromParent(); FrontendModuleRootInstance = nullptr; }
	if (InventoryWidgetInstance)
	{
		InventoryWidgetInstance->SetViewModel(nullptr); InventoryWidgetInstance->RemoveFromParent(); InventoryWidgetInstance = nullptr;
		UHSRInventoryRewardViewModel* OldVM = InventoryViewModelInstance; InventoryViewModelInstance = nullptr; ShutdownInventoryViewModelCandidate(OldVM);
	}
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
	if (!InventoryWidgetClass
#if WITH_DEV_AUTOMATION_TESTS
		&& !(bUseAutomationBackend && bAutomationHasInventoryClass)
#endif
	) return EHSRUIScreenResult::MissingWidgetClass;

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
	const FHSRInputModePolicy OldPolicy = GetResolvedInputPolicy();
	const FHSRFrontendRouteSnapshot OldRoute = FrontendRouter->GetSnapshot();
	if (!AttachInventoryCandidate(Candidate))
	{
		ReleaseInventoryCandidates(Candidate, ViewModelCandidate);
		const bool bRestore = ApplyInventoryPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		if (!bRestore) { bInconsistent = true; return EHSRUIScreenResult::CompensationFailed; }
		return EHSRUIScreenResult::ViewportAttachFailed;
	}
	if (!ApplyInventoryPolicyBackend(PC, GetResolvedInputPolicy(), EHSRPlayerControlMode::UIOnly))
	{
		ReleaseInventoryCandidates(Candidate, ViewModelCandidate);
		const bool bRestore = ApplyInventoryPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		if (!bRestore) { bInconsistent = true; return EHSRUIScreenResult::CompensationFailed; }
		return EHSRUIScreenResult::PolicyApplyFailed;
	}
	const EHSRFocusApplyResult FocusResult = ApplyInventoryFocusBackend(PC, Candidate->GetPreferredFocusWidget(), Candidate);
	if (FocusResult == EHSRFocusApplyResult::Unavailable)
	{
		ReleaseInventoryCandidates(Candidate, ViewModelCandidate);
		const bool bPolicyRestored = ApplyInventoryPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		return ResolveCompensation(bPolicyRestored, EHSRUIScreenResult::FocusApplyFailed);
	}
	if (FrontendRouter && FrontendShellInstance)
	{
		FHSRFrontendRouteRequest RouteRequest; RouteRequest.RequestToken = AllocateFrontendRequestToken(); RouteRequest.Route.Module = EHSRFrontendModule::Inventory;
		if (SubmitFrontendRoute(RouteRequest) != EHSRFrontendRouteResult::Success)
		{
			ReleaseInventoryCandidates(Candidate, ViewModelCandidate);
			FrontendRouter->RestoreSnapshotForTransaction(OldRoute);
			const bool bPolicyRestored = ApplyInventoryPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
			const bool bFocusRestored = RestoreFrontendModuleFocus(PC, OldRoute.GetActiveRoute().Module);
			return ResolveCompensation(bPolicyRestored && bFocusRestored, EHSRUIScreenResult::StackRejected);
		}
		FrontendShellInstance->PresentRoute(FrontendRouter->GetSnapshot());
	}
	if (FrontendModuleRootInstance) { FrontendModuleRootInstance->RemoveFromParent(); FrontendModuleRootInstance = nullptr; }
	if (CharacterDetailWidgetInstance) { CharacterDetailWidgetInstance->RemoveFromParent(); CharacterDetailWidgetInstance = nullptr; }
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
	if (FrontendModuleRootInstance || (ActiveFrontendModule >= EHSRFrontendModule::Party
		&& ActiveFrontendModule <= EHSRFrontendModule::Save))
	{
		const FHSRFrontendRouteSnapshot OldRoute = FrontendRouter->GetSnapshot();
		const FHSRInputModePolicy OldPolicy = GetResolvedInputPolicy();
		if (!ApplyPolicyBackend(RegisteredPlayerController.Get(), GetResolvedInputPolicy(), EHSRPlayerControlMode::UIOnly)
			|| ApplyFocusBackend(RegisteredPlayerController.Get(), FrontendShellInstance->GetPreferredFocusWidget(), FrontendShellInstance) == EHSRFocusApplyResult::Unavailable)
		{
			const bool bPolicyRestored = ApplyPolicyBackend(RegisteredPlayerController.Get(), OldPolicy, EHSRPlayerControlMode::UIOnly);
			return ResolveCompensation(bPolicyRestored, EHSRUIScreenResult::FocusApplyFailed);
		}
		FHSRFrontendRouteRequest RouteRequest;
		RouteRequest.RequestToken = AllocateFrontendRequestToken();
		RouteRequest.Operation = EHSRFrontendRouteOperation::Back;
		if (!FrontendRouter || FrontendRouter->Submit(RouteRequest) != EHSRFrontendRouteResult::Success)
		{
			if (FrontendRouter) FrontendRouter->RestoreSnapshotForTransaction(OldRoute);
			const bool bPolicyRestored = ApplyPolicyBackend(RegisteredPlayerController.Get(), OldPolicy, EHSRPlayerControlMode::UIOnly);
			return ResolveCompensation(bPolicyRestored, EHSRUIScreenResult::StackRejected);
		}
		if (FrontendModuleRootInstance) FrontendModuleRootInstance->RemoveFromParent();
		FrontendModuleRootInstance = nullptr;
		FrontendShellInstance->PresentRoute(FrontendRouter->GetSnapshot());
		return EHSRUIScreenResult::Success;
	}
	if (ActiveFrontendModule == EHSRFrontendModule::Inventory)
	{
		if (!InventoryWidgetInstance || !InventoryViewModelInstance || CharacterDetailWidgetInstance)
		{
			bInconsistent = true;
			return EHSRUIScreenResult::Inconsistent;
		}
		return CloseInventoryScreen();
	}
	if (ActiveFrontendModule == EHSRFrontendModule::Character)
	{
		if (!CharacterDetailWidgetInstance || InventoryWidgetInstance)
		{
			bInconsistent = true;
			return EHSRUIScreenResult::Inconsistent;
		}
		return CloseCharacterDetailScreen();
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
	case EHSRFrontendModule::Save:
	{
		if (!FrontendModuleRootClass
#if WITH_DEV_AUTOMATION_TESTS
			&& !bUseAutomationBackend
#endif
		) return CompleteModuleAttempt(EHSRUIScreenResult::MissingWidgetClass);
		UHSRFrontendModuleRootWidget* Candidate = nullptr;
#if WITH_DEV_AUTOMATION_TESTS
		if (bUseAutomationBackend) Candidate = NewObject<UHSRFrontendModuleRootWidget>(this);
		else
#endif
		Candidate = CreateWidget<UHSRFrontendModuleRootWidget>(RegisteredPlayerController.Get(), FrontendModuleRootClass);
		if (!Candidate) return CompleteModuleAttempt(EHSRUIScreenResult::WidgetCreationFailed);
		Candidate->SetOwningUIManager(this);
		Candidate->PresentModule(Module);
		const FHSRFrontendRouteSnapshot OldRoute = FrontendRouter->GetSnapshot();
		const FHSRInputModePolicy OldPolicy = GetResolvedInputPolicy();
#if WITH_DEV_AUTOMATION_TESTS
		if (!bUseAutomationBackend)
#endif
		{
			Candidate->AddToViewport(110);
			if (!Candidate->IsInViewport())
			{
				Candidate->RemoveFromParent();
				return CompleteModuleAttempt(EHSRUIScreenResult::ViewportAttachFailed);
			}
		}
		if (!ApplyPolicyBackend(RegisteredPlayerController.Get(), GetResolvedInputPolicy(), EHSRPlayerControlMode::UIOnly))
		{
			Candidate->RemoveFromParent();
			const bool bPolicyRestored = ApplyPolicyBackend(RegisteredPlayerController.Get(), OldPolicy, EHSRPlayerControlMode::UIOnly);
			return CompleteModuleAttempt(ResolveCompensation(bPolicyRestored, EHSRUIScreenResult::PolicyApplyFailed));
		}
		if (ApplyFocusBackend(RegisteredPlayerController.Get(), Candidate->GetPreferredFocusWidget(), Candidate) == EHSRFocusApplyResult::Unavailable)
		{
			Candidate->RemoveFromParent();
			const bool bPolicyRestored = ApplyPolicyBackend(RegisteredPlayerController.Get(), OldPolicy, EHSRPlayerControlMode::UIOnly);
			return CompleteModuleAttempt(ResolveCompensation(bPolicyRestored, EHSRUIScreenResult::FocusApplyFailed));
		}
		FHSRFrontendRouteRequest RouteRequest;
		RouteRequest.RequestToken = AllocateFrontendRequestToken();
		RouteRequest.Route.Module = Module;
		const EHSRFrontendRouteResult RouteResult = SubmitFrontendRoute(RouteRequest);
		if (RouteResult != EHSRFrontendRouteResult::Success && RouteResult != EHSRFrontendRouteResult::NoOp)
		{
			Candidate->RemoveFromParent();
			FrontendRouter->RestoreSnapshotForTransaction(OldRoute);
			const bool bPolicyRestored = ApplyPolicyBackend(RegisteredPlayerController.Get(), OldPolicy, EHSRPlayerControlMode::UIOnly);
			const bool bFocusRestored = RestoreFrontendModuleFocus(
				RegisteredPlayerController.Get(), OldRoute.GetActiveRoute().Module);
			return CompleteModuleAttempt(ResolveCompensation(
				bPolicyRestored && bFocusRestored, EHSRUIScreenResult::StackRejected));
		}
		if (FrontendModuleRootInstance) FrontendModuleRootInstance->RemoveFromParent();
		if (CharacterDetailWidgetInstance) { CharacterDetailWidgetInstance->RemoveFromParent(); CharacterDetailWidgetInstance = nullptr; }
		if (InventoryWidgetInstance)
		{
			InventoryWidgetInstance->SetViewModel(nullptr); InventoryWidgetInstance->RemoveFromParent(); InventoryWidgetInstance = nullptr;
			UHSRInventoryRewardViewModel* OldVM = InventoryViewModelInstance; InventoryViewModelInstance = nullptr; ShutdownInventoryViewModelCandidate(OldVM);
		}
		FrontendModuleRootInstance = Candidate;
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
	if (FrontendModuleRootInstance) { FrontendModuleRootInstance->RemoveFromParent(); FrontendModuleRootInstance = nullptr; }
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
	bInconsistent = true;
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
	ActiveHostGeneration = 0;
}

FName UHSRUIManagerSubsystem::SelectRestorableScreenId() const
{
	if (!ScreenStack || bInconsistent || HasInventoryOwnershipMismatch()) return NAME_None;
	const FHSRScreenStackSnapshot Snapshot = ScreenStack->GetSnapshot();
	if (Snapshot.Entries.Num() != 2) return NAME_None;
	const FName TopId = Snapshot.Entries.Last().ScreenId;
	if (TopId == CharacterDetailScreenId && CharacterDetailWidgetInstance && !FrontendShellInstance
		&& !InventoryWidgetInstance && !InventoryViewModelInstance) return TopId;
	if (TopId == InventoryScreenId && InventoryWidgetInstance && InventoryViewModelInstance
		&& !FrontendShellInstance && !CharacterDetailWidgetInstance) return TopId;
	return NAME_None;
}

EHSRUIScreenResult UHSRUIManagerSubsystem::CaptureAndTeardownTravelHost()
{
	const int64 CapturedHost = ActiveHostGeneration;
	const FName Restorable = NAME_None; // Frontend routes are intentionally discarded across travel.
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
		Result = EHSRUIScreenResult::Inconsistent;
		UE_LOG(LogTemp, Warning, TEXT("HSRUI P17 TravelFreeze forced non-owned stack cleanup to root"));
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
	if (!ScreenId.IsNone()) Result = EHSRUIScreenResult::Inconsistent;
	else
	{
		AHSRPlayerController* PC = RegisteredPlayerController.Get();
		Result = ApplyPolicyBackend(PC, GetResolvedInputPolicy(), EHSRPlayerControlMode::Exploration)
			? EHSRUIScreenResult::Success : EHSRUIScreenResult::PolicyApplyFailed;
		ApplyFocusBackend(PC, RegisteredRootWidget.Get(), RegisteredRootWidget.Get());
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

UHSRCharacterDetailWidget* UHSRUIManagerSubsystem::CreateCharacterDetailCandidate(AHSRPlayerController* PlayerController)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return bAutomationDetailCreateSucceeds ? NewObject<UHSRCharacterDetailWidget>(this) : nullptr;
	}
#endif
	return CreateWidget<UHSRCharacterDetailWidget>(PlayerController, CharacterDetailWidgetClass);
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

bool UHSRUIManagerSubsystem::AttachCharacterDetailCandidate(UHSRCharacterDetailWidget* Candidate)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return bAutomationDetailAttachSucceeds;
	}
#endif
	Candidate->AddToViewport(50);
	return Candidate->IsInViewport();
}

bool UHSRUIManagerSubsystem::AttachInventoryCandidate(UHSRInventoryWidget* Candidate)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		if (bAutomationInventoryAttachSucceeds) Candidate->AttachForAutomation();
		return bAutomationInventoryAttachSucceeds;
	}
#endif
	Candidate->AddToViewport(50);
	return Candidate->IsInViewport();
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
		return EHSRUIScreenResult::Inconsistent;
	}
	if (AutomationHostIdentity == HostIdentity)
	{
		bAutomationExploration = bInExplorationMode;
		bAutomationHasPauseClass = bHasPauseClass;
		return EHSRUIScreenResult::NoOp;
	}
	if (AutomationHostIdentity != 0 || FrontendShellInstance || CharacterDetailWidgetInstance || InventoryWidgetInstance
		|| InventoryViewModelInstance)
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

void UHSRUIManagerSubsystem::ConfigureAutomationDetailBackend(const bool bHasClass, const bool bCreateSucceeds,
	const bool bAttachSucceeds, const bool bPolicySucceeds, const bool bFocusSucceeds)
{
	bUseAutomationBackend = true;
	bAutomationHasDetailClass = bHasClass;
	bAutomationDetailCreateSucceeds = bCreateSucceeds;
	bAutomationDetailAttachSucceeds = bAttachSucceeds;
	bAutomationDetailPolicySucceeds = bPolicySucceeds;
	bAutomationDetailFocusSucceeds = bFocusSucceeds;
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
	bUseAutomationBackend = false;
	bAutomationHostValid = false;
	AutomationHostIdentity = 0;
	ActiveHostGeneration = 0;
	bTravelRestorePending = false;
	bTravelArrivalObserved = false;
}
#endif
