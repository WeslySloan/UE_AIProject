#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../ItemData.h"
#include "../EnemyManagerComponent.h"
#include "MainGameUI.generated.h"

class UTextBlock;
class UGridBoardWidget;
class USectionedStorageWidget;
class UEquipmentSlotWidget;
class UScrollBox;
class UVerticalBox;
class UProgressBar;
class USizeBox;
class UImage;
class UTexture2D;

UCLASS()
class GRIDLOOTMASTER_API UMainGameUI : public UUserWidget
{
    GENERATED_BODY()
    
public:
    virtual bool Initialize() override;
    void RefreshMinimaps(class UMapManagerComponent* InMapManager);

    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
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

    UFUNCTION()
    void UpdateBackgroundForPhase();

    UFUNCTION()
    void UpdateEquipmentSlotSizes();

    UFUNCTION(BlueprintCallable, Category = "Game")
    void ShowGameResult(bool bIsWin);

    UFUNCTION()
    void OnSellButtonClicked();

    UFUNCTION()
    void OnSellAllButtonClicked();

    UFUNCTION()
    void OnBangButtonClicked();
    UFUNCTION()
    void OnReloadButtonClicked();
    UFUNCTION()
    void OnCombatMoveButtonClicked();
    UFUNCTION()
    void OnCombatDirectionNorthButtonClicked();
    UFUNCTION()
    void OnCombatDirectionWestButtonClicked();
    UFUNCTION()
    void OnCombatDirectionEastButtonClicked();
    UFUNCTION()
    void OnCombatDirectionSouthButtonClicked();
    UFUNCTION()
    void OnCombatDirectionCancelButtonClicked();
    UFUNCTION()
    void OnApproachButtonClicked();
    UFUNCTION()
    void OnRetreatButtonClicked();
    UFUNCTION()
    void OnCombatFleeButtonClicked();
    UFUNCTION()
    void OnPlayerAmbushButtonClicked();
    UFUNCTION()
    void OnAmbushWaitButtonClicked();
    UFUNCTION()
    void OnAmbushCancelButtonClicked();
    UFUNCTION()
    void OnAmbushLetPassButtonClicked();
    UFUNCTION()
    void OnAmbushAssaultButtonClicked();
    UFUNCTION()
    void OnEnemyAmbushSearchButtonClicked();
    UFUNCTION()
    void OnEnemyAmbushCoverButtonClicked();
    UFUNCTION()
    void OnEnemyAmbushFleeButtonClicked();

    UFUNCTION()
    void OnToggleModeClicked();

    UFUNCTION()
    void OnStashButtonClicked();

    UFUNCTION()
    void OnStartRaidClicked();

    UFUNCTION()
    void RestoreRaidInputFocus();

    bool HasRaidUIFocus() const;
    bool HasActiveRaidTransientPopup() const;
    void ScheduleRaidInputFocusRestore();

    UFUNCTION()
    void OnExtractButtonClicked();

    UFUNCTION()
    void OnDebugSpawnEnemyClicked();

    UFUNCTION()
    void OnSearchButtonClicked();

    void SetLootInventory(class UGridInventoryComponent* Inventory);
    void ClearCorpseLootView();
    void RefreshEnemyMarkers();

public:
    UPROPERTY()
    UImage* BackgroundImage;

    UPROPERTY()
    UTexture2D* BGStashTexture;

    UPROPERTY()
    UTexture2D* BGRaidTexture;

    UPROPERTY()
    class UWidgetSwitcher* RightPanelSwitcher;

    UPROPERTY()
    class UMinimapWidget* MinimapUI;

    UPROPERTY()
    class UMinimapWidget* CompactMinimapUI;

    UPROPERTY()
    UGridBoardWidget* StashBoard;

    UPROPERTY()
    UVerticalBox* RetirementAccountPanel;

    UPROPERTY()
    UTextBlock* RetirementBalanceText;

    UPROPERTY()
    UTextBlock* RetirementStatusText;

    UPROPERTY()
    UProgressBar* RetirementProgressBar;

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
    UTextBlock* CombatActionText;

    UPROPERTY()
    UTextBlock* EventNotificationText;

