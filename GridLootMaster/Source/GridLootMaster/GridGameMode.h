#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
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

class UGridInventoryComponent;
class UEquipmentComponent;
class UMainGameUI;
class UItemInstance;
class UItemDataTable;
class UMapManagerComponent;
class UCombatComponent;

UCLASS()
class GRIDLOOTMASTER_API AGridGameMode : public AGameModeBase
{
    GENERATED_BODY()
    
public:
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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map")
    FIntPoint CurrentPlayerCoord;

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

    UFUNCTION(BlueprintCallable, Category = "Stash")
    bool SaveStash();

    UFUNCTION(BlueprintCallable, Category = "Stash")
    bool LoadStash();

    UFUNCTION(BlueprintCallable, Category = "Raid")
    bool ExtractRaid();

    UFUNCTION(BlueprintCallable, Category = "Raid")
    bool IsAtExtractionPoint() const;

    UFUNCTION(BlueprintCallable, Category = "Raid")
    void FailRaid();

#if WITH_DEV_AUTOMATION_TESTS
    void GameTimerUpdateForTest();
    void SearchPhaseCompleteForTest();
#endif

    FName MakeUniqueInstanceID(FName PreferredID) const;

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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment")
    class UEquipmentComponent* EquipmentComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    UCombatComponent* CombatComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    int32 EncounterChancePercent = 25;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void HandlePlayerMoved(FIntPoint NewCoordinate);

    // [탐색] 버튼을 눌렀을 때 서칭 시퀀스 시작
    UFUNCTION()
    void StartContainerSearch();

protected:
    UPROPERTY()
    class UMainGameUI* MainUI;

    FTimerHandle GameTimerHandle;

    // 서칭 페이즈 타이머
    FTimerHandle SearchPhaseTimer;
    FTimerHandle ExamineTimer;
    
    // 식별 대기열
    TArray<FName> ItemsToExamine;

    UFUNCTION()
    void GameTimerUpdate();

    UFUNCTION()
    void OnSearchPhaseComplete(); // 1초 분석 후 실루엣 생성

    UFUNCTION()
    void ProcessNextExamine(); // 0.5초마다 아이템 1개씩 식별 완료 처리
};
