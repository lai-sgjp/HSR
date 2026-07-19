# TASK-P5-001 ִ�б���

## ������Ϣ

| �ֶ� | ֵ |
|---|---|
| ������ | TASK-P5-001 |
| ��ɫ | Implementation Agent / �ͼ�ִ��ģ�� |
| ���� | 2026-07-19 |
| Ŀ�� | ������Ч Encounter Context��Battle runtime exactly-once ���������ؽ����/���˲����ߺ� ASC����� ActorInfo ��ʼ�������� Return Context������ʧ��·�� |

## ����/�޸ĵ��ļ�

| �ļ� | ���� | ˵�� |
|---|---|---|
| Source/HSR/Battle/HSRBattleTypes.h | ���� | ��ֵ DTO��EHSRBattleParticipantTeam��EHSRBattleCoordinatorState ״̬����FHSRBattleParticipantDefinition��FHSRBattleReturnContext��FHSRBattleRequestContext��FHSRBattleResult |
| Source/HSR/Battle/HSRBattleParticipant.h | ���� | ���� C++ �ڲ��ṹ�壬�� TWeakObjectPtr ����ʱ������ + �ȶ� ID/Team���� USTRUCT���� Coordinator ���ҳ��� |
| Source/HSR/Battle/HSRBattleCoordinator.h | ���� | UObject ״̬����Idle -> Consuming -> Spawned/Failed���ύ����ӿڡ��������ؽ��ӿڡ�Reset �ӿ� |
| Source/HSR/Battle/HSRBattleCoordinator.cpp | �������޶� | ״̬��ʵ�� + �޸����� RegisterComponent �� DestroyComponent ���� |
| Source/HSR/Battle/HSRBattleGameMode.h | ���� | Battle World ��� AGameModeBase ���࣬���� Coordinator |
| Source/HSR/Battle/HSRBattleGameMode.cpp | ���� | BeginPlay ���̣�NewObject Coordinator -> ConsumePendingEncounter -> SubmitBattleRequest -> BuildParticipants -> ��ϸ��־ |
| 	asks/execution-result.md | ���������� | �����棨�� PIE ֤�ݣ� |

## ʵ��Ҫ��

### Coordinator ״̬��

- Idle -> Consuming��SubmitBattleRequest() ��֤ RequestId/EncounterId/EnemyDefinitionId/BattleMapPath ��Ч��ԭ��д��
- Consuming -> Spawned��BuildParticipants() ������ Player APawn + ASC ��ʼ���������� Enemy APawn + ASC ��ʼ����ȫ���ɹ����� Spawned
- Consuming -> Failed���κ�ʧ�ܣ�World=null��Spawn ʧ�ܡ�ASC ��ʼ��ʧ�ܣ�ԭ���� Failed������м����

### ASC ��ʼ����InitParticipantASC��

1. AddComponentByClass(UHSRAbilitySystemComponent::StaticClass(), false, FTransform::Identity, false) ����ʱ��ӣ�auto-register��
2. SetIsReplicated(false) + SetComponentTickEnabled(false)
3. ASC->InitStats(UHSRCoreAttributeSet::StaticClass(), nullptr) ע�����Լ�
4. ASC->InitAbilityActorInfo(TargetActor, TargetActor) ���� Owner=Avatar=self
5. ��֤ AbilityActorInfo.IsValid()
6. ʧ��ʱ DestroyComponent(true) ��������

### Battle GameMode �������

`
BeginPlay
  -> NewObject<UHSRBattleCoordinator>(this)
  -> UHSRBattleTransitionSubsystem::ConsumePendingEncounter()
  -> Coordinator::SubmitBattleRequest(ConsumedRequest)
  -> Coordinator::BuildParticipants(GetWorld())
  -> ��ϸ��־��״̬������������ReturnContext
`

### ʧ��·������

| ���� | ��Ϊ |
|---|---|
| ��Ч RequestId | SubmitBattleRequest �ܾ������� Failure |
| EncounterId=None | SubmitBattleRequest �ܾ� |
| EnemyDefinitionId=None | SubmitBattleRequest �ܾ� |
| BattleMapPath=None | SubmitBattleRequest �ܾ� |
| BattleWorld=null | BuildParticipants �� Failed����־�� RequestId/EncounterId |
| ASC Init ʧ�� | DestroyComponent(true) ����������� Failed |

## ��֤���

### ������֤
- HSREditor Win64 Development fresh UHT/C++/Link: **Succeeded**
- 7 �� UHT ����ͷ�ļ����ɣ�5 �����붯��
- �˳��� 0��0 ���󣬽� 1 ����֪����ϱ������汾����

### PIE ��֤���û��ṩ��

**��·����Encounter -> Battle Map ���У���**
`
UHSRBattleTransitionSubsystem::ConsumePendingEncounter - SUCCESS
UHSRBattleCoordinator::SubmitBattleRequest - SUCCESS
SpawnParticipantActor - SUCCESS Actor=Pawn_0 Team=0
InitParticipantASC - SUCCESS Actor=Pawn_0 ASC=... ActorInfo valid
SpawnParticipantActor - SUCCESS Actor=Pawn_1 Team=1
InitParticipantASC - SUCCESS Actor=Pawn_1 ASC=... ActorInfo valid
BuildParticipants - SUCCESS RequestId=... Participants=2
BeginPlay - COMPLETE CoordinatorState=2 Participants=2 ReturnMap=/Game/Maps/Map_Phase1_Exploration
  Participant[0]: Id=Player DefId=Enc_Test Valid=1
  Participant[1]: Id=Enemy DefId=Enemy_TestGoblin Valid=1
`

**Exactly-Once ��֤��Battle Map ���μ��أ���**
`
ConsumePendingEncounter - FAILED AlreadyConsumed
BeginPlay - No pending encounter to consume (type=6).
`

### �޸���¼
- �Ƴ����� ASC->RegisterComponent() ���ã�AddComponentByClass auto-register �����ע��������棩
- DestroyComponent(false) -> DestroyComponent(true) ȷ����������

### �û�����
- ʧ��·�����ԣ��ظ� RequestId��ȱ Definition �ȣ����û�ѡ��������������"��Щ���� phase4 ���Թ�"����֪δ��֤�߽��ѱ����

### ���쳣
- �� Editor/PIE �������� Ensure/Assert���� Blueprint runtime error

## ��ȷδʵ�֣�������Χ�⣩

TurnManager��Speed ����GameplayAbility���˺���������Victory/Defeat��BattleResult������̽����ս�� UI��Cost/Cooldown/Energy������ AI��SaveGame�����硢�����Ż�

## �ύ��¼

| commit | ˵�� |
|---|---|
| 73361c6 | ��ʼʵ�֣�6 �� Source �ļ� + ִ�б��� |
| 6dffdd7 | �޸����� RegisterComponent + ���� PIE ֤�� |

## ����

Implementation Agent ����� TASK-P5-001 �� C++ ʵ�֡�������֤���û� PIE ��֤����·���� exactly-once ����ͨ������ǰ״̬��**�� Coordinator/���� Reviewer ���**��
