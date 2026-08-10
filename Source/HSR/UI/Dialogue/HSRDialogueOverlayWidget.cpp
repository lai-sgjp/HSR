#include "HSRDialogueOverlayWidget.h"

#include "HSRDialoguePresentationViewModel.h"
#include "../../UI/HSRUIManagerSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"

UHSRDialogueOverlayWidget::UHSRDialogueOverlayWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UHSRDialogueOverlayWidget::SetOwningUIManager(UHSRUIManagerSubsystem* InManager)
{
	OwningUIManager = InManager;
}

void UHSRDialogueOverlayWidget::SetViewModel(UHSRDialoguePresentationViewModel* InViewModel)
{
	if (ViewModel == InViewModel)
	{
		return;
	}

	if (ViewModel)
	{
		ViewModel->OnSnapshotChanged.RemoveDynamic(this, &ThisClass::HandleViewModelSnapshotChanged);
	}

	ViewModel = InViewModel;
	if (ViewModel)
	{
		ViewModel->OnSnapshotChanged.AddUniqueDynamic(this, &ThisClass::HandleViewModelSnapshotChanged);
		FHSRDialoguePresentationSnapshot Snapshot;
		if (ViewModel->GetSnapshot(Snapshot))
		{
			HandleViewModelSnapshotChanged(Snapshot);
		}
	}
}

bool UHSRDialogueOverlayWidget::GetSnapshot(FHSRDialoguePresentationSnapshot& OutSnapshot) const
{
	OutSnapshot = FHSRDialoguePresentationSnapshot();
	return ViewModel && ViewModel->GetSnapshot(OutSnapshot);
}

int32 UHSRDialogueOverlayWidget::GetChoiceCount() const
{
	FHSRDialoguePresentationSnapshot Snapshot;
	return GetSnapshot(Snapshot) ? Snapshot.Choices.Num() : 0;
}

bool UHSRDialogueOverlayWidget::GetChoiceAt(const int32 Index,
	FHSRDialoguePresentationChoice& OutChoice) const
{
	OutChoice = FHSRDialoguePresentationChoice();
	FHSRDialoguePresentationSnapshot Snapshot;
	if (!GetSnapshot(Snapshot) || !Snapshot.Choices.IsValidIndex(Index))
	{
		return false;
	}
	OutChoice = Snapshot.Choices[Index];
	return true;
}

FText UHSRDialogueOverlayWidget::GetSpeakerText() const
{
	FHSRDialoguePresentationSnapshot Snapshot;
	return GetSnapshot(Snapshot) ? Snapshot.SpeakerText : FText::GetEmpty();
}

FText UHSRDialogueOverlayWidget::GetBodyText() const
{
	FHSRDialoguePresentationSnapshot Snapshot;
	return GetSnapshot(Snapshot) ? Snapshot.BodyText : FText::GetEmpty();
}

bool UHSRDialogueOverlayWidget::GetChoiceDisplayText(const int32 Index, FText& OutText) const
{
	FHSRDialoguePresentationChoice Choice;
	if (!GetChoiceAt(Index, Choice))
	{
		OutText = FText::GetEmpty();
		return false;
	}
	OutText = Choice.DisplayText;
	return true;
}

bool UHSRDialogueOverlayWidget::GetChoiceEnabled(const int32 Index, bool& bOutEnabled) const
{
	FHSRDialoguePresentationChoice Choice;
	if (!GetChoiceAt(Index, Choice))
	{
		bOutEnabled = false;
		return false;
	}
	bOutEnabled = Choice.bEnabled;
	return true;
}

