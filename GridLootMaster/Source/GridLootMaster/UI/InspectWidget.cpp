#include "InspectWidget.h"
#include "ModSlotWidget.h"
#include "../ItemInstance.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"

bool UInspectWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (WidgetTree && !WidgetTree->RootWidget)
    {
        UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
        WidgetTree->RootWidget = RootCanvas;

        // 반투명 배경
        UBorder* BlockingBG = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        BlockingBG->SetBrushColor(FLinearColor(0, 0, 0, 0.7f));
        UCanvasPanelSlot* BGSlot = RootCanvas->AddChildToCanvas(BlockingBG);
        BGSlot->SetAnchors(FAnchors(0, 0, 1, 1));
        BGSlot->SetOffsets(FMargin(0, 0, 0, 0));

        // 중앙 창
        UBorder* WindowBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        WindowBorder->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.05f, 1.0f));
        UCanvasPanelSlot* WindowSlot = RootCanvas->AddChildToCanvas(WindowBorder);
        WindowSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
        WindowSlot->SetAlignment(FVector2D(0.5f, 0.5f));
        WindowSlot->SetSize(FVector2D(400.0f, 300.0f));

        MainBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        WindowBorder->AddChild(MainBox);

        // 상단 바 (제목 + 닫기 버튼)
        UHorizontalBox* TopBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        MainBox->AddChild(TopBar);

        TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        TitleText->SetText(FText::FromString("Item Name"));
        TitleText->SetFont(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16));
        TopBar->AddChild(TitleText);

        UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
        UTextBlock* CloseText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        CloseText->SetText(FText::FromString("X"));
        CloseText->SetColorAndOpacity(FLinearColor::Black);
        CloseButton->AddChild(CloseText);
        CloseButton->OnClicked.AddDynamic(this, &UInspectWidget::OnCloseClicked);
        TopBar->AddChild(CloseButton);

        // 설명
        DescText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        MainBox->AddChild(DescText);

        // 모딩 슬롯 영역
        ModBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        MainBox->AddChild(ModBox);
    }
    return true;
}

void UInspectWidget::Setup(UItemInstance* InItemObj)
{
    TargetItem = InItemObj;
    if (!TargetItem) return;

    if (TitleText)
    {
        TitleText->SetText(FText::FromString(TargetItem->ItemName));
    }

    if (DescText)
    {
        FString Desc = FString::Printf(TEXT("Category: %d\nRarity: %d\nSize: %dx%d"), 
            (int32)TargetItem->Category, (int32)TargetItem->Rarity, 
            TargetItem->BaseSize.X, TargetItem->BaseSize.Y);
        DescText->SetText(FText::FromString(Desc));
    }

    if (ModBox)
    {
        ModBox->ClearChildren();

        if (TargetItem->Category == EItemCategory::Weapon)
        {
            SightSlot = WidgetTree->ConstructWidget<UModSlotWidget>(UModSlotWidget::StaticClass());
            SightSlot->Setup(TargetItem, EAttachmentType::Sight);
            USizeBox* SightBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
            SightBox->SetWidthOverride(100.0f);
            SightBox->SetHeightOverride(100.0f);
            SightBox->AddChild(SightSlot);
            if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(ModBox->AddChild(SightBox)))
            {
                HSlot->SetPadding(FMargin(0, 0, 10.0f, 0));
            }

            MuzzleSlot = WidgetTree->ConstructWidget<UModSlotWidget>(UModSlotWidget::StaticClass());
            MuzzleSlot->Setup(TargetItem, EAttachmentType::Muzzle);
            USizeBox* MuzzleBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
            MuzzleBox->SetWidthOverride(100.0f);
            MuzzleBox->SetHeightOverride(100.0f);
            MuzzleBox->AddChild(MuzzleSlot);
            if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(ModBox->AddChild(MuzzleBox)))
            {
                HSlot->SetPadding(FMargin(0, 0, 10.0f, 0));
            }

            MagazineSlot = WidgetTree->ConstructWidget<UModSlotWidget>(UModSlotWidget::StaticClass());
            MagazineSlot->Setup(TargetItem, EAttachmentType::Magazine);
            USizeBox* MagBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
            MagBox->SetWidthOverride(100.0f);
            MagBox->SetHeightOverride(100.0f);
            MagBox->AddChild(MagazineSlot);
            if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(ModBox->AddChild(MagBox)))
            {
                HSlot->SetPadding(FMargin(0, 0, 10.0f, 0));
            }
        }
    }
    
    TargetItem->bIsExamined = true;
}

void UInspectWidget::OnCloseClicked()
{
    RemoveFromParent();
}
