// Development-only, explicit console opt-in. This drives ordinary movement for
// repeatable packaged-build measurements; it never alters rewards or saves.
#if !UE_BUILD_SHIPPING
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace HSRPresentationBenchmark
{
static bool bRunning = false;
void Start(const TArray<FString>&, UWorld* World)
{
	if (bRunning || !World || !World->GetMapName().Contains(TEXT("NewEriduSixthStreetMetro"))) return;
	bRunning = true;
	struct FRun
	{
		TWeakObjectPtr<UWorld> World;
		double Began = FPlatformTime::Seconds();
		double Last = Began;
		double Measured = 0;
		int32 Target = 0;
		int32 Reached = 0;
		bool bCapturing = false;
		TArray<double> FrameMs;
	};
	auto Run = MakeShared<FRun>(); Run->World = World;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float)
	{
		UWorld* ActiveWorld = Run->World.Get();
		if (!ActiveWorld) { bRunning = false; return false; }
		APlayerController* PC = ActiveWorld->GetFirstPlayerController();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		const double Now = FPlatformTime::Seconds();
		const double FrameSeconds = Now - Run->Last; Run->Last = Now;
		if (!Pawn || Now - Run->Began < 10) return true;
		if (!Run->bCapturing)
		{
			PC->ConsoleCommand(TEXT("csvprofile start"), false);
			PC->SetControlRotation(FRotator(-10, 90, 0));
			Run->bCapturing = true;
			return true;
		}
		Run->Measured += FrameSeconds;
		Run->FrameMs.Add(FrameSeconds * 1000);
		const FVector Points[] = { FVector(0,-9000,0), FVector(0,-3500,0), FVector(3500,-3500,0),
			FVector(3500,0,0), FVector(10000,0,0), FVector(3500,0,0), FVector(3500,-3500,0), FVector(0,-3500,0) };
		FVector Direction = Points[Run->Target] - Pawn->GetActorLocation(); Direction.Z = 0;
		if (Direction.SizeSquared() < FMath::Square(120.f))
		{
			Run->Target = (Run->Target + 1) % UE_ARRAY_COUNT(Points); ++Run->Reached;
		}
		else Pawn->AddMovementInput(Direction.GetSafeNormal());
		if (Run->Measured < 60) return true;
		PC->ConsoleCommand(TEXT("csvprofile stop"), false);
		Run->FrameMs.Sort();
		auto Report = MakeShared<FJsonObject>();
		Report->SetStringField(TEXT("map"), ActiveWorld->GetMapName());
		Report->SetNumberField(TEXT("seconds"), Run->Measured);
		Report->SetNumberField(TEXT("frames"), Run->FrameMs.Num());
		Report->SetNumberField(TEXT("average_fps"), Run->FrameMs.Num() / Run->Measured);
		Report->SetNumberField(TEXT("p95_frame_ms"), Run->FrameMs[FMath::Min(Run->FrameMs.Num()-1, FMath::FloorToInt(Run->FrameMs.Num() * .95))]);
		Report->SetNumberField(TEXT("waypoints_reached"), Run->Reached);
		Report->SetStringField(TEXT("final_position"), Pawn->GetActorLocation().ToString());
		Report->SetStringField(TEXT("method"), TEXT("60s real frame intervals after 10s warmup; ordinary Pawn movement; CSV profiler also captured"));
		const FString Folder = FPaths::ProjectSavedDir() / TEXT("Presentation/Performance");
		IFileManager::Get().MakeDirectory(*Folder, true);
		FString Json; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
		FFileHelper::SaveStringToFile(Json, *(Folder / TEXT("packaged_route.json")));
		UE_LOG(LogTemp, Display, TEXT("HSR Presentation benchmark complete: %.2f FPS, %d waypoints"), Run->FrameMs.Num()/Run->Measured, Run->Reached);
		bRunning = false;
		return false;
	}));
}
static FAutoConsoleCommandWithWorldAndArgs Command(TEXT("HSR.PresentationBenchmark"),
	TEXT("Run the fixed city route for 60 seconds after warmup and record local frame times."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Start));
}
#endif
