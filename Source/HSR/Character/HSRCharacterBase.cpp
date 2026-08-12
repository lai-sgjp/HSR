#include "HSRCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "../GAS/HSRAbilitySystemComponent.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"
#include "../UI/HSRAttributeViewModel.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Equipment/HSREquipmentEffectBridge.h"
#include "../Equipment/HSREquipmentStatAggregator.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Equipment/HSREquipmentTypes.h"
#include "GameplayEffect.h"
#include "Engine/GameInstance.h"

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
	UnprojectEquipmentFromAbilitySystem();

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
		// The authored character definition is the single source of truth for base stats. If the
		// character id is already known (player spawned after RestartPlayer), re-assert the base
		// values so the GE's default magnitudes never stack on top of the definition values.
		ApplyCharacterBaseStatsToAbilitySystem();
		UE_LOG(LogTemp, Log, TEXT("%s::ApplyInitialAttributes - GE applied successfully via WasSuccessfullyApplied (total=%d)"), *GetName(), InitialAttributesApplySuccessCount);
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

	const FString PackagePath = TestEffect->GetOutermost()->GetName();
	const bool bAllowed = PackagePath == TEXT("/Game/GameplayEffects/BP_GE_Test_HealthBelowZero")
		|| PackagePath == TEXT("/Game/GameplayEffects/BP_GE_Test_HealthAboveMax")
		|| PackagePath == TEXT("/Game/GameplayEffects/BP_GE_Test_LowerMaxHealth")
		|| PackagePath == TEXT("/Game/GameplayEffects/BP_GE_Test_EnergyBounds")
		|| PackagePath == TEXT("/Game/GameplayEffects/BP_GE_Test_SpeedBelowZero");

	if (!bAllowed)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestApplyPhase2TestEffect - Effect %s not in allowed Phase 2 test list"), *GetName(), *PackagePath);
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
	UE_LOG(LogTemp, Log, TEXT("%s::RequestApplyPhase2TestEffect - Applied %s success=%d"), *GetName(), *PackagePath, bResult);
	return bResult;
#endif
}

bool AHSRCharacterBase::RequestPhase2Repossess()
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	UE_LOG(LogTemp, Warning, TEXT("%s::RequestPhase2Repossess - Rejected in Test/Shipping"), *GetName());
	return false;
