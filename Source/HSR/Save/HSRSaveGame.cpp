// HSRSaveGame.cpp
//
// USaveGame 子类的实现文件。整个存档系统的逻辑并不在这里——这个类只作为
// 旧版（legacy）USaveGame 容器，用于承载一个 FHSRSaveData 成员，以便
// UGameplayStatics::LoadGameFromSlot 能按旧的 UObject 序列化方式读出数据。
// 新版（P11+）的存档已经切换到自定义二进制信封（envelope）格式，由
// HSRSaveVersion::EncodeEnvelope / DecodeEnvelope 负责，因此这里只剩一个
// include，不需要任何实现代码。
#include "HSRSaveGame.h"
