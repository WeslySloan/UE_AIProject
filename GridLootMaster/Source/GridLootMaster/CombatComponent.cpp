#include "CombatComponent.h"
#include "GridGameMode.h"

UCombatComponent::UCombatComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UCombatComponent::SpawnEnemy(const FEnemyDefinition& EnemyDefinition)
{
    if (AGridGameMode* GM = Cast<AGridGameMode>(GetOwner()))
    {
        if (GM->RaidState != ERaidState::InRaid) return;
    }

    if (bHasActiveEnemy || EnemyDefinition.EnemyID.IsNone()) return;

    CurrentEnemy.Definition = EnemyDefinition;
    CurrentEnemy.Definition.MaxHealth = FMath::Max(1, CurrentEnemy.Definition.MaxHealth);
    CurrentEnemy.Definition.AttackDamage = FMath::Max(0, CurrentEnemy.Definition.AttackDamage);
    CurrentEnemy.Definition.AccuracyPercent = FMath::Clamp(CurrentEnemy.Definition.AccuracyPercent, 0, 100);
    CurrentEnemy.Definition.Armor = FMath::Max(0, CurrentEnemy.Definition.Armor);
    CurrentEnemy.CurrentHealth = CurrentEnemy.Definition.MaxHealth;
    CurrentEnemy.bIsDefeated = false;
    bHasActiveEnemy = true;
    LastCombatMessage = FString::Printf(TEXT("적이 나타났다!! %s"), *CurrentEnemy.Definition.DisplayName);
    OnCombatStateChanged.Broadcast();
}

bool UCombatComponent::AttackEnemy(int32 DamageAmount)
{
    if (AGridGameMode* GM = Cast<AGridGameMode>(GetOwner()))
    {
        if (GM->RaidState != ERaidState::InRaid) return false;
    }

    if (!bHasActiveEnemy || DamageAmount <= 0) return false;

    const int32 AppliedDamage = FMath::Max(1, DamageAmount - CurrentEnemy.Definition.Armor);
    CurrentEnemy.CurrentHealth = FMath::Max(0, CurrentEnemy.CurrentHealth - AppliedDamage);
    LastCombatMessage = FString::Printf(TEXT("플레이어가 %d 피해를 입혔습니다."), AppliedDamage);
    if (CurrentEnemy.CurrentHealth == 0)
    {
        CurrentEnemy.bIsDefeated = true;
        bHasActiveEnemy = false;
        LastCombatMessage += TEXT(" 적을 처치했습니다.");

        if (AGridGameMode* GM = Cast<AGridGameMode>(GetOwner()))
        {
            GM->AddScore(CurrentEnemy.Definition.Reward);
        }
    }

    OnCombatStateChanged.Broadcast();
    return true;
}

void UCombatComponent::EnemyAttackPlayer()
{
    if (AGridGameMode* GM = Cast<AGridGameMode>(GetOwner()))
    {
        if (GM->RaidState != ERaidState::InRaid) return;
    }

    if (!bHasActiveEnemy) return;

    if (FMath::RandRange(1, 100) > CurrentEnemy.Definition.AccuracyPercent)
    {
        LastCombatMessage = TEXT("적의 반격이 빗나갔습니다.");
        OnCombatStateChanged.Broadcast();
        return;
    }

    LastCombatMessage = FString::Printf(TEXT("적의 반격! %d 피해를 입었습니다."), CurrentEnemy.Definition.AttackDamage);

    if (AGridGameMode* GM = Cast<AGridGameMode>(GetOwner()))
    {
        GM->ApplyPlayerDamage(CurrentEnemy.Definition.AttackDamage);
        if (GM->RaidState == ERaidState::InRaid)
        {
            OnCombatStateChanged.Broadcast();
        }
    }
}

void UCombatComponent::ClearEnemy()
{
    CurrentEnemy = FEnemyInstanceData();
    bHasActiveEnemy = false;
    LastCombatMessage = TEXT("전투가 종료되었습니다.");
    OnCombatStateChanged.Broadcast();
}
