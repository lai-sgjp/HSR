#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "HSRScreenStackTypes.h"
#include "Frontend/HSRFrontendRouteTypes.h"
#include "../Map/HSRMapTypes.h"
#include "HSRUIManagerSubsystem.generated.h"

class UHSRInputModeCoordinator;
class UHSRScreenStack;
class UHSRScreenWidget;
class UHSRInventoryWidget;
class UHSRInventoryModuleWidget;
class UHSRInventoryRewardViewModel;
class UHSRUserWidget;
class UHSRFrontendRouter;
class UHSRFrontendShellWidget;
class UHSRFrontendModuleRootWidget;
class UUserWidget;
class AHSRHUD;
class AHSRPlayerController;
class UWidget;
class UWorld;
enum class EHSRPlayerControlMode : uint8;

UCLASS()
class HSR_API UHSRUIManagerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	EHSRScreenStackResult SubmitScreenRequest(const FHSRScreenRequest& Request);
	FHSRInputModePolicy GetResolvedInputPolicy() const;
	const UHSRScreenStack* GetScreenStack() const { return ScreenStack; }

	EHSRUIScreenResult RegisterExplorationHost(AHSRHUD* HUD, AHSRPlayerController* PlayerController,
		UHSRUserWidget* RootWidget, TSubclassOf<UHSRFrontendShellWidget> InFrontendShellClass,
		TSubclassOf<UHSRFrontendModuleRootWidget> InFrontendModuleRootClass,
		TSubclassOf<UHSRScreenWidget> InCharacterDetailWidgetClass,
		TSubclassOf<UHSRInventoryWidget> InInventoryWidgetClass,
		TSubclassOf<UHSRInventoryModuleWidget> InInventoryModuleWidgetClass,
		TSubclassOf<UUserWidget> InPartyWidgetClass,
		TSubclassOf<UUserWidget> InMapWidgetClass,
		TSubclassOf<UUserWidget> InChallengeWidgetClass,
		TSubclassOf<UUserWidget> InQuestWidgetClass,
		TSubclassOf<UUserWidget> InSaveWidgetClass);
	EHSRUIScreenResult UnregisterExplorationHost(AHSRHUD* HUD, AHSRPlayerController* PlayerController);
	EHSRUIScreenResult TeardownExplorationHostForTravel(AHSRHUD* HUD, AHSRPlayerController* PlayerController);
	EHSRUIScreenResult PrepareExplorationTravel();

	UFUNCTION(BlueprintCallable, Category = "HSR|UI")
	EHSRUIScreenResult OpenPauseScreen();

	UFUNCTION(BlueprintCallable, Category = "HSR|UI")
	EHSRUIScreenResult OpenCharacterDetailScreen();

	UFUNCTION(BlueprintCallable, Category = "HSR|UI")
	EHSRUIScreenResult OpenInventoryScreen();

	UFUNCTION(BlueprintCallable, Category = "HSR|UI")
	EHSRUIScreenResult RequestBack();

	UFUNCTION(BlueprintCallable, Category = "HSR|UI|Frontend")
	EHSRUIScreenResult OpenFrontendModule(EHSRFrontendModule Module);

	UFUNCTION(BlueprintCallable, Category = "HSR|UI|Frontend")
	EHSRUIScreenResult CloseFrontendToRoot();

	UFUNCTION(BlueprintPure, Category = "HSR|UI")
	bool HasOpenPauseScreen() const { return FrontendShellInstance != nullptr; }
	const UHSRFrontendRouter* GetFrontendRouter() const { return FrontendRouter; }

	UFUNCTION(BlueprintPure, Category = "HSR|UI")
	bool HasOpenCharacterDetailScreen() const { return CharacterDetailWidgetInstance != nullptr; }

	UFUNCTION(BlueprintPure, Category = "HSR|UI")
	bool HasOpenInventoryScreen() const
	{
		return InventoryWidgetInstance != nullptr
			|| (FrontendModuleContentModule == EHSRFrontendModule::Inventory
				&& FrontendModuleContentInstance != nullptr);
	}
	bool HasInventoryViewModel() const { return InventoryViewModelInstance != nullptr; }

	UFUNCTION(BlueprintPure, Category = "HSR|UI")
	int32 GetLogicalScreenCount() const;
	bool IsInconsistent() const { return bInconsistent; }

