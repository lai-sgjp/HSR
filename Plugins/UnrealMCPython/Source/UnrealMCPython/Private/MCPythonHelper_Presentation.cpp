#include "MCPythonHelper.h"
#include "Editor.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"
#include "UObject/StrongObjectPtr.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

namespace
{
    TStrongObjectPtr<ULevelEditorPlaySettings> PresentationPlaySettings;
    TSharedPtr<SWindow> PresentationWindow;
}

bool UMCPythonHelper::BeginPresentationPIE(int32 Width, int32 Height)
{
    if (!GEditor || GEditor->PlayWorld || Width < 640 || Width > 3840 || Height < 360 || Height > 2160) return false;
    PresentationPlaySettings.Reset(DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage()));
    PresentationPlaySettings->NewWindowWidth = Width;
    PresentationPlaySettings->NewWindowHeight = Height;
    PresentationPlaySettings->CenterNewWindow = false;
    FRequestPlaySessionParams Params;
    Params.EditorPlaySettings = PresentationPlaySettings.Get();
    Params.bAllowOnlineSubsystem = false;
    PresentationWindow = SNew(SWindow)
        .Title(FText::FromString(TEXT("HSR Presentation Verification")))
        .ClientSize(FVector2D(Width, Height)).ScreenPosition(FVector2D::ZeroVector)
        .AutoCenter(EAutoCenter::None).SaneWindowPlacement(false)
        .CreateTitleBar(false).UseOSWindowBorder(false).LayoutBorder(FMargin(0))
        .SizingRule(ESizingRule::FixedSize).AdjustInitialSizeAndPositionForDPIScale(false);
    FSlateApplication::Get().AddWindow(PresentationWindow.ToSharedRef());
    Params.CustomPIEWindow = PresentationWindow;
    GEditor->RequestPlaySession(Params);
    return true;
}

FString UMCPythonHelper::CapturePlayViewport(const FString& FileName)
{
    if (!GEditor || !GEditor->PlayWorld || FileName.IsEmpty() || FileName.Contains(TEXT(".."))
        || FileName.Contains(TEXT("/")) || FileName.Contains(TEXT("\\")) || FileName.Contains(TEXT(":"))) return FString();
    UGameViewportClient* Client = GEditor->PlayWorld->GetGameViewport();
    const auto Viewport = Client ? Client->GetGameViewportWidget() : nullptr;
    if (!Viewport.IsValid()) return FString();
    TArray<FColor> Pixels;
    FIntVector Size;
    if (!FSlateApplication::Get().TakeScreenshot(Viewport.ToSharedRef(), Pixels, Size) || Size.X <= 0 || Size.Y <= 0) return FString();
    TArray64<uint8> Compressed;
    FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Compressed);
    const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Presentation/Captures"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Path = Directory / (FileName + TEXT(".png"));
    return FFileHelper::SaveArrayToFile(Compressed, *Path) ? Path : FString();
}
