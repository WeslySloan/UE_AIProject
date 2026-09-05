#include "CombatComponent.h"
#include "GridGameMode.h"
#include "EnemyManagerComponent.h"
#include "Map/MapManagerComponent.h"
#include "EquipmentComponent.h"
#include "GridInventoryComponent.h"
#include "ItemInstance.h"

UCombatComponent::UCombatComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    AdvanceCombatTime(DeltaTime);
}

void UCombatComponent::SpawnEnemy(const FEnemyDefinition& EnemyDefinition, bool bGrantPlayerInitiative)
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
    CurrentEnemy.Definition.AttackIntervalSeconds = FMath::Max(0.1f, CurrentEnemy.Definition.AttackIntervalSeconds);
    CurrentEnemy.Definition.ReactionTimeSeconds = FMath::Max(0.0f, CurrentEnemy.Definition.ReactionTimeSeconds);
    CurrentEnemy.Definition.AttackRangeTiles = FMath::Max(0, CurrentEnemy.Definition.AttackRangeTiles);
    CurrentEnemy.CurrentHealth = CurrentEnemy.Definition.MaxHealth;
    CurrentEnemy.bIsDefeated = false;
    bHasActiveEnemy = true;
    PlayerAttackCooldownRemaining = 0.0f;
    EnemyAttackCooldownRemaining = CurrentEnemy.Definition.ReactionTimeSeconds;
    bLastPlayerAttackHit = false;
    bPlayerHasInitiative = bGrantPlayerInitiative;
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
            if (GM->EnemyManagerComponent)
            {
                GM->EnemyManagerComponent->SyncCombatContact();
            }
        }
    }

    OnCombatStateChanged.Broadcast();
    return true;
}

bool UCombatComponent::RequestPlayerAttack(int32 DamageAmount, int32 AccuracyOverride,
    float AttackIntervalOverride, int32 RangeOverride,
    float RecoilPerShotOverride, float RecoilRecoveryOverride,
    int32 OptimalRangeOverride)
{
    int32 DistancePenalty = 0;
    if (AGridGameMode* GM = Cast<AGridGameMode>(GetOwner()))
    {
        if (GM->RaidState != ERaidState::InRaid) return false;
    }

    if (PlayerActionState != ECombatPlayerActionState::None)
    {
        LastCombatMessage = TEXT("무기 교체 또는 재장전 중에는 공격할 수 없습니다.");
        OnCombatStateChanged.Broadcast();
        return false;
    }

    if (!bHasActiveEnemy || DamageAmount <= 0 || PlayerAttackCooldownRemaining > KINDA_SMALL_NUMBER)
    {
        if (PlayerAttackCooldownRemaining > KINDA_SMALL_NUMBER)
        {
            LastCombatMessage = TEXT("공격이 아직 준비되지 않았습니다.");
            OnCombatStateChanged.Broadcast();
        }
        return false;
    }

    if (AGridGameMode* GM = Cast<AGridGameMode>(GetOwner()))
    {
        if (GM->EnemyManagerComponent && !GM->EnemyManagerComponent->GetActiveEnemyInstanceID().IsNone())
        {
            FIntPoint EnemyCoordinate;
            const bool bHasEnemyCoordinate = GM->EnemyManagerComponent->GetActiveEnemyCoordinate(EnemyCoordinate);
            const int32 Distance = bHasEnemyCoordinate && GM->MapManagerComponent
                ? GM->MapManagerComponent->GetTileDistance(GM->CurrentPlayerCoord, EnemyCoordinate)
                : MAX_int32;
            if (!bHasEnemyCoordinate || !GM->MapManagerComponent ||
                Distance > FMath::Max(0, RangeOverride >= 0 ? RangeOverride : CombatRangeTiles) ||
                !GM->MapManagerComponent->HasLineOfSight(GM->CurrentPlayerCoord, EnemyCoordinate))
            {
                LastCombatMessage = TEXT("적이 공격 사거리 또는 시야 밖에 있습니다.");
                OnCombatStateChanged.Broadcast();
                return false;
            }

            if (OptimalRangeOverride >= 0)
            {
                DistancePenalty = FMath::Max(0, Distance - OptimalRangeOverride) * 15;
            }
        }
    }

    const float AttackInterval = AttackIntervalOverride > 0.0f
        ? AttackIntervalOverride
        : PlayerAttackIntervalSeconds;
    const int32 AccuracyPercent = AccuracyOverride >= 0
        ? AccuracyOverride
        : PlayerAccuracyPercent;
    const float RecoilPerShot = RecoilPerShotOverride >= 0.0f
        ? RecoilPerShotOverride
        : PlayerRecoilPerShot;
    if (RecoilRecoveryOverride >= 0.0f)
    {
        PlayerRecoilRecoveryPerSecond = FMath::Max(0.0f, RecoilRecoveryOverride);
    }
    PlayerAttackCooldownRemaining = FMath::Max(0.1f, AttackInterval);
    bLastPlayerAttackHit = false;

    const int32 EffectiveAccuracyPercent = FMath::Clamp(
        FMath::RoundToInt(static_cast<float>(AccuracyPercent - DistancePenalty) - CurrentRecoil), 0, 100);
    if (FMath::RandRange(1, 100) > EffectiveAccuracyPercent)
    {
        LastCombatMessage = TEXT("플레이어의 공격이 빗나갔습니다.");
        OnCombatStateChanged.Broadcast();
        CurrentRecoil = FMath::Clamp(CurrentRecoil + FMath::Max(0.0f, RecoilPerShot), 0.0f, 100.0f);
        return true;
    }

    if (!AttackEnemy(DamageAmount))
    {
        PlayerAttackCooldownRemaining = 0.0f;
        return false;
    }

    bLastPlayerAttackHit = true;
    CurrentRecoil = FMath::Clamp(CurrentRecoil + FMath::Max(0.0f, RecoilPerShot), 0.0f, 100.0f);
    return true;
}

