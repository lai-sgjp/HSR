#include "HSRCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "../GAS/HSRAbilitySystemComponent.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"
#include "../UI/HSRAttributeViewModel.h"
#include "GameplayEffect.h"

AHSRCharacterBase::AHSRCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;

	AbilitySystemComponent = CreateDefaultSubobject<UHSRAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(false);
	AbilitySystemComponent->SetComponentTickEnabled(false);

	CoreAttributeSet = CreateDefaultSubobject<UHSRCoreAttributeSet>(TEXT("CoreAttributeSet"));
	AttributeViewModel = CreateDefaultSubobject<UHSRAttributeViewModel>(TEXT("AttributeViewModel"));
}

UAbilitySystemComponent* AHSRCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AHSRCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (!AbilitySystemComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::BeginPlay - AbilitySystemComponent is nullptr"), *GetName());
		return;
	}

	InitializeAbilityActorInfo();
	ApplyInitialAttributes();
	BindAttributeDelegates();
}

void AHSRCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AttributeViewModel)
	{
		AttributeViewModel->Teardown();
	}

	Super::EndPlay(EndPlayReason);
}

void AHSRCharacterBase::InitializeAbilityActorInfo()
{
	if (bActorInfoInitialized)
	{
		return;
	}

	if (!AbilitySystemComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::InitializeAbilityActorInfo - AbilitySystemComponent is nullptr"), *GetName());
		return;
	}

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	if (AbilitySystemComponent->AbilityActorInfo.IsValid())
	{
		bActorInfoInitialized = true;
		UE_LOG(LogTemp, Log, TEXT("%s::InitializeAbilityActorInfo - Owner=Avatar=self, ActorInfo valid"), *GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::InitializeAbilityActorInfo - ActorInfo is NOT valid after Init"), *GetName());
	}
}

void AHSRCharacterBase::ApplyInitialAttributes()
{
	if (bInitialAttributesApplied)
	{
		return;
	}

	if (!AbilitySystemComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::ApplyInitialAttributes - AbilitySystemComponent is nullptr"), *GetName());
		return;
	}

	if (!bActorInfoInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::ApplyInitialAttributes - ActorInfo not initialized yet"), *GetName());
		return;
	}

	if (!InitialAttributesEffect)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::ApplyInitialAttributes - InitialAttributesEffect is not set"), *GetName());
		return;
	}

	FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
	FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(InitialAttributesEffect, 1.0f, Context);
	if (!Spec.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::ApplyInitialAttributes - MakeOutgoingSpec failed"), *GetName());
		return;
	}

	FActiveGameplayEffectHandle Handle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	if (Handle.WasSuccessfullyApplied())
	{
		bInitialAttributesApplied = true;
		InitialAttributesApplySuccessCount++;
		UE_LOG(LogTemp, Log, TEXT("%s::ApplyInitialAttributes - GE applied successfully via WasSuccessfullyApplied (total=%d)"), *GetName(), InitialAttributesApplySuccessCount, *GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::ApplyInitialAttributes - GE was NOT successfully applied"), *GetName());
	}
}


bool AHSRCharacterBase::RequestApplyPhase2TestEffect(TSubclassOf<UGameplayEffect> TestEffect)
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	UE_LOG(LogTemp, Warning, TEXT("%s::RequestApplyPhase2TestEffect - Rejected in Test/Shipping"), *GetName());
	return false;
#else
	if (!TestEffect)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestApplyPhase2TestEffect - TestEffect class is null"), *GetName());
		return false;
	}

	if (!AbilitySystemComponent || !AbilitySystemComponent->AbilityActorInfo.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestApplyPhase2TestEffect - ASC or ActorInfo invalid"), *GetName());
		return false;
	}

	const FString Path = TestEffect->GetPathName();
	const bool bAllowed = Path == TEXT("/Game/GameplayEffects/BP_GE_Test_HealthBelowZero.BP_GE_Test_HealthBelowZero_C")
		|| Path == TEXT("/Game/GameplayEffects/BP_GE_Test_HealthAboveMax.BP_GE_Test_HealthAboveMax_C")
		|| Path == TEXT("/Game/GameplayEffects/BP_GE_Test_LowerMaxHealth.BP_GE_Test_LowerMaxHealth_C")
		|| Path == TEXT("/Game/GameplayEffects/BP_GE_Test_EnergyBounds.BP_GE_Test_EnergyBounds_C")
		|| Path == TEXT("/Game/GameplayEffects/BP_GE_Test_SpeedBelowZero.BP_GE_Test_SpeedBelowZero_C");

	if (!bAllowed)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestApplyPhase2TestEffect - Effect %s not in allowed Phase 2 test list"), *GetName(), *Path);
		return false;
	}

	FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
	FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(TestEffect, 1.0f, Context);
	if (!Spec.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestApplyPhase2TestEffect - MakeOutgoingSpec failed"), *GetName());
		return false;
	}

	FActiveGameplayEffectHandle Handle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	const bool bResult = Handle.WasSuccessfullyApplied();
	UE_LOG(LogTemp, Log, TEXT("%s::RequestApplyPhase2TestEffect - Applied %s success=%d"), *GetName(), *Path, bResult);
	return bResult;
