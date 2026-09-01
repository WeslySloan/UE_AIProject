#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../ItemData.h"
#include "MainGameUI.generated.h"

class UTextBlock;
class UWrapBox;
class UGridBoardWidget;

UCLASS()
class GRIDLOOTMASTER_API UMainGameUI : public UUserWidget
{
    GENERATED_BODY()
    
public:
    virtual bool Initialize() override;

    UFUNCTION()
    void UpdateScore(int32 NewScore);

    UFUNCTION()
    void UpdateTimer(float RemainingTime);

    UFUNCTION()
    void ShowGameResult(bool bIsWin);

    // 새 아이템을 Loot Pool에 추가 (UI 생성)
    UFUNCTION()
    void AddItemToLootPool(FName ItemID, FIntPoint Size, int32 Value, EItemRarity Rarity = EItemRarity::Common);

    // Sell 버튼 클릭 시
    UFUNCTION()
    void OnSellButtonClicked();

public:
    UPROPERTY()
    UTextBlock* ScoreText;

    UPROPERTY()
    UTextBlock* TimerText;

    UPROPERTY()
    UWrapBox* LootPoolBox;

    UPROPERTY()
    UGridBoardWidget* GridBoard;
};