#if WITH_DEV_AUTOMATION_TESTS
	void InitializeForAutomation();
	void DeinitializeForAutomation();
	void RegisterHostForAutomation(bool bInExplorationMode = true, bool bHasPauseClass = true);
	EHSRUIScreenResult RegisterHostIdentityForAutomation(int32 HostIdentity,
		bool bInExplorationMode = true, bool bHasPauseClass = true);
	EHSRUIScreenResult UnregisterHostIdentityForAutomation(int32 HostIdentity);
	void ConfigureAutomationBackend(bool bCreateSucceeds, bool bAttachSucceeds, bool bPolicySucceeds,
		bool bPauseSucceeds, bool bFocusSucceeds, bool bInitiallyPaused = false);
	void FailNextAutomationPolicyApply() { bAutomationFailNextPolicyApply = true; }
	void FailNextAutomationPauseApply() { bAutomationFailNextPauseApply = true; }
	void FailNextAutomationFocusApply() { bAutomationFailNextFocusApply = true; }
	void FailNextAutomationRouteSubmit() { bAutomationFailNextRouteSubmit = true; }
	void FailAutomationOldModuleFocusRestore() { bAutomationFailOldModuleFocusRestore = true; }
	void FailAutomationPauseRestore() { bAutomationFailPauseRestore = true; }
	void FailSecondAutomationPolicyApply() { AutomationPolicyCallsUntilFailure = 2; }
	void FailSecondAutomationPauseApply() { AutomationPauseCallsUntilFailure = 2; }
	void ConfigureAutomationDetailBackend(bool bHasClass, bool bCreateSucceeds, bool bAttachSucceeds,
		bool bPolicySucceeds, bool bFocusSucceeds);
	void FailNextAutomationDetailPolicyApply() { bAutomationFailNextDetailPolicyApply = true; }
	void ConfigureAutomationInventoryBackend(bool bHasClass, bool bCreateSucceeds, bool bViewModelSucceeds,
		bool bAttachSucceeds, bool bPolicySucceeds, bool bFocusSucceeds);
	void ConfigureAutomationInventoryViewModelStages(bool bDependenciesSucceed, bool bCreateSucceeds,
		bool bSnapshotSucceeds);
	void InjectInventoryHalfPairForAutomation(bool bWidgetOnly);
	int32 GetInventoryBindCountForAutomation() const;
	int32 GetLastReleasedInventoryBindCountForAutomation() const { return LastReleasedInventoryBindCount; }
	int32 GetLastReleasedInventoryUnbindCountForAutomation() const { return LastReleasedInventoryUnbindCount; }
	int32 GetInventoryCandidateShutdownCountForAutomation() const { return InventoryCandidateShutdownCount; }
	bool IsPausedForAutomation() const { return bAutomationPaused; }
	EHSRFrontendModule GetLastAutomationFocusModule() const { return LastAutomationFocusModule; }
	EHSRUIScreenResult TeardownHostIdentityForTravelForAutomation(int32 HostIdentity);
	void NotifyArrivalCommittedForAutomation(int64 CommitGeneration, FName MapId = TEXT("Map.Automation"));
	bool HasPendingTravelRestoreForAutomation() const { return bTravelRestorePending; }
	bool IsInconsistencyTravelRecoverableForAutomation() const { return bInconsistencyIsTravelRecoverable; }
	bool HasFrontendModuleRootForAutomation() const { return FrontendModuleRootInstance != nullptr; }
	int64 GetHostGenerationForAutomation() const { return ActiveHostGeneration; }
	void FailNextAutomationInventoryPolicyApply() { bAutomationFailNextInventoryPolicyApply = true; }
	void ConfigureAutomationFrontendModuleBackend(bool bHasClass, bool bCreateSucceeds, bool bAttachSucceeds);
	void ConfigureAutomationInventoryModuleBackend(bool bHasClass, bool bCreateSucceeds,
		bool bAttachSucceeds);
	int32 GetFrontendModuleContentCountForAutomation() const;
	EHSRFrontendModule GetFrontendModuleContentModuleForAutomation() const;
