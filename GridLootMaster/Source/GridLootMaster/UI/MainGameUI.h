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
    void RefreshMinimaps(class UMapManagerComponent* InMapManager);

    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
    virtual bool NativeSupportsKeyboardFocus() const override { return true; }

    UFUNCTION()
    void UpdateScore(int32 NewScore);

    UFUNCTION()
    void UpdateTimer(float RemainingTime);

    UFUNCTION()
    void UpdateHealth(int32 NewHealth, int32 NewMaxHealth);

    UFUNCTION()
    void UpdateCombatUI();

    UFUNCTION()
    void ShowEventNotification(FString Message);

    UFUNCTION()
    void QueueEventNotification(FString Message);

    UFUNCTION()
    void OnMinimapPlayerMoved(FIntPoint NewCoordinate);

    UFUNCTION()
    void UpdateActionAvailability();

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

    UFUNCTION()
    void OnStashButtonClicked();

    UFUNCTION()
    void OnStartRaidClicked();

    UFUNCTION()
    void OnExtractButtonClicked();

public:
    UPROPERTY()
    class UWidgetSwitcher* RightPanelSwitcher;

    UPROPERTY()
    class UMinimapWidget* MinimapUI;

    UPROPERTY()
    class UMinimapWidget* CompactMinimapUI;

    UPROPERTY()
    UGridBoardWidget* StashBoard;

    UPROPERTY()
    class UButton* ToggleModeButton;

    UPROPERTY()
    UTextBlock* ScoreText;

    UPROPERTY()
    UTextBlock* TimerText;

    UPROPERTY()
    UTextBlock* HealthText;

    UPROPERTY()
    UTextBlock* CombatText;

    UPROPERTY()
    UTextBlock* EventNotificationText;

    UPROPERTY()
    class UBorder* EventNotificationBorder;

    UPROPERTY()
    TArray<FString> PendingEventNotifications;

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
    FName ActiveWeaponSlot = TEXT("Primary1");

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

    UPROPERTY()
    class UButton* ExtractBtn;

    UPROPERTY()
    class UButton* SellBtn;

    UPROPERTY()
    class UButton* SellAllBtn;

    UPROPERTY()
    class UButton* BangBtn;

    UPROPERTY()
    class UButton* StartRaidBtn;

    UPROPERTY()
    class UButton* StashBtn;

protected:
    UFUNCTION()
    void OnStashInventoryChanged();

    UFUNCTION()
    void OnEventNotificationTimerExpired();

    FTimerHandle EventNotificationTimerHandle;
};
