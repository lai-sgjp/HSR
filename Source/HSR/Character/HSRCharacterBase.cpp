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

	CoreAttributeSet = AbilitySystemComponent->GetSet<UHSRCoreAttributeSet>();
	if (!CoreAttributeSet)
	{
		CoreAttributeSet = AbilitySystemComponent->AddAttributeSetSubobject(NewObject<UHSRCoreAttributeSet>(AbilitySystemComponent.Get()));
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
		UE_LOG(LogTemp, Log, TEXT("%s::ApplyInitialAttributes - GE applied successfully via WasSuccessfullyApplied"), *GetName());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::ApplyInitialAttributes - GE was NOT successfully applied (WasSuccessfullyApplied=false)"), *GetName());
	}
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
