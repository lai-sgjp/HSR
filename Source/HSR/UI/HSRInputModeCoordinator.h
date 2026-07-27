#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HSRScreenStackTypes.h"
#include "HSRInputModeCoordinator.generated.h"

class UHSRScreenStack;
class AHSRPlayerController;
class UWidget;
enum class EHSRPlayerControlMode : uint8;

UCLASS()
class HSR_API UHSRInputModeCoordinator : public UObject
{
	GENERATED_BODY()

public:
	FHSRInputModePolicy ResolvePolicy(const UHSRScreenStack* ScreenStack) const;
	bool ApplyPolicy(AHSRPlayerController* PlayerController, const FHSRInputModePolicy& Policy,
		EHSRPlayerControlMode SemanticMode) const;
	EHSRFocusApplyResult ApplyFocus(AHSRPlayerController* PlayerController, UWidget* PreferredWidget,
		UWidget* ScreenFallback) const;
	static EHSRFocusApplyResult ChooseFocusTarget(bool bOwnerValid, bool bPreferredEligible,
		bool bFallbackEligible);
};
