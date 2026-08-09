#include "HSREquipmentDetailViewModel.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Equipment/HSREquipmentStatAggregator.h"
void UHSREquipmentDetailViewModel::BeginDestroy(){Shutdown();Super::BeginDestroy();}
void UHSREquipmentDetailViewModel::Initialize(UHSREquipmentSubsystem* In,const FGuid& Id){Shutdown();Equipment=In;CharacterId=Id;if(In&&Id.IsValid()){Subscription=In->OnLoadoutChanged().AddUObject(this,&ThisClass::Rebuild);Rebuild(Id,0);return;}Snapshot=FHSREquipmentDetailSnapshot();Snapshot.CharacterId=Id;Snapshot.FailureReason=EHSREquipmentDetailResult::NotInitialized;bHas=true;Changed.Broadcast(Snapshot);OnSnapshotChanged.Broadcast(Snapshot);}
void UHSREquipmentDetailViewModel::Shutdown(){if(Equipment.IsValid())Equipment->OnLoadoutChanged().Remove(Subscription);Equipment.Reset();bHas=false;Snapshot=FHSREquipmentDetailSnapshot();}
void UHSREquipmentDetailViewModel::Rebuild(const FGuid& Id,int32){if(Id!=CharacterId||!Equipment.IsValid())return;FHSREquipmentLoadout L;int32 Revision=0;Snapshot=FHSREquipmentDetailSnapshot();Snapshot.CharacterId=CharacterId;if(!Equipment->GetLoadout(CharacterId,L,Revision)){Snapshot.bIsValid=true;Snapshot.FailureReason=EHSREquipmentDetailResult::Empty;}else{Snapshot.Revision=Revision;for(const auto& P:L.Equipment)Snapshot.Items.Add(P.Value);for(const auto& P:L.Relics)Snapshot.Items.Add(P.Value);Snapshot.Items.Sort([](const auto& A,const auto& B){return A.InstanceId<B.InstanceId;});for(const FHSREquipmentInstance& Item:Snapshot.Items)for(int32 I=0;I<Item.Modifiers.Num();++I){const auto& M=Item.Modifiers[I];FHSREquipmentSourceRow R;R.SourceId=FName(*FString::Printf(TEXT("%s.%d"),*Item.InstanceId.ToString(EGuidFormats::Digits),I));R.DefinitionId=Item.DefinitionId;R.Stat=M.Stat;R.AuthoredValue=M.Value;R.EffectiveValue=M.Value;Snapshot.Sources.Add(R);}
		// Totals come from the aggregator rather than a local sum, so the panel cannot display a
		// value the authority would reject (non-finite or negative modifiers).
		FHSREquipmentAggregate Totals;
		if (UHSREquipmentStatAggregator::Aggregate(L, Revision, Totals))
		{
			Snapshot.MaxHealth = Totals.MaxHealth;
			Snapshot.Attack = Totals.Attack;
			Snapshot.Defense = Totals.Defense;
			Snapshot.Speed = Totals.Speed;
		}
		else
		{
			Snapshot.FailureReason = EHSREquipmentDetailResult::InvalidSnapshot;
		}
TArray<FHSRRelicSetSnapshot> Sets;Equipment->GetRelicSetSnapshots(CharacterId,Sets);for(const auto& Set:Sets){FHSRRelicSetDetailRow Row;Row.SetId=Set.SetId;Row.SetSourceId=Set.SetSourceId;Row.EquippedCount=Set.EquippedCount;Row.Threshold=Set.Threshold;Row.bActive=Set.bActive;Snapshot.RelicSets.Add(Row);if(Set.bActive){FHSREquipmentSourceRow Source;Source.SourceId=Set.SetSourceId;Source.DefinitionId=Set.SetId;Snapshot.Sources.Add(Source);}}Snapshot.Sources.Sort([](const auto& A,const auto& B){return A.SourceId.LexicalLess(B.SourceId);});Snapshot.bIsValid=true;Snapshot.FailureReason=EHSREquipmentDetailResult::Success;}bHas=true;Changed.Broadcast(Snapshot);OnSnapshotChanged.Broadcast(Snapshot);}
