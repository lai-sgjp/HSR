#pragma once

#include "CoreMinimal.h"

class UGameInstance;
class UHSREquipmentSubsystem;
class UHSRInventorySubsystem;

class HSR_API FHSREquipmentDevelopmentHarness
{
public:
	static bool SetupFixedLoadout(UGameInstance* GameInstance);
	static bool RemoveSecondRelic(UGameInstance* GameInstance);
	static bool RestoreSecondRelic(UGameInstance* GameInstance);
	static bool ClearLoadout(UGameInstance* GameInstance);
	static bool Save(UGameInstance* GameInstance);
	static bool Load(UGameInstance* GameInstance);
	static bool CleanupSave();
	static bool SetupFixedLoadoutForTest(UHSREquipmentSubsystem* Equipment);
	static bool RemoveSecondRelicForTest(UHSREquipmentSubsystem* Equipment);
	static bool ClearLoadoutForTest(UHSREquipmentSubsystem* Equipment);
	static bool RunP17MovementAudit(UGameInstance* GameInstance);
	static bool RunP17MovementAuditForTest(UHSREquipmentSubsystem* Equipment,UHSRInventorySubsystem* Inventory);
	static bool RunP17RelicFixture(UGameInstance* GameInstance);
	static bool RunP17RelicFixtureForTest(UHSREquipmentSubsystem* Equipment,UHSRInventorySubsystem* Inventory);
};
