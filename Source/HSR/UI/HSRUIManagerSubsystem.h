#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "HSRScreenStackTypes.h"
#include "../Map/HSRMapTypes.h"
#include "HSRUIManagerSubsystem.generated.h"

class UHSRInputModeCoordinator;
class UHSRScreenStack;
class UHSRScreenWidget;
class UHSRCharacterDetailWidget;
class UHSRInventoryWidget;
class UHSRInventoryRewardViewModel;
class UHSRUserWidget;
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
		UHSRUserWidget* RootWidget, TSubclassOf<UHSRScreenWidget> InPauseWidgetClass,
		TSubclassOf<UHSRCharacterDetailWidget> InCharacterDetailWidgetClass,
		TSubclassOf<UHSRInventoryWidget> InInventoryWidgetClass);
	EHSRUIScreenResult UnregisterExplorationHost(AHSRHUD* HUD, AHSRPlayerController* PlayerController);
	EHSRUIScreenResult TeardownExplorationHostForTravel(AHSRHUD* HUD, AHSRPlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "HSR|UI")
	EHSRUIScreenResult OpenPauseScreen();

	UFUNCTION(BlueprintCallable, Category = "HSR|UI")
	EHSRUIScreenResult OpenCharacterDetailScreen();

	UFUNCTION(BlueprintCallable, Category = "HSR|UI")
	EHSRUIScreenResult OpenInventoryScreen();

	UFUNCTION(BlueprintCallable, Category = "HSR|UI")
	EHSRUIScreenResult RequestBack();

	UFUNCTION(BlueprintPure, Category = "HSR|UI")
	bool HasOpenPauseScreen() const { return PauseWidgetInstance != nullptr; }

	UFUNCTION(BlueprintPure, Category = "HSR|UI")
	bool HasOpenCharacterDetailScreen() const { return CharacterDetailWidgetInstance != nullptr; }

	UFUNCTION(BlueprintPure, Category = "HSR|UI")
	bool HasOpenInventoryScreen() const { return InventoryWidgetInstance != nullptr; }
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
	EHSRUIScreenResult TeardownHostIdentityForTravelForAutomation(int32 HostIdentity);
	void NotifyArrivalCommittedForAutomation(int64 CommitGeneration, FName MapId = TEXT("Map.Automation"));
	bool HasPendingTravelRestoreForAutomation() const { return bTravelRestorePending; }
	int64 GetHostGenerationForAutomation() const { return ActiveHostGeneration; }
	void FailNextAutomationInventoryPolicyApply() { bAutomationFailNextInventoryPolicyApply = true; }
#endif

private:
	int64 AllocateRequestToken();
	FHSRScreenRequest MakeRootRequest(int64 Token) const;
	FHSRScreenRequest MakePauseRequest(int64 Token) const;
	FHSRScreenRequest MakeCharacterDetailRequest(int64 Token) const;
	FHSRScreenRequest MakeInventoryRequest(int64 Token) const;
	FHSRScreenRequest MakePopRequest(int64 Token) const;
	bool CompensatePop(const FHSRInputModePolicy& RestorePolicy, AHSRPlayerController* PlayerController,
		UHSRScreenWidget* CandidateWidget);
	bool CompensatePausePush(AHSRPlayerController* PlayerController);
	bool CompensateCharacterDetailPush(AHSRPlayerController* PlayerController);
	EHSRUIScreenResult CloseCharacterDetailScreen();
	EHSRUIScreenResult CloseInventoryScreen();
	bool CompensateInventoryPush(AHSRPlayerController* PlayerController);
	void ReleaseInventoryCandidates(UHSRInventoryWidget*& Widget, UHSRInventoryRewardViewModel*& ViewModel);
	void ShutdownInventoryViewModelCandidate(UHSRInventoryRewardViewModel*& ViewModel);
	bool HasInventoryOwnershipMismatch() const;
	void HandleArrivalCommitted(const FHSRMapArrivalCommitInfo& Info);
	void TryRestoreTravelDescriptor();
	FName SelectRestorableScreenId() const;
	EHSRUIScreenResult CaptureAndTeardownTravelHost();
	EHSRUIScreenResult TeardownCurrentHost();
	void ClearHostReferences();
	bool IsBackendHostValid(AHSRPlayerController* PlayerController, UHSRUserWidget* RootWidget, UWorld* World) const;
	bool IsBackendExploration(AHSRPlayerController* PlayerController) const;
	bool IsBackendPaused(UWorld* World) const;
	UHSRScreenWidget* CreatePauseCandidate(AHSRPlayerController* PlayerController);
	UHSRCharacterDetailWidget* CreateCharacterDetailCandidate(AHSRPlayerController* PlayerController);
	UHSRInventoryWidget* CreateInventoryCandidate(AHSRPlayerController* PlayerController);
	UHSRInventoryRewardViewModel* CreateInventoryViewModelCandidate();
	bool AttachPauseCandidate(UHSRScreenWidget* Candidate);
	bool AttachCharacterDetailCandidate(UHSRCharacterDetailWidget* Candidate);
	bool AttachInventoryCandidate(UHSRInventoryWidget* Candidate);
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
	UPROPERTY(Transient)
	TObjectPtr<UHSRScreenStack> ScreenStack;

	UPROPERTY(Transient)
	TObjectPtr<UHSRInputModeCoordinator> InputModeCoordinator;

	UPROPERTY(Transient)
	TObjectPtr<UHSRScreenWidget> PauseWidgetInstance;

	UPROPERTY(Transient)
	TSubclassOf<UHSRScreenWidget> PauseWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UHSRCharacterDetailWidget> CharacterDetailWidgetInstance;

	UPROPERTY(Transient)
	TSubclassOf<UHSRCharacterDetailWidget> CharacterDetailWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UHSRInventoryWidget> InventoryWidgetInstance;

	UPROPERTY(Transient)
	TObjectPtr<UHSRInventoryRewardViewModel> InventoryViewModelInstance;

	UPROPERTY(Transient)
	TSubclassOf<UHSRInventoryWidget> InventoryWidgetClass;

	UPROPERTY(Transient)
	TWeakObjectPtr<AHSRHUD> RegisteredHUD;

	UPROPERTY(Transient)
	TWeakObjectPtr<AHSRPlayerController> RegisteredPlayerController;

	UPROPERTY(Transient)
	TWeakObjectPtr<UHSRUserWidget> RegisteredRootWidget;

	FGuid PauseOwnerToken;
	int64 NextRequestToken = 1;
	bool bInitialized = false;
	bool bInconsistent = false;
	int64 NextHostGeneration = 1;
	int64 ActiveHostGeneration = 0;
	int64 NextTravelRestoreGeneration = 1;
	int64 TravelRestoreGeneration = 0;
	int64 TravelCapturedHostGeneration = 0;
	int64 MinimumArrivalCommitGeneration = 0;
	int64 LatchedArrivalCommitGeneration = 0;
	int64 LastObservedArrivalCommitGeneration = 0;
	FName TravelRestoreScreenId = NAME_None;
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
	bool bAutomationPaused = false;
	bool bAutomationHasDetailClass = true;
	bool bAutomationDetailCreateSucceeds = true;
	bool bAutomationDetailAttachSucceeds = true;
	bool bAutomationDetailPolicySucceeds = true;
	bool bAutomationDetailFocusSucceeds = true;
	bool bAutomationFailNextDetailPolicyApply = false;
	bool bAutomationHasInventoryClass = true;
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
