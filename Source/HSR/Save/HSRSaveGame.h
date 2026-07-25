#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "HSRSaveTypes.h"
#include "HSRSaveGame.generated.h"

UCLASS()
class HSR_API UHSRSaveGame : public USaveGame { GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite) FHSRSaveData Data;
};

UCLASS()
class HSR_API UHSRForeignSaveGame : public USaveGame { GENERATED_BODY() };
