#include "DraggableItemWidget.h"
#include "ItemDragDropOperation.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Input/Reply.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/TextBlock.h"

void UDraggableItemWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(true); 
}

void UDraggableItemWidget::InitWidgetUI()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        USizeBox* RootBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RootBox"));
        RootBox->SetWidthOverride(ItemSize.X * 64.0f);
        RootBox->SetHeightOverride(ItemSize.Y * 64.0f);
        WidgetTree->RootWidget = RootBox;

        UBorder* BG = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        // Delta Force 스타일처럼 짙은 반투명 회색 배경에, 아이템 구분을 위한 옅은 틴트 적용
        FLinearColor RandomColor = FLinearColor::MakeRandomColor();
        FLinearColor DarkTint = FLinearColor(RandomColor.R * 0.3f, RandomColor.G * 0.3f, RandomColor.B * 0.3f, 0.7f);
        BG->SetBrushColor(DarkTint);
        RootBox->AddChild(BG);
        
        UTextBlock* NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        // 뒤에 붙는 _1234 고유번호를 잘라내고 원래 이름만 표시 (옵션)
        FString DisplayName = ItemID.ToString();
        int32 UnderscoreIdx;
        if (DisplayName.FindChar('_', UnderscoreIdx))
        {
            DisplayName = DisplayName.Left(UnderscoreIdx);
        }
        NameText->SetText(FText::FromString(DisplayName));
        
        // 텍스트 스타일 지정 (작게, 좌상단, 그림자)
        NameText->SetColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f, 1.0f));
        NameText->Font.Size = 12;
        NameText->SetShadowOffset(FVector2D(1.0f, 1.0f));
        NameText->SetShadowColorAndOpacity(FLinearColor::Black);
        
        // 텍스트를 좌상단에 배치
        if (UBorderSlot* TextSlot = Cast<UBorderSlot>(BG->AddChild(NameText)))
        {
            TextSlot->SetHorizontalAlignment(HAlign_Left);
            TextSlot->SetVerticalAlignment(VAlign_Top);
            TextSlot->SetPadding(FMargin(4.0f, 2.0f, 0.0f, 0.0f)); // 모서리에서 살짝 떨어지게
        }
    }
}

FReply UDraggableItemWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    FReply Reply = Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
    
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton).SetUserFocus(TakeWidget());
    }
    
    return Reply;
}

void UDraggableItemWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

    UItemDragDropOperation* DragDropOp = NewObject<UItemDragDropOperation>();
    DragDropOp->ItemID = this->ItemID;
    DragDropOp->ItemSize = this->ItemSize;
    DragDropOp->bIsRotated = this->bIsRotated;
    DragDropOp->OriginalWidget = this; 
    
    // 드래그 시작 시점의 아이템 좌상단(Top-Left) 절대 좌표(화면 좌표)와 마우스 좌표 간의 차이를 저장합니다.
    // DPI 스케일링이 적용되더라도 스크린 스페이스(절대 좌표)끼리 빼는 것이 정확합니다.
    FVector2D TopLeftScreenPos = InGeometry.GetAbsolutePosition();
    DragDropOp->MouseOffset = InMouseEvent.GetScreenSpacePosition() - TopLeftScreenPos;
    
    DragDropOp->DefaultDragVisual = this;
    DragDropOp->Pivot = EDragPivot::MouseDown;

    // 현재 오퍼레이션 추적
    CurrentDragOp = DragDropOp;

    OutOperation = DragDropOp;
}

FReply UDraggableItemWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::R)
    {
        bIsRotated = !bIsRotated;
        
        // 회전 시각화
        if (WidgetTree && WidgetTree->RootWidget)
        {
            if (USizeBox* RootBox = Cast<USizeBox>(WidgetTree->RootWidget))
            {
                float W = bIsRotated ? (ItemSize.Y * 64.0f) : (ItemSize.X * 64.0f);
                float H = bIsRotated ? (ItemSize.X * 64.0f) : (ItemSize.Y * 64.0f);
                RootBox->SetWidthOverride(W);
                RootBox->SetHeightOverride(H);
            }
        }

        // 드래그 오퍼레이션 데이터에도 회전 반영
        if (CurrentDragOp)
        {
            CurrentDragOp->bIsRotated = bIsRotated;
        }

        OnItemRotated();
        return FReply::Handled();
    }
    
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
