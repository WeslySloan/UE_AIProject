#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../Map/MapManagerComponent.h"
#include "MinimapWidget.generated.h"

UCLASS()
class GRIDLOOTMASTER_API UMinimapWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY()
    class UUniformGridPanel* MapGridPanel;
    
    UPROPERTY()
    class UButton* AdvanceButton;
    
    UPROPERTY()
    class UTextBlock* AdvanceText;

    UPROPERTY()
    UMapManagerComponent* MapManager;

    UPROPERTY()
    TMap<FIntPoint, class UMinimapTileWidget*> TileWidgets;

    FIntPoint CurrentPlayerCoord;
    FIntPoint CurrentTargetCoord;
    TArray<FIntPoint> CurrentPath;

    int32 CurrentMoveProgress; // 0, 1, 2 (3번 누르면 도착)

    void InitMinimap(UMapManagerComponent* InMapManager);
    void HandleTileClicked(FIntPoint ClickedCoord);

    UFUNCTION()
    void OnAdvanceClicked();

protected:
    virtual void NativeConstruct() override;
    
    void UpdatePathHighlight();
    void MovePlayerTo(FIntPoint NewCoord);
    void UpdateAdvanceButtonText();
};