bool UCombatComponent::RequestWeaponSwap(FName TargetSlot)
{
    AGridGameMode* GM = Cast<AGridGameMode>(GetOwner());
    if (!GM || !GM->EquipmentComponent || TargetSlot == NAME_None)
    {
        return false;
    }

    if (GM->PlayerPosture == EPlayerRaidPosture::Ambushing ||
        (GM->EnemyManagerComponent && GM->EnemyManagerComponent->HasActiveAmbushReaction()))
    {
        LastCombatMessage = TEXT("매복 중에는 무기를 교체할 수 없습니다.");
        OnCombatStateChanged.Broadcast();
        return false;
    }

    if (TargetSlot == ActiveWeaponSlot)
    {
        return false;
    }

    if (PlayerActionState != ECombatPlayerActionState::None)
    {
        LastCombatMessage = TEXT("현재 다른 행동을 처리 중입니다.");
        OnCombatStateChanged.Broadcast();
        return false;
    }

    UItemInstance* TargetWeapon = GM->EquipmentComponent->GetEquippedItem(TargetSlot);
    if (!TargetWeapon || TargetWeapon->Category != EItemCategory::Weapon)
    {
        LastCombatMessage = TEXT("교체할 무기가 없습니다.");
        OnCombatStateChanged.Broadcast();
        return false;
    }

    // 비전투에서는 기존 동작처럼 즉시 교체한다.
    if (!bHasActiveEnemy)
    {
        ActiveWeaponSlot = TargetSlot;
        LastCombatMessage = FString::Printf(TEXT("무기 교체 완료: %s"), *TargetWeapon->ItemName);
        OnCombatStateChanged.Broadcast();
        return true;
    }

    const float SwapDuration = FMath::Max(0.0f, TargetWeapon->SwapTimeSeconds);
    if (SwapDuration <= KINDA_SMALL_NUMBER)
    {
        ActiveWeaponSlot = TargetSlot;
        LastCombatMessage = FString::Printf(TEXT("무기 교체 완료: %s"), *TargetWeapon->ItemName);
        OnCombatStateChanged.Broadcast();
        return true;
    }

    PlayerActionState = ECombatPlayerActionState::Swapping;
    PlayerActionTimeRemaining = SwapDuration;
    PendingWeaponSlot = TargetSlot;
    LastCombatMessage = FString::Printf(TEXT("무기 교체 중... %.1f초"), SwapDuration);
    OnCombatStateChanged.Broadcast();
    return true;
}

