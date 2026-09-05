#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../Map/MapData.h"
#include "MinimapTileWidget.generated.h"

UCLASS()
class GRIDLOOTMASTER_API UMinimapTileWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY()
    class UBorder* BackgroundBorder;

    UPROPERTY()
    class UBorder* NorthWall;
    UPROPERTY()
    class UBorder* SouthWall;
    UPROPERTY()
    class UBorder* EastWall;
    UPROPERTY()
    class UBorder* WestWall;

    UPROPERTY()
    class USizeBox* NorthWallBox;
    UPROPERTY()
    class USizeBox* SouthWallBox;
    UPROPERTY()
    class USizeBox* EastWallBox;
    UPROPERTY()
    class USizeBox* WestWallBox;

    UPROPERTY()
    class UImage* PlayerIcon;

    UPROPERTY()
    class UButton* ClickButton;

    UPROPERTY()
    class UBorder* HighlightBorder; // 경로 하이라이트

    UPROPERTY()
    class UTextBlock* ZoneText;

    UPROPERTY()
    class UBorder* EnemyMarkerBorder;

    UPROPERTY()
    class UTextBlock* EnemyMarkerText;

    FIntPoint TileCoord;
    class UMinimapWidget* ParentMinimap;

    void InitTile(const FTileData& InData, class UMinimapWidget* InParent, float InTileSize = 64.0f,
        bool bInRenderSouthEdge = true, bool bInRenderEastEdge = true);
    void RefreshTileData(const FTileData& InData);
    void SetIsPath(bool bIsPath);
    void SetHasPlayer(bool bHasPlayer);
    void SetEnemyDebugMarker(bool bVisible, const FString& MarkerText, const FLinearColor& MarkerColor,
        const FString& TooltipText);
    void TriggerClick();

protected:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

    UFUNCTION()
    void OnTileButtonClicked();

private:
    bool bRenderSouthEdge = true;
    bool bRenderEastEdge = true;
    bool bCompactTile = false;

};
