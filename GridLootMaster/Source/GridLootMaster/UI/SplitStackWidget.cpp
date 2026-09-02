#include "SplitStackWidget.h"
#include "Components/Slider.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"

bool USplitStackWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (WidgetTree && !WidgetTree->RootWidget)
    {
        // 최상위 캔버스 패널
        UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
        WidgetTree->RootWidget = RootCanvas;

        // 반투명한 전체 배경 (클릭 차단)
        UBorder* BlockingBG = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        BlockingBG->SetBrushColor(FLinearColor(0, 0, 0, 0.5f));
        UCanvasPanelSlot* BGSlot = RootCanvas->AddChildToCanvas(BlockingBG);
        BGSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
        BGSlot->SetOffsets(FMargin(0, 0, 0, 0));

        // 팝업 창 보더
        UBorder* WindowBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        WindowBorder->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.1f, 1.0f));
        UCanvasPanelSlot* WindowSlot = RootCanvas->AddChildToCanvas(WindowBorder);
        WindowSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
        WindowSlot->SetAlignment(FVector2D(0.5f, 0.5f));
        WindowSlot->SetSize(FVector2D(300.0f, 150.0f));

        UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        WindowBorder->AddChild(VBox);

        TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        TitleText->SetText(FText::FromString(TEXT("Split Amount")));
        TitleText->SetJustification(ETextJustify::Center);
        VBox->AddChild(TitleText);

        AmountTextBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
        AmountTextBox->OnTextChanged.AddDynamic(this, &USplitStackWidget::OnTextBoxTextChanged);
        VBox->AddChild(AmountTextBox);

        AmountSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
        AmountSlider->OnValueChanged.AddDynamic(this, &USplitStackWidget::OnSliderValueChanged);
        VBox->AddChild(AmountSlider);

        UHorizontalBox* HBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        VBox->AddChild(HBox);

        OkButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
        UTextBlock* OkText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        OkText->SetText(FText::FromString(TEXT("OK")));
        OkText->SetColorAndOpacity(FLinearColor::Black);
        OkButton->AddChild(OkText);
        OkButton->OnClicked.AddDynamic(this, &USplitStackWidget::OnOkClicked);
        HBox->AddChild(OkButton);

        CancelButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
        UTextBlock* CancelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        CancelText->SetText(FText::FromString(TEXT("Cancel")));
        CancelText->SetColorAndOpacity(FLinearColor::Black);
        CancelButton->AddChild(CancelText);
        CancelButton->OnClicked.AddDynamic(this, &USplitStackWidget::OnCancelClicked);
        HBox->AddChild(CancelButton);
    }

    return true;
}

void USplitStackWidget::Setup(int32 MaxAmount)
{
    MaxStackToSplit = MaxAmount;
    CurrentSplitAmount = MaxAmount / 2;
    if (CurrentSplitAmount < 1) CurrentSplitAmount = 1;

    if (AmountSlider)
    {
        AmountSlider->SetMinValue(1.0f);
        AmountSlider->SetMaxValue((float)MaxStackToSplit);
        AmountSlider->SetValue((float)CurrentSplitAmount);
    }

    if (AmountTextBox)
    {
        AmountTextBox->SetText(FText::FromString(FString::FromInt(CurrentSplitAmount)));
    }
}

void USplitStackWidget::OnSliderValueChanged(float Value)
{
    CurrentSplitAmount = FMath::RoundToInt(Value);
    if (AmountTextBox)
    {
        AmountTextBox->SetText(FText::FromString(FString::FromInt(CurrentSplitAmount)));
    }
}

void USplitStackWidget::OnTextBoxTextChanged(const FText& Text)
{
    if (Text.ToString().IsNumeric())
    {
        CurrentSplitAmount = FMath::Clamp(FCString::Atoi(*Text.ToString()), 1, MaxStackToSplit);
        if (AmountSlider)
        {
            AmountSlider->SetValue((float)CurrentSplitAmount);
        }
    }
}

void USplitStackWidget::OnOkClicked()
{
    OnSplitConfirmed.Broadcast(CurrentSplitAmount);
    RemoveFromParent();
}

void USplitStackWidget::OnCancelClicked()
{
    OnSplitCancelled.Broadcast();
    RemoveFromParent();
}