#else
	// Pre-snapshot
	UE_LOG(LogTemp, Log, TEXT("%s::RequestPhase2Repossess - PRE: Controller=%s, Pawn=%s, ASC=%s, InitApplyCount=%d"),
		*GetName(),
		GetController() ? *GetController()->GetName() : TEXT("null"),
		*GetName(),
		AbilitySystemComponent ? *AbilitySystemComponent->GetName() : TEXT("null"),
		InitialAttributesApplySuccessCount);
	if (AbilitySystemComponent && AbilitySystemComponent->AbilityActorInfo.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("%s::RequestPhase2Repossess - PRE: ActorInfo valid, Owner=%s, Avatar=%s"),
			*GetName(),
			AbilitySystemComponent->AbilityActorInfo->OwnerActor.IsValid() ? *AbilitySystemComponent->AbilityActorInfo->OwnerActor->GetName() : TEXT("null"),
			AbilitySystemComponent->AbilityActorInfo->AvatarActor.IsValid() ? *AbilitySystemComponent->AbilityActorInfo->AvatarActor->GetName() : TEXT("null"));
		const UHSRCoreAttributeSet* CoreSet = AbilitySystemComponent->GetSet<UHSRCoreAttributeSet>();
		if (CoreSet)
		{
			UE_LOG(LogTemp, Log, TEXT("%s::RequestPhase2Repossess - PRE: Health=%f MaxHealth=%f Energy=%f MaxEnergy=%f Speed=%f"),
				*GetName(), CoreSet->GetHealth(), CoreSet->GetMaxHealth(), CoreSet->GetEnergy(), CoreSet->GetMaxEnergy(), CoreSet->GetSpeed());
		}
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("%s::RequestPhase2Repossess - PRE: ActorInfo invalid"), *GetName());
	}

	AController* OriginalController = GetController();
	if (!OriginalController)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestPhase2Repossess - No Controller"), *GetName());
		return false;
	}
	if (OriginalController->GetPawn() != this)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestPhase2Repossess - Current Controller %s does not possess this"), *GetName(), *OriginalController->GetName());
		return false;
	}

	OriginalController->UnPossess();
	OriginalController->Possess(this);

	// Individual null checks
	if (!OriginalController)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestPhase2Repossess - FAIL: Controller destroyed"), *GetName());
		return false;
	}
	AActor* PostPawn = OriginalController->GetPawn();
	if (PostPawn != this)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestPhase2Repossess - FAIL: GetPawn()=%s"), *GetName(), PostPawn ? *PostPawn->GetName() : TEXT("null"));
		return false;
	}
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestPhase2Repossess - FAIL: ASC destroyed"), *GetName());
		return false;
	}
	if (!AbilitySystemComponent->AbilityActorInfo.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestPhase2Repossess - FAIL: ActorInfo invalid"), *GetName());
		return false;
	}
	const AActor* OwnerActor = AbilitySystemComponent->AbilityActorInfo->OwnerActor.Get();
	const AActor* AvatarActor = AbilitySystemComponent->AbilityActorInfo->AvatarActor.Get();
	if (!OwnerActor || OwnerActor != this)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestPhase2Repossess - FAIL: Owner=%s"), *GetName(), OwnerActor ? *OwnerActor->GetName() : TEXT("null"));
		return false;
	}
	if (!AvatarActor || AvatarActor != this)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::RequestPhase2Repossess - FAIL: Avatar=%s"), *GetName(), AvatarActor ? *AvatarActor->GetName() : TEXT("null"));
		return false;
	}

	// Post-snapshot
	const UHSRCoreAttributeSet* CoreSetPost = AbilitySystemComponent->GetSet<UHSRCoreAttributeSet>();
	if (CoreSetPost)
	{
		UE_LOG(LogTemp, Log, TEXT("%s::RequestPhase2Repossess - POST: Health=%f MaxHealth=%f Energy=%f MaxEnergy=%f Speed=%f"),
			*GetName(), CoreSetPost->GetHealth(), CoreSetPost->GetMaxHealth(), CoreSetPost->GetEnergy(), CoreSetPost->GetMaxEnergy(), CoreSetPost->GetSpeed());
	}
	UE_LOG(LogTemp, Log, TEXT("%s::RequestPhase2Repossess - SUCCESS: Cont=%s, Pawn=this, Owner=Avatar=self, ActorInfo valid, InitApplyCount=%d"),
		*GetName(), *OriginalController->GetName(), InitialAttributesApplySuccessCount);

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

bool AHSRCharacterBase::SetProjectedCharacterId(const FName CharacterId)
{
	if (CharacterId.IsNone())
	{
		return false;
	}
	ProjectedCharacterId = CharacterId;
	ApplyCharacterBaseStatsToAbilitySystem();
	ProjectEquipmentToAbilitySystem();
	return true;
}

void AHSRCharacterBase::ApplyCharacterBaseStatsToAbilitySystem()
{
	if (ProjectedCharacterId.IsNone() || !AbilitySystemComponent)
	{
		return;
	}
	UGameInstance* GameInstance = GetGameInstance();
	UHSRCharacterProfileSubsystem* Profiles = GameInstance ? GameInstance->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
	if (!Profiles)
	{
		return;
	}
	const UHSRCharacterDefinition* Definition = nullptr;
	if (!Profiles->GetDefinition(ProjectedCharacterId, Definition) || !Definition)
	{
		return;
	}
	UHSRCoreAttributeSet* Core = const_cast<UHSRCoreAttributeSet*>(AbilitySystemComponent->GetSet<UHSRCoreAttributeSet>());
	if (!Core)
	{
		return;
	}
	const float MaxHealth = Definition->BaseMaxHealth;
	AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxHealthAttribute(), MaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), MaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxEnergyAttribute(), Definition->BaseMaxEnergy);
	AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetEnergyAttribute(), Definition->BaseMaxEnergy);
	AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetAttackAttribute(), Definition->BaseAttack);
	AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetDefenseAttribute(), Definition->BaseDefense);
	AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), Definition->BaseSpeed);
	UE_LOG(LogTemp, Log, TEXT("AHSRCharacterBase::ApplyCharacterBaseStatsToAbilitySystem - %s base MaxHealth=%.0f Health=%.0f MaxEnergy=%.0f Energy=%.0f Speed=%.0f"),
		*GetName(), MaxHealth,
		AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()),
		Definition->BaseMaxEnergy,
		AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute()),
		Definition->BaseSpeed);
}

