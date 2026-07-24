#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HSRCharacterProfileTypes.h"
#include "HSRCharacterProfileSubsystem.generated.h"

class UHSRCharacterDefinition;
class UHSRCharacterCatalog;
struct FHSRCharacterProgressionContext;

UCLASS()
class HSR_API UHSRCharacterProfileSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	EHSRCharacterProfileResult RegisterDefinition(const UHSRCharacterDefinition* Definition);
	EHSRCharacterProfileResult RegisterDefinitions(const TArray<const UHSRCharacterDefinition*>& InDefinitions);
	EHSRCharacterProfileResult RegisterLoadedCatalog(const UHSRCharacterCatalog* Catalog);
	EHSRCharacterProfileResult GrantExperience(FName CharacterId, int32 ExperienceToGrant);
	EHSRCharacterProfileResult SetSkillLevel(FName CharacterId, FName SkillId, int32 SkillLevel);
	bool GetProfileSnapshot(FName CharacterId, FHSRCharacterProfileSnapshot& OutSnapshot) const;
	bool GetProgressionContext(FName CharacterId, FHSRCharacterProgressionContext& OutContext) const;
	bool GetDefinition(FName CharacterId, const UHSRCharacterDefinition*& OutDefinition) const;
	FHSRCharacterProfileChanged& OnProfileChanged() { return ProfileChanged; }

private:
	UPROPERTY() TMap<FName, TObjectPtr<const UHSRCharacterDefinition>> Definitions;
	UPROPERTY() TMap<FName, FHSRCharacterProfileSnapshot> Profiles;
	FHSRCharacterProfileChanged ProfileChanged;
};
