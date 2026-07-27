#include "HSRInputModeCoordinator.h"
#include "HSRScreenStack.h"
#include "../Player/HSRPlayerController.h"
#include "Components/Widget.h"

FHSRInputModePolicy UHSRInputModeCoordinator::ResolvePolicy(const UHSRScreenStack* ScreenStack) const
{
	FHSRInputModePolicy Policy;
	if (!ScreenStack)
	{
		return Policy;
	}

	FHSRScreenStackEntry ActiveEntry;
	if (!ScreenStack->GetActiveEntry(ActiveEntry))
	{
		return Policy;
	}

	Policy.InputIntent = ActiveEntry.InputIntent;
	Policy.bShowMouseCursor = ActiveEntry.InputIntent != EHSRUIInputIntent::GameOnly;
	Policy.PreferredFocusToken = ActiveEntry.FocusToken;
	Policy.OwningScreenId = ActiveEntry.ScreenId;
	return Policy;
}

bool UHSRInputModeCoordinator::ApplyPolicy(AHSRPlayerController* PlayerController, const FHSRInputModePolicy& Policy,
	const EHSRPlayerControlMode SemanticMode) const
{
	return PlayerController && PlayerController->ApplyUIInputPolicy(Policy, SemanticMode);
}

EHSRFocusApplyResult UHSRInputModeCoordinator::ApplyFocus(AHSRPlayerController* PlayerController,
	UWidget* PreferredWidget, UWidget* ScreenFallback) const
{
	const auto IsEligible = [](const UWidget* Widget)
	{
		return Widget && Widget->GetIsEnabled() && Widget->IsVisible();
	};
	const EHSRFocusApplyResult Choice = ChooseFocusTarget(
		PlayerController && PlayerController->IsLocalPlayerController(), IsEligible(PreferredWidget), IsEligible(ScreenFallback));
	if (Choice == EHSRFocusApplyResult::Preferred)
	{
		PreferredWidget->SetUserFocus(PlayerController);
	}
	else if (Choice == EHSRFocusApplyResult::ScreenFallback)
	{
		ScreenFallback->SetUserFocus(PlayerController);
	}
	return Choice;
}

EHSRFocusApplyResult UHSRInputModeCoordinator::ChooseFocusTarget(const bool bOwnerValid,
	const bool bPreferredEligible, const bool bFallbackEligible)
{
	if (!bOwnerValid)
	{
		return EHSRFocusApplyResult::Unavailable;
	}
	if (bPreferredEligible)
	{
		return EHSRFocusApplyResult::Preferred;
	}
	return bFallbackEligible ? EHSRFocusApplyResult::ScreenFallback : EHSRFocusApplyResult::Unavailable;
}