#endif

private:
	int64 AllocateRequestToken();
	int64 AllocateFrontendRequestToken();
	EHSRFrontendRouteResult SubmitFrontendRoute(const FHSRFrontendRouteRequest& Request);
	EHSRUIScreenResult ResolveCompensation(bool bRecovered, EHSRUIScreenResult OriginalFailure);
	FHSRScreenRequest MakeRootRequest(int64 Token) const;
	FHSRScreenRequest MakePauseRequest(int64 Token) const;
	FHSRScreenRequest MakePopRequest(int64 Token) const;
	bool CompensatePop(const FHSRInputModePolicy& RestorePolicy, AHSRPlayerController* PlayerController,
		UHSRScreenWidget* CandidateWidget);
	bool CompensatePausePush(AHSRPlayerController* PlayerController);
	EHSRUIScreenResult OpenCharacterDetailInternal();
	EHSRUIScreenResult OpenInventoryInternal();
	EHSRUIScreenResult CloseCharacterDetailScreen();
	EHSRUIScreenResult CloseInventoryScreen();
	void ReleaseInventoryCandidates(UHSRInventoryWidget*& Widget, UHSRInventoryRewardViewModel*& ViewModel);
	void ShutdownInventoryViewModelCandidate(UHSRInventoryRewardViewModel*& ViewModel);
	bool HasInventoryOwnershipMismatch() const;
	void HandleArrivalCommitted(const FHSRMapArrivalCommitInfo& Info);
	void TryRestoreTravelDescriptor();
	FName SelectRestorableScreenId() const;
	EHSRUIScreenResult CaptureAndTeardownTravelHost();
	EHSRUIScreenResult TeardownCurrentHost();
	/** True when the stack is exactly the exploration root and no module instance is owned. */
	bool IsAtCleanExplorationRoot() const;
	/** Clears a travel-scoped inconsistency once a fresh host proves the UI is coherent again. */
	void TryClearRecoverableInconsistency();
	void ClearHostReferences();
	FGuid ResolveInventoryCharacterGuid() const;
	TSubclassOf<UUserWidget> GetFrontendModuleWidgetClass(EHSRFrontendModule Module) const;
	UUserWidget* CreateFrontendModuleContentCandidate(AHSRPlayerController* PlayerController,
		EHSRFrontendModule Module);
	bool AttachFrontendModuleContentCandidate(UHSRFrontendModuleRootWidget* RootCandidate, UUserWidget* ContentCandidate);
	void ReleaseFrontendModuleContent();
	bool IsBackendHostValid(AHSRPlayerController* PlayerController, UHSRUserWidget* RootWidget, UWorld* World) const;
	bool IsBackendExploration(AHSRPlayerController* PlayerController) const;
	bool IsBackendPaused(UWorld* World) const;
	bool IsTravelPending() const;
	UHSRFrontendShellWidget* CreatePauseCandidate(AHSRPlayerController* PlayerController);
	UHSRFrontendModuleRootWidget* CreateFrontendModuleRootCandidate(AHSRPlayerController* PlayerController);
	UHSRScreenWidget* CreateCharacterDetailCandidate(AHSRPlayerController* PlayerController);
	UHSRInventoryWidget* CreateInventoryCandidate(AHSRPlayerController* PlayerController);
	UHSRInventoryRewardViewModel* CreateInventoryViewModelCandidate();
	bool AttachPauseCandidate(UHSRFrontendShellWidget* Candidate);
	bool AttachFrontendModuleRootCandidate(UHSRFrontendModuleRootWidget* Candidate);
	bool ApplyPolicyBackend(AHSRPlayerController* PlayerController, const FHSRInputModePolicy& Policy,
		EHSRPlayerControlMode SemanticMode);
	bool ApplyPauseBackend(UWorld* World, bool bPaused);
	bool ApplyCharacterDetailPolicyBackend(AHSRPlayerController* PlayerController,
		const FHSRInputModePolicy& Policy, EHSRPlayerControlMode SemanticMode);
	EHSRFocusApplyResult ApplyFocusBackend(AHSRPlayerController* PlayerController, UWidget* Preferred, UWidget* Fallback);
	EHSRFocusApplyResult ApplyCharacterDetailFocusBackend(AHSRPlayerController* PlayerController,
		UWidget* Preferred, UWidget* Fallback);
	bool ApplyInventoryPolicyBackend(AHSRPlayerController* PlayerController, const FHSRInputModePolicy& Policy,
		EHSRPlayerControlMode SemanticMode);
	EHSRFocusApplyResult ApplyInventoryFocusBackend(AHSRPlayerController* PlayerController,
		UWidget* Preferred, UWidget* Fallback);
	bool RestoreFrontendModuleFocus(AHSRPlayerController* PlayerController, EHSRFrontendModule Module);
	UPROPERTY(Transient)
	TObjectPtr<UHSRScreenStack> ScreenStack;

	UPROPERTY(Transient)
	TObjectPtr<UHSRInputModeCoordinator> InputModeCoordinator;

	UPROPERTY(Transient)
	TObjectPtr<UHSRFrontendRouter> FrontendRouter;

	UPROPERTY(Transient)
	TObjectPtr<UHSRFrontendShellWidget> FrontendShellInstance;

	UPROPERTY(Transient)
	TSubclassOf<UHSRFrontendShellWidget> FrontendShellClass;

	UPROPERTY(Transient)
	TObjectPtr<UHSRFrontendModuleRootWidget> FrontendModuleRootInstance;

	UPROPERTY(Transient)
	TSubclassOf<UHSRFrontendModuleRootWidget> FrontendModuleRootClass;

	UPROPERTY(Transient)
	TObjectPtr<UHSRScreenWidget> CharacterDetailWidgetInstance;

	UPROPERTY(Transient)
	TSubclassOf<UHSRScreenWidget> CharacterDetailWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UHSRInventoryWidget> InventoryWidgetInstance;

	UPROPERTY(Transient)
	TObjectPtr<UHSRInventoryRewardViewModel> InventoryViewModelInstance;

	UPROPERTY(Transient)
	TSubclassOf<UHSRInventoryWidget> InventoryWidgetClass;

	UPROPERTY(Transient)
	TSubclassOf<UHSRInventoryModuleWidget> InventoryModuleWidgetClass;

	UPROPERTY(Transient)
	TSubclassOf<UUserWidget> PartyWidgetClass;

	UPROPERTY(Transient)
	TSubclassOf<UUserWidget> MapWidgetClass;

	UPROPERTY(Transient)
	TSubclassOf<UUserWidget> ChallengeWidgetClass;

	UPROPERTY(Transient)
	TSubclassOf<UUserWidget> QuestWidgetClass;

	UPROPERTY(Transient)
	TSubclassOf<UUserWidget> SaveWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> FrontendModuleContentInstance;

	EHSRFrontendModule FrontendModuleContentModule = EHSRFrontendModule::None;

	UPROPERTY(Transient)
	TWeakObjectPtr<AHSRHUD> RegisteredHUD;

	UPROPERTY(Transient)
	TWeakObjectPtr<AHSRPlayerController> RegisteredPlayerController;

	UPROPERTY(Transient)
	TWeakObjectPtr<UHSRUserWidget> RegisteredRootWidget;

	FGuid PauseOwnerToken;
	int64 NextRequestToken = 1;
	int64 NextFrontendRequestToken = 1;
	bool bInitialized = false;
	bool bInconsistent = false;
	/**
	 * Set when bInconsistent was raised solely by travel-scoped forced cleanup, which is
	 * recoverable: a fresh host plus a clean exploration root proves the UI is coherent again.
	 * Never set for genuine corruption (compensation failure, ownership mismatch, broken invariants).
	 */
	bool bInconsistencyIsTravelRecoverable = false;
	int64 NextHostGeneration = 1;
	int64 ActiveHostGeneration = 0;
	int64 NextTravelRestoreGeneration = 1;
	int64 TravelRestoreGeneration = 0;
	int64 TravelCapturedHostGeneration = 0;
	int64 MinimumArrivalCommitGeneration = 0;
	int64 LatchedArrivalCommitGeneration = 0;
	int64 LastObservedArrivalCommitGeneration = 0;
	FName TravelRestoreScreenId = NAME_None;
	bool bTravelRestoredModule = false;
	bool bTravelRestorePending = false;
	bool bTravelArrivalObserved = false;
	FDelegateHandle ArrivalCommittedHandle;

