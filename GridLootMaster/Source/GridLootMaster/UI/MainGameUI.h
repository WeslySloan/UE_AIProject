#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../ItemData.h"
#include "MainGameUI.generated.h"

class UTextBlock;
class UGridBoardWidget;
class UEquipmentSlotWidget;

UCLASS()
class GRIDLOOTMASTER_API UMainGameUI : public UUserWidget
{
    GENERATED_BODY()
    
public:
    virtual bool Initialize() override;

    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
    virtual bool NativeSupportsKeyboardFocus() const override { return true; }

    UFUNCTION()
    void UpdateScore(int32 NewScore);

    UFUNCTION()
    void UpdateTimer(float RemainingTime);

    UFUNCTION(BlueprintCallable, Category = "Game")
    void ShowGameResult(bool bIsWin);

    UFUNCTION()
    void OnSellButtonClicked();

    UFUNCTION()
    void OnSellAllButtonClicked();

    UFUNCTION()
    void OnBangButtonClicked();

    UFUNCTION()
    void OnToggleModeClicked();

public:
    UPROPERTY()
    class UWidgetSwitcher* RightPanelSwitcher;

    UPROPERTY()
    class UMinimapWidget* MinimapUI;

    UPROPERTY()
    class UButton* ToggleModeButton;

    UPROPERTY()
    UTextBlock* ScoreText;

    UPROPERTY()
    UTextBlock* TimerText;

    // 기존의 UWrapBox 대신 새로운 루트 컨테이너용 그리드 보드 사용
    UPROPERTY()
    UGridBoardWidget* ContainerBoard;

    UPROPERTY()
    UGridBoardWidget* GridBoard;

    UPROPERTY()
    class UVerticalBox* LeftPanel;

    UPROPERTY()
    UGridBoardWidget* SafeBoxBoard;

    UPROPERTY()
    UGridBoardWidget* RigBoard;

    UPROPERTY()
    UGridBoardWidget* PocketBoard;

    UPROPERTY()
    FName ActiveWeaponSlot;

    UPROPERTY(BlueprintReadOnly, Category = "Equipment")
    UEquipmentSlotWidget* HelmetSlot;

    UPROPERTY(BlueprintReadOnly, Category = "Equipment")
    UEquipmentSlotWidget* ArmorSlot;

    UPROPERTY(BlueprintReadOnly, Category = "Equipment")
    UEquipmentSlotWidget* WeaponSlot1;

    UPROPERTY(BlueprintReadOnly, Category = "Equipment")
    UEquipmentSlotWidget* WeaponSlot2;

    UPROPERTY(BlueprintReadOnly, Category = "Equipment")
    UEquipmentSlotWidget* BackpackSlot;

    UPROPERTY(BlueprintReadOnly, Category = "Equipment")
    UEquipmentSlotWidget* SafeBoxSlot;

    UPROPERTY(BlueprintReadOnly, Category = "Equipment")
    UEquipmentSlotWidget* RigSlot;

    // 상자 뒤지기(탐색) 버튼
    UPROPERTY()
    class UButton* SearchBtn;
};
