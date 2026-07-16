#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HSRUserWidget.generated.h"

UCLASS()
class HSR_API UHSRUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UHSRUserWidget(const FObjectInitializer& ObjectInitializer);
};
