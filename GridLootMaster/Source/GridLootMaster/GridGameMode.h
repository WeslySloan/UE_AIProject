#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ItemData.h"
#include "GridGameMode.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameStateChanged);

UENUM(BlueprintType)
enum class ERaidState : uint8
{
    Lobby       UMETA(DisplayName = "Lobby"),
    InRaid      UMETA(DisplayName = "In Raid"),
    Succeeded   UMETA(DisplayName = "Succeeded"),
    Failed      UMETA(DisplayName = "Failed")
};

UENUM(BlueprintType)
enum class EPlayerRaidPosture : uint8
{
    Normal,
    Ambushing
};

class UGridInventoryComponent;
class UEquipmentComponent;
class UMainGameUI;
class UItemInstance;
class UItemDataTable;
class UMapManagerComponent;
class UCombatComponent;
class UEnemyManagerComponent;

UCLASS()
class GRIDLOOTMASTER_API AGridGameMode : public AGameModeBase
{
    GENERATED_BODY()
    
public:
    static constexpr int64 RetirementGoal = 5000000;

    AGridGameMode();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map")
    UMapManagerComponent* MapManagerComponent;

protected:
    virtual void BeginPlay() override;

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    class UDataTable* ItemDataTable;
    // 게임 시간 제한 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Rules")
    float TotalTimeLimit;
    
    // 남은 시간
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Rules")
    float RemainingTime;

    // 현재 점수
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Rules")
    int32 CurrentScore;

    // 목표 점수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Rules")
    int32 QuotaScore;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    int32 MaxHealth;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    int32 CurrentHealth;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Raid")
    ERaidState RaidState;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Raid|Posture")
    EPlayerRaidPosture PlayerPosture = EPlayerRaidPosture::Normal;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map")
    FIntPoint CurrentPlayerCoord;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map")
    FIntPoint PreviousPlayerCoord;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raid|Detection")
    int32 PlayerPerception = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raid|Detection")
    int32 PlayerStealth = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raid|Detection")
    int32 PlayerDetectionRangeTiles = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raid|Detection")
    int32 PlayerDetectionPower = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raid|Ambush")
    int32 PlayerMobility = 50;

    UPROPERTY(BlueprintAssignable, Category = "Game|Events")
    FOnGameStateChanged OnGameStateChanged;

    UFUNCTION(BlueprintCallable, Category = "Game")
    void AddScore(int32 Amount);

    UFUNCTION(BlueprintCallable, Category = "Game")
    void CheckWinCondition();

    FName FindCompatibleAmmoID(const UItemInstance* Magazine) const;

    UFUNCTION(BlueprintCallable, Category = "Raid")
    void SetRaidState(ERaidState NewState);

    UFUNCTION(BlueprintCallable, Category = "Raid")
    bool StartRaid();

#if WITH_DEV_AUTOMATION_TESTS
    void SetRaidStartPointForTest(FIntPoint StartPoint);
#endif

    UFUNCTION(BlueprintCallable, Category = "Stash")
    bool SaveStash();

    UFUNCTION(BlueprintCallable, Category = "Stash")
    bool LoadStash();

    int32 LoadedInitialQAMagazineSeedVersion = 0;

    UFUNCTION(BlueprintCallable, Category = "Raid")
    bool ExtractRaid();

    UFUNCTION(BlueprintCallable, Category = "Raid")
    bool IsAtExtractionPoint() const;

    UFUNCTION(BlueprintCallable, Category = "Raid")
    void FailRaid();

    void AdvanceRaidWorldTick();

#if WITH_DEV_AUTOMATION_TESTS
    bool bHasForcedRaidStartPointForTest = false;
    FIntPoint ForcedRaidStartPointForTest = FIntPoint::ZeroValue;
#endif

    UFUNCTION(BlueprintCallable, Category = "Raid|Ambush")
    bool RequestPlayerAmbush();

    UFUNCTION(BlueprintCallable, Category = "Raid|Ambush")
    bool RequestAmbushWait();

    UFUNCTION(BlueprintCallable, Category = "Raid|Ambush")
    bool RequestAmbushCancel();

    UFUNCTION(BlueprintCallable, Category = "Raid|Ambush")
    bool RequestAmbushLetPass();

    UFUNCTION(BlueprintCallable, Category = "Raid|Ambush")
    bool RequestAmbushAssault();