EHSRDialoguePresentationResult UHSRDialogueOverlayWidget::SubmitChoiceByIndex(const int32 Index)
{
	FHSRDialoguePresentationSnapshot Snapshot;
	FHSRDialoguePresentationChoice Choice;
	if (!GetSnapshot(Snapshot) || !Snapshot.Choices.IsValidIndex(Index)
		|| !GetChoiceAt(Index, Choice))
	{
		return EHSRDialoguePresentationResult::InvalidChoice;
	}

	FHSRDialoguePresentationChoiceRequest Request;
	Request.QueryId = Snapshot.QueryId;
	Request.DialogueId = Snapshot.DialogueId;
	Request.NodeId = Snapshot.NodeId;
	Request.ChoiceId = Choice.ChoiceId;
	const EHSRDialoguePresentationResult Result = ViewModel->SubmitChoice(Request);

	FHSRDialoguePresentationSnapshot After;
	if (Result == EHSRDialoguePresentationResult::Success
		&& ViewModel->GetSnapshot(After)
		&& After.Status == EHSRDialoguePresentationStatus::Closed
		&& OwningUIManager.IsValid())
	{
		OwningUIManager->CloseDialogueOverlay();
	}
	return Result;
}

EHSRUIScreenResult UHSRDialogueOverlayWidget::RequestCloseDialogue()
{
	return OwningUIManager.IsValid()
		? OwningUIManager->CloseDialogueOverlay()
		: EHSRUIScreenResult::InvalidHost;
}

void UHSRDialogueOverlayWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
}

void UHSRDialogueOverlayWidget::NativeDestruct()
{
	if (ViewModel)
	{
		ViewModel->OnSnapshotChanged.RemoveDynamic(this, &ThisClass::HandleViewModelSnapshotChanged);
	}
	ViewModel = nullptr;
	OwningUIManager.Reset();
	Super::NativeDestruct();
}

FReply UHSRDialogueOverlayWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if ((IsBackKey(Key) || IsCloseToRootKey(Key)) && OwningUIManager.IsValid())
	{
		OwningUIManager->CloseDialogueOverlay();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UHSRDialogueOverlayWidget::HandleViewModelSnapshotChanged(
	const FHSRDialoguePresentationSnapshot& Snapshot)
{
	OnDialogueSnapshotChanged(Snapshot);
	PopulateView();
}

void UHSRDialogueOverlayWidget::PopulateView()
{
	UWidgetTree* Tree = WidgetTree;
	if (!Tree) return;

	if (UTextBlock* Speaker = FindTextByName(TEXT("TXT_Speaker")))
	{
		Speaker->SetText(GetSpeakerText());
	}
	if (UTextBlock* Body = FindTextByName(TEXT("TXT_Body")))
	{
		Body->SetText(GetBodyText());
	}

	const int32 Count = GetChoiceCount();
	for (int32 SlotIndex = 0; SlotIndex < 4; ++SlotIndex)
	{
		UButton* Choice = FindButtonByName(*FString::Printf(TEXT("ChoiceButton_%d"), SlotIndex));
		if (!Choice) continue;
		UTextBlock* Label = FindTextByName(*FString::Printf(TEXT("TXT_Choice_%d"), SlotIndex));
		if (SlotIndex < Count)
		{
			Choice->SetVisibility(ESlateVisibility::Visible);
			bool bEnabled = true;
			GetChoiceEnabled(SlotIndex, bEnabled);
			Choice->SetIsEnabled(bEnabled);
			FText DisplayText;
			if (GetChoiceDisplayText(SlotIndex, DisplayText) && Label)
			{
				Label->SetText(DisplayText);
			}
		}
		else
		{
			Choice->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

UButton* UHSRDialogueOverlayWidget::FindButtonByName(const FName Name) const
{
	return WidgetTree ? WidgetTree->FindWidget<UButton>(Name) : nullptr;
}

UTextBlock* UHSRDialogueOverlayWidget::FindTextByName(const FName Name) const
{
	return WidgetTree ? WidgetTree->FindWidget<UTextBlock>(Name) : nullptr;
}

bool UHSRDialogueOverlayWidget::IsBackKey(const FKey& Key)
{
	return Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right;
}

bool UHSRDialogueOverlayWidget::IsCloseToRootKey(const FKey& Key)
{
	return Key == EKeys::X;
}

#if WITH_DEV_AUTOMATION_TESTS
bool UHSRDialogueOverlayWidget::ShouldConsumeBackKeyForAutomation(const FKey& Key) const
{
	return IsBackKey(Key);
}

bool UHSRDialogueOverlayWidget::ShouldConsumeCloseToRootKeyForAutomation(const FKey& Key) const
{
	return IsCloseToRootKey(Key);
}
#endif
