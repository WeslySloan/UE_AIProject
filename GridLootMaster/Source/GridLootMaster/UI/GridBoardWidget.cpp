#include "GridBoardWidget.h"
#include "ItemDragDropOperation.h"
#include "../GridInventoryComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/TextBlock.h"
#include "DraggableItemWidget.h"
#include "../GridGameMode.h"
#include "Kismet/GameplayStatics.h"

bool UGridBoardWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (WidgetTree && !WidgetTree->RootWidget)
    {
        GridCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GridCanvas"));
        WidgetTree->RootWidget = GridCanvas;

        // 1. 전체 영역에 대한 히트 판정을 받기 위한 투명 배경 (마우스가 그리드 밖으로 조금 나가도 놓치지 않음)
        UBorder* HitTestBG = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        HitTestBG->SetBrushColor(FLinearColor::Transparent);
        UCanvasPanelSlot* HitSlot = GridCanvas->AddChildToCanvas(HitTestBG);
        HitSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
        HitSlot->SetOffsets(FMargin(0, 0, 0, 0));

        // 2. 실제 시각적인 그리드 배경 (384x512 고정)
        UBorder* BG = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        BG->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f));
        UCanvasPanelSlot* BGSlot = GridCanvas->AddChildToCanvas(BG);
        BGSlot->SetPosition(FVector2D(0.0f, 0.0f));
        // 가로 6칸, 세로 8칸 * 64픽셀 = 384 x 512
        BGSlot->SetSize(FVector2D(6 * 64.0f, 8 * 64.0f));
        
        // 미리보기용 외곽선 (평소엔 숨김)
        PreviewBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PreviewBorder"));
        PreviewBorder->SetVisibility(ESlateVisibility::Hidden);
        UCanvasPanelSlot* PreviewSlot = GridCanvas->AddChildToCanvas(PreviewBorder);
        PreviewSlot->SetZOrder(100); // 아이템들보다 항상 위에 표시
    }
    return true;
}

bool UGridBoardWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);

    if (PreviewBorder)
    {
        PreviewBorder->SetVisibility(ESlateVisibility::Hidden);
    }

    UItemDragDropOperation* ItemDropOp = Cast<UItemDragDropOperation>(InOperation);
    if (ItemDropOp && InventoryComponent)
    {
        int32 GridX = 0;
        int32 GridY = 0;

        if (GetGridCellFromMousePosition(InGeometry, InDragDropEvent, ItemDropOp, GridX, GridY))
        {
            int32 Width = ItemDropOp->bIsRotated ? ItemDropOp->ItemSize.Y : ItemDropOp->ItemSize.X;
            int32 Height = ItemDropOp->bIsRotated ? ItemDropOp->ItemSize.X : ItemDropOp->ItemSize.Y;

            if (InventoryComponent->CheckItemFit(ItemDropOp->ItemID, GridX, GridY, Width, Height))
            {
                // 드래그 전의 기존 위치에서 아이템 제거 (가방 -> 가방, 혹은 상자 -> 가방 이동 시 중복 방지)
                AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
                if (GM)
                {
                    if (GM->InventoryComponent) GM->InventoryComponent->RemoveItem(ItemDropOp->ItemID);
                    if (GM->LootContainerComponent) GM->LootContainerComponent->RemoveItem(ItemDropOp->ItemID);
                }
                else
                {
                    InventoryComponent->RemoveItem(ItemDropOp->ItemID);
                }
                
                if (InventoryComponent->AddItem(ItemDropOp->ItemID, GridX, GridY, Width, Height, ItemDropOp->Rarity, true))
                {
                    // 성공 시 원본 UI를 화면(대기열)에서 제거
                    if (ItemDropOp->OriginalWidget)
                    {
                        ItemDropOp->OriginalWidget->RemoveFromParent();
                    }
                    RefreshGridUI();
                    return true;
                }
            }
        }
    }
    return false;
}

void UGridBoardWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);
    // 아무 처리도 하지 않지만, 이벤트를 받아들이기 위해 오버라이드
}

