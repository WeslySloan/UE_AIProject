#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../Map/MapManagerComponent.h"
#include "MinimapWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerMoved, FIntPoint, NewCoordinate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMovementMessage, FString, Message);

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
    UMinimapWidget* MovementStateMirror = nullptr;

    UPROPERTY()
    UMinimapWidget* MovementStateSource = nullptr;

    UPROPERTY()
    TMap<FIntPoint, class UMinimapTileWidget*> TileWidgets;

    UPROPERTY(BlueprintAssignable, Category = "Map|Events")
    FOnPlayerMoved OnPlayerMoved;

    UPROPERTY(BlueprintAssignable, Category = "Map|Events")
    FOnMovementMessage OnMovementMessage;

    FIntPoint CurrentPlayerCoord;
    FIntPoint CurrentTargetCoord;
    TArray<FIntPoint> CurrentPath;

    bool bCompactMode = false;

    int32 CurrentMoveProgress = 0; // v1에서는 한 번의 전진이 한 칸 이동

    void InitMinimap(UMapManagerComponent* InMapManager, bool bInCompactMode = false);
    void SetMovementStateMirror(UMinimapWidget* InMirror);
    void ApplySharedMovementState(FIntPoint InPlayerCoord, FIntPoint InTargetCoord,
        const TArray<FIntPoint>& InPath, int32 InMoveProgress);
    void ResetMovement();
    void SetPlayerCoordinateForCombat(FIntPoint NewCoordinate);
    void HandleTileClicked(FIntPoint ClickedCoord);
    void RefreshEnemyDebugMarkers();

    UFUNCTION()
    void OnAdvanceClicked();

protected:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    
    void UpdatePathHighlight();
    bool MovePlayerTo(FIntPoint NewCoord);
    void UpdateAdvanceButtonText();
    void PublishMovementState();
    void NotifyMovementMessage(const FString& Message);
};
