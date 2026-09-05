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
#include "Components/ButtonSlot.h"
#include "Input/Reply.h"

void UMinimapTileWidget::NativeConstruct()
{
    Super::NativeConstruct();
}

void UMinimapTileWidget::InitTile(const FTileData& InData, UMinimapWidget* InParent, float InTileSize,
    bool bInRenderSouthEdge, bool bInRenderEastEdge)
{
    ParentMinimap = InParent;
    TileCoord = InData.Coordinate;
    bRenderSouthEdge = bInRenderSouthEdge;
    bRenderEastEdge = bInRenderEastEdge;
    bCompactTile = InTileSize <= 24.0f;

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
    // Tile 간 시각적 틈이 생기지 않도록 Button 내부 패딩도 제거합니다.
    TransparentButtonStyle.SetNormalPadding(FMargin(0.0f));
    TransparentButtonStyle.SetPressedPadding(FMargin(0.0f));
    ClickButton->SetStyle(TransparentButtonStyle);
    ClickButton->OnClicked.AddDynamic(this, &UMinimapTileWidget::OnTileButtonClicked);
    RootBox->AddChild(ClickButton);

    UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    if (UButtonSlot* ButtonContentSlot = Cast<UButtonSlot>(ClickButton->AddChild(Overlay)))
    {
        // UButtonSlot의 기본 ContentPadding 때문에 타일 색 영역이 안쪽으로 줄어들어
        // 타일 사이에 큰 틈이 있는 것처럼 보이던 문제를 제거합니다.
        ButtonContentSlot->SetPadding(FMargin(0.0f));
        ButtonContentSlot->SetHorizontalAlignment(HAlign_Fill);
        ButtonContentSlot->SetVerticalAlignment(VAlign_Fill);
    }

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

    EnemyMarkerBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    EnemyMarkerBorder->SetBrushColor(FLinearColor(0.85f, 0.05f, 0.02f, 0.95f));
    EnemyMarkerBorder->SetVisibility(ESlateVisibility::Hidden);
    EnemyMarkerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    EnemyMarkerText->SetText(FText::FromString(TEXT("E")));
    EnemyMarkerText->SetColorAndOpacity(FLinearColor::White);
    FSlateFontInfo EnemyFont = EnemyMarkerText->GetFont();
    EnemyFont.Size = InTileSize <= 24.0f ? 7 : 11;
    EnemyMarkerText->SetFont(EnemyFont);
    EnemyMarkerText->SetJustification(ETextJustify::Center);
    EnemyMarkerBorder->AddChild(EnemyMarkerText);
    if (UOverlaySlot* EnemySlot = Cast<UOverlaySlot>(Overlay->AddChild(EnemyMarkerBorder)))
    {
        EnemySlot->SetHorizontalAlignment(HAlign_Right);
        EnemySlot->SetVerticalAlignment(VAlign_Top);
        EnemySlot->SetPadding(FMargin(InTileSize <= 24.0f ? 1.0f : 2.0f));
    }

    // 5. 4면 벽(Edge) 표시
    // Padding으로 두께를 흉내 내면 East/West 벽이 0폭에 가깝게 배치될 수 있으므로
    // 실제 SizeBox 막대를 Edge에 붙여서 렌더링합니다. 입력은 계속 ClickButton이 받습니다.
    auto CreateWall = [&](bool bIsOpen, bool bOwnedEdge, bool bHorizontal,
        EVerticalAlignment VAlign, EHorizontalAlignment HAlign,
        USizeBox*& OutWallBox, UBorder*& OutWall)
    {
        const bool bCompact = bCompactTile;
        const float OpenThickness = 1.0f;
        // Compact에서도 실제 벽이 명확히 읽히도록 과거 가독성 좋은 비율에 가깝게 둡니다.
        const float ClosedThickness = bCompact ? 3.0f : 4.0f;
        const float Thickness = bIsOpen ? OpenThickness : ClosedThickness;

        const FLinearColor OpenGridColor(0.10f, 0.10f, 0.10f, bCompact ? 0.35f : 0.45f);
        const FLinearColor ClosedWallColor(0.02f, 0.02f, 0.02f, 1.0f);

        OutWallBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        OutWallBox->SetVisibility(bOwnedEdge ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
        if (bHorizontal)
        {
            OutWallBox->SetHeightOverride(Thickness);
        }
        else
        {
            OutWallBox->SetWidthOverride(Thickness);
        }

        OutWall = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        OutWall->SetBrushColor(bIsOpen ? OpenGridColor : ClosedWallColor);
        OutWall->SetVisibility(ESlateVisibility::HitTestInvisible);
        OutWall->SetPadding(FMargin(0.0f));
        OutWallBox->AddChild(OutWall);

        if (UOverlaySlot* WallSlot = Cast<UOverlaySlot>(Overlay->AddChild(OutWallBox)))
        {
            WallSlot->SetHorizontalAlignment(bHorizontal ? HAlign_Fill : HAlign);
            WallSlot->SetVerticalAlignment(bHorizontal ? VAlign : VAlign_Fill);
        }
    };

    // 벽은 양쪽 타일 면에 모두 그립니다. 예를 들어 A의 East가 막혀 있고 B의 West도 막혀 있으면
    // 경계 양쪽 안쪽에 각각 Wall Bar가 생겨 과거 미니맵처럼 실제 벽 두께가 더 명확하게 보입니다.
    CreateWall(InData.bOpenNorth, true, true, VAlign_Top, HAlign_Fill, NorthWallBox, NorthWall);
    CreateWall(InData.bOpenSouth, bRenderSouthEdge, true, VAlign_Bottom, HAlign_Fill, SouthWallBox, SouthWall);
    CreateWall(InData.bOpenWest, true, false, VAlign_Fill, HAlign_Left, WestWallBox, WestWall);
    CreateWall(InData.bOpenEast, bRenderEastEdge, false, VAlign_Fill, HAlign_Right, EastWallBox, EastWall);

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

    auto RefreshWall = [](USizeBox* WallBox, UBorder* Wall, bool bIsOpen, bool bOwnedEdge,
        bool bHorizontal, bool bCompact)
    {
        if (!WallBox || !Wall) return;

        const float Thickness = bIsOpen ? 1.0f : (bCompact ? 3.0f : 4.0f);
        if (bHorizontal)
        {
            WallBox->SetHeightOverride(Thickness);
        }
        else
        {
            WallBox->SetWidthOverride(Thickness);
        }

        Wall->SetBrushColor(bIsOpen
            ? FLinearColor(0.10f, 0.10f, 0.10f, bCompact ? 0.35f : 0.45f)
            : FLinearColor(0.02f, 0.02f, 0.02f, 1.0f));
        WallBox->SetVisibility(bOwnedEdge ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    };

    RefreshWall(NorthWallBox, NorthWall, InData.bOpenNorth, true, true, bCompactTile);
    RefreshWall(SouthWallBox, SouthWall, InData.bOpenSouth, bRenderSouthEdge, true, bCompactTile);
    RefreshWall(WestWallBox, WestWall, InData.bOpenWest, true, false, bCompactTile);
    RefreshWall(EastWallBox, EastWall, InData.bOpenEast, bRenderEastEdge, false, bCompactTile);
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

void UMinimapTileWidget::SetEnemyDebugMarker(bool bVisible, const FString& MarkerText,
    const FLinearColor& MarkerColor, const FString& TooltipText)
{
#if UE_BUILD_SHIPPING
    bVisible = bVisible && MarkerText == TEXT("D");
#endif
    if (EnemyMarkerBorder)
    {
        EnemyMarkerBorder->SetBrushColor(MarkerColor);
        EnemyMarkerBorder->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
        EnemyMarkerBorder->SetToolTipText(FText::FromString(TooltipText));
    }
    if (EnemyMarkerText)
    {
        EnemyMarkerText->SetText(FText::FromString(MarkerText));
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