bool UGridBoardWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    Super::NativeOnDragOver(InGeometry, InDragDropEvent, InOperation);

    UItemDragDropOperation* ItemDropOp = Cast<UItemDragDropOperation>(InOperation);
    if (ItemDropOp && InventoryComponent && PreviewBorder)
    {
        int32 GridX = 0;
        int32 GridY = 0;

        if (GetGridCellFromMousePosition(InGeometry, InDragDropEvent, ItemDropOp, GridX, GridY))
        {
            int32 Width = ItemDropOp->bIsRotated ? ItemDropOp->ItemSize.Y : ItemDropOp->ItemSize.X;
            int32 Height = ItemDropOp->bIsRotated ? ItemDropOp->ItemSize.X : ItemDropOp->ItemSize.Y;

            bool bFits = InventoryComponent->CheckItemFit(ItemDropOp->ItemID, GridX, GridY, Width, Height);

            PreviewBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
            PreviewBorder->SetBrushColor(bFits ? FLinearColor(0.0f, 1.0f, 0.0f, 0.4f) : FLinearColor(1.0f, 0.0f, 0.0f, 0.4f));

            if (UCanvasPanelSlot* PreviewSlot = Cast<UCanvasPanelSlot>(PreviewBorder->Slot))
            {
                PreviewSlot->SetPosition(FVector2D(GridX * 64.0f, GridY * 64.0f));
                PreviewSlot->SetSize(FVector2D(Width * 64.0f, Height * 64.0f));
            }
            return true;
        }
    }
    return false;
}

void UGridBoardWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    Super::NativeOnDragLeave(InDragDropEvent, InOperation);
    
    if (PreviewBorder)
    {
        PreviewBorder->SetVisibility(ESlateVisibility::Hidden);
    }
}

bool UGridBoardWidget::GetGridCellFromMousePosition(const FGeometry& Geometry, const FPointerEvent& PointerEvent, UItemDragDropOperation* Operation, int32& OutX, int32& OutY)
{
    // 마우스 커서의 스크린 좌표
    FVector2D MouseScreenPos = PointerEvent.GetScreenSpacePosition();
    
    // 드래그 중인 비주얼의 실제 '왼쪽 상단(Top-Left)' 스크린 좌표 계산
    FVector2D VisualTopLeftScreenPos = MouseScreenPos;
    if (Operation)
    {
        VisualTopLeftScreenPos -= Operation->MouseOffset;
    }

    // 그리드 위젯 내부의 로컬 좌표로 변환
    FVector2D LocalPos = Geometry.AbsoluteToLocal(VisualTopLeftScreenPos);
    const float CellSize = 64.0f; 
    
    OutX = FMath::FloorToInt(LocalPos.X / CellSize);
    OutY = FMath::FloorToInt(LocalPos.Y / CellSize);
    return true;
}