#endif
}

bool AHSRCharacterBase::RequestPhase2Repossess()
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	UE_LOG(LogTemp, Warning, TEXT("%s::RequestPhase2Repossess - Rejected in Test/Shipping"), *GetName());
	return false;
#else
	AController* OriginalController = GetController();
	if (!OriginalController)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestPhase2Repossess - No Controller to re-possess"), *GetName());
		return false;
	}

	if (OriginalController->GetPawn() != this)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestPhase2Repossess - Controller %s does not possess this character"), *GetName(), *OriginalController->GetName());
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("%s::RequestPhase2Repossess - Re-possessing via controller %s"), *GetName(), *OriginalController->GetName());

	OriginalController->UnPossess();
	OriginalController->Possess(this);

	// Validate post-possess state: controller, ASC, ActorInfo all correct
	if (OriginalController->GetPawn() != this)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestPhase2Repossess - Validation FAILED: GetPawn() != this"), *GetName());
		return false;
	}

	if (!AbilitySystemComponent || !AbilitySystemComponent->AbilityActorInfo.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestPhase2Repossess - Validation FAILED: ASC or ActorInfo invalid"), *GetName());
		return false;
	}

	if (AbilitySystemComponent->AbilityActorInfo->OwnerActor != this || AbilitySystemComponent->AbilityActorInfo->AvatarActor != this)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestPhase2Repossess - Validation FAILED: OwnerActor=%s AvatarActor=%s not both self"),
			*GetName(),
			*AbilitySystemComponent->AbilityActorInfo->OwnerActor->GetName(),
			*AbilitySystemComponent->AbilityActorInfo->AvatarActor->GetName());
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("%s::RequestPhase2Repossess - RE-POSSESS SUCCESS: Controller=%s, Pawn=this, Owner=Avatar=self, InitialAttributesApplySuccessCount=%d"),
		*GetName(),
		*OriginalController->GetName(),
		InitialAttributesApplySuccessCount);

	return true;
#endif
}

void AHSRCharacterBase::BindAttributeDelegates()
{
	if (bAttributeDelegatesBound)
	{
		return;
	}

	if (!AbilitySystemComponent || !CoreAttributeSet)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::BindAttributeDelegates - ASC or CoreAttributeSet is nullptr"), *GetName());
		return;
	}

	if (!bActorInfoInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::BindAttributeDelegates - ActorInfo not initialized"), *GetName());
		return;
	}

	if (!AttributeViewModel)
	{
		AttributeViewModel = NewObject<UHSRAttributeViewModel>(this);
	}

	AttributeViewModel->InitializeFromASC(AbilitySystemComponent);
	bAttributeDelegatesBound = true;
	UE_LOG(LogTemp, Log, TEXT("%s::BindAttributeDelegates - ViewModel initialized and delegates bound"), *GetName());
}