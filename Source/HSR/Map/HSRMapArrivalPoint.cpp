#include "HSRMapArrivalPoint.h"

#include "Components/SceneComponent.h"

// 地图到达点 Actor：只作为关卡中的“落点标记”存在，本身不参与传送逻辑。
// 传送到达提交时，ArrivalConsumer 会扫描关卡里所有到达点，按 ArrivalId 匹配后取得摆放位置。
AHSRMapArrivalPoint::AHSRMapArrivalPoint()
{
	// 到达点是纯标记，不需要每帧更新。
	PrimaryActorTick.bCanEverTick = false;
	// 用 SceneComponent 作为根组件，保证 Actor 有可用的 Transform 可供摆放。
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}
