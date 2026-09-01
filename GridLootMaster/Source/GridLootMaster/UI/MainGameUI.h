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

    UFUNCTION(BlueprintCallable, Category = "Game")
    void ShowGameResult(bool bIsWin);

    UFUNCTION()
    void OnSellButtonClicked();

public:
    UPROPERTY()
    UTextBlock* ScoreText;

    UPROPERTY()
    UTextBlock* TimerText;

    // 기존의 UWrapBox 대신 새로운 루트 컨테이너용 그리드 보드 사용
    UPROPERTY()
    UGridBoardWidget* ContainerBoard;

    UPROPERTY()
    UGridBoardWidget* GridBoard;

    // 상자 뒤지기(탐색) 버튼
    UPROPERTY()
    class UButton* SearchBtn;
};
