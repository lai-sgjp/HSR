#pragma once

#include "CoreMinimal.h"
#include "HSRCharacterBase.h"
#include "HSRExplorationCharacter.generated.h"

class USpringArmComponent;
class UHSRInteractionComponent;
class UCameraComponent;
class UInputAction;

UCLASS()
class HSR_API AHSRExplorationCharacter : public AHSRCharacterBase
{
	GENERATED_BODY()

public:
	AHSRExplorationCharacter();
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	const UInputAction* GetMoveAction() const { return MoveAction; }
	const UInputAction* GetLookAction() const { return LookAction; }
	const UInputAction* GetJumpAction() const { return JumpAction; }
	const UInputAction* GetInteractAction() const { return InteractAction; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	UHSRInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }

protected:
	void Move(const struct FInputActionValue& Value);
	void Look(const struct FInputActionValue& Value);
	void HSJump();
	void HSStopJumping();
	void Interact();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHSRInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> InteractAction;
};
