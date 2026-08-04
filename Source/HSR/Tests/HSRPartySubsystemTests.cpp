#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Curves/CurveFloat.h"
#include "Engine/GameInstance.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Party/HSRPartySubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRPartySubsystemTest,"HSR.Party.FixedSlots",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHSRPartySubsystemTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI=NewObject<UGameInstance>(); UHSRCharacterProfileSubsystem* Profiles=NewObject<UHSRCharacterProfileSubsystem>(GI); UHSRPartySubsystem* Party=NewObject<UHSRPartySubsystem>(GI);
	UHSRCharacterDefinition* A=NewObject<UHSRCharacterDefinition>();A->CharacterId=TEXT("A");A->MaxLevel=2;UCurveFloat* CA=NewObject<UCurveFloat>(A);CA->FloatCurve.AddKey(2,100);A->CumulativeExperienceCurve=CA;
	UHSRCharacterDefinition* B=NewObject<UHSRCharacterDefinition>();B->CharacterId=TEXT("B");B->MaxLevel=2;UCurveFloat* CB=NewObject<UCurveFloat>(B);CB->FloatCurve.AddKey(2,100);B->CumulativeExperienceCurve=CB;
	UHSRCharacterDefinition* C=NewObject<UHSRCharacterDefinition>();C->CharacterId=TEXT("C");C->MaxLevel=2;UCurveFloat* CC=NewObject<UCurveFloat>(C);CC->FloatCurve.AddKey(2,100);C->CumulativeExperienceCurve=CC;
	Profiles->RegisterDefinition(A);Profiles->RegisterDefinition(B);Profiles->RegisterDefinition(C); Party->InitializeForDevelopmentTest(Profiles);
	int32 Events=0; Party->OnPartyChanged().AddLambda([&Events](int64){++Events;});
	TestEqual(TEXT("Add A"),Party->AddCharacter(TEXT("A")),EHSRPartyResult::Success);TestEqual(TEXT("Add B"),Party->AddCharacter(TEXT("B")),EHSRPartyResult::Success);TestEqual(TEXT("Duplicate rejected"),Party->AddCharacter(TEXT("A")),EHSRPartyResult::DuplicateCharacter);TestEqual(TEXT("Full rejected"),Party->AddCharacter(TEXT("B")),EHSRPartyResult::DuplicateCharacter);
	FHSRPartySnapshot S;Party->GetSnapshot(S);TestEqual(TEXT("Capacity fixed"),S.Slots.Num(),2);TestEqual(TEXT("Revision once per commit"),S.Revision,static_cast<int64>(2));TestEqual(TEXT("Event count"),Events,2);
	const int64 RevisionBeforeFailures=S.Revision; const int32 EventsBeforeFailures=Events; const TArray<FHSRPartySlot> SlotsBeforeFailures=S.Slots;
	TestEqual(TEXT("None rejected"),Party->AddCharacter(NAME_None),EHSRPartyResult::ProfileNotFound);
	TestEqual(TEXT("Unknown profile rejected"),Party->AddCharacter(TEXT("Unknown")),EHSRPartyResult::ProfileNotFound);
	TestEqual(TEXT("Full with new C"),Party->AddCharacter(TEXT("C")),EHSRPartyResult::Full);
	TestEqual(TEXT("Invalid add slot"),Party->AddCharacter(TEXT("C"),9),EHSRPartyResult::InvalidSlot);
	TestEqual(TEXT("Replace unknown rejected"),Party->ReplaceCharacter(0,TEXT("Unknown")),EHSRPartyResult::ProfileNotFound);
	TestEqual(TEXT("Replace duplicate rejected"),Party->ReplaceCharacter(0,TEXT("B")),EHSRPartyResult::DuplicateCharacter);
	TestEqual(TEXT("Remove invalid slot"),Party->RemoveCharacter(9),EHSRPartyResult::InvalidSlot);
	TestEqual(TEXT("Swap same rejected"),Party->SwapSlots(0,0),EHSRPartyResult::EmptySlot);
	TestEqual(TEXT("Failure revision unchanged"),Party->GetSnapshot(S),true); TestEqual(TEXT("Failure revision"),S.Revision,RevisionBeforeFailures); TestEqual(TEXT("Failure events"),Events,EventsBeforeFailures); TestEqual(TEXT("Failure slots unchanged"),S.Slots[0].CharacterId,SlotsBeforeFailures[0].CharacterId);
	TestEqual(TEXT("Swap succeeds"),Party->SwapSlots(0,1),EHSRPartyResult::Success);TestEqual(TEXT("Remove succeeds"),Party->RemoveCharacter(0),EHSRPartyResult::Success);TestEqual(TEXT("Replace succeeds"),Party->ReplaceCharacter(0,TEXT("B")),EHSRPartyResult::Success);TestEqual(TEXT("Invalid slot"),Party->RemoveCharacter(9),EHSRPartyResult::InvalidSlot);
	TestEqual(TEXT("Remove to empty succeeds"),Party->RemoveCharacter(0),EHSRPartyResult::Success); TestEqual(TEXT("Swap empty rejected"),Party->SwapSlots(0,1),EHSRPartyResult::EmptySlot); TestEqual(TEXT("Remove empty rejected"),Party->RemoveCharacter(0),EHSRPartyResult::EmptySlot);
	S.Slots[0].CharacterId=TEXT("Mutated");FHSRPartySnapshot Fresh;Party->GetSnapshot(Fresh);TestNotEqual(TEXT("Snapshot isolated"),Fresh.Slots[0].CharacterId,FName(TEXT("Mutated")));return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRPartyCandidateCommitTest,"HSR.Party.CandidateCommit",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHSRPartyCandidateCommitTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI=NewObject<UGameInstance>(); UHSRCharacterProfileSubsystem* Profiles=NewObject<UHSRCharacterProfileSubsystem>(GI); UHSRPartySubsystem* Party=NewObject<UHSRPartySubsystem>(GI);
	auto Register=[Profiles](const TCHAR* Id){ UHSRCharacterDefinition* D=NewObject<UHSRCharacterDefinition>(Profiles); D->CharacterId=FName(Id); D->MaxLevel=2; UCurveFloat* C=NewObject<UCurveFloat>(D); C->FloatCurve.AddKey(2,100); D->CumulativeExperienceCurve=C; return Profiles->RegisterDefinition(D); };
	TestEqual(TEXT("register B"),Register(TEXT("Character.B")),EHSRCharacterProfileResult::Success);
	TestEqual(TEXT("register A"),Register(TEXT("Character.A")),EHSRCharacterProfileResult::Success);
	TestEqual(TEXT("register C"),Register(TEXT("Character.C")),EHSRCharacterProfileResult::Success);
	Party->InitializeForDevelopmentTest(Profiles);

	TArray<FHSRCharacterProfileSnapshot> Available;
	TestTrue(TEXT("available profiles projected"),Profiles->GetAllProfileSnapshots(Available));
	TestEqual(TEXT("all profiles included"),Available.Num(),3);
	TestEqual(TEXT("profiles sorted deterministically"),Available[0].RuntimeState.CharacterId,FName(TEXT("Character.A")));

	int32 Events=0; Party->OnPartyChanged().AddLambda([&Events](int64){++Events;});
	FHSRPartySnapshot Candidate; Party->GetSnapshot(Candidate);
	Candidate.Slots[0].CharacterId=TEXT("Character.A"); Candidate.Slots[1].CharacterId=TEXT("Character.B");
	TestEqual(TEXT("candidate commits atomically"),Party->CommitCandidate(Candidate),EHSRPartyResult::Success);
	FHSRPartySnapshot Committed; Party->GetSnapshot(Committed);
	TestEqual(TEXT("single revision increment"),Committed.Revision,static_cast<int64>(1));
	TestEqual(TEXT("single coherent event"),Events,1);

	FHSRPartySnapshot Stale=Candidate; Stale.Slots[1].CharacterId=TEXT("Character.C");
	TestEqual(TEXT("stale candidate rejected"),Party->CommitCandidate(Stale),EHSRPartyResult::RevisionConflict);
	FHSRPartySnapshot AfterStale; Party->GetSnapshot(AfterStale);
	TestEqual(TEXT("stale leaves revision"),AfterStale.Revision,Committed.Revision);
	TestEqual(TEXT("stale leaves slots"),AfterStale.Slots[1].CharacterId,Committed.Slots[1].CharacterId);
	TestEqual(TEXT("stale emits no event"),Events,1);

	FHSRPartySnapshot Duplicate=Committed; Duplicate.Slots[1].CharacterId=Duplicate.Slots[0].CharacterId;
	TestEqual(TEXT("duplicate candidate rejected"),Party->CommitCandidate(Duplicate),EHSRPartyResult::DuplicateCharacter);
	FHSRPartySnapshot AfterDuplicate; Party->GetSnapshot(AfterDuplicate);
	TestEqual(TEXT("invalid candidate leaves revision"),AfterDuplicate.Revision,Committed.Revision);
	TestEqual(TEXT("invalid candidate emits no event"),Events,1);
	return true;
}
#endif
