#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HSRCharacterPreviewWidget.generated.h"

class UHSRCharacterPreviewSession;
class UImage;
class UTextBlock;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** Input and presentation only; the session owns rendering and resource requests. */
UCLASS()
class HSR_API UHSRCharacterPreviewWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetCharacter(FName CharacterId);
	void SetPreviewMaterial(UMaterialInterface* Material);
	UFUNCTION(BlueprintPure, Category="HSR|Preview")
	UHSRCharacterPreviewSession* GetPreviewSession() const { return Session; }
	UFUNCTION(BlueprintCallable, Category="HSR|Preview") void ResetPreviewView();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry&, const FPointerEvent&) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry&, const FPointerEvent&) override;
	virtual FReply NativeOnMouseMove(const FGeometry&, const FPointerEvent&) override;
	virtual FReply NativeOnMouseWheel(const FGeometry&, const FPointerEvent&) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent&) override;

private:
	UFUNCTION() void HandlePreviewChanged();
	UPROPERTY(Transient) TObjectPtr<UHSRCharacterPreviewSession> Session;
	UPROPERTY(Transient) TObjectPtr<UImage> PreviewImage;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> PreviewMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> DisplayMaterial;
	FName PendingCharacterId;
	FVector2D LastSize = FVector2D::ZeroVector;
	bool bDragging = false;
};
