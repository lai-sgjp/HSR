# TASK-P15-001：Map 领域契约、Definition 与纯值 Authority

Status: `ARCHIVED / PASS`

唯一结果：Map/Teleport Definition、稳定 Map/Teleport DTO 和 `UHSRMapSubsystem` 可注册、查询、解锁并构造纯值请求；错误输入零副作用。本任务不执行 OpenLevel。

实现范围：`Source/HSR/Map/HSRMapTypes.h`、`HSRMapSubsystem.h/.cpp`、两个 Map/Teleport Definition 和 `HSRMapSubsystemTests.cpp`。

证据：Development Editor Build 成功；`HSR.Map` 2/2 Success、exit code 0；用户创建、保存并重开两地图、两 Map Definition 和 AB/BA Teleport；Independent Reviewer 最终 `PASS`。

策略：package existence 延迟至 P15-002 preflight；目标 Region 是旅行门禁；ArrivalId 在本包为非空 opaque stable ID。
