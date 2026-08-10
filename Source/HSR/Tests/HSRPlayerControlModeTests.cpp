#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../Battle/HSRBattleGameMode.h"
#include "../Framework/HSRGameModeBase.h"
#include "../Player/HSRPlayerController.h"
#include "../UI/HSRScreenStackTypes.h"

/**
 * Guards the control-mode -> input-policy mapping. This regressed once already: the derivation
 * was a pair of either/or expressions that only tested for UIOnly, so Battle fell through to the
 * Exploration branch and resolved to GameOnly with a hidden cursor. Setting Battle mode was
 * therefore a no-op, and the mouse kept rotating the exploration camera with no visible cursor.
 *
 * The mapping is static and world-free, so it is asserted directly rather than through a PIE
 * session -- the defect was pure logic and never needed a world to reproduce.
 */
namespace HSR::Input::Tests
{
	static FHSRInputModePolicy PolicyFor(const EHSRPlayerControlMode Mode)
	{
		FHSRInputModePolicy Policy;
		AHSRPlayerController::BuildPolicyForControlMode(Mode, Policy);
		return Policy;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRPlayerControlModePolicyTest, "HSR.Player.ControlMode.Policy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRPlayerControlModePolicyTest::RunTest(const FString&)
{
	using namespace HSR::Input::Tests;

	const FHSRInputModePolicy Exploration = PolicyFor(EHSRPlayerControlMode::Exploration);
	TestEqual(TEXT("exploration routes input to the game only"),
		Exploration.InputIntent, EHSRUIInputIntent::GameOnly);
	TestFalse(TEXT("exploration hides the cursor"), Exploration.bShowMouseCursor);

	// The battle command panel needs clicks, so the cursor must be visible and UI must receive
	// input. GameAndUI rather than UIOnly because the battle world still runs game input.
	const FHSRInputModePolicy Battle = PolicyFor(EHSRPlayerControlMode::Battle);
	TestEqual(TEXT("battle routes input to both game and UI"),
		Battle.InputIntent, EHSRUIInputIntent::GameAndUI);
	TestTrue(TEXT("battle shows the cursor so the command panel is clickable"),
		Battle.bShowMouseCursor);

	const FHSRInputModePolicy UIOnly = PolicyFor(EHSRPlayerControlMode::UIOnly);
	TestEqual(TEXT("UIOnly withholds input from the pawn"),
		UIOnly.InputIntent, EHSRUIInputIntent::UIOnly);
	TestTrue(TEXT("UIOnly shows the cursor"), UIOnly.bShowMouseCursor);

	// The actual regression, stated as its own assertion: Battle must not be a silent alias for
	// Exploration. Comparing the two directly fails loudly if a future mode is added to the enum
	// without a matching switch case and falls into the default branch.
	TestNotEqual(TEXT("battle intent differs from exploration"),
		Battle.InputIntent, Exploration.InputIntent);
	TestNotEqual(TEXT("battle cursor visibility differs from exploration"),
		Battle.bShowMouseCursor, Exploration.bShowMouseCursor);

	return true;
}

/**
 * Guards the other half of the same failure, which the policy test above cannot see. The mapping
 * was correct and every assertion passed while battle input stayed broken, because the battle
 * GameMode never named AHSRPlayerController: PlayerControllerClass was set only in
 * BP_HSRGameMode's asset, and the battle GameMode Blueprint simply omitted it. The battle world
 * therefore spawned the engine's APlayerController and no HSR control-mode code ran at all.
 *
 * Asserted against the CDO rather than a live world: the defect is a class default, so it
 * reproduces without PIE, and a world-based test would need a map load to say the same thing.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRGameModeControllerClassTest, "HSR.Player.ControlMode.GameModeControllerClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRGameModeControllerClassTest::RunTest(const FString&)
{
	const AHSRBattleGameMode* BattleDefaults = GetDefault<AHSRBattleGameMode>();
	TestNotNull(TEXT("battle GameMode CDO resolves"), BattleDefaults);
	if (BattleDefaults)
	{
		TestEqual(TEXT("battle GameMode spawns the HSR player controller"),
			BattleDefaults->PlayerControllerClass.Get(),
			static_cast<UClass*>(AHSRPlayerController::StaticClass()));
	}

	const AHSRGameModeBase* ExplorationDefaults = GetDefault<AHSRGameModeBase>();
	TestNotNull(TEXT("exploration GameMode CDO resolves"), ExplorationDefaults);
	if (ExplorationDefaults)
	{
		TestEqual(TEXT("exploration GameMode spawns the HSR player controller"),
			ExplorationDefaults->PlayerControllerClass.Get(),
			static_cast<UClass*>(AHSRPlayerController::StaticClass()));
	}

	return true;
}

#endif