void AHSRCharacterBase::ProjectEquipmentToAbilitySystem()
{
	if (ProjectedCharacterId.IsNone() || !AbilitySystemComponent)
	{
		return;
	}
	UGameInstance* GameInstance = GetGameInstance();
	UHSREquipmentSubsystem* Equipment = GameInstance ? GameInstance->GetSubsystem<UHSREquipmentSubsystem>() : nullptr;
	if (!Equipment)
	{
		return;
	}
	UnprojectEquipmentFromAbilitySystem();
	if (!EquipmentEffectBridge)
	{
		EquipmentEffectBridge = NewObject<UHSREquipmentEffectBridge>(this);
	}
	TSubclassOf<UGameplayEffect> EquipmentEffect =
		LoadClass<UGameplayEffect>(nullptr, TEXT("/Game/GameplayEffects/GE_Equipment_P12.GE_Equipment_P12_C"));
	const FGuid CharacterGuid = HSRCharacterGuidFromProfileName(ProjectedCharacterId);
	FHSREquipmentLoadout Loadout;
	int32 Revision = 0;
	if (EquipmentEffect && Equipment->GetLoadout(CharacterGuid, Loadout, Revision))
	{
		for (const auto& Pair : Loadout.Equipment)
		{
			FHSREquipmentAggregate Aggregate;
			if (UHSREquipmentStatAggregator::AddInstance(Pair.Value, Aggregate))
			{
				Aggregate.Revision = Revision;
				EquipmentEffectBridge->Apply(Pair.Value.InstanceId, AbilitySystemComponent, EquipmentEffect, Aggregate);
			}
		}
		for (const auto& Pair : Loadout.Relics)
		{
			FHSREquipmentAggregate Aggregate;
			if (UHSREquipmentStatAggregator::AddInstance(Pair.Value, Aggregate))
			{
				Aggregate.Revision = Revision;
				EquipmentEffectBridge->Apply(Pair.Value.InstanceId, AbilitySystemComponent, EquipmentEffect, Aggregate);
			}
		}
	}
	EquipmentLoadoutChangedHandle = Equipment->OnLoadoutChanged().AddUObject(this, &AHSRCharacterBase::HandleEquipmentLoadoutChanged);
	UE_LOG(LogTemp, Log, TEXT("AHSRCharacterBase::ProjectEquipmentToAbilitySystem - %s projected equipment loadout to ASC"), *GetName());
}

void AHSRCharacterBase::UnprojectEquipmentFromAbilitySystem()
{
	if (EquipmentLoadoutChangedHandle.IsValid())
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UHSREquipmentSubsystem* Equipment = GameInstance->GetSubsystem<UHSREquipmentSubsystem>())
			{
				Equipment->OnLoadoutChanged().Remove(EquipmentLoadoutChangedHandle);
			}
		}
		EquipmentLoadoutChangedHandle.Reset();
	}
	if (EquipmentEffectBridge)
	{
		EquipmentEffectBridge->RemoveAll();
	}
}

void AHSRCharacterBase::HandleEquipmentLoadoutChanged(const FGuid& CharacterId, int32 Revision)
{
	const FGuid MyGuid = HSRCharacterGuidFromProfileName(ProjectedCharacterId);
	if (CharacterId != MyGuid)
	{
		return;
	}
	ProjectEquipmentToAbilitySystem();
}
