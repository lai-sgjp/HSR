#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HSRBattleStage.generated.h"

class UCameraComponent;

/** World-local presentation only. Never decides damage, targets or turn completion. */
UCLASS()
class HSR_API AHSRBattleStage : public AActor
{
    GENERATED_BODY()
public:
    AHSRBattleStage();
    UPROPERTY(EditAnywhere, Category="Stage") TArray<FTransform> PlayerSlots;
    UPROPERTY(EditAnywhere, Category="Stage") TArray<FTransform> EnemySlots;
    UPROPERTY(EditAnywhere, Category="Stage") FTransform BossSlot;
    UPROPERTY(EditAnywhere, Category="Stage") TArray<FName> BossDefinitionIds;
    UPROPERTY(EditAnywhere, Category="Stage|Camera") FTransform EnemyView;
    UPROPERTY(EditAnywhere, Category="Stage|Camera") FTransform AllyView;
    UPROPERTY(EditAnywhere, Category="Stage|Camera") FTransform ResultView;
    UPROPERTY(EditAnywhere, Category="Stage|Camera") FTransform ActionView;
    UPROPERTY(VisibleAnywhere, Category="Stage|Camera") TObjectPtr<UCameraComponent> Camera;
    bool ResolveSlot(bool bPlayer, int32 Index, bool bBoss, FTransform& Out) const;
    UFUNCTION(BlueprintCallable, Category="Stage") void ShowSide(bool bAllies, bool bResult=false);
    /** A short optional camera accent. Its timer has no connection to battle authority. */
    UFUNCTION(BlueprintCallable, Category="Stage") void ShowAction();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    FTimerHandle ActionCameraTimer;
    bool bShowingAllies = false;
    bool bShowingResult = false;
    void RestoreSide();
};
