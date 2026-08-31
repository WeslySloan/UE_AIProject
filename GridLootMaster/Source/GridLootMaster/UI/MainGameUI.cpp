#include "MainGameUI.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GridBoardWidget.h"
#include "DraggableItemWidget.h"
#include "../GridGameMode.h"
#include "../GridInventoryComponent.h"
#include "Kismet/GameplayStatics.h"

bool UMainGameUI::Initialize()
{
    if (!Super::Initialize()) return false;

    if (WidgetTree && !WidgetTree->RootWidget)
    {
        // 1. Root: Canvas Panel
        UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
        WidgetTree->RootWidget = RootCanvas;

        // 2. 전체 레이아웃 (가로 2분할: 왼쪽(LootPool/상태), 오른쪽(그리드))
        UHorizontalBox* HBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MainLayout"));
        UCanvasPanelSlot* HBoxSlot = RootCanvas->AddChildToCanvas(HBox);
        HBoxSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
        HBoxSlot->SetOffsets(FMargin(50.0f, 50.0f, 50.0f, 50.0f)); // 여백

        // === 왼쪽 패널 (상태바 + 대기열 + 버튼) ===
        UVerticalBox* LeftPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LeftPanel"));
        UHorizontalBoxSlot* LeftPanelSlot = HBox->AddChildToHorizontalBox(LeftPanel);
        LeftPanelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

        // Timer Text
        TimerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TimerText"));
        TimerText->SetText(FText::FromString(TEXT("Time: 60s")));
        TimerText->Font.Size = 24;
        LeftPanel->AddChildToVerticalBox(TimerText);

        // Score Text
        ScoreText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ScoreText"));
        ScoreText->SetText(FText::FromString(TEXT("Score: 0 / 1000")));
        ScoreText->Font.Size = 24;
        LeftPanel->AddChildToVerticalBox(ScoreText);

        // 빈 공간
        UBorder* Spacer1 = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Spacer1->SetBrushColor(FLinearColor::Transparent);
        UVerticalBoxSlot* SpacerSlot1 = LeftPanel->AddChildToVerticalBox(Spacer1);
        SpacerSlot1->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        SpacerSlot1->SetPadding(FMargin(0, 20));

        // 대기열 제목
        UTextBlock* PoolTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        PoolTitle->SetText(FText::FromString(TEXT("Loot Pool (Drag items to bag)")));
        LeftPanel->AddChildToVerticalBox(PoolTitle);

        // Loot Pool (WrapBox)
        LootPoolBox = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("LootPoolBox"));
        UVerticalBoxSlot* PoolSlot = LeftPanel->AddChildToVerticalBox(LootPoolBox);
        PoolSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); // 남은 공간 모두 차지

        // Sell Button
        UButton* SellBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SellButton"));
        UTextBlock* SellBtnText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        SellBtnText->SetText(FText::FromString(TEXT("SELL BAG")));
        SellBtnText->SetColorAndOpacity(FLinearColor::Black);
        SellBtn->AddChild(SellBtnText);
        SellBtn->OnClicked.AddDynamic(this, &UMainGameUI::OnSellButtonClicked);
        
        UVerticalBoxSlot* SellSlot = LeftPanel->AddChildToVerticalBox(SellBtn);
        SellSlot->SetPadding(FMargin(0, 20, 0, 0));
        SellSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); // 고정 크기

        // === 오른쪽 패널 (그리드 보드) ===
        GridBoard = WidgetTree->ConstructWidget<UGridBoardWidget>(UGridBoardWidget::StaticClass(), TEXT("GridBoard"));
        UHorizontalBoxSlot* RightPanelSlot = HBox->AddChildToHorizontalBox(GridBoard);
        RightPanelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

        // 게임모드에서 인벤토리 컴포넌트 연결
        AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
        if (GM && GM->InventoryComponent)
        {
            GridBoard->InventoryComponent = GM->InventoryComponent;
            GM->InventoryComponent->OnInventoryChanged.AddDynamic(GridBoard, &UGridBoardWidget::RefreshGridUI);
            
            // 초기 빈 그리드 표시를 위해 수동으로 한 번 호출
            GridBoard->RefreshGridUI();
        }
    }
    return true;
}

void UMainGameUI::UpdateScore(int32 NewScore)
{
    if (ScoreText)
    {
        ScoreText->SetText(FText::FromString(FString::Printf(TEXT("Score: %d / 1000"), NewScore)));
    }
}

void UMainGameUI::UpdateTimer(float RemainingTime)
{
    if (TimerText)
    {
        TimerText->SetText(FText::FromString(FString::Printf(TEXT("Time: %d s"), FMath::FloorToInt(RemainingTime))));
    }
}

void UMainGameUI::ShowGameResult(bool bIsWin)
{
    if (TimerText)
    {
        FString ResultStr = bIsWin ? TEXT("YOU WIN!") : TEXT("GAME OVER!");
        TimerText->SetText(FText::FromString(ResultStr));
        TimerText->SetColorAndOpacity(bIsWin ? FLinearColor::Green : FLinearColor::Red);
    }
}

void UMainGameUI::AddItemToLootPool(FName ItemID, FIntPoint Size, int32 Value)
{
    if (LootPoolBox)
    {
        UDraggableItemWidget* NewItem = CreateWidget<UDraggableItemWidget>(this, UDraggableItemWidget::StaticClass());
        if (NewItem)
        {
            NewItem->ItemID = ItemID;
            NewItem->ItemSize = Size;
            NewItem->Value = Value;
            NewItem->InitWidgetUI();
            
            if (UWrapBoxSlot* WrapSlot = Cast<UWrapBoxSlot>(LootPoolBox->AddChildToWrapBox(NewItem)))
            {
                WrapSlot->SetPadding(FMargin(5.0f));
                WrapSlot->SetHorizontalAlignment(HAlign_Left);
                WrapSlot->SetVerticalAlignment(VAlign_Top);
            }
        }
    }
}

void UMainGameUI::OnSellButtonClicked()
{
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (GM && GM->InventoryComponent)
    {
        // 간단한 방식: 꽉 차있는 칸 1칸당 10점으로 임시 계산, 혹은 아이템 ID를 통해 가치 계산
        // 여기선 꽉 차있는 칸 개수 * 10점
        int32 TotalValue = 0;
        for (FName id : GM->InventoryComponent->GridCells)
        {
            if (id != NAME_None) TotalValue += 10;
        }

        if (TotalValue > 0)
        {
            GM->AddScore(TotalValue);
            GM->InventoryComponent->ClearInventory();
        }
    }
}