bool UCombatComponent::RequestReload(FName WeaponSlot)
{
    AGridGameMode* GM = Cast<AGridGameMode>(GetOwner());
    if (!GM || GM->RaidState != ERaidState::InRaid || !GM->EquipmentComponent ||
        !GM->RigComponent || !GM->PocketComponent || PlayerActionState != ECombatPlayerActionState::None)
    {
        return false;
    }

    if (GM->PlayerPosture == EPlayerRaidPosture::Ambushing ||
        (GM->EnemyManagerComponent && GM->EnemyManagerComponent->HasActiveAmbushReaction()))
    {
        LastCombatMessage = TEXT("매복 중에는 재장전할 수 없습니다.");
        OnCombatStateChanged.Broadcast();
        return false;
    }

    const FName RequestedSlot = WeaponSlot == NAME_None ? ActiveWeaponSlot : WeaponSlot;
    UItemInstance* Weapon = GM->EquipmentComponent->GetEquippedItem(RequestedSlot);
    if (!Weapon || Weapon->Category != EItemCategory::Weapon ||
        Weapon->WeaponAttackType != EWeaponAttackType::Firearm)
    {
        LastCombatMessage = TEXT("재장전할 총기 또는 탄창이 없습니다.");
        OnCombatStateChanged.Broadcast();
        return false;
    }

    struct FReloadCandidate
    {
        UGridInventoryComponent* Source = nullptr;
        UItemInstance* Magazine = nullptr;
        FIntPoint GridCoord = FIntPoint::ZeroValue;
        int32 SectionIndex = 0;
        int32 SourcePriority = 0;
    };

    FReloadCandidate BestCandidate;
    bool bHasCandidate = false;
    const TArray<UGridInventoryComponent*> Sources = { GM->RigComponent, GM->PocketComponent };
    for (int32 SourceIndex = 0; SourceIndex < Sources.Num(); ++SourceIndex)
    {
        UGridInventoryComponent* Source = Sources[SourceIndex];
        if (!Source) continue;

        for (const TPair<FName, UItemInstance*>& Pair : Source->ItemInstances)
        {
            UItemInstance* CandidateMagazine = Pair.Value;
            if (!CandidateMagazine || !Weapon->IsCompatibleMagazine(CandidateMagazine)) continue;

            int32 CandidateSection = INDEX_NONE;
            FIntPoint CandidateCoord = FIntPoint::ZeroValue;
            if (!Source->FindItemPlacement(CandidateMagazine->InstanceID, CandidateSection, CandidateCoord.X, CandidateCoord.Y)) continue;
            const bool bBetter = !bHasCandidate ||
                CandidateMagazine->CurrentAmmo > BestCandidate.Magazine->CurrentAmmo ||
                (CandidateMagazine->CurrentAmmo == BestCandidate.Magazine->CurrentAmmo &&
                    (SourceIndex < BestCandidate.SourcePriority ||
                        (SourceIndex == BestCandidate.SourcePriority &&
                            (CandidateSection < BestCandidate.SectionIndex ||
                                (CandidateSection == BestCandidate.SectionIndex && CandidateCoord.Y < BestCandidate.GridCoord.Y) ||
                                (CandidateSection == BestCandidate.SectionIndex &&
                                CandidateCoord.Y == BestCandidate.GridCoord.Y &&
                                    (CandidateCoord.X < BestCandidate.GridCoord.X ||
                                        (CandidateCoord == BestCandidate.GridCoord &&
                                            CandidateMagazine->InstanceID.ToString() < BestCandidate.Magazine->InstanceID.ToString())))))));
            if (bBetter)
            {
                BestCandidate = { Source, CandidateMagazine, CandidateCoord, CandidateSection, SourceIndex };
                bHasCandidate = true;
            }
        }
    }

    UItemInstance* CurrentMagazine = Weapon->EquippedMagazine;
    if (!bHasCandidate || (CurrentMagazine && CurrentMagazine->CurrentAmmo >= CurrentMagazine->MaxAmmo &&
        CurrentMagazine->CurrentAmmo >= BestCandidate.Magazine->CurrentAmmo))
    {
        LastCombatMessage = CurrentMagazine && CurrentMagazine->CurrentAmmo >= CurrentMagazine->MaxAmmo
            ? TEXT("더 나은 호환 예비 탄창이 없습니다.")
            : TEXT("호환되는 예비 탄창이 없습니다.");
        OnCombatStateChanged.Broadcast();
        return false;
    }

    PendingReloadWeapon = Weapon;
    PendingWeaponSlot = RequestedSlot;
    PendingReloadMagazineID = BestCandidate.Magazine->InstanceID;
    PendingReloadSource = BestCandidate.Source;
    PendingReloadSectionIndex = BestCandidate.SectionIndex;
    PendingReloadGridCoord = BestCandidate.GridCoord;
    PlayerActionState = ECombatPlayerActionState::Reloading;
    PlayerActionTimeRemaining = FMath::Max(0.0f, Weapon->ReloadTimeSeconds);
    LastCombatMessage = PlayerActionTimeRemaining > KINDA_SMALL_NUMBER
        ? FString::Printf(TEXT("재장전 중... %.1f초"), PlayerActionTimeRemaining)
        : TEXT("재장전 중...");
    OnCombatStateChanged.Broadcast();

    if (PlayerActionTimeRemaining <= KINDA_SMALL_NUMBER)
    {
        CompletePlayerAction();
    }
    return true;
}

