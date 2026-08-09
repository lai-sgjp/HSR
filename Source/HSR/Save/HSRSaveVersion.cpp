#include "HSRSaveVersion.h"
#include "../Challenge/HSRChallengeProgressionSubsystem.h"
#include "Misc/DateTime.h"
#include <type_traits>

namespace
{
constexpr uint32 MaxCount = HSRSaveVersion::MaxRecordCount;
FString CanonicalName(const FName& Value) { return Value.IsNone() ? FString() : Value.ToString().ToLower(); }
bool NameLess(const FName& A,const FName& B) { return CanonicalName(A).Compare(CanonicalName(B),ESearchCase::CaseSensitive)<0; }

uint32 RotateRight(const uint32 Value, const uint32 Count)
{
	return (Value >> Count) | (Value << (32 - Count));
}

void ComputeSha256(const uint8* Data, const int64 Size, uint8 OutDigest[32])
{
	static constexpr uint32 K[64] = {
		0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
		0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
		0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
		0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
		0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
		0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
		0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
		0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
	uint32 H[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
	TArray<uint8> Message;
	Message.Append(Data, Size);
	Message.Add(0x80);
	while ((Message.Num() % 64) != 56) Message.Add(0);
	const uint64 BitSize = static_cast<uint64>(Size) * 8;
	for (int32 Shift = 56; Shift >= 0; Shift -= 8) Message.Add(static_cast<uint8>(BitSize >> Shift));
	for (int32 Offset = 0; Offset < Message.Num(); Offset += 64)
	{
		uint32 W[64];
		for (int32 I = 0; I < 16; ++I) W[I] = (uint32(Message[Offset+I*4])<<24)|(uint32(Message[Offset+I*4+1])<<16)|(uint32(Message[Offset+I*4+2])<<8)|Message[Offset+I*4+3];
		for (int32 I = 16; I < 64; ++I) { const uint32 S0=RotateRight(W[I-15],7)^RotateRight(W[I-15],18)^(W[I-15]>>3); const uint32 S1=RotateRight(W[I-2],17)^RotateRight(W[I-2],19)^(W[I-2]>>10); W[I]=W[I-16]+S0+W[I-7]+S1; }
		uint32 A=H[0],B=H[1],C=H[2],D=H[3],E=H[4],F=H[5],G=H[6],HH=H[7];
		for (int32 I=0;I<64;++I){const uint32 S1=RotateRight(E,6)^RotateRight(E,11)^RotateRight(E,25);const uint32 Ch=(E&F)^((~E)&G);const uint32 T1=HH+S1+Ch+K[I]+W[I];const uint32 S0=RotateRight(A,2)^RotateRight(A,13)^RotateRight(A,22);const uint32 Maj=(A&B)^(A&C)^(B&C);const uint32 T2=S0+Maj;HH=G;G=F;F=E;E=D+T1;D=C;C=B;B=A;A=T1+T2;}
		H[0]+=A;H[1]+=B;H[2]+=C;H[3]+=D;H[4]+=E;H[5]+=F;H[6]+=G;H[7]+=HH;
	}
	for(int32 I=0;I<8;++I)for(int32 J=0;J<4;++J)OutDigest[I*4+J]=static_cast<uint8>(H[I]>>(24-J*8));
}

struct FWriter
{
	TArray<uint8>& B;
	explicit FWriter(TArray<uint8>& In) : B(In) {}
	void U8(uint8 V) { B.Add(V); }
	template<typename T> void I(T V) { using U=std::make_unsigned_t<T>; for (int32 I = 0; I < sizeof(T); ++I) U8(static_cast<uint8>((static_cast<U>(V) >> (I * 8)) & 0xff)); }
	void Guid(const FGuid& V) { I<uint32>(V.A); I<uint32>(V.B); I<uint32>(V.C); I<uint32>(V.D); }
	bool Name(const FName& V) { FString S = CanonicalName(V); if(!HSRSaveVersion::IsCanonicalIdToken(S))return false; FTCHARToUTF8 Utf8(*S); I<uint32>(Utf8.Length()); B.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length()); return true; }
	bool NameRequired(const FName& V) { return !V.IsNone()&&Name(V); }
	bool D(double V) { if (!FMath::IsFinite(V)) return false; if (V == 0.0) V = 0.0; uint64 Bits; FMemory::Memcpy(&Bits, &V, sizeof(Bits)); I<uint64>(Bits); return true; }
};
struct FReader
{
	const TArray<uint8>& B; int64 P = 0; bool bOk = true;
	explicit FReader(const TArray<uint8>& In) : B(In) {}
	uint8 U8() { return P < B.Num() ? B[P++] : (bOk = false, 0); }
	template<typename T> T I() { using U=std::make_unsigned_t<T>; U V = 0; for (int32 X=0;X<sizeof(T);++X) V |= static_cast<U>(U8()) << (X*8); return static_cast<T>(V); }
	FGuid Guid() { const uint32 A=I<uint32>(); const uint32 C1=I<uint32>(); const uint32 C2=I<uint32>(); const uint32 D=I<uint32>(); return FGuid(A,C1,C2,D); }
	bool Name(FName& Out) { const uint32 N=I<uint32>(); if(!bOk||N>HSRSaveVersion::MaxTokenBytes||P+N>B.Num()) return bOk=false; FString S;
		FUTF8ToTCHAR Conv(reinterpret_cast<const ANSICHAR*>(B.GetData()+P), N); S=FString(Conv.Length(),Conv.Get()); P+=N;
		if (!S.IsEmpty()) { for(TCHAR C:S) if(!((C>='a'&&C<='z')||(C>='0'&&C<='9')||C=='.'||C=='_'||C=='-')) return bOk=false; if (FName(*S).ToString().ToLower()!=S) return bOk=false; Out=FName(*S); } else Out=NAME_None; return true; }
	bool NameRequired(FName& Out) { return Name(Out)&&!Out.IsNone(); }
	bool D(double& Out) { const uint64 Bits=I<uint64>(); FMemory::Memcpy(&Out,&Bits,sizeof(Bits)); if(!FMath::IsFinite(Out)) return bOk=false; if(Out==0.0) Out=0.0; return bOk; }
};
template<typename T> bool Count(FWriter& W,const TArray<T>& A){if(static_cast<uint64>(A.Num())>HSRSaveVersion::MaxRecordCount)return false;W.I<uint32>(A.Num());return true;}
template<typename T> bool ReadCount(FReader& R,TArray<T>& A){const uint32 N=R.I<uint32>();if(!R.bOk||N>HSRSaveVersion::MaxRecordCount)return R.bOk=false;A.SetNum(N);return true;}
bool Transform(FWriter& W,const FTransform& T){const FQuat Q=T.GetRotation();const FVector L=T.GetLocation(),S=T.GetScale3D();return W.D(Q.X)&&W.D(Q.Y)&&W.D(Q.Z)&&W.D(Q.W)&&W.D(L.X)&&W.D(L.Y)&&W.D(L.Z)&&W.D(S.X)&&W.D(S.Y)&&W.D(S.Z);}
bool Transform(FReader& R,FTransform& T){double X,Y,Z,W,A,B,C,D,E,F;if(!R.D(X)||!R.D(Y)||!R.D(Z)||!R.D(W)||!R.D(A)||!R.D(B)||!R.D(C)||!R.D(D)||!R.D(E)||!R.D(F))return false;T=FTransform(FQuat(X,Y,Z,W),FVector(A,B,C),FVector(D,E,F));return true;}
bool Profile(FWriter&W,const FHSRSaveProfileDto&P){if(!W.NameRequired(P.State.CharacterId))return false;W.I<int32>(P.State.Level);W.I<int32>(P.State.Experience);W.I<int32>(P.State.Ascension);TArray<FName>K;P.State.SkillLevels.GetKeys(K);K.Sort(NameLess);if(!Count(W,K))return false;for(FName N:K){if(!W.NameRequired(N))return false;W.I<int32>(P.State.SkillLevels[N]);}W.I<int64>(P.RuntimeRevision);return true;}
bool Profile(FReader&R,FHSRSaveProfileDto&P){if(!R.NameRequired(P.State.CharacterId))return false;P.State.Level=R.I<int32>();P.State.Experience=R.I<int32>();P.State.Ascension=R.I<int32>();uint32 N=R.I<uint32>();if(!R.bOk||N>HSRSaveVersion::MaxRecordCount)return R.bOk=false;FName Prev;bool bHasPrev=false;for(uint32 I=0;I<N;++I){FName K;if(!R.NameRequired(K)||(bHasPrev&&!NameLess(Prev,K)))return R.bOk=false;Prev=K;bHasPrev=true;P.State.SkillLevels.Add(K,R.I<int32>());}P.RuntimeRevision=R.I<int64>();return R.bOk;}
bool Location(FWriter&W,const FHSRMapLocation&L){return W.Name(L.MapId)&&W.Name(L.ArrivalId)&&Transform(W,L.WorldTransform);} bool Location(FReader&R,FHSRMapLocation&L){return R.Name(L.MapId)&&R.Name(L.ArrivalId)&&Transform(R,L.WorldTransform);}
bool Names(FWriter&W,TArray<FName> A){A.Sort(NameLess);for(int32 I=1;I<A.Num();++I)if(CanonicalName(A[I-1])==CanonicalName(A[I]))return false;if(!Count(W,A))return false;for(FName N:A)if(!W.NameRequired(N))return false;return true;} bool Names(FReader&R,TArray<FName>&A){if(!ReadCount(R,A))return false;FName P;bool bHasPrev=false;for(FName&N:A){if(!R.NameRequired(N)||(bHasPrev&&!NameLess(P,N)))return R.bOk=false;P=N;bHasPrev=true;}return true;}
bool Inventory(FWriter&W,const FHSRInventorySaveData&D){auto S=D.Stacks;S.Sort([](const auto&A,const auto&B){return NameLess(A.ItemId,B.ItemId);});for(int32 I=1;I<S.Num();++I)if(CanonicalName(S[I-1].ItemId)==CanonicalName(S[I].ItemId))return false;if(!Count(W,S))return false;for(const auto&X:S){if(!W.NameRequired(X.ItemId))return false;W.I<int32>(X.Quantity);}auto U=D.UniqueItems;U.Sort([](const auto&A,const auto&B){return A.InstanceId<B.InstanceId;});for(int32 I=1;I<U.Num();++I)if(U[I-1].InstanceId==U[I].InstanceId)return false;if(!Count(W,U))return false;for(const auto&X:U){if(!X.InstanceId.IsValid())return false;W.Guid(X.InstanceId);if(!W.NameRequired(X.DefinitionId))return false;}W.I<int64>(D.Revision);return true;}
bool Inventory(FReader&R,FHSRInventorySaveData&D){if(!ReadCount(R,D.Stacks))return false;FName P;bool bHasPrev=false;for(auto&X:D.Stacks){if(!R.NameRequired(X.ItemId)||(bHasPrev&&!NameLess(P,X.ItemId)))return R.bOk=false;P=X.ItemId;bHasPrev=true;X.Quantity=R.I<int32>();}if(!ReadCount(R,D.UniqueItems))return false;FGuid G;for(int32 I=0;I<D.UniqueItems.Num();++I){auto&X=D.UniqueItems[I];X.InstanceId=R.Guid();if(!X.InstanceId.IsValid()||(I&&!(G<X.InstanceId)))return R.bOk=false;G=X.InstanceId;if(!R.NameRequired(X.DefinitionId))return false;}D.Revision=R.I<int64>();return R.bOk;}
bool Reward(FWriter&W,const FHSRRewardSaveData&D){auto A=D.Receipts;A.Sort([](const auto&X,const auto&Y){return X.Request.ClaimId<Y.Request.ClaimId;});for(int32 I=1;I<A.Num();++I)if(A[I-1].Request.ClaimId==A[I].Request.ClaimId)return false;if(!Count(W,A))return false;for(auto X:A){if(!X.Request.ClaimId.IsValid())return false;W.Guid(X.Request.ClaimId);if(!W.NameRequired(X.Request.RewardDefinitionId))return false;W.I<int32>(X.Request.Seed);auto G=X.Grants;G.Sort([](const auto&L,const auto&R){return NameLess(L.ItemId,R.ItemId);});for(int32 I=1;I<G.Num();++I)if(CanonicalName(G[I-1].ItemId)==CanonicalName(G[I].ItemId))return false;if(!Count(W,G))return false;for(auto&V:G){if(!W.NameRequired(V.ItemId))return false;W.I<int32>(V.Quantity);V.InstanceIds.Sort();for(int32 I=0;I<V.InstanceIds.Num();++I)if(!V.InstanceIds[I].IsValid()||(I&&V.InstanceIds[I-1]==V.InstanceIds[I]))return false;if(!Count(W,V.InstanceIds))return false;for(const FGuid&I:V.InstanceIds)W.Guid(I);}W.I<int64>(X.Revision);}W.I<int64>(D.Revision);return true;}
bool Reward(FReader&R,FHSRRewardSaveData&D){if(!ReadCount(R,D.Receipts))return false;FGuid P;for(int32 I=0;I<D.Receipts.Num();++I){auto&X=D.Receipts[I];X.Request.ClaimId=R.Guid();if(!X.Request.ClaimId.IsValid()||(I&&!(P<X.Request.ClaimId)))return R.bOk=false;P=X.Request.ClaimId;if(!R.NameRequired(X.Request.RewardDefinitionId))return false;X.Request.Seed=R.I<int32>();if(!ReadCount(R,X.Grants))return false;FName PN;bool bHasPrev=false;for(auto&V:X.Grants){if(!R.NameRequired(V.ItemId)||(bHasPrev&&!NameLess(PN,V.ItemId)))return R.bOk=false;PN=V.ItemId;bHasPrev=true;V.Quantity=R.I<int32>();if(!ReadCount(R,V.InstanceIds))return false;FGuid PG;for(int32 J=0;J<V.InstanceIds.Num();++J){V.InstanceIds[J]=R.Guid();if(!V.InstanceIds[J].IsValid()||(J&&!(PG<V.InstanceIds[J])))return R.bOk=false;PG=V.InstanceIds[J];}}X.Revision=R.I<int64>();}D.Revision=R.I<int64>();return R.bOk;}
bool Quest(FWriter&W,const FHSRQuestSaveData&D){auto A=D.States;A.Sort([](const auto&X,const auto&Y){return NameLess(X.QuestId,Y.QuestId);});for(int32 I=1;I<A.Num();++I)if(CanonicalName(A[I-1].QuestId)==CanonicalName(A[I].QuestId))return false;if(!Count(W,A))return false;for(auto X:A){if(!W.NameRequired(X.QuestId)||static_cast<uint8>(X.State)>static_cast<uint8>(EHSRQuestState::Completed))return false;W.U8(uint8(X.State));auto O=X.Objectives;O.Sort([](const auto&L,const auto&R){return NameLess(L.ObjectiveId,R.ObjectiveId);});for(int32 I=1;I<O.Num();++I)if(CanonicalName(O[I-1].ObjectiveId)==CanonicalName(O[I].ObjectiveId))return false;if(!Count(W,O))return false;for(const auto&V:O){if(!W.NameRequired(V.ObjectiveId))return false;W.I<int32>(V.CurrentCount);W.I<int32>(V.RequiredCount);W.U8(V.bCompleted?1:0);}W.U8(X.bRewardClaimed?1:0);W.Guid(X.RewardClaimId);W.I<int64>(X.Revision);}W.I<int64>(D.Revision);return true;}
bool Quest(FReader&R,FHSRQuestSaveData&D){if(!ReadCount(R,D.States))return false;FName P;bool bHasPrev=false;for(auto&X:D.States){if(!R.NameRequired(X.QuestId)||(bHasPrev&&!NameLess(P,X.QuestId)))return R.bOk=false;P=X.QuestId;bHasPrev=true;uint8 S=R.U8();if(S>uint8(EHSRQuestState::Completed))return R.bOk=false;X.State=EHSRQuestState(S);if(!ReadCount(R,X.Objectives))return false;FName PO;bool bHasObjective=false;for(auto&V:X.Objectives){if(!R.NameRequired(V.ObjectiveId)||(bHasObjective&&!NameLess(PO,V.ObjectiveId)))return R.bOk=false;PO=V.ObjectiveId;bHasObjective=true;V.CurrentCount=R.I<int32>();V.RequiredCount=R.I<int32>();uint8 C=R.U8();if(C>1)return R.bOk=false;V.bCompleted=C!=0;}uint8 C=R.U8();if(C>1)return R.bOk=false;X.bRewardClaimed=C!=0;X.RewardClaimId=R.Guid();X.Revision=R.I<int64>();}D.Revision=R.I<int64>();return R.bOk;}
bool ChallengeProgression(FWriter&W,const FHSRChallengeProgressionSaveData&D){if(D.Revision<0)return false;auto A=D.CompletedEncounterIds;A.Sort(NameLess);for(int32 I=1;I<A.Num();++I)if(CanonicalName(A[I-1])==CanonicalName(A[I]))return false;if(!Count(W,A))return false;for(const FName Id:A)if(!W.NameRequired(Id))return false;W.I<int64>(D.Revision);return true;}
bool ChallengeProgression(FReader&R,FHSRChallengeProgressionSaveData&D){if(!ReadCount(R,D.CompletedEncounterIds))return false;FName P;bool bHasPrev=false;for(FName& Id:D.CompletedEncounterIds){if(!R.NameRequired(Id)||(bHasPrev&&!NameLess(P,Id)))return R.bOk=false;P=Id;bHasPrev=true;}D.Revision=R.I<int64>();return R.bOk&&D.Revision>=0;}
}

bool HSRSaveVersion::IsValidSlot(const FString& SlotName,int32 User){return !SlotName.IsEmpty()&&!SlotName.Contains(TEXT(":"))&&User>=0;}
bool HSRSaveVersion::IsValidPayloadSize(uint64 PayloadBytes){return PayloadBytes<=static_cast<uint64>(FHSRSaveEnvelopeHeader::MaxPayloadBytes);}
bool HSRSaveVersion::IsCanonicalIdToken(const FString& Token){FTCHARToUTF8 Utf8(*Token);if(Utf8.Length()>MaxTokenBytes)return false;for(TCHAR C:Token)if(!((C>='a'&&C<='z')||(C>='0'&&C<='9')||C=='.'||C=='_'||C=='-'))return false;return true;}
uint64 HSRSaveVersion::MakeSlotIdentity(const FString& Slot,int32 User){FTCHARToUTF8 S(*(Slot+TEXT(":")+LexToString(User)));uint64 H=14695981039346656037ull;for(int32 I=0;I<S.Length();++I){H^=static_cast<uint8>(S.Get()[I]);H*=1099511628211ull;}return H;}

bool HSRSaveVersion::EncodeCanonicalPayload(const FHSRSaveData& D,TArray<uint8>& O)
{
	if(D.SchemaVersion>=7&&!D.Equipment.IsEmpty())return false;
	O.Reset();FWriter W(O);W.I<uint32>(D.SchemaVersion);TArray<FHSRSaveProfileDto>P=D.Profiles;P.Sort([](const auto&A,const auto&B){return NameLess(A.State.CharacterId,B.State.CharacterId);});for(int32 I=1;I<P.Num();++I)if(CanonicalName(P[I-1].State.CharacterId)==CanonicalName(P[I].State.CharacterId))return false;if(!Count(W,P))return false;for(const auto&X:P)if(!Profile(W,X))return false;
	if(D.PartySlots.Num()!=static_cast<int32>(PartySlotCountForSchema(D.SchemaVersion))||!Count(W,D.PartySlots))return false; for(const auto& X:D.PartySlots)if(!W.Name(X.CharacterId))return false; W.I<int64>(D.PartyRevision);
	// The remaining domain DTOs are intentionally encoded as stable, field-ordered records.
	if(D.SchemaVersion>=7){auto E=D.EquipmentRegistry;E.Sort([](const auto&A,const auto&B){return A.InstanceId<B.InstanceId;});for(int32 I=1;I<E.Num();++I)if(E[I-1].InstanceId==E[I].InstanceId)return false;if(!Count(W,E))return false;for(const auto&X:E){if(!X.InstanceId.IsValid()||X.Kind<0||X.Kind>1||!W.NameRequired(X.DefinitionId))return false;W.Guid(X.InstanceId);W.I<int32>(X.Kind);W.I<int32>(X.EnhancementLevel);if(!Count(W,X.Modifiers))return false;for(const auto&M:X.Modifiers){if(static_cast<uint8>(M.Stat)>static_cast<uint8>(EHSREquipmentStat::Speed))return false;W.U8(static_cast<uint8>(M.Stat));if(!W.D(M.Value))return false;}if(!W.Name(X.SetId))return false;}auto L=D.EquipmentPlacements;L.Sort([](const auto&A,const auto&B){if(A.CharacterId!=B.CharacterId)return A.CharacterId<B.CharacterId;if(A.Kind!=B.Kind)return A.Kind<B.Kind;if(A.Slot!=B.Slot)return A.Slot<B.Slot;return A.InstanceId<B.InstanceId;});if(!Count(W,L))return false;for(const auto&X:L){if(!X.InstanceId.IsValid()||!X.CharacterId.IsValid()||X.Kind<0||X.Kind>1||X.Slot<0)return false;W.Guid(X.InstanceId);W.Guid(X.CharacterId);W.I<int32>(X.Kind);W.I<int32>(X.Slot);W.I<int32>(X.AuthorityRevision);}}
	else{TArray<FHSREquipmentSaveDto>E=D.Equipment;E.Sort([](const auto&A,const auto&B){return A.InstanceId<B.InstanceId;});for(int32 I=1;I<E.Num();++I)if(E[I-1].InstanceId==E[I].InstanceId)return false;if(!Count(W,E))return false;for(const auto&X:E){if(!X.InstanceId.IsValid()||!X.CharacterId.IsValid()||X.Kind<0||X.Kind>1||X.Slot<0||(X.Kind==0&&X.Slot>static_cast<int32>(EHSREquipmentSlot::Feet))||(X.Kind==1&&X.Slot>static_cast<int32>(EHSRRelicSlot::LinkRope))||!W.NameRequired(X.DefinitionId))return false;W.Guid(X.InstanceId);W.Guid(X.CharacterId);W.I<int32>(X.Kind);W.I<int32>(X.Slot);W.I<int32>(X.EnhancementLevel);if(!Count(W,X.Modifiers))return false;for(const auto&M:X.Modifiers){if(static_cast<uint8>(M.Stat)>static_cast<uint8>(EHSREquipmentStat::Speed))return false;W.I<uint8>(static_cast<uint8>(M.Stat));if(!W.D(M.Value))return false;}if(!W.Name(X.SetId))return false;W.I<int32>(X.AuthorityRevision);}}
	if(!Inventory(W,D.Inventory)||!Reward(W,D.Rewards)||!Quest(W,D.Quests))return false;
	if(!Names(W,D.Map.UnlockedRegionIds)||!Names(W,D.Map.UnlockedTeleportIds)||!Names(W,D.Map.ExplorationFlags)||!Location(W,D.Map.CurrentLocation))return false;W.I<int64>(D.Map.Revision);
	if(D.SchemaVersion>=8&&!ChallengeProgression(W,D.ChallengeProgression))return false;
	return IsValidPayloadSize(O.Num());
}
EHSRSaveDecodeResult HSRSaveVersion::DecodeCanonicalPayload(const TArray<uint8>& P,FHSRSaveData& O)
{
	FReader R(P);FHSRSaveData D;D.SchemaVersion=R.I<uint32>();const uint32 N=R.I<uint32>();
	if(!R.bOk||N>MaxCount)return EHSRSaveDecodeResult::InvalidPayload;
	FName PreviousProfile;
	for(uint32 I=0;I<N;++I){FHSRSaveProfileDto X;if(!Profile(R,X)||(!PreviousProfile.IsNone()&&!NameLess(PreviousProfile,X.State.CharacterId)))return EHSRSaveDecodeResult::NonCanonical;PreviousProfile=X.State.CharacterId;D.Profiles.Add(MoveTemp(X));}
	const uint32 PartyCount=R.I<uint32>();if(!R.bOk||PartyCount!=PartySlotCountForSchema(D.SchemaVersion))return EHSRSaveDecodeResult::NonCanonical;
	for(uint32 I=0;I<PartyCount;++I){FHSRPartySlot S;if(!R.Name(S.CharacterId))return EHSRSaveDecodeResult::NonCanonical;D.PartySlots.Add(S);}D.PartyRevision=R.I<int64>();
	if(D.SchemaVersion>=7){if(!ReadCount(R,D.EquipmentRegistry))return EHSRSaveDecodeResult::InvalidPayload;FGuid Prev;for(int32 I=0;I<D.EquipmentRegistry.Num();++I){auto&X=D.EquipmentRegistry[I];if(!R.NameRequired(X.DefinitionId))return EHSRSaveDecodeResult::NonCanonical;X.InstanceId=R.Guid();X.Kind=R.I<int32>();X.EnhancementLevel=R.I<int32>();if(!X.InstanceId.IsValid()||(I&&!(Prev<X.InstanceId))||X.Kind<0||X.Kind>1)return EHSRSaveDecodeResult::NonCanonical;Prev=X.InstanceId;const uint32 M=R.I<uint32>();if(!R.bOk||M>MaxCount)return EHSRSaveDecodeResult::InvalidPayload;X.Modifiers.SetNum(M);for(auto&V:X.Modifiers){const uint8 S=R.U8();if(S>static_cast<uint8>(EHSREquipmentStat::Speed))return EHSRSaveDecodeResult::NonCanonical;V.Stat=static_cast<EHSREquipmentStat>(S);double Z;if(!R.D(Z))return EHSRSaveDecodeResult::NonCanonical;V.Value=Z;}if(!R.Name(X.SetId))return EHSRSaveDecodeResult::NonCanonical;}if(!ReadCount(R,D.EquipmentPlacements))return EHSRSaveDecodeResult::InvalidPayload;FHSREquipmentPlacementDto PreviousPlacement;for(int32 I=0;I<D.EquipmentPlacements.Num();++I){auto&X=D.EquipmentPlacements[I];X.InstanceId=R.Guid();X.CharacterId=R.Guid();X.Kind=R.I<int32>();X.Slot=R.I<int32>();X.AuthorityRevision=R.I<int32>();if(!X.InstanceId.IsValid()||!X.CharacterId.IsValid()||X.Kind<0||X.Kind>1||X.Slot<0||(X.Kind==0&&X.Slot>static_cast<int32>(EHSREquipmentSlot::Feet))||(X.Kind==1&&X.Slot>static_cast<int32>(EHSRRelicSlot::LinkRope))||X.AuthorityRevision<0)return EHSRSaveDecodeResult::NonCanonical;if(I&&(X.CharacterId<PreviousPlacement.CharacterId||(X.CharacterId==PreviousPlacement.CharacterId&&(X.Kind<PreviousPlacement.Kind||(X.Kind==PreviousPlacement.Kind&&X.Slot<=PreviousPlacement.Slot)))))return EHSRSaveDecodeResult::NonCanonical;PreviousPlacement=X;}}
	else{if(!ReadCount(R,D.Equipment))return EHSRSaveDecodeResult::InvalidPayload;FGuid PrevGuid;for(int32 I=0;I<D.Equipment.Num();++I){auto&X=D.Equipment[I];if(!R.NameRequired(X.DefinitionId))return EHSRSaveDecodeResult::NonCanonical;X.InstanceId=R.Guid();X.CharacterId=R.Guid();X.Kind=R.I<int32>();X.Slot=R.I<int32>();X.EnhancementLevel=R.I<int32>();if(!X.InstanceId.IsValid()||!X.CharacterId.IsValid()||X.Kind<0||X.Kind>1||X.Slot<0||(X.Kind==0&&X.Slot>static_cast<int32>(EHSREquipmentSlot::Feet))||(X.Kind==1&&X.Slot>static_cast<int32>(EHSRRelicSlot::LinkRope)))return EHSRSaveDecodeResult::NonCanonical;const uint32 M=R.I<uint32>();if(!R.bOk||M>MaxCount)return EHSRSaveDecodeResult::InvalidPayload;X.Modifiers.SetNum(M);for(auto&V:X.Modifiers){const uint8 Stat=R.U8();if(Stat>static_cast<uint8>(EHSREquipmentStat::Speed))return EHSRSaveDecodeResult::NonCanonical;V.Stat=static_cast<EHSREquipmentStat>(Stat);double Z;if(!R.D(Z))return EHSRSaveDecodeResult::NonCanonical;V.Value=Z;}if(!R.Name(X.SetId))return EHSRSaveDecodeResult::NonCanonical;X.AuthorityRevision=R.I<int32>();if(I&&!(PrevGuid<X.InstanceId))return EHSRSaveDecodeResult::NonCanonical;PrevGuid=X.InstanceId;}}
	if(!Inventory(R,D.Inventory)||!Reward(R,D.Rewards)||!Quest(R,D.Quests))return EHSRSaveDecodeResult::NonCanonical;
	if(!Names(R,D.Map.UnlockedRegionIds)||!Names(R,D.Map.UnlockedTeleportIds)||!Names(R,D.Map.ExplorationFlags)||!Location(R,D.Map.CurrentLocation))return EHSRSaveDecodeResult::NonCanonical;
	D.Map.Revision=R.I<int64>();if(D.SchemaVersion>=8&&!ChallengeProgression(R,D.ChallengeProgression))return EHSRSaveDecodeResult::NonCanonical;if(!R.bOk||R.P!=P.Num())return EHSRSaveDecodeResult::InvalidPayload;O=MoveTemp(D);return EHSRSaveDecodeResult::Success;
}
EHSRSaveDecodeResult HSRSaveVersion::MigrateToCurrent(FHSRSaveData& D){if(D.SchemaVersion<1)return EHSRSaveDecodeResult::TooOld;if(D.SchemaVersion>CurrentSchema)return EHSRSaveDecodeResult::FutureSchema;while(D.SchemaVersion<CurrentSchema){switch(D.SchemaVersion){case 1:D.Equipment.Reset();D.Inventory=FHSRInventorySaveData();D.Rewards=FHSRRewardSaveData();++D.SchemaVersion;break;case 2:D.Inventory=FHSRInventorySaveData();D.Rewards=FHSRRewardSaveData();++D.SchemaVersion;break;case 3:D.Quests=FHSRQuestSaveData();++D.SchemaVersion;break;case 4:D.Map=FHSRMapSaveData();++D.SchemaVersion;break;case 5:++D.SchemaVersion;break;case 6:for(const auto&X:D.Equipment){FHSREquipmentRegistryDto R;R.DefinitionId=X.DefinitionId;R.InstanceId=X.InstanceId;R.Kind=X.Kind;R.EnhancementLevel=X.EnhancementLevel;R.Modifiers=X.Modifiers;R.SetId=X.SetId;D.EquipmentRegistry.Add(MoveTemp(R));FHSREquipmentPlacementDto P;P.InstanceId=X.InstanceId;P.CharacterId=X.CharacterId;P.Kind=X.Kind;P.Slot=X.Slot;P.AuthorityRevision=X.AuthorityRevision;D.EquipmentPlacements.Add(P);}D.Equipment.Reset();++D.SchemaVersion;break;case 7:D.ChallengeProgression=FHSRChallengeProgressionSaveData();++D.SchemaVersion;break;case 8:{const int32 Widened=static_cast<int32>(PartySlotCount);if(D.PartySlots.Num()>Widened)return EHSRSaveDecodeResult::MigrationFailed;while(D.PartySlots.Num()<Widened)D.PartySlots.Add(FHSRPartySlot());}++D.SchemaVersion;break;default:return EHSRSaveDecodeResult::MigrationFailed;}}
	if(D.PartySlots.Num()!=static_cast<int32>(PartySlotCount))return EHSRSaveDecodeResult::MigrationFailed;if(D.PartyRevision<0||D.Inventory.Revision<0||D.Rewards.Revision<0||D.Quests.Revision<0||D.Map.Revision<0||!UHSRChallengeProgressionSubsystem::ValidateSaveData(D.ChallengeProgression))return EHSRSaveDecodeResult::MigrationFailed;for(const FHSRSaveProfileDto& P:D.Profiles)if(P.RuntimeRevision<0||P.State.Level<1||P.State.Experience<0||P.State.Ascension<0)return EHSRSaveDecodeResult::MigrationFailed;return EHSRSaveDecodeResult::Success;}
bool HSRSaveVersion::ComputeSha256(const TArray<uint8>& Bytes,TArray<uint8>& Out){if(!Bytes.IsEmpty()&&Bytes.Num()>MAX_uint32)return false;Out.SetNumUninitialized(32);::ComputeSha256(Bytes.GetData(),Bytes.Num(),Out.GetData());return true;}
bool HSRSaveVersion::EncodeEnvelopeAtUtc(const FHSRSaveData& D,const FString& Slot,int32 User,const FGuid& Id,uint64 Gen,int64 Utc,TArray<uint8>& O){if(!IsValidSlot(Slot,User)||!Id.IsValid()||Gen==0)return false;TArray<uint8>P;if(!EncodeCanonicalPayload(D,P))return false;O.Reset();FWriter W(O);const uint8 Magic[8]={'H','S','R','S','A','V','E',0};O.Append(Magic,8);W.I<uint16>(1);W.I<uint16>(104);W.I<uint32>(D.SchemaVersion);W.I<uint32>(1);W.I<uint16>(1);W.I<uint16>(0);W.I<uint64>(P.Num());W.Guid(Id);W.I<uint64>(Gen);W.I<int64>(Utc);W.I<uint64>(MakeSlotIdentity(Slot,User));for(int32 I=0;I<32;++I)W.U8(0);O.Append(P);TArray<uint8> Digest;if(!ComputeSha256(O,Digest))return false;FMemory::Memcpy(O.GetData()+72,Digest.GetData(),32);return true;}
bool HSRSaveVersion::EncodeEnvelope(const FHSRSaveData& D,const FString& Slot,int32 User,const FGuid& Id,uint64 Gen,TArray<uint8>& O){return EncodeEnvelopeAtUtc(D,Slot,User,Id,Gen,FDateTime::UtcNow().ToUnixTimestamp()*1000,O);}
EHSRSaveDecodeResult HSRSaveVersion::DecodeEnvelope(const TArray<uint8>& B,const FString& Slot,int32 User,FHSRSaveData& O,FHSRSaveEnvelopeHeader* H)
{
	if(!IsValidSlot(Slot,User))return EHSRSaveDecodeResult::InvalidArgument;
	if(B.Num()<FHSRSaveEnvelopeHeader::HeaderBytes)return EHSRSaveDecodeResult::TooShort;
	const uint8 Magic[8]={'H','S','R','S','A','V','E',0};if(FMemory::Memcmp(B.GetData(),Magic,8)!=0)return EHSRSaveDecodeResult::BadMagic;
	FReader R(B);R.P=8;const uint16 Format=R.I<uint16>(),HeaderSize=R.I<uint16>();FHSRSaveEnvelopeHeader X;X.SchemaVersion=R.I<uint32>();X.MinimumCompatibleSchema=R.I<uint32>();const uint16 Codec=R.I<uint16>(),Flags=R.I<uint16>();const uint64 Size=R.I<uint64>();X.SaveId=R.Guid();X.Generation=R.I<uint64>();X.UtcUnixMilliseconds=R.I<int64>();X.SlotIdentity=R.I<uint64>();
	if(!R.bOk)return EHSRSaveDecodeResult::InvalidHeader;
	if(Format!=FHSRSaveEnvelopeHeader::FormatVersion||HeaderSize!=FHSRSaveEnvelopeHeader::HeaderBytes||Codec!=FHSRSaveEnvelopeHeader::PayloadCodecVersion||Flags!=0)return EHSRSaveDecodeResult::UnsupportedFormat;
	if(!X.SaveId.IsValid()||X.Generation==0)return EHSRSaveDecodeResult::InvalidHeader;
	if(!IsValidPayloadSize(Size)||Size!=static_cast<uint64>(B.Num()-FHSRSaveEnvelopeHeader::HeaderBytes))return EHSRSaveDecodeResult::InvalidSize;
	if(X.SlotIdentity!=MakeSlotIdentity(Slot,User))return EHSRSaveDecodeResult::SlotMismatch;
	TArray<uint8> Copy=B,Digest;FMemory::Memset(Copy.GetData()+72,0,32);if(!ComputeSha256(Copy,Digest)||FMemory::Memcmp(B.GetData()+72,Digest.GetData(),32)!=0)return EHSRSaveDecodeResult::ChecksumMismatch;
	if(H)*H=X;
	if(X.SchemaVersion>CurrentSchema)return EHSRSaveDecodeResult::FutureSchema;
	if(X.SchemaVersion<1)return EHSRSaveDecodeResult::TooOld;
	if(X.MinimumCompatibleSchema<1||X.MinimumCompatibleSchema>X.SchemaVersion)return EHSRSaveDecodeResult::InvalidHeader;
	TArray<uint8>P;P.Append(B.GetData()+FHSRSaveEnvelopeHeader::HeaderBytes,Size);EHSRSaveDecodeResult Result=DecodeCanonicalPayload(P,O);if(Result!=EHSRSaveDecodeResult::Success)return Result;if(O.SchemaVersion!=X.SchemaVersion)return EHSRSaveDecodeResult::NonCanonical;return MigrateToCurrent(O);
}