    UPROPERTY()
    class UBorder* EventNotificationBorder;

    UPROPERTY()
    class UBorder* EventLogBorder;

    UPROPERTY()
    UScrollBox* EventLogScrollBox;

    UPROPERTY()
    UTextBlock* StatusPanelText;

    UPROPERTY()
    class UBorder* StatusPanel;

    UPROPERTY()
    UTextBlock* LootContainerTitle;

    UPROPERTY()
    TArray<FString> PendingEventNotifications;

    UPROPERTY()
    TArray<FString> EventLogEntries;

    UPROPERTY()
    FString LastDisplayedCombatMessage;

    // 기존의 UWrapBox 대신 새로운 루트 컨테이너용 그리드 보드 사용
    UPROPERTY()
    UGridBoardWidget* ContainerBoard;

    UPROPERTY()
    USectionedStorageWidget* GridBoard;

    UPROPERTY()
    class UVerticalBox* LeftPanel;

    UPROPERTY()
    UGridBoardWidget* SafeBoxBoard;

    UPROPERTY()
    class USizeBox* RigSlotSizeBox;

    UPROPERTY()
    class USizeBox* BackpackSlotSizeBox;

    UPROPERTY()
    USectionedStorageWidget* RigBoard;

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
    UTextBlock* SearchBtnText;

    UPROPERTY()
    class UButton* ExtractBtn;

    UPROPERTY()
    class UButton* SellBtn;

    UPROPERTY()
    class UButton* SellAllBtn;

    UPROPERTY()
    class UButton* BangBtn;

    UPROPERTY()
    UTextBlock* BangButtonText;

    UPROPERTY()
    class UButton* ReloadBtn;

    UPROPERTY()
    class UButton* CombatMoveBtn;

    UPROPERTY()
    class UButton* CombatDirectionNorthBtn;

    UPROPERTY()
    class UButton* CombatDirectionWestBtn;

    UPROPERTY()
    class UButton* CombatDirectionEastBtn;

    UPROPERTY()
    class UButton* CombatDirectionSouthBtn;

    UPROPERTY()
    class UButton* CombatDirectionCancelBtn;

    UPROPERTY()
    class UButton* ApproachBtn;

    UPROPERTY()
    class UButton* RetreatBtn;

    UPROPERTY()
    class UButton* CombatFleeBtn;

    UPROPERTY()
    class UButton* PlayerAmbushBtn;

    UPROPERTY()
    class UButton* AmbushWaitBtn;

    UPROPERTY()
    class UButton* AmbushCancelBtn;

    UPROPERTY()
    class UButton* AmbushLetPassBtn;

    UPROPERTY()
    class UButton* AmbushAssaultBtn;

    UPROPERTY()
    class UButton* EnemyAmbushSearchBtn;

    UPROPERTY()
    class UButton* EnemyAmbushCoverBtn;

    UPROPERTY()
    class UButton* EnemyAmbushFleeBtn;

    UPROPERTY()
    class UButton* StartRaidBtn;

    UPROPERTY()
    class UButton* StashBtn;

    UPROPERTY()
    class UButton* DebugSpawnEnemyBtn;

protected:
    UFUNCTION()
    void OnStashInventoryChanged();

    UFUNCTION()
    void OnEventNotificationTimerExpired();

    void AddEventLogEntry(const FString& Message);
    void UpdateStatusPanel();
    void UpdateEnemyEventLog();

    TMap<FName, FIntPoint> LastEnemyCoordinates;
    TMap<FName, EEnemyKnowledgeState> LastEnemyKnowledgeStates;
    bool bEnemyEventLogRaidActive = false;
    bool bLastCombatActive = false;

    enum class ECombatDirectionMode : uint8
    {
        None,
        Move,
        Flee
    };
    ECombatDirectionMode CombatDirectionMode = ECombatDirectionMode::None;

    void SetCombatDirectionMode(ECombatDirectionMode InMode);
    void RequestCombatDirection(ECombatMovementDirection Direction);

    FTimerHandle EventNotificationTimerHandle;
    bool bRaidFocusRestorePending = false;
};