#if WITH_DEV_AUTOMATION_TESTS
	bool bUseAutomationBackend = false;
	bool bAutomationHostValid = false;
	bool bAutomationExploration = true;
	bool bAutomationHasPauseClass = true;
	bool bAutomationCreateSucceeds = true;
	bool bAutomationAttachSucceeds = true;
	bool bAutomationPolicySucceeds = true;
	bool bAutomationPauseSucceeds = true;
	bool bAutomationFocusSucceeds = true;
	bool bAutomationFailNextPolicyApply = false;
	bool bAutomationFailNextPauseApply = false;
	bool bAutomationFailNextFocusApply = false;
	bool bAutomationFailNextRouteSubmit = false;
	bool bAutomationFailOldModuleFocusRestore = false;
	bool bAutomationFailPauseRestore = false;
	EHSRFrontendModule LastAutomationFocusModule = EHSRFrontendModule::None;
	int32 AutomationPolicyCallsUntilFailure = 0;
	int32 AutomationPauseCallsUntilFailure = 0;
	bool bAutomationPaused = false;
	bool bAutomationHasDetailClass = true;
	bool bAutomationDetailCreateSucceeds = true;
	bool bAutomationDetailAttachSucceeds = true;
	bool bAutomationDetailPolicySucceeds = true;
	bool bAutomationDetailFocusSucceeds = true;
	bool bAutomationFailNextDetailPolicyApply = false;
	bool bAutomationHasInventoryClass = true;
	bool bAutomationHasFrontendModuleClass = true;
	bool bAutomationFrontendModuleCreateSucceeds = true;
	bool bAutomationFrontendModuleAttachSucceeds = true;
	bool bAutomationUseInventoryModuleContent = false;
	bool bAutomationInventoryModuleCreateSucceeds = true;
	bool bAutomationInventoryModuleAttachSucceeds = true;
	bool bAutomationInventoryCreateSucceeds = true;
	bool bAutomationInventoryViewModelSucceeds = true;
	bool bAutomationInventoryDependenciesSucceed = true;
	bool bAutomationInventorySnapshotSucceeds = true;
	bool bAutomationInventoryAttachSucceeds = true;
	bool bAutomationInventoryPolicySucceeds = true;
	bool bAutomationInventoryFocusSucceeds = true;
	bool bAutomationFailNextInventoryPolicyApply = false;
	int32 LastReleasedInventoryBindCount = 0;
	int32 LastReleasedInventoryUnbindCount = 0;
	int32 InventoryCandidateShutdownCount = 0;
	int32 AutomationHostIdentity = 0;
#endif
};
