#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HSRGameModeBase.generated.h"

class AController;
class UHSRCharacterCatalog;
class UHSRMapCatalog;

UENUM(BlueprintType)
enum class EHSRCharacterBootstrapMode : uint8
{
	NewGameDefaults,
	UseCommittedRuntime
};

UENUM(BlueprintType)
enum class EHSRCharacterBootstrapResult : uint8
{
	Success,
	NoOp,
	MissingCatalog,
	InvalidInitialCharacter,
	CatalogConflict,
	ProfileRegistrationFailed,
	PartyUnavailable,
	NoCommittedSelection,
	PawnProjectionFailed
};

UENUM(BlueprintType)
enum class EHSRMapBootstrapResult : uint8
{
	Success,
	NoOp,
	MissingCatalog,
	MapRegistrationFailed,
	TeleportRegistrationFailed,
	InitialLocationFailed
};

UCLASS()
class HSR_API AHSRGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHSRGameModeBase();
	virtual void RestartPlayer(AController* NewPlayer) override;
	EHSRCharacterBootstrapResult BootstrapCharacterIdentity(EHSRCharacterBootstrapMode Mode);
	EHSRCharacterBootstrapResult GetLastCharacterBootstrapResult() const { return LastCharacterBootstrapResult; }
	FName GetResolvedCharacterId() const { return ResolvedCharacterId; }
	EHSRMapBootstrapResult BootstrapMapDefinitions();
	EHSRMapBootstrapResult GetLastMapBootstrapResult() const { return LastMapBootstrapResult; }

#if WITH_DEV_AUTOMATION_TESTS
	void ConfigureCharacterBootstrapForAutomation(UHSRCharacterCatalog* InCatalog, FName InInitialCharacterId,
		AController* InController);
	void ConfigureMapBootstrapForAutomation(UHSRMapCatalog* InCatalog, FName InInitialMapId);
#endif

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Bootstrap")
	TObjectPtr<UHSRCharacterCatalog> CharacterCatalog;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Bootstrap")
	FName InitialCharacterId = TEXT("Character.A");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Bootstrap")
	EHSRCharacterBootstrapMode CharacterBootstrapMode = EHSRCharacterBootstrapMode::NewGameDefaults;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map|Bootstrap")
	TObjectPtr<UHSRMapCatalog> MapCatalog;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map|Bootstrap")
	FName InitialMapId = NAME_None;

private:
	EHSRCharacterBootstrapResult FinishBootstrap(EHSRCharacterBootstrapResult Result, FName CharacterId = NAME_None);
	AController* ResolveBootstrapController() const;

	UPROPERTY(Transient)
	EHSRCharacterBootstrapResult LastCharacterBootstrapResult = EHSRCharacterBootstrapResult::MissingCatalog;

	UPROPERTY(Transient)
	FName ResolvedCharacterId;

	UPROPERTY(Transient)
	EHSRMapBootstrapResult LastMapBootstrapResult = EHSRMapBootstrapResult::MissingCatalog;

#if WITH_DEV_AUTOMATION_TESTS
	TWeakObjectPtr<AController> AutomationController;
#endif
};
