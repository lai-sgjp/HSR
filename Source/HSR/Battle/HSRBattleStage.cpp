#include "HSRBattleStage.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"

AHSRBattleStage::AHSRBattleStage()
{
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("StageOrigin"));
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("StageCamera"));
    Camera->SetupAttachment(RootComponent);
    Camera->FieldOfView = 65.f;
    Camera->bConstrainAspectRatio = false;
    for (int32 I=0; I<4; ++I) PlayerSlots.Add(FTransform(FRotator(0,0,0), FVector(-600,(I-1.5f)*240,100)));
    for (int32 I=0; I<5; ++I) EnemySlots.Add(FTransform(FRotator(0,180,0), FVector(650,(I-2.f)*240,100)));
    BossSlot = FTransform(FRotator(0,180,0), FVector(850,0,100));
    EnemyView = FTransform(FRotator(-10,24,0), FVector(-1250,-1100,430));
    AllyView = FTransform(FRotator(-8,160,0), FVector(550,-1000,330));
    ResultView = FTransform(FRotator(-8,180,0), FVector(700,0,350));
    ActionView = FTransform(FRotator(-17,40,0), FVector(-1650,-1400,850));
    Camera->SetRelativeTransform(EnemyView);
}

bool AHSRBattleStage::ResolveSlot(bool bPlayer, int32 Index, bool bBoss, FTransform& Out) const
{
    const TArray<FTransform>& Slots = bPlayer ? PlayerSlots : EnemySlots;
    if (!Slots.IsValidIndex(Index)) return false;
    const FTransform& Local = !bPlayer && bBoss && Index == 0 ? BossSlot : Slots[Index];
    Out = Local * GetActorTransform();
    return !Out.ContainsNaN();
}

void AHSRBattleStage::BeginPlay()
{
    Super::BeginPlay();
    ShowSide(false);
}

void AHSRBattleStage::ShowSide(bool bAllies, bool bResult)
{
    GetWorldTimerManager().ClearTimer(ActionCameraTimer);
    bShowingAllies = bAllies;
    bShowingResult = bResult;
    Camera->SetRelativeTransform(bResult ? ResultView : bAllies ? AllyView : EnemyView);
    if (UWorld* World = GetWorld())
        if (APlayerController* PC = World->GetFirstPlayerController()) PC->SetViewTarget(this);
}

void AHSRBattleStage::ShowAction()
{
    if (bShowingResult) return;
    Camera->SetRelativeTransform(ActionView);
    GetWorldTimerManager().SetTimer(ActionCameraTimer,this,&ThisClass::RestoreSide,1.4f,false);
}

void AHSRBattleStage::RestoreSide() { ShowSide(bShowingAllies,bShowingResult); }

void AHSRBattleStage::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(ActionCameraTimer);
    Super::EndPlay(EndPlayReason);
}
