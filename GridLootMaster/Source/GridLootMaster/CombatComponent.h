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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat")
    float AttackIntervalSeconds = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat")
    float ReactionTimeSeconds = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat")
    int32 AttackRangeTiles = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Detection")
    int32 VisionRangeTiles = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Detection")
    int32 DetectionPower = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Detection")
    int32 Stealth = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Ambush")
    int32 AmbushRangeTiles = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Ambush")
    int32 AmbushPower = 50;
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

UENUM(BlueprintType)
enum class ECombatPlayerActionState : uint8
{
    None,
    Swapping,
    Reloading
};

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Timing")
    float PlayerAttackIntervalSeconds = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Timing")
    int32 PlayerAccuracyPercent = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Range")
    int32 CombatRangeTiles = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Recoil")
    float PlayerRecoilPerShot = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Recoil")
    float PlayerRecoilRecoveryPerSecond = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Recoil")
    float CurrentRecoil = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Timing")
    float PlayerAttackCooldownRemaining = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Timing")
    float EnemyAttackCooldownRemaining = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Action")
    ECombatPlayerActionState PlayerActionState = ECombatPlayerActionState::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Action")
    float PlayerActionTimeRemaining = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Weapon")
    FName ActiveWeaponSlot = TEXT("Primary1");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Result")
    bool bLastPlayerAttackHit = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Initiative")
    bool bPlayerHasInitiative = false;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnCombatStateChanged OnCombatStateChanged;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void SpawnEnemy(const FEnemyDefinition& EnemyDefinition, bool bGrantPlayerInitiative = false);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool AttackEnemy(int32 DamageAmount);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool RequestPlayerAttack(int32 DamageAmount, int32 AccuracyOverride = -1,
        float AttackIntervalOverride = -1.0f, int32 RangeOverride = -1,
        float RecoilPerShotOverride = -1.0f, float RecoilRecoveryOverride = -1.0f,
        int32 OptimalRangeOverride = -1);

    UFUNCTION(BlueprintCallable, Category = "Combat|Weapon")
    bool RequestWeaponSwap(FName TargetSlot);

    UFUNCTION(BlueprintCallable, Category = "Combat|Weapon")
    bool RequestReload(FName WeaponSlot = NAME_None);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void EnemyAttackPlayer(float DamageMultiplier = 1.0f);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ClearEnemy();

#if WITH_DEV_AUTOMATION_TESTS
    void AdvanceCombatTimeForTest(float DeltaTime);
#endif

protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    void AdvanceCombatTime(float DeltaTime);
    void CompletePlayerAction();
    void CancelPlayerAction(const FString& Message);
    bool IsCombatRangeValid(int32 RangeTiles) const;

    UPROPERTY()
    FName PendingWeaponSlot = NAME_None;

    UPROPERTY()
    FName PendingReloadAmmoID = NAME_None;

    UPROPERTY()
    TObjectPtr<class UItemInstance> PendingReloadWeapon;
};
