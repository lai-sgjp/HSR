#pragma once

#include "CoreMinimal.h"
#include "HSRScreenStackTypes.generated.h"

UENUM(BlueprintType)
enum class EHSRUIScreenLayer : uint8
{
	HUD,
	Menu,
	Modal
};

UENUM(BlueprintType)
enum class EHSRUIInputIntent : uint8
{
	GameOnly,
	GameAndUI,
	UIOnly
};

UENUM(BlueprintType)
enum class EHSRScreenStackOperation : uint8
{
	Push,
	Pop,
	Replace,
	CloseToRoot
};

UENUM(BlueprintType)
enum class EHSRScreenStackResult : uint8
{
	Success,
	NoOp,
	AlreadyProcessed,
	InvalidRequest,
	InvalidScreenId,
	DuplicateScreen,
	EmptyStack,
	RootRequired,
	RootAlreadyExists,
	RootProtected,
	StaleRequest
};

UENUM(BlueprintType)
enum class EHSRUIScreenResult : uint8
{
	Success,
	NoOp,
	NotInitialized,
	InvalidHost,
	NotExploration,
	MissingWidgetClass,
	WidgetCreationFailed,
	ViewModelInitializationFailed,
	ViewportAttachFailed,
	PolicyApplyFailed,
	PauseApplyFailed,
	FocusApplyFailed,
	AlreadyOpen,
	NothingOpen,
	ExternalPause,
	StackRejected,
	CompensationFailed,
	Inconsistent
};

UENUM(BlueprintType)
enum class EHSRFocusApplyResult : uint8
{
	Preferred,
	ScreenFallback,
	Unavailable
};

USTRUCT(BlueprintType)
struct FHSRScreenRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int64 RequestToken = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EHSRScreenStackOperation Operation = EHSRScreenStackOperation::Push;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ScreenId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EHSRUIScreenLayer Layer = EHSRUIScreenLayer::Menu;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EHSRUIInputIntent InputIntent = EHSRUIInputIntent::UIOnly;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName FocusToken = NAME_None;
};

USTRUCT(BlueprintType)
struct FHSRScreenStackEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName ScreenId = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	EHSRUIScreenLayer Layer = EHSRUIScreenLayer::Menu;

	UPROPERTY(BlueprintReadOnly)
	EHSRUIInputIntent InputIntent = EHSRUIInputIntent::UIOnly;

	UPROPERTY(BlueprintReadOnly)
	FName FocusToken = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	int64 OpenedByRequestToken = 0;

	bool operator==(const FHSRScreenStackEntry& Other) const
	{
		return ScreenId == Other.ScreenId
			&& Layer == Other.Layer
			&& InputIntent == Other.InputIntent
			&& FocusToken == Other.FocusToken
			&& OpenedByRequestToken == Other.OpenedByRequestToken;
	}
};

USTRUCT(BlueprintType)
struct FHSRInputModePolicy
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	EHSRUIInputIntent InputIntent = EHSRUIInputIntent::GameOnly;

	UPROPERTY(BlueprintReadOnly)
	bool bShowMouseCursor = false;

	UPROPERTY(BlueprintReadOnly)
	FName PreferredFocusToken = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	FName OwningScreenId = NAME_None;

	bool operator==(const FHSRInputModePolicy& Other) const
	{
		return InputIntent == Other.InputIntent
			&& bShowMouseCursor == Other.bShowMouseCursor
			&& PreferredFocusToken == Other.PreferredFocusToken
			&& OwningScreenId == Other.OwningScreenId;
	}
};

USTRUCT(BlueprintType)
struct FHSRScreenStackSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TArray<FHSRScreenStackEntry> Entries;

	UPROPERTY(BlueprintReadOnly)
	int64 LastProcessedRequestToken = 0;

	bool operator==(const FHSRScreenStackSnapshot& Other) const
	{
		return Entries == Other.Entries && LastProcessedRequestToken == Other.LastProcessedRequestToken;
	}
};
