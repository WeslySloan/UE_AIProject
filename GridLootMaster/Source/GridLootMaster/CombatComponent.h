#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatComponent.generated.h"

USTRUCT(BlueprintType)
struct GRIDLOOTMASTER_API FEnemyDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    FName EnemyID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    int32 MaxHealth = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    int32 AttackDamage = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    int32 AccuracyPercent = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    int32 Armor = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
    int32 Reward = 100;
};

USTRUCT(BlueprintType)
struct GRIDLOOTMASTER_API FEnemyInstanceData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
    FEnemyDefinition Definition;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
    int32 CurrentHealth = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
    bool bIsDefeated = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCombatStateChanged);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class GRIDLOOTMASTER_API UCombatComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCombatComponent();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    FEnemyInstanceData CurrentEnemy;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    bool bHasActiveEnemy = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    FString LastCombatMessage;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnCombatStateChanged OnCombatStateChanged;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void SpawnEnemy(const FEnemyDefinition& EnemyDefinition);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool AttackEnemy(int32 DamageAmount);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void EnemyAttackPlayer();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ClearEnemy();
};
