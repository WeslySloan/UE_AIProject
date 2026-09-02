#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GridGameMode.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameStateChanged);

class UGridInventoryComponent;
class UEquipmentComponent;
class UMainGameUI;
class UItemDataTable;
class UMapManagerComponent;

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

    UPROPERTY(BlueprintAssignable, Category = "Game|Events")
    FOnGameStateChanged OnGameStateChanged;

    UFUNCTION(BlueprintCallable, Category = "Game")
    void AddScore(int32 Amount);

    UFUNCTION(BlueprintCallable, Category = "Game")
    void CheckWinCondition();

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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment")
    class UEquipmentComponent* EquipmentComponent;

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
