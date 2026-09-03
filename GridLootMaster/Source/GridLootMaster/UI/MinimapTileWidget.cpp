#include "MinimapTileWidget.h"
#include "MinimapWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Button.h"
#include "Input/Reply.h"

void UMinimapTileWidget::NativeConstruct()
{
    Super::NativeConstruct();
}

void UMinimapTileWidget::InitTile(const FTileData& InData, UMinimapWidget* InParent, float InTileSize)
{
    ParentMinimap = InParent;
    TileCoord = InData.Coordinate;

    if (!WidgetTree)
    {
        WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
    }

    USizeBox* RootBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    RootBox->SetWidthOverride(InTileSize);
    RootBox->SetHeightOverride(InTileSize);
    WidgetTree->RootWidget = RootBox;

    ClickButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    FButtonStyle TransparentButtonStyle;
    FSlateBrush EmptyBrush;
    EmptyBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
    TransparentButtonStyle.SetNormal(EmptyBrush);
    TransparentButtonStyle.SetHovered(EmptyBrush);
    TransparentButtonStyle.SetPressed(EmptyBrush);
    TransparentButtonStyle.SetDisabled(EmptyBrush);
    ClickButton->SetStyle(TransparentButtonStyle);
    ClickButton->OnClicked.AddDynamic(this, &UMinimapTileWidget::OnTileButtonClicked);
    RootBox->AddChild(ClickButton);

    UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    ClickButton->AddChild(Overlay);

    // 1. 구역별 배경색
    BackgroundBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    FLinearColor ZoneColor;
    if (InData.TileType == ETileType::Extraction)
    {
        ZoneColor = FLinearColor(0.1f, 0.9f, 0.2f, 1.0f);
    }
    else
    {
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
    }
    BackgroundBorder->SetBrushColor(ZoneColor);
    BackgroundBorder->SetPadding(FMargin(0.0f));
    BackgroundBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
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
    if (InData.TileType == ETileType::Extraction)
    {
        ZoneText->SetText(FText::FromString(TEXT("EXIT")));
    }
    else
    {
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
    }
    FSlateFontInfo FontInfo = ZoneText->GetFont();
    FontInfo.Size = InTileSize <= 24.0f ? 6 : 10;
    ZoneText->SetFont(FontInfo);
    ZoneText->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.5f));
    ZoneText->SetVisibility(ESlateVisibility::HitTestInvisible);
    if (UOverlaySlot* TextSlot = Cast<UOverlaySlot>(Overlay->AddChild(ZoneText)))
    {
        TextSlot->SetHorizontalAlignment(HAlign_Left);
        TextSlot->SetVerticalAlignment(VAlign_Top);
        TextSlot->SetPadding(FMargin(InTileSize <= 24.0f ? 1.0f : 2.0f));
    }

    // 4. 플레이어 아이콘 (기본 숨김)
    PlayerIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
    PlayerIcon->SetColorAndOpacity(FLinearColor(0.0f, 1.0f, 1.0f, 1.0f)); 
    PlayerIcon->SetVisibility(ESlateVisibility::Hidden);
    if (UOverlaySlot* PlayerSlot = Cast<UOverlaySlot>(Overlay->AddChild(PlayerIcon)))
    {
        PlayerSlot->SetHorizontalAlignment(HAlign_Center);
        PlayerSlot->SetVerticalAlignment(VAlign_Center);
        PlayerSlot->SetPadding(FMargin(FMath::Max(2.0f, InTileSize * 0.15625f)));
    }

    // 5. 4면 벽(Edge) 표시
    auto CreateWall = [&](bool bIsOpen, EVerticalAlignment VAlign, EHorizontalAlignment HAlign) {
        UBorder* Wall = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Wall->SetBrushColor(bIsOpen ? FLinearColor(0.0f, 0.0f, 0.0f, 0.1f) : FLinearColor(0.0f, 0.0f, 0.0f, 1.0f));
        Wall->SetVisibility(ESlateVisibility::HitTestInvisible);
        if (UOverlaySlot* WallSlot = Cast<UOverlaySlot>(Overlay->AddChild(Wall)))
        {
            WallSlot->SetHorizontalAlignment(HAlign);
            WallSlot->SetVerticalAlignment(VAlign);
            const float EdgePadding = FMath::Max(0.0f, InTileSize - 4.0f);
            if (VAlign == VAlign_Top) WallSlot->SetPadding(FMargin(0, 0, 0, EdgePadding));
            else if (VAlign == VAlign_Bottom) WallSlot->SetPadding(FMargin(0, EdgePadding, 0, 0));
            else if (HAlign == HAlign_Left) WallSlot->SetPadding(FMargin(0, 0, EdgePadding, 0));
            else if (HAlign == HAlign_Right) WallSlot->SetPadding(FMargin(EdgePadding, 0, 0, 0));
        }
        return Wall;
    };

    NorthWall = CreateWall(InData.bOpenNorth, VAlign_Top, HAlign_Fill);
    SouthWall = CreateWall(InData.bOpenSouth, VAlign_Bottom, HAlign_Fill);
    WestWall = CreateWall(InData.bOpenWest, VAlign_Fill, HAlign_Left);
    EastWall = CreateWall(InData.bOpenEast, VAlign_Fill, HAlign_Right);

}

void UMinimapTileWidget::RefreshTileData(const FTileData& InData)
{
    TileCoord = InData.Coordinate;

    if (BackgroundBorder)
    {
        FLinearColor ZoneColor;
        if (InData.TileType == ETileType::Extraction)
        {
            ZoneColor = FLinearColor(0.1f, 0.9f, 0.2f, 1.0f);
        }
        else
        {
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
        }
        BackgroundBorder->SetBrushColor(ZoneColor);
    }

    if (ZoneText)
    {
        FString ZonePrefix = TEXT("X");
        if (InData.TileType == ETileType::Extraction)
        {
            ZonePrefix = TEXT("EXIT");
        }
        else
        {
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
            ZonePrefix = FString::Printf(TEXT("%s-%d"), *ZonePrefix,
                (InData.Coordinate.X % 3) + (InData.Coordinate.Y % 3) * 3 + 1);
        }
        ZoneText->SetText(FText::FromString(ZonePrefix));
    }

    auto RefreshWall = [](UBorder* Wall, bool bIsOpen) {
        if (Wall)
        {
            Wall->SetBrushColor(bIsOpen
                ? FLinearColor(0.0f, 0.0f, 0.0f, 0.1f)
                : FLinearColor(0.0f, 0.0f, 0.0f, 1.0f));
        }
    };
    RefreshWall(NorthWall, InData.bOpenNorth);
    RefreshWall(SouthWall, InData.bOpenSouth);
    RefreshWall(WestWall, InData.bOpenWest);
    RefreshWall(EastWall, InData.bOpenEast);
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
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && ParentMinimap)
    {
        TriggerClick();
        return FReply::Handled();
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UMinimapTileWidget::TriggerClick()
{
    if (ParentMinimap)
    {
        ParentMinimap->HandleTileClicked(TileCoord);
    }
}

void UMinimapTileWidget::OnTileButtonClicked()
{
    TriggerClick();
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
