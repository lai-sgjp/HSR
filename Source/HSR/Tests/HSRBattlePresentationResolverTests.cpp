#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../Battle/HSRBattlePresentationResolver.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRBattlePresentationResolverMappingTest,
	"HSR.Battle.PresentationResolver.Mapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRBattlePresentationResolverMappingTest::RunTest(const FString&)
{
	FHSRBattlePresentationResolver Resolver;
	FHSRBattlePresentationMapping Mapping;
	Mapping.EventType = EHSRPresentationEventType::Damage;
	Mapping.AttackPresentationId = TEXT("Demo.Presentation.BasicAttack");
	Mapping.HitPresentationId = TEXT("Demo.Presentation.Impact");
	Resolver.AddMapping(Mapping);

	FHSRBattlePresentationEvent Event;
	Event.EventId = FGuid(18, 1, 0, 1);
	Event.ActionId = FGuid(18, 1, 0, 2);
	Event.SourceParticipantId = TEXT("Demo.Character.Vanguard");
	Event.TargetParticipantId = TEXT("Demo.Character.AshWarden");
	Event.EventType = EHSRPresentationEventType::Damage;
	Event.Value = 42.0f;

	FHSRBattlePresentationIntent Intent;
	TestTrue(TEXT("authored mapping resolves"), Resolver.Resolve(Event, Intent));
	TestEqual(TEXT("attack mapping id is authored Demo data"), Intent.AttackPresentationId, Mapping.AttackPresentationId);
	TestEqual(TEXT("hit mapping id is authored Demo data"), Intent.HitPresentationId, Mapping.HitPresentationId);
	TestFalse(TEXT("mapped event does not use fallback"), Intent.bFallback);
	TestEqual(TEXT("resolver preserves authoritative value"), Intent.Value, Event.Value);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRBattlePresentationResolverFallbackTest,
	"HSR.Battle.PresentationResolver.MissingAssetFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRBattlePresentationResolverFallbackTest::RunTest(const FString&)
{
	FHSRBattlePresentationResolver Resolver;
	FHSRBattlePresentationEvent Event;
	Event.EventId = FGuid(18, 2, 0, 1);
	Event.EventType = EHSRPresentationEventType::Break;
	Event.Value = 7.0f;

	FHSRBattlePresentationIntent Intent;
	TestTrue(TEXT("missing mapping resolves through fallback"), Resolver.Resolve(Event, Intent));
	TestTrue(TEXT("fallback is explicit"), Intent.bFallback);
	TestEqual(TEXT("fallback attack id is stable"), Intent.AttackPresentationId, FName(TEXT("Demo.Presentation.Fallback.Attack")));
	TestEqual(TEXT("fallback hit id is stable"), Intent.HitPresentationId, FName(TEXT("Demo.Presentation.Fallback.Hit")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRBattlePresentationResolverDedupTest,
	"HSR.Battle.PresentationResolver.DeduplicatesEvents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRBattlePresentationResolverDedupTest::RunTest(const FString&)
{
	FHSRBattlePresentationResolver Resolver;
	FHSRBattlePresentationEvent Event;
	Event.EventId = FGuid(18, 3, 0, 1);
	Event.EventType = EHSRPresentationEventType::Damage;

	FHSRBattlePresentationIntent First;
	FHSRBattlePresentationIntent Duplicate;
	TestTrue(TEXT("first event is consumed"), Resolver.Consume(Event, First));
	TestFalse(TEXT("duplicate event is ignored"), Resolver.Consume(Event, Duplicate));
	TestEqual(TEXT("only first event increments consumption count"), Resolver.GetConsumedEventCount(), 1);
	return true;
}

#endif