bool UCombatComponent::RequestCombatMovement(ECombatMovementAction MovementAction)
{
    AGridGameMode* GM = Cast<AGridGameMode>(GetOwner());
    if (!GM || GM->RaidState != ERaidState::InRaid || !bHasActiveEnemy ||
        PlayerActionState != ECombatPlayerActionState::None || !GM->MapManagerComponent ||
        !GM->EnemyManagerComponent)
    {
        return false;
    }

    FIntPoint EnemyCoordinate;
    if (!GM->EnemyManagerComponent->GetActiveEnemyCoordinate(EnemyCoordinate)) return false;

    UItemInstance* Weapon = GM->EquipmentComponent
        ? GM->EquipmentComponent->GetEquippedItem(ActiveWeaponSlot) : nullptr;
    const TArray<FIntPoint> Candidates = {
        GM->CurrentPlayerCoord + FIntPoint(1, 0), GM->CurrentPlayerCoord + FIntPoint(-1, 0),
        GM->CurrentPlayerCoord + FIntPoint(0, 1), GM->CurrentPlayerCoord + FIntPoint(0, -1) };
    FIntPoint BestCoord = FIntPoint::ZeroValue;
    bool bHasBest = false;
    const int32 CurrentDistance = GM->MapManagerComponent->GetTileDistance(GM->CurrentPlayerCoord, EnemyCoordinate);
    const bool bCurrentCanAttack = Weapon &&
        GM->MapManagerComponent->GetTileDistance(GM->CurrentPlayerCoord, EnemyCoordinate) <= Weapon->MaxRangeTiles &&
        GM->MapManagerComponent->HasLineOfSight(GM->CurrentPlayerCoord, EnemyCoordinate);

    for (const FIntPoint& Candidate : Candidates)
    {
        if (Candidate == EnemyCoordinate || !GM->MapManagerComponent->CanMoveBetween(GM->CurrentPlayerCoord, Candidate)) continue;
        const int32 CandidateDistance = GM->MapManagerComponent->GetTileDistance(Candidate, EnemyCoordinate);
        const bool bCanAttack = Weapon && CandidateDistance <= Weapon->MaxRangeTiles &&
            GM->MapManagerComponent->HasLineOfSight(Candidate, EnemyCoordinate);
        bool bBetter = false;
        if (MovementAction == ECombatMovementAction::Approach)
        {
            bBetter = (!bCurrentCanAttack && bCanAttack) ||
                ((!bCurrentCanAttack || !bCanAttack) && CandidateDistance < CurrentDistance);
        }
        else if (MovementAction == ECombatMovementAction::Retreat)
        {
            const bool bCandidateBreaksLOS = !GM->MapManagerComponent->HasLineOfSight(Candidate, EnemyCoordinate);
            const bool bBestBreaksLOS = bHasBest && !GM->MapManagerComponent->HasLineOfSight(BestCoord, EnemyCoordinate);
            bBetter = !bHasBest || CandidateDistance > GM->MapManagerComponent->GetTileDistance(BestCoord, EnemyCoordinate) ||
                (CandidateDistance == GM->MapManagerComponent->GetTileDistance(BestCoord, EnemyCoordinate) && bCandidateBreaksLOS && !bBestBreaksLOS);
        }
        else
        {
            const bool bCandidateBreaksLOS = !GM->MapManagerComponent->HasLineOfSight(Candidate, EnemyCoordinate);
            const bool bBestBreaksLOS = bHasBest && !GM->MapManagerComponent->HasLineOfSight(BestCoord, EnemyCoordinate);
            bBetter = !bHasBest || (bCandidateBreaksLOS && !bBestBreaksLOS) ||
                (bCandidateBreaksLOS == bBestBreaksLOS && CandidateDistance > GM->MapManagerComponent->GetTileDistance(BestCoord, EnemyCoordinate));
        }
        if (bBetter)
        {
            BestCoord = Candidate;
            bHasBest = true;
        }
    }

    if (!bHasBest)
    {
        LastCombatMessage = TEXT("이동할 수 있는 전투 타일이 없습니다.");
        OnCombatStateChanged.Broadcast();
        return false;
    }

    PendingCombatMovement = MovementAction;
    PendingCombatMoveCoord = BestCoord;
    PlayerActionState = ECombatPlayerActionState::Moving;
    PlayerActionTimeRemaining = FMath::Max(0.0f, CombatMoveActionDuration);
    LastCombatMessage = TEXT("전투 이동 중...");
    OnCombatStateChanged.Broadcast();
    if (PlayerActionTimeRemaining <= KINDA_SMALL_NUMBER) CompletePlayerAction();
    return true;
}

