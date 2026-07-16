#pragma once

#include "CoreMinimal.h"
#include "HSRCharacterBase.h"
#include "HSRExplorationCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;

UCLASS()
class HSR_API AHSRExplorationCharacter : public AHSRCharacterBase
{
	GENERATED_BODY()

public:
	AHSRExplorationCharacter();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;
};