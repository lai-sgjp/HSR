#pragma once

#include "CoreMinimal.h"
#include "../HSRScreenWidget.h"
#include "HSRCharacterShellTypes.h"
#include "HSRCharacterShellWidget.generated.h"

class UHSRCharacterShellViewModel;
class UHSRCharacterShellWidget;
class UHSRRelicEquipmentWidget;
class UHSRInventoryCatalog;
class UHSRItemEquipmentMappingCatalog;
class UPanelWidget;

UCLASS()
class HSR_API UHSRCharacterChoiceButton : public UObject
{
	GENERATED_BODY()
public:
	void Initialize(UHSRCharacterShellWidget* InOwner, FName InCharacterId, int32 InTab = INDEX_NONE);
	UFUNCTION() void HandleClicked();
private:
	TWeakObjectPtr<UHSRCharacterShellWidget> Owner;
	FName CharacterId;
	int32 Tab = INDEX_NONE;
};

UCLASS(Blueprintable)
class HSR_API UHSRCharacterShellWidget : public UHSRScreenWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "HSR|Character Shell")
	EHSRCharacterShellResult SelectCharacter(FName CharacterId);

	UFUNCTION(BlueprintCallable, Category = "HSR|Character Shell")
	EHSRCharacterShellResult SelectTab(EHSRCharacterShellTab Tab);

	UFUNCTION(BlueprintCallable, Category = "HSR|Character Shell")
	EHSRCharacterShellResult RefreshShell();

	UFUNCTION(BlueprintCallable, Category = "HSR|Character Shell")
	bool RequestChangeWeapon();

	UFUNCTION(BlueprintPure, Category = "HSR|Character Shell")
	bool GetCurrentSnapshot(FHSRCharacterShellSnapshot& OutSnapshot) const;

	UFUNCTION(BlueprintPure, Category = "HSR|Character Shell")
	int32 GetRefreshCount() const { return RefreshCount; }

#if WITH_DEV_AUTOMATION_TESTS
	void ApplySnapshotForAutomation(const FHSRCharacterShellSnapshot& Snapshot) { HandleShellChanged(Snapshot); }
#endif

	UFUNCTION(BlueprintImplementableEvent, Category = "HSR|Character Shell")
	void OnShellSnapshotChanged(const FHSRCharacterShellSnapshot& Snapshot);

	UFUNCTION(BlueprintImplementableEvent, Category = "HSR|Character Shell")
	void OnShellUnavailable(EHSRCharacterShellResult Result);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void HandleShellChanged(const FHSRCharacterShellSnapshot& InSnapshot);
	void UpdateDetailStats(const FHSRCharacterShellSnapshot& InSnapshot);
	void RefreshPresentation();
	void PopulateCharacters();
	void PopulateTabContent();
	void AddContentText(UPanelWidget* Host, const FText& Text, int32 FontSize = 20);
	FText EquipmentName(FName DefinitionId) const;
	UFUNCTION() void HandleChangeWeaponClicked();

	UPROPERTY(Transient) TObjectPtr<UHSRCharacterShellViewModel> ViewModel;
	UPROPERTY(Transient) TArray<TObjectPtr<UHSRCharacterChoiceButton>> ChoiceBindings;
	UPROPERTY(Transient) TObjectPtr<UHSRRelicEquipmentWidget> RelicPanel;
	UPROPERTY(Transient) TObjectPtr<UPanelWidget> NativeTabContent;
	UPROPERTY(EditAnywhere, Category = "Presentation") TSubclassOf<UHSRRelicEquipmentWidget> RelicWidgetClass;
	UPROPERTY(EditAnywhere, Category = "Presentation") TObjectPtr<UHSRInventoryCatalog> PresentationCatalog;
	UPROPERTY(EditAnywhere, Category = "Presentation") TObjectPtr<UHSRItemEquipmentMappingCatalog> EquipmentMappingCatalog;
	FName RelicPanelCharacterId;
	FDelegateHandle ShellChangedHandle;
	FHSRCharacterShellSnapshot CurrentSnapshot;
	bool bHasCurrentSnapshot = false;
	int32 RefreshCount = 0;
};
