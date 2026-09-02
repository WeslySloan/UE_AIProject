#include "MinimapWidget.h"
#include "MinimapTileWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UMinimapWidget::NativeConstruct()
{
    Super::NativeConstruct();
}

void UMinimapWidget::InitMinimap(UMapManagerComponent* InMapManager)
{
    MapManager = InMapManager;
    CurrentPlayerCoord = FIntPoint(0, 0); // 시작 위치
    CurrentMoveProgress = 0;

    if (!WidgetTree)
    {
        WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
    }

    if (!MapManager) return;

    UVerticalBox* RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    WidgetTree->RootWidget = RootBox;

    // 맵 렌더링을 위한 패널
    MapGridPanel = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
    MapGridPanel->SetSlotPadding(FMargin(1.0f));
    
    if (UVerticalBoxSlot* MapSlot = Cast<UVerticalBoxSlot>(RootBox->AddChild(MapGridPanel)))
    {
        MapSlot->SetHorizontalAlignment(HAlign_Center);
        MapSlot->SetVerticalAlignment(VAlign_Center);
        MapSlot->SetPadding(FMargin(20.0f));
    }

    // 전진 버튼 생성
    AdvanceButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    AdvanceButton->OnClicked.AddDynamic(this, &UMinimapWidget::OnAdvanceClicked);
    AdvanceButton->SetVisibility(ESlateVisibility::Hidden);

    AdvanceText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    AdvanceText->SetText(FText::FromString(TEXT("전진 (0/3)")));
    AdvanceButton->AddChild(AdvanceText);

    if (UVerticalBoxSlot* BtnSlot = Cast<UVerticalBoxSlot>(RootBox->AddChild(AdvanceButton)))
    {
        BtnSlot->SetHorizontalAlignment(HAlign_Center);
        BtnSlot->SetPadding(FMargin(10.0f));
    }

    // 타일 위젯 생성
    TileWidgets.Empty();
    for (const FTileData& TileData : MapManager->MapGrid)
    {
        UMinimapTileWidget* TileWidget = WidgetTree->ConstructWidget<UMinimapTileWidget>(UMinimapTileWidget::StaticClass());
        TileWidget->InitTile(TileData, this);

        UUniformGridSlot* GridSlot = MapGridPanel->AddChildToUniformGrid(TileWidget, TileData.Coordinate.Y, TileData.Coordinate.X);
        GridSlot->SetHorizontalAlignment(HAlign_Fill);
        GridSlot->SetVerticalAlignment(VAlign_Fill);

        TileWidgets.Add(TileData.Coordinate, TileWidget);
    }

    MovePlayerTo(CurrentPlayerCoord);
}

void UMinimapWidget::HandleTileClicked(FIntPoint ClickedCoord)
{
    if (CurrentMoveProgress > 0)
    {
        // 이동 도중에는 경로 변경 불가 (기획에 따라 다를 수 있으나 심플하게 처리)
        // 일단 이동 도중 경로 변경 허용하려면 아래 리턴 제거
        // return;
    }

    // A* 길찾기로 경로 계산
    CurrentPath = MapManager->FindPath(CurrentPlayerCoord, ClickedCoord);
    
    if (CurrentPath.Num() > 0)
    {
        CurrentTargetCoord = ClickedCoord;
        CurrentMoveProgress = 0;
        AdvanceButton->SetVisibility(ESlateVisibility::Visible);
        UpdateAdvanceButtonText();
    }
    else
    {
        AdvanceButton->SetVisibility(ESlateVisibility::Hidden);
    }

    UpdatePathHighlight();
}

void UMinimapWidget::OnAdvanceClicked()
{
    if (CurrentPath.Num() == 0) return;

    CurrentMoveProgress++;
    if (CurrentMoveProgress >= 3)
    {
        // 3턴 경과 시 다음 칸으로 이동
        CurrentMoveProgress = 0;
        MovePlayerTo(CurrentPath[0]); // Path의 첫번째가 다음 칸
        CurrentPath.RemoveAt(0);

        if (CurrentPath.Num() == 0)
        {
            // 최종 도착
            AdvanceButton->SetVisibility(ESlateVisibility::Hidden);
        }
        UpdatePathHighlight();
    }
    
    UpdateAdvanceButtonText();
    
    // TODO: 여기서 1턴 경과 이벤트(허기 감소, 랜덤 인카운터 굴림 등) 호출
}

void UMinimapWidget::UpdateAdvanceButtonText()
{
    if (AdvanceText)
    {
        FString TextStr = FString::Printf(TEXT("전진 (%d/3)"), CurrentMoveProgress);
        AdvanceText->SetText(FText::FromString(TextStr));
    }
}

void UMinimapWidget::UpdatePathHighlight()
{
    // 모든 타일 하이라이트 초기화
    for (auto& Pair : TileWidgets)
    {
        Pair.Value->SetIsPath(false);
    }

    // 새로운 경로 하이라이트
    for (const FIntPoint& PathCoord : CurrentPath)
    {
        if (UMinimapTileWidget** FoundTile = TileWidgets.Find(PathCoord))
        {
            (*FoundTile)->SetIsPath(true);
        }
    }
}

void UMinimapWidget::MovePlayerTo(FIntPoint NewCoord)
{
    // 기존 위치 플레이어 아이콘 숨김
    if (UMinimapTileWidget** OldTile = TileWidgets.Find(CurrentPlayerCoord))
    {
        (*OldTile)->SetHasPlayer(false);
    }

    CurrentPlayerCoord = NewCoord;

    // 새 위치 플레이어 아이콘 표시
    if (UMinimapTileWidget** NewTile = TileWidgets.Find(CurrentPlayerCoord))
    {
        (*NewTile)->SetHasPlayer(true);
    }
}