bool UCombatComponent::RequestCombatMovementDirection(ECombatMovementAction MovementAction,
    ECombatMovementDirection Direction)
{
    AGridGameMode* GM = Cast<AGridGameMode>(GetOwner());
    if (!GM || GM->RaidState != ERaidState::InRaid || !bHasActiveEnemy ||
        PlayerActionState != ECombatPlayerActionState::None || !GM->MapManagerComponent)
    {
        return false;
    }

    FIntPoint Delta = FIntPoint::ZeroValue;
    switch (Direction)
    {
        case ECombatMovementDirection::North: Delta = FIntPoint(0, -1); break;
        case ECombatMovementDirection::West: Delta = FIntPoint(-1, 0); break;
        case ECombatMovementDirection::East: Delta = FIntPoint(1, 0); break;
        case ECombatMovementDirection::South: Delta = FIntPoint(0, 1); break;
        default: return false;
    }

    const FIntPoint CandidateCoord = GM->CurrentPlayerCoord + Delta;
    if (!GM->MapManagerComponent->CanMoveBetween(GM->CurrentPlayerCoord, CandidateCoord))
    {
        LastCombatMessage = TEXT("선택한 방향으로 이동할 수 없습니다.");
        OnCombatStateChanged.Broadcast();
        return false;
    }

    PendingCombatMovement = MovementAction;
    PendingCombatMoveCoord = CandidateCoord;
    PlayerActionState = ECombatPlayerActionState::Moving;
    PlayerActionTimeRemaining = FMath::Max(0.0f, CombatMoveActionDuration);
    LastCombatMessage = MovementAction == ECombatMovementAction::Flee
        ? TEXT("FLEE 이동 중...") : TEXT("MOVE 이동 중...");
    OnCombatStateChanged.Broadcast();
    if (PlayerActionTimeRemaining <= KINDA_SMALL_NUMBER) CompletePlayerAction();
    return true;
}