    UFUNCTION(BlueprintCallable, Category = "Raid|Ambush")
    bool RequestAmbushSearch();

    UFUNCTION(BlueprintCallable, Category = "Raid|Ambush")
    bool RequestAmbushCover();

    UFUNCTION(BlueprintCallable, Category = "Raid|Ambush")
    bool RequestAmbushFlee();

    bool TryRestorePreviousPlayerCoord();
    bool RequestPlayerCardinalMove(FIntPoint Delta);
    bool MovePlayerDuringCombat(FIntPoint NewCoordinate);

    bool RequestSearchDeadBody();
    bool HasDeadBodyAtCurrentPlayerCoord() const;
    bool FindSearchableDeadBodyAt(FIntPoint Coordinate, FName& OutInstanceID) const;
    bool FindCorpseLootInventory(FName ItemID, UGridInventoryComponent*& OutInventory) const;
    const TMap<FName, UGridInventoryComponent*>& GetCorpseLootInventories() const;
    int32 GetCorpseLootGenerationCount(FName EnemyInstanceID) const;
    void InvalidateCorpseSearchIfPlayerLeftTile();

#if WITH_DEV_AUTOMATION_TESTS
    void GameTimerUpdateForTest();
    void SearchPhaseCompleteForTest();
    bool SeedCorpseLootForTest(FName EnemyInstanceID, const FItemData& ItemData);
    int32 GetPendingExamineCountForTest() const;
    void ProcessNextExamineForTest();
#endif

    FName MakeUniqueInstanceID(FName PreferredID) const;

    bool ReconfigureStorageForEquipmentSlot(FName SlotID, const UItemInstance* StorageItem);
    bool TryStandaloneStorageUnequip(FName SlotID, UItemInstance* Item);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ApplyPlayerDamage(int32 DamageAmount);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    class UGridInventoryComponent* InventoryComponent;

    // 루트 컨테이너용 인벤토리 컴포넌트 추가
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    class UGridInventoryComponent* LootContainerComponent;

    // 안전 금고(SafeBox)용 인벤토리 컴포넌트 추가
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    class UGridInventoryComponent* SafeBoxComponent;

    // 조끼(Rig)용 인벤토리 컴포넌트 추가
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    class UGridInventoryComponent* RigComponent;

    // 주머니(Pocket)용 인벤토리 컴포넌트 추가
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    class UGridInventoryComponent* PocketComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stash")
    class UGridInventoryComponent* StashComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stash")
    FString StashSaveSlot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Progression")
    int64 RetirementBalance = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment")
    class UEquipmentComponent* EquipmentComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    UCombatComponent* CombatComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy World")
    UEnemyManagerComponent* EnemyManagerComponent;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void HandlePlayerMoved(FIntPoint NewCoordinate);
    void RefreshEnemyWorldUI();

    // [탐색] 버튼을 눌렀을 때 서칭 시퀀스 시작
    UFUNCTION()
    void StartContainerSearch();

    void InvalidateContainerSearchIfPlayerLeftTile();

protected:
    UPROPERTY()
    class UMainGameUI* MainUI;

    FTimerHandle GameTimerHandle;

    // 서칭 페이즈 타이머
    FTimerHandle SearchPhaseTimer;
    FTimerHandle ExamineTimer;

    FIntPoint ActiveContainerSearchCoord = FIntPoint::ZeroValue;
    bool bHasActiveContainerSearch = false;

    UPROPERTY()
    TMap<FName, UGridInventoryComponent*> CorpseLootInventories;

    UPROPERTY()
    TMap<FName, int32> CorpseLootGenerationCounts;

    FName ActiveCorpseInstanceID = NAME_None;
    FIntPoint ActiveCorpseSearchCoord = FIntPoint::ZeroValue;

    void ClearCorpseLoot();
    bool EnsureCorpseLootGenerated(FName EnemyInstanceID);
    void BindLootInventoryToUI(UGridInventoryComponent* Inventory);
    
    // 식별 대기열
    TArray<FName> ItemsToExamine;

    UPROPERTY()
    UGridInventoryComponent* ActiveExamineInventory = nullptr;

    UFUNCTION()
    void GameTimerUpdate();

    UFUNCTION()
    void OnSearchPhaseComplete(); // 1초 분석 후 실루엣 생성

    UFUNCTION()
    void ProcessNextExamine(); // 0.5초마다 아이템 1개씩 식별 완료 처리
};
