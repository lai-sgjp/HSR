#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HSRPartyTypes.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "HSRPartySubsystem.generated.h"

UCLASS()
class HSR_API UHSRPartySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	static constexpr int32 Capacity = 2;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	EHSRPartyResult AddCharacter(FName CharacterId, int32 PreferredSlot = INDEX_NONE);
	EHSRPartyResult RemoveCharacter(int32 Slot);
	EHSRPartyResult ReplaceCharacter(int32 Slot, FName CharacterId);
	EHSRPartyResult SwapSlots(int32 FirstSlot, int32 SecondSlot);
	bool GetSnapshot(FHSRPartySnapshot& OutSnapshot) const;
	FHSRPartyChanged& OnPartyChanged() { return PartyChanged; }
#if WITH_EDITOR
	void InitializeForDevelopmentTest(UHSRCharacterProfileSubsystem* InProfiles) { Slots.SetNum(Capacity); Profiles = InProfiles; }
#endif

private:
	bool IsValidSlot(int32 Slot) const { return Slot >= 0 && Slot < Capacity; }
	bool IsKnownProfile(FName CharacterId) const;
	bool IsDuplicate(const TArray<FHSRPartySlot>& Candidate, FName CharacterId, int32 IgnoreSlot = INDEX_NONE) const;
	bool Commit(TArray<FHSRPartySlot>&& Candidate);
	UPROPERTY() TArray<FHSRPartySlot> Slots;
	UPROPERTY() int64 Revision = 0;
	TWeakObjectPtr<UHSRCharacterProfileSubsystem> Profiles;
	FHSRPartyChanged PartyChanged;
};
