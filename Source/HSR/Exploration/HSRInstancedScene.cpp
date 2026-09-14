#include "HSRInstancedScene.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
AHSRInstancedScene::AHSRInstancedScene()
{
    PrimaryActorTick.bCanEverTick=false;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent->SetMobility(EComponentMobility::Static);
}
void AHSRInstancedScene::AddSceneInstance(UStaticMesh* Mesh,UMaterialInterface* Material,const FTransform& Transform,bool bCollision)
{
    if (!Mesh || Transform.ContainsNaN()) return;
    TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Components(this);
    UHierarchicalInstancedStaticMeshComponent* Group=nullptr;
    for (auto* C : Components)
        if (C->GetStaticMesh()==Mesh && (!Material || C->GetMaterial(0)==Material)
            && (C->GetCollisionEnabled()!=ECollisionEnabled::NoCollision)==bCollision) { Group=C; break; }
    if (!Group)
    {
        Group=NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
        AddInstanceComponent(Group);
        Group->SetupAttachment(RootComponent);
        Group->SetMobility(EComponentMobility::Static);
        Group->SetStaticMesh(Mesh);
        if (Material) Group->SetMaterial(0,Material);
        Group->SetCollisionProfileName(bCollision ? TEXT("BlockAll") : TEXT("NoCollision"));
        Group->SetCullDistances(0,45000);
        Group->RegisterComponent();
    }
    Group->AddInstance(Transform,true);
}
