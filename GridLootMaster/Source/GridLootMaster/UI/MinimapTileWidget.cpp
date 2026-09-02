#include "MinimapTileWidget.h"
#include "MinimapWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Input/Reply.h"

void UMinimapTileWidget::NativeConstruct()
{
    Super::NativeConstruct();
}

void UMinimapTileWidget::InitTile(const FTileData& InData, UMinimapWidget* InParent)
{
    ParentMinimap = InParent;
    TileCoord = InData.Coordinate;

    if (!WidgetTree)
    {
        WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
    }

    USizeBox* RootBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    RootBox->SetWidthOverride(64.0f);
    RootBox->SetHeightOverride(64.0f);
    WidgetTree->RootWidget = RootBox;

    UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    RootBox->AddChild(Overlay);

    // 1. 구역별 배경색
    BackgroundBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    FLinearColor ZoneColor;
    switch (InData.Zone)
    {
        case ETileZone::Zone_A: ZoneColor = FLinearColor(0.2f, 0.5f, 0.8f, 0.8f); break; 
        case ETileZone::Zone_B: ZoneColor = FLinearColor(0.2f, 0.8f, 0.3f, 0.8f); break; 
        case ETileZone::Zone_C: ZoneColor = FLinearColor(0.9f, 0.9f, 0.2f, 0.8f); break; 
        case ETileZone::Zone_D: ZoneColor = FLinearColor(0.9f, 0.6f, 0.2f, 0.8f); break; 
        case ETileZone::Zone_E: ZoneColor = FLinearColor(0.6f, 0.4f, 0.9f, 0.8f); break; 
        case ETileZone::Zone_F: ZoneColor = FLinearColor(0.9f, 0.3f, 0.3f, 0.8f); break; 
        case ETileZone::Zone_G: ZoneColor = FLinearColor(0.6f, 0.5f, 0.4f, 0.8f); break; 
        case ETileZone::Zone_H: ZoneColor = FLinearColor(0.9f, 0.5f, 0.7f, 0.8f); break; 
        case ETileZone::Zone_I: ZoneColor = FLinearColor(0.7f, 0.7f, 0.7f, 0.8f); break; 
        default: ZoneColor = FLinearColor(0.1f, 0.1f, 0.1f, 0.8f); break;
    }
    BackgroundBorder->SetBrushColor(ZoneColor);
    BackgroundBorder->SetPadding(FMargin(0.0f));
    if (UOverlaySlot* BGSlope = Cast<UOverlaySlot>(Overlay->AddChild(BackgroundBorder)))
    {
        BGSlope->SetHorizontalAlignment(HAlign_Fill);
        BGSlope->SetVerticalAlignment(VAlign_Fill);
    }

    // 2. 경로 하이라이트 보더 (기본 숨김)
    HighlightBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    HighlightBorder->SetBrushColor(FLinearColor(1.0f, 1.0f, 0.0f, 0.4f));
    HighlightBorder->SetVisibility(ESlateVisibility::Hidden);
    if (UOverlaySlot* HLSlot = Cast<UOverlaySlot>(Overlay->AddChild(HighlightBorder)))
    {
        HLSlot->SetHorizontalAlignment(HAlign_Fill);
        HLSlot->SetVerticalAlignment(VAlign_Fill);
    }

    // 3. 텍스트 (A-1 등)
    ZoneText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    FString ZonePrefix = TEXT("X");
    switch (InData.Zone)
    {
        case ETileZone::Zone_A: ZonePrefix = TEXT("A"); break;
        case ETileZone::Zone_B: ZonePrefix = TEXT("B"); break;
        case ETileZone::Zone_C: ZonePrefix = TEXT("C"); break;
        case ETileZone::Zone_D: ZonePrefix = TEXT("D"); break;
        case ETileZone::Zone_E: ZonePrefix = TEXT("E"); break;
        case ETileZone::Zone_F: ZonePrefix = TEXT("F"); break;
        case ETileZone::Zone_G: ZonePrefix = TEXT("G"); break;
        case ETileZone::Zone_H: ZonePrefix = TEXT("H"); break;
        case ETileZone::Zone_I: ZonePrefix = TEXT("I"); break;
    }
    
    ZoneText->SetText(FText::FromString(FString::Printf(TEXT("%s-%d"), *ZonePrefix, (InData.Coordinate.X % 3) + (InData.Coordinate.Y % 3) * 3 + 1)));
    FSlateFontInfo FontInfo = ZoneText->GetFont();
    FontInfo.Size = 10;
    ZoneText->SetFont(FontInfo);
    ZoneText->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.5f));
    if (UOverlaySlot* TextSlot = Cast<UOverlaySlot>(Overlay->AddChild(ZoneText)))
    {
        TextSlot->SetHorizontalAlignment(HAlign_Left);
        TextSlot->SetVerticalAlignment(VAlign_Top);
        TextSlot->SetPadding(FMargin(2.0f));
    }

    // 4. 플레이어 아이콘 (기본 숨김)
    PlayerIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
    PlayerIcon->SetColorAndOpacity(FLinearColor(0.0f, 1.0f, 1.0f, 1.0f)); 
    PlayerIcon->SetVisibility(ESlateVisibility::Hidden);
    if (UOverlaySlot* PlayerSlot = Cast<UOverlaySlot>(Overlay->AddChild(PlayerIcon)))
    {
        PlayerSlot->SetHorizontalAlignment(HAlign_Center);
        PlayerSlot->SetVerticalAlignment(VAlign_Center);
        PlayerSlot->SetPadding(FMargin(10.0f)); 
    }

    // 5. 4면 벽(Edge) 표시
    auto CreateWall = [&](bool bIsOpen, EVerticalAlignment VAlign, EHorizontalAlignment HAlign) {
        UBorder* Wall = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Wall->SetBrushColor(bIsOpen ? FLinearColor(0.0f, 0.0f, 0.0f, 0.1f) : FLinearColor(0.0f, 0.0f, 0.0f, 1.0f));
        if (UOverlaySlot* WallSlot = Cast<UOverlaySlot>(Overlay->AddChild(Wall)))
        {
            WallSlot->SetHorizontalAlignment(HAlign);
            WallSlot->SetVerticalAlignment(VAlign);
            if (VAlign == VAlign_Top) WallSlot->SetPadding(FMargin(0, 0, 0, 60)); 
            else if (VAlign == VAlign_Bottom) WallSlot->SetPadding(FMargin(0, 60, 0, 0)); 
            else if (HAlign == HAlign_Left) WallSlot->SetPadding(FMargin(0, 0, 60, 0)); 
            else if (HAlign == HAlign_Right) WallSlot->SetPadding(FMargin(60, 0, 0, 0)); 
        }
        return Wall;
    };

    NorthWall = CreateWall(InData.bOpenNorth, VAlign_Top, HAlign_Fill);
    SouthWall = CreateWall(InData.bOpenSouth, VAlign_Bottom, HAlign_Fill);
    WestWall = CreateWall(InData.bOpenWest, VAlign_Fill, HAlign_Left);
    EastWall = CreateWall(InData.bOpenEast, VAlign_Fill, HAlign_Right);
}

void UMinimapTileWidget::SetIsPath(bool bIsPath)
{
    if (HighlightBorder)
    {
        HighlightBorder->SetVisibility(bIsPath ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
}

void UMinimapTileWidget::SetHasPlayer(bool bHasPlayer)
{
    if (PlayerIcon)
    {
        PlayerIcon->SetVisibility(bHasPlayer ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
}

FReply UMinimapTileWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (ParentMinimap)
    {
        ParentMinimap->HandleTileClicked(TileCoord);
        return FReply::Handled();
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UMinimapTileWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
    if (BackgroundBorder) BackgroundBorder->SetContentColorAndOpacity(FLinearColor(1.5f, 1.5f, 1.5f, 1.0f));
}

void UMinimapTileWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
    Super::NativeOnMouseLeave(InMouseEvent);
    if (BackgroundBorder) BackgroundBorder->SetContentColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
}