#include "DraggableItemWidget.h"
#include "ItemDragDropOperation.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Input/Reply.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/SizeBox.h"
#include "Components/ProgressBar.h"
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
        
        // Rarity 기반 색상 결정
        FLinearColor RarityColor;
        switch (Rarity)
        {
            case EItemRarity::Common:    RarityColor = FLinearColor(0.3f, 0.3f, 0.3f, 1.0f); break; // 짙은 회색
            case EItemRarity::Uncommon:  RarityColor = FLinearColor(0.2f, 0.8f, 0.2f, 1.0f); break; // 녹색
            case EItemRarity::Rare:      RarityColor = FLinearColor(0.1f, 0.4f, 1.0f, 1.0f); break; // 파란색
            case EItemRarity::Epic:      RarityColor = FLinearColor(0.6f, 0.1f, 0.8f, 1.0f); break; // 보라색
            case EItemRarity::Legendary: RarityColor = FLinearColor(1.0f, 0.8f, 0.1f, 1.0f); break; // 금색
            case EItemRarity::Mythic:    RarityColor = FLinearColor(1.0f, 0.1f, 0.1f, 1.0f); break; // 빨간색
            default:                     RarityColor = FLinearColor(0.3f, 0.3f, 0.3f, 1.0f); break;
        }

        // 미식별 상태면 시커먼 실루엣(회색 틴트)으로 강제 덮어쓰기
        if (!bIsExamined)
        {
            RarityColor = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);
        }

        // Delta Force 스타일처럼 짙은 반투명 회색 배경에 Rarity 색상을 약간 섞음
        FLinearColor DarkTint = FLinearColor(RarityColor.R * 0.3f, RarityColor.G * 0.3f, RarityColor.B * 0.3f, 0.85f);
        BG->SetBrushColor(DarkTint);
        RootBox->AddChild(BG);
        
        // --- 텍스트 설정 ---
        UTextBlock* NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        FString DisplayName = ItemID.ToString();
        int32 UnderscoreIdx;
        if (DisplayName.FindChar('_', UnderscoreIdx))
        {
            DisplayName = DisplayName.Left(UnderscoreIdx);
        }
        
        // 미식별 상태면 텍스트를 ??? 로 숨김
        if (!bIsExamined)
        {
            DisplayName = TEXT("???");
            NameText->SetColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)); // 어두운 회색 텍스트
        }
        else
        {
            NameText->SetColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f, 1.0f)); // 밝은 흰색 텍스트
        }

        NameText->SetText(FText::FromString(DisplayName));
        NameText->Font.Size = 12;
        NameText->SetShadowOffset(FVector2D(1.0f, 1.0f));
        NameText->SetShadowColorAndOpacity(FLinearColor::Black);
        
        // 텍스트를 좌상단에 배치
        if (UBorderSlot* TextSlot = Cast<UBorderSlot>(BG->AddChild(NameText)))
        {
            TextSlot->SetHorizontalAlignment(HAlign_Left);
            TextSlot->SetVerticalAlignment(VAlign_Top);
            TextSlot->SetPadding(FMargin(4.0f, 2.0f, 0.0f, 0.0f));
        }

        // --- 프로그레스 바 (식별 중 표시용) ---
        ExamineProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
        ExamineProgressBar->SetVisibility(ESlateVisibility::Hidden); // 기본 숨김
        ExamineProgressBar->SetPercent(0.0f);
        ExamineProgressBar->WidgetStyle.FillImage.TintColor = FLinearColor::White;
        
        // 프로그레스 바를 하단에 배치
        if (UBorderSlot* ProgressSlot = Cast<UBorderSlot>(BG->AddChild(ExamineProgressBar)))
        {
            ProgressSlot->SetHorizontalAlignment(HAlign_Fill);
            ProgressSlot->SetVerticalAlignment(VAlign_Bottom);
            ProgressSlot->SetPadding(FMargin(4.0f, 0.0f, 4.0f, 4.0f));
            // 높이 강제 조정 (UProgressBar 자체 속성 조절이 한계가 있으면 슬롯 단위에서 패딩으로 해결하거나 사이즈박스로 감싸야함)
        }
    }
}

FReply UDraggableItemWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    FEventReply Reply = UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton);
    return Reply.NativeReply;
}

void UDraggableItemWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

    // 미식별 상태면 드래그 불가
    if (!bIsExamined)
    {
        return;
    }

    UItemDragDropOperation* DragDropOp = NewObject<UItemDragDropOperation>();
    DragDropOp->ItemID = this->ItemID;
    DragDropOp->ItemSize = this->ItemSize;
    DragDropOp->Rarity = this->Rarity;
    DragDropOp->bIsRotated = this->bIsRotated;
    DragDropOp->OriginalWidget = this; 
    
    // 드래그 시작 시점의 아이템 좌상단(Top-Left) 절대 좌표(화면 좌표)와 마우스 좌표 간의 차이를 저장합니다.
    FVector2D TopLeftScreenPos = InGeometry.GetAbsolutePosition();
    DragDropOp->MouseOffset = InMouseEvent.GetScreenSpacePosition() - TopLeftScreenPos;
    
    // 드래그 비주얼 생성
    UDraggableItemWidget* DragVisual = CreateWidget<UDraggableItemWidget>(GetWorld(), UDraggableItemWidget::StaticClass());
    DragVisual->ItemID = this->ItemID;
    DragVisual->ItemSize = this->ItemSize;
    DragVisual->bIsRotated = this->bIsRotated;
    DragVisual->Rarity = this->Rarity; // Rarity도 복사!
    DragVisual->InitWidgetUI();
    
    DragDropOp->DefaultDragVisual = DragVisual;
    DragDropOp->Pivot = EDragPivot::MouseDown;

    // 현재 오퍼레이션 추적
    CurrentDragOp = DragDropOp;
    CurrentDragVisual = DragVisual;

    OutOperation = DragDropOp;
}

FReply UDraggableItemWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::R)
    {
        bIsRotated = !bIsRotated;
        
        float W = bIsRotated ? (ItemSize.Y * 64.0f) : (ItemSize.X * 64.0f);
        float H = bIsRotated ? (ItemSize.X * 64.0f) : (ItemSize.Y * 64.0f);

        // 원본 비주얼 회전
        if (WidgetTree && WidgetTree->RootWidget)
        {
            if (USizeBox* RootBox = Cast<USizeBox>(WidgetTree->RootWidget))
            {
                RootBox->SetWidthOverride(W);
                RootBox->SetHeightOverride(H);
            }
        }
        
        // 마우스에 붙어있는 드래그 비주얼도 같이 회전
        if (CurrentDragVisual && CurrentDragVisual->WidgetTree && CurrentDragVisual->WidgetTree->RootWidget)
        {
            if (USizeBox* DragRootBox = Cast<USizeBox>(CurrentDragVisual->WidgetTree->RootWidget))
            {
                DragRootBox->SetWidthOverride(W);
                DragRootBox->SetHeightOverride(H);
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
