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
    class UImage* PlayerIcon;

    UPROPERTY()
    class UBorder* HighlightBorder; // 경로 하이라이트

    UPROPERTY()
    class UTextBlock* ZoneText;

    FIntPoint TileCoord;
    class UMinimapWidget* ParentMinimap;

    void InitTile(const FTileData& InData, class UMinimapWidget* InParent);
    void SetIsPath(bool bIsPath);
    void SetHasPlayer(bool bHasPlayer);

protected:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
};