void UCombatComponent::EnemyAttackPlayer(float DamageMultiplier)
{
    if (AGridGameMode* GM = Cast<AGridGameMode>(GetOwner()))
    {
        if (GM->RaidState != ERaidState::InRaid) return;
        FIntPoint EnemyCoordinate;
        if (GM->EnemyManagerComponent && GM->MapManagerComponent &&
            GM->EnemyManagerComponent->GetActiveEnemyCoordinate(EnemyCoordinate) &&
            (GM->MapManagerComponent->GetTileDistance(EnemyCoordinate, GM->CurrentPlayerCoord) > CurrentEnemy.Definition.AttackRangeTiles ||
                !GM->MapManagerComponent->HasLineOfSight(EnemyCoordinate, GM->CurrentPlayerCoord)))
        {
            LastCombatMessage = TEXT("적이 공격 사거리 또는 시야 밖에 있습니다.");
            OnCombatStateChanged.Broadcast();
            return;
        }
    }

    if (!bHasActiveEnemy) return;

    if (FMath::RandRange(1, 100) > CurrentEnemy.Definition.AccuracyPercent)
    {
        LastCombatMessage = TEXT("적의 반격이 빗나갔습니다.");
        OnCombatStateChanged.Broadcast();
        return;
    }

    const int32 ModifiedDamage = FMath::Max(1, FMath::CeilToInt(
        static_cast<float>(CurrentEnemy.Definition.AttackDamage) * FMath::Clamp(DamageMultiplier, 0.0f, 1.0f)));
    LastCombatMessage = FString::Printf(TEXT("적의 반격! %d 피해를 입었습니다."), ModifiedDamage);

    if (AGridGameMode* GM = Cast<AGridGameMode>(GetOwner()))
    {
        GM->ApplyPlayerDamage(ModifiedDamage);
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
    PlayerAttackCooldownRemaining = 0.0f;
    EnemyAttackCooldownRemaining = 0.0f;
    bLastPlayerAttackHit = false;
    bPlayerHasInitiative = false;
    CurrentRecoil = 0.0f;
    PlayerActionState = ECombatPlayerActionState::None;
    PlayerActionTimeRemaining = 0.0f;
    PendingWeaponSlot = NAME_None;
    PendingReloadMagazineID = NAME_None;
    PendingReloadSource = nullptr;
    PendingReloadSectionIndex = 0;
    PendingReloadGridCoord = FIntPoint::ZeroValue;
    PendingReloadWeapon = nullptr;
    LastCombatMessage = TEXT("전투가 종료되었습니다.");
    OnCombatStateChanged.Broadcast();
}

void UCombatComponent::AdvanceCombatTime(float DeltaTime)
{
    AGridGameMode* GM = Cast<AGridGameMode>(GetOwner());
    if (!GM || GM->RaidState != ERaidState::InRaid)
    {
        return;
    }

    const float SafeDeltaTime = FMath::Max(0.0f, DeltaTime);

    if (PlayerActionState != ECombatPlayerActionState::None)
    {
        PlayerActionTimeRemaining = FMath::Max(0.0f, PlayerActionTimeRemaining - SafeDeltaTime);
        if (PlayerActionTimeRemaining <= KINDA_SMALL_NUMBER)
        {
            CompletePlayerAction();
        }
    }

    if (!bHasActiveEnemy)
    {
        return;
    }

    PlayerAttackCooldownRemaining = FMath::Max(0.0f, PlayerAttackCooldownRemaining - SafeDeltaTime);
    EnemyAttackCooldownRemaining = FMath::Max(0.0f, EnemyAttackCooldownRemaining - SafeDeltaTime);
    const float RecoilRecovery = PlayerRecoilRecoveryPerSecond;
    CurrentRecoil = FMath::Max(0.0f, CurrentRecoil - FMath::Max(0.0f, RecoilRecovery) * SafeDeltaTime);

    if (EnemyAttackCooldownRemaining <= KINDA_SMALL_NUMBER && bHasActiveEnemy)
    {
        if (IsCombatRangeValid(CurrentEnemy.Definition.AttackRangeTiles))
        {
            EnemyAttackPlayer();
        }
        else
        {
            LastCombatMessage = TEXT("적이 공격 사거리 또는 시야 밖에 있습니다.");
            OnCombatStateChanged.Broadcast();
        }
        if (bHasActiveEnemy && GM->RaidState == ERaidState::InRaid)
        {
            EnemyAttackCooldownRemaining = FMath::Max(0.1f, CurrentEnemy.Definition.AttackIntervalSeconds);
        }
    }
}

void UCombatComponent::CompletePlayerAction()
{
    AGridGameMode* GM = Cast<AGridGameMode>(GetOwner());
    if (!GM)
    {
        CancelPlayerAction(TEXT("행동을 완료할 게임 상태가 없습니다."));
        return;
    }

    if (PlayerActionState == ECombatPlayerActionState::Swapping)
    {
        UItemInstance* TargetWeapon = GM->EquipmentComponent
            ? GM->EquipmentComponent->GetEquippedItem(PendingWeaponSlot)
            : nullptr;
        if (!TargetWeapon || TargetWeapon->Category != EItemCategory::Weapon)
        {
            CancelPlayerAction(TEXT("무기 교체 대상이 사라졌습니다."));
            return;
        }

        ActiveWeaponSlot = PendingWeaponSlot;
        LastCombatMessage = FString::Printf(TEXT("무기 교체 완료: %s"), *TargetWeapon->ItemName);
    }
    else if (PlayerActionState == ECombatPlayerActionState::Reloading)
    {
        UItemInstance* Weapon = GM->EquipmentComponent
            ? GM->EquipmentComponent->GetEquippedItem(PendingWeaponSlot)
            : nullptr;
        UGridInventoryComponent* Source = PendingReloadSource;
        UItemInstance* SelectedMagazine = Source ? Source->GetItemInstance(PendingReloadMagazineID) : nullptr;
        UItemInstance* CurrentMagazine = Weapon ? Weapon->EquippedMagazine : nullptr;
        if (!Weapon || Weapon != PendingReloadWeapon || !Source || !SelectedMagazine ||
            SelectedMagazine->InstanceID != PendingReloadMagazineID ||
            !Source->IsValidSection(PendingReloadSectionIndex) ||
            Source->GetCellItemID(PendingReloadSectionIndex, PendingReloadGridCoord.X, PendingReloadGridCoord.Y) != PendingReloadMagazineID ||
            !Weapon->IsCompatibleMagazine(SelectedMagazine))
        {
            CancelPlayerAction(TEXT("재장전 대상 또는 예비 탄창이 사라졌습니다."));
            return;
        }

        if (CurrentMagazine && !Source->CheckItemFitInSection(SelectedMagazine->InstanceID, PendingReloadSectionIndex,
            PendingReloadGridCoord.X, PendingReloadGridCoord.Y,
            CurrentMagazine->GetCurrentSize().X, CurrentMagazine->GetCurrentSize().Y))
        {
            CancelPlayerAction(TEXT("예비 탄창을 교환할 공간이 없습니다."));
            return;
        }

        if (!Source->RemoveItem(SelectedMagazine->InstanceID))
        {
            CancelPlayerAction(TEXT("예비 탄창을 교환할 수 없습니다."));
            return;
        }
        if (CurrentMagazine && !Source->AddItemToSection(CurrentMagazine, PendingReloadSectionIndex, PendingReloadGridCoord.X, PendingReloadGridCoord.Y))
        {
            if (!Source->AddItemToSection(SelectedMagazine, PendingReloadSectionIndex, PendingReloadGridCoord.X, PendingReloadGridCoord.Y))
            {
                UE_LOG(LogTemp, Error, TEXT("Reload rollback failed for magazine %s at section %d (%d,%d)."),
                    *SelectedMagazine->InstanceID.ToString(), PendingReloadSectionIndex,
                    PendingReloadGridCoord.X, PendingReloadGridCoord.Y);
            }
            CancelPlayerAction(TEXT("예비 탄창 교환을 취소했습니다."));
            return;
        }

        Weapon->EquippedMagazine = SelectedMagazine;
        SelectedMagazine->OnItemModified.Broadcast();
        if (CurrentMagazine) CurrentMagazine->OnItemModified.Broadcast();
        Weapon->OnItemModified.Broadcast();
        GM->EquipmentComponent->OnEquipmentChanged.Broadcast();
        LastCombatMessage = TEXT("재장전 완료: 탄창 교체");
    }
    else if (PlayerActionState == ECombatPlayerActionState::Moving)
    {
        const ECombatMovementAction CompletedMovement = PendingCombatMovement;
        if (!CommitCombatMovement())
        {
            CancelPlayerAction(TEXT("전투 이동을 완료할 수 없습니다."));
            return;
        }
        if (CompletedMovement == ECombatMovementAction::Flee)
        {
            LastCombatMessage = bHasActiveEnemy
                ? TEXT("FLEE FAILED - Enemy still has line of sight.")
                : TEXT("FLEE SUCCESS - Combat disengaged.");
        }
        else
        {
            LastCombatMessage = TEXT("전투 이동 완료");
        }
    }

    PlayerActionState = ECombatPlayerActionState::None;
    PlayerActionTimeRemaining = 0.0f;
    PendingWeaponSlot = NAME_None;
    PendingReloadMagazineID = NAME_None;
    PendingReloadSource = nullptr;
    PendingReloadSectionIndex = 0;
    PendingReloadGridCoord = FIntPoint::ZeroValue;
    PendingReloadWeapon = nullptr;
    PendingCombatMoveCoord = FIntPoint::ZeroValue;
    OnCombatStateChanged.Broadcast();
}

void UCombatComponent::CancelPlayerAction(const FString& Message)
{
    PlayerActionState = ECombatPlayerActionState::None;
    PlayerActionTimeRemaining = 0.0f;
    PendingWeaponSlot = NAME_None;
    PendingReloadMagazineID = NAME_None;
    PendingReloadSource = nullptr;
    PendingReloadGridCoord = FIntPoint::ZeroValue;
    PendingReloadWeapon = nullptr;
    PendingCombatMoveCoord = FIntPoint::ZeroValue;
    LastCombatMessage = Message;
    OnCombatStateChanged.Broadcast();
}

#if WITH_DEV_AUTOMATION_TESTS
void UCombatComponent::AdvanceCombatTimeForTest(float DeltaTime)
{
    AdvanceCombatTime(DeltaTime);
}
#endif

bool UCombatComponent::CommitCombatMovement()
{
    AGridGameMode* GM = Cast<AGridGameMode>(GetOwner());
    if (!GM || !GM->MovePlayerDuringCombat(PendingCombatMoveCoord)) return false;

    if (PendingCombatMovement == ECombatMovementAction::Flee && GM->EnemyManagerComponent)
    {
        FIntPoint EnemyCoordinate;
        if (GM->EnemyManagerComponent->GetActiveEnemyCoordinate(EnemyCoordinate) && GM->MapManagerComponent &&
            !GM->MapManagerComponent->HasLineOfSight(GM->CurrentPlayerCoord, EnemyCoordinate))
        {
            ClearEnemy();
            GM->EnemyManagerComponent->EndCombatContact();
        }
    }
    return true;
}

bool UCombatComponent::IsCombatRangeValid(int32 RangeTiles) const
{
    const AGridGameMode* GM = Cast<AGridGameMode>(GetOwner());
    if (!GM || !GM->EnemyManagerComponent || GM->EnemyManagerComponent->GetActiveEnemyInstanceID().IsNone())
    {
        return true;
    }

    FIntPoint EnemyCoordinate;
    return GM->EnemyManagerComponent->GetActiveEnemyCoordinate(EnemyCoordinate) &&
        GM->MapManagerComponent &&
        GM->MapManagerComponent->GetTileDistance(GM->CurrentPlayerCoord, EnemyCoordinate) <= FMath::Max(0, RangeTiles) &&
        GM->MapManagerComponent->HasLineOfSight(GM->CurrentPlayerCoord, EnemyCoordinate);
}
