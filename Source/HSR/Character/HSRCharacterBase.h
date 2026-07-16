#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HSRCharacterBase.generated.h"

UCLASS(Abstract)
class HSR_API AHSRCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	AHSRCharacterBase();
};