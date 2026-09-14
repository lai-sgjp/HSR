#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HSRInstancedScene.generated.h"
class UStaticMesh;
class UMaterialInterface;
/** Editor-authored static decoration grouped by mesh, material and collision policy. */
UCLASS()
class HSR_API AHSRInstancedScene : public AActor
{
    GENERATED_BODY()
public:
    AHSRInstancedScene();
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Authoring")
    void AddSceneInstance(UStaticMesh* Mesh, UMaterialInterface* Material, const FTransform& Transform, bool bCollision);
};
