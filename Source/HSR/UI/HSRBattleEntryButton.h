#pragma once
#include "CoreMinimal.h"
#include "Components/Button.h"
#include "HSRBattleCommandTypes.h"
#include "HSRBattleEntryButton.generated.h"
class UHSRBattleCommandWidget;
class UTextBlock;
class UImage;
class UProgressBar;

/** Reusable ID-based target/skill card. Persistent across state refreshes to preserve focus. */
UCLASS()
class HSR_API UHSRBattleEntryButton : public UButton
{
    GENERATED_BODY()
public:
    void InitializeCard(UHSRBattleCommandWidget* InOwner, FName InId, bool bInSkill);
    void ShowParticipant(const FHSRBattleParticipantView& View, bool bSelected, bool bAvailable);
    void ShowSkill(const FHSRBattleCommandSkillView& View, bool bSelected, bool bUnlocked);
    void ShowForecast(const FHSRBattleParticipantView& View, int32 Position);
private:
    UFUNCTION() void Select();
    void SetSelected(bool bSelected);
    FName Id;
    bool bSkill = false;
    bool bCanSelect = false;
    TWeakObjectPtr<UHSRBattleCommandWidget> Owner;
    UPROPERTY() TObjectPtr<UTextBlock> Label;
    UPROPERTY() TObjectPtr<UTextBlock> Detail;
    UPROPERTY() TObjectPtr<UImage> Portrait;
    UPROPERTY() TObjectPtr<UProgressBar> Health;
    UPROPERTY() TObjectPtr<UProgressBar> Energy;
};