void UGridBoardWidget::RefreshGridUI()
{
    if (!GridCanvas || !InventoryComponent) return;

    // 기존에 추가했던 아이템 위젯들과 그리드 타일 제거
    // 초기에 추가된 3개의 배경(HitTestBG, BG, PreviewBorder)은 남겨둬야 함.
    // 역순으로 순회하며 안전하게 제거
    for (int32 i = GridCanvas->GetChildrenCount() - 1; i >= 0; --i)
    {
        UWidget* Child = GridCanvas->GetChildAt(i);
        // 처음 만들어진 3개 위젯은 지우지 않음 (이름으로 구분하거나 인덱스 0,1은 고정)
        if (Child->GetName() == TEXT("PreviewBorder") || i <= 1)
        {
            continue;
        }
        GridCanvas->RemoveChildAt(i);
    }

    // 1. 빈 그리드 타일(칸) 먼저 모두 그리기
    for (int Y = 0; Y < InventoryComponent->GridHeight; ++Y)
    {
        for (int X = 0; X < InventoryComponent->GridWidth; ++X)
        {
            UBorder* CellVisual = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
            CellVisual->SetBrushColor(FLinearColor(0.5f, 0.5f, 0.5f, 0.4f)); // 더 밝고 선명한 회색 (칸 구분을 확실하게)
            CellVisual->SetVisibility(ESlateVisibility::HitTestInvisible);
            
            UCanvasPanelSlot* CellSlot = GridCanvas->AddChildToCanvas(CellVisual);
            // 외곽선 효과를 위해 1픽셀 마진을 줌
            CellSlot->SetPosition(FVector2D(X * 64.0f + 1.0f, Y * 64.0f + 1.0f));
            CellSlot->SetSize(FVector2D(62.0f, 62.0f)); 
        }
    }

    TArray<FName> CheckedItems;

    for (int Y = 0; Y < InventoryComponent->GridHeight; ++Y)
    {
        for (int X = 0; X < InventoryComponent->GridWidth; ++X)
        {
            int32 Index = InventoryComponent->GetIndex(X, Y);
            FName ItemID = InventoryComponent->GridCells[Index];

            if (ItemID != NAME_None && !CheckedItems.Contains(ItemID))
            {
                CheckedItems.Add(ItemID);

                // 이 아이템이 차지하는 실제 크기(Width, Height) 계산
                int32 MinX = X, MaxX = X;
                int32 MinY = Y, MaxY = Y;

                for (int ty = Y; ty < InventoryComponent->GridHeight; ++ty)
                {
                    for (int tx = 0; tx < InventoryComponent->GridWidth; ++tx)
                    {
                        if (InventoryComponent->GridCells[InventoryComponent->GetIndex(tx, ty)] == ItemID)
                        {
                            if (tx > MaxX) MaxX = tx;
                            if (tx < MinX) MinX = tx; // (X는 루프상 MinX보다 작을 수도 있음)
                            if (ty > MaxY) MaxY = ty;
                        }
                    }
                }

                int32 ItemW = MaxX - MinX + 1;
                int32 ItemH = MaxY - MinY + 1;

                UDraggableItemWidget* ItemVisual = WidgetTree->ConstructWidget<UDraggableItemWidget>(UDraggableItemWidget::StaticClass());
                ItemVisual->ItemID = ItemID;
                ItemVisual->ItemSize = FIntPoint(ItemW, ItemH);
                ItemVisual->bIsRotated = false; // 그리드 내의 현재 모양을 기본 모양으로 취급
                
                // 저장된 희귀도 불러오기 (기본값 Common)
                EItemRarity SavedRarity = EItemRarity::Common;
                if (const EItemRarity* FoundRarity = InventoryComponent->ItemRarityMap.Find(ItemID))
                {
                    SavedRarity = *FoundRarity;
                }
                ItemVisual->Rarity = SavedRarity;

                // 식별(Examined) 상태 불러오기 (기본값 true)
                bool bSavedExamined = true;
                if (const bool* FoundExamined = InventoryComponent->ItemExaminedMap.Find(ItemID))
                {
                    bSavedExamined = *FoundExamined;
                }
                ItemVisual->bIsExamined = bSavedExamined;

                // InitWidgetUI()를 호출하면 DraggableItemWidget 내부에서 텍스트와 배경이 세팅됨
                ItemVisual->InitWidgetUI();
                
                // 가방 내부에서도 클릭하여 드래그할 수 있도록 Visible로 설정 (기본이 Visible)
                ItemVisual->SetVisibility(ESlateVisibility::Visible);

                UCanvasPanelSlot* CanvasSlot = GridCanvas->AddChildToCanvas(ItemVisual);
                
                // 위치와 크기를 64배수로 딱 맞추어 드래그 시 좌표 오차가 없도록 함
                CanvasSlot->SetPosition(FVector2D(MinX * 64.0f, MinY * 64.0f));
                CanvasSlot->SetAutoSize(true); // DraggableItem 내부의 SizeBox에 크기를 위임
            }
        }
    }
}
