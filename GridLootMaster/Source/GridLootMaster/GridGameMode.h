#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GridGameMode.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameStateChanged);

UCLASS()
class GRIDLOOTMASTER_API AGridGameMode : public AGameModeBase
{
    GENERATED_BODY()
    
public:
    AGridGameMode();

protected:
    virtual void BeginPlay() override;

public:
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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UGridInventoryComponent* InventoryComponent;

    UPROPERTY()
    class UMainGameUI* MainUI;

    // 아이템 스폰 딜레이 (초)
    float SpawnDelay = 2.0f;
    FTimerHandle SpawnTimerHandle;

    UFUNCTION()
    void SpawnRandomItem();

protected:
    // 1초마다 타이머 감소
    void GameTimerUpdate();

    FTimerHandle GameTimerHandle;
};
