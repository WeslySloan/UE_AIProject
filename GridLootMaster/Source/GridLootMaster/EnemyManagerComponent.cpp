#include "EnemyManagerComponent.h"

#include "GridGameMode.h"
#include "Map/MapManagerComponent.h"

UEnemyManagerComponent::UEnemyManagerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    ScheduledEnemyDefinition.EnemyID = TEXT("WorldScav");
    ScheduledEnemyDefinition.DisplayName = TEXT("World Scav");
    ScheduledEnemyDefinition.MaxHealth = 100;
    ScheduledEnemyDefinition.AttackDamage = 10;
}

void UEnemyManagerComponent::ResetForRaid()
{
    EnemyInstances.Empty();
    OccupiedTiles.Empty();
    ActiveEnemyInstanceID = NAME_None;
    RaidWorldTick = 0;
    NextSpawnTick = FMath::Max(0, InitialSpawnDelayTicks);
    SpawnRandomStream.Initialize(SpawnSeed);
}

void UEnemyManagerComponent::AdvanceWorldTick()
{
    const AGridGameMode* GameMode = Cast<AGridGameMode>(GetOwner());
    if (!GameMode || GameMode->RaidState != ERaidState::InRaid)
    {
        return;
    }

    SyncCombatContact();
    ++RaidWorldTick;
    if (RaidWorldTick >= NextSpawnTick)
    {
        TrySpawnScheduledEnemy();
        ScheduleNextSpawn();
    }
    AdvanceEnemyMovement();
    EvaluateDetectionAndContact();
}

bool UEnemyManagerComponent::SpawnEnemyAt(const FEnemyDefinition& EnemyDefinition, FIntPoint Coordinate,
    EEnemyBehaviorProfile BehaviorProfile)
{
    if (EnemyDefinition.EnemyID.IsNone() || !IsValidSpawnCoordinate(Coordinate))
    {
        return false;
    }

    FEnemyWorldInstance NewInstance;
    NewInstance.InstanceID = MakeUniqueInstanceID(EnemyDefinition.EnemyID);
    NewInstance.Definition = EnemyDefinition;
    NewInstance.Coordinate = Coordinate;
    NewInstance.HomeCoordinate = Coordinate;
    NewInstance.BehaviorProfile = BehaviorProfile;
    NewInstance.NextMoveWorldTick = RaidWorldTick + 1;

    EnemyInstances.Add(NewInstance);
    OccupiedTiles.Add(Coordinate, NewInstance.InstanceID);
    return true;
}

bool UEnemyManagerComponent::HasEnemyAt(FIntPoint Coordinate) const
{
    return OccupiedTiles.Contains(Coordinate);
}

int32 UEnemyManagerComponent::GetAliveEnemyCount() const
{
    int32 AliveCount = 0;
    for (const FEnemyWorldInstance& Instance : EnemyInstances)
    {
        if (Instance.bAlive)
        {
            ++AliveCount;
        }
    }
    return AliveCount;
}

int32 UEnemyManagerComponent::GetNextSpawnTick() const
{
    return NextSpawnTick;
}

FName UEnemyManagerComponent::GetActiveEnemyInstanceID() const
{
    return ActiveEnemyInstanceID;
}

bool UEnemyManagerComponent::GetActiveEnemyCoordinate(FIntPoint& OutCoordinate) const
{
    for (const FEnemyWorldInstance& Instance : EnemyInstances)
    {
        if (Instance.bAlive && Instance.InstanceID == ActiveEnemyInstanceID)
        {
            OutCoordinate = Instance.Coordinate;
            return true;
        }
    }

    return false;
}

const TArray<FEnemyWorldInstance>& UEnemyManagerComponent::GetEnemyInstances() const
{
    return EnemyInstances;
}

bool UEnemyManagerComponent::FindPlayerAmbushTarget(FName& OutInstanceID) const
{
    const AGridGameMode* GameMode = Cast<AGridGameMode>(GetOwner());
    const UMapManagerComponent* MapManager = GameMode ? GameMode->MapManagerComponent : nullptr;
    if (!GameMode || !MapManager || GameMode->PlayerPosture != EPlayerRaidPosture::Ambushing)
    {
        return false;
    }

    int32 BestDistance = MAX_int32;
    for (const FEnemyWorldInstance& Instance : EnemyInstances)
    {
        if (!Instance.bAlive || Instance.WorldState == EEnemyWorldState::InCombat ||
            Instance.KnowledgeState == EEnemyKnowledgeState::Hidden ||
            !MapManager->HasLineOfSight(GameMode->CurrentPlayerCoord, Instance.Coordinate))
        {
            continue;
        }

        const int32 Distance = MapManager->GetTileDistance(GameMode->CurrentPlayerCoord, Instance.Coordinate);
        if (Distance <= FMath::Max(0, GameMode->PlayerDetectionRangeTiles) && Distance < BestDistance)
        {
            BestDistance = Distance;
            OutInstanceID = Instance.InstanceID;
        }
    }

    return OutInstanceID != NAME_None;
}

bool UEnemyManagerComponent::StartPlayerAmbushContact(FName InstanceID)
{
    AGridGameMode* GameMode = Cast<AGridGameMode>(GetOwner());
    if (!GameMode || !GameMode->CombatComponent || GameMode->CombatComponent->bHasActiveEnemy)
    {
        return false;
    }

    for (FEnemyWorldInstance& Instance : EnemyInstances)
    {
        if (Instance.InstanceID != InstanceID || !Instance.bAlive ||
            Instance.KnowledgeState == EEnemyKnowledgeState::Hidden)
        {
            continue;
        }

        Instance.KnowledgeState = EEnemyKnowledgeState::Revealed;
        Instance.bRevealedToPlayer = true;
        Instance.WorldState = EEnemyWorldState::InCombat;
        ActiveEnemyInstanceID = Instance.InstanceID;
        GameMode->CombatComponent->SpawnEnemy(Instance.Definition, true);
        if (GameMode->CombatComponent->bHasActiveEnemy)
        {
            return true;
        }

        Instance.WorldState = EEnemyWorldState::Idle;
        ActiveEnemyInstanceID = NAME_None;
        return false;
    }

    return false;
}

bool UEnemyManagerComponent::IsValidSpawnCoordinate(FIntPoint Coordinate) const
{
    const AGridGameMode* GameMode = Cast<AGridGameMode>(GetOwner());
    if (!GameMode || !GameMode->MapManagerComponent)
    {
        return false;
    }

    FTileData TileData;
    if (!GameMode->MapManagerComponent->GetTileData(Coordinate.X, Coordinate.Y, TileData))
    {
        return false;
    }

    if (OccupiedTiles.Contains(Coordinate))
    {
        return false;
    }

    // 일반 Scheduler 후보에서 Map v1 정적 금지 타일을 제외합니다.
    // SpawnEnemyAt 자체는 스크립트 전용 적 배치를 위해 허용합니다.

    return GameMode->RaidState != ERaidState::InRaid || GameMode->CurrentPlayerCoord != Coordinate;
}

FName UEnemyManagerComponent::MakeUniqueInstanceID(FName PreferredID) const
{
    FName Candidate = PreferredID;
    int32 Suffix = 1;

    auto IsUsed = [this](FName InstanceID)
    {
        for (const FEnemyWorldInstance& Instance : EnemyInstances)
        {
            if (Instance.InstanceID == InstanceID)
            {
                return true;
            }
        }
        return false;
    };

    while (IsUsed(Candidate))
    {
        Candidate = FName(*FString::Printf(TEXT("%s_%d"), *PreferredID.ToString(), Suffix++));
    }

    return Candidate;
}

bool UEnemyManagerComponent::TrySpawnScheduledEnemy()
{
    if (ScheduledEnemyDefinition.EnemyID.IsNone() || GetAliveEnemyCount() >= FMath::Max(0, MaxAliveEnemies))
    {
        return false;
    }

    const AGridGameMode* GameMode = Cast<AGridGameMode>(GetOwner());
    UMapManagerComponent* MapManager = GameMode ? GameMode->MapManagerComponent : nullptr;
    if (!GameMode || !MapManager)
    {
        return false;
    }

    TArray<FIntPoint> Candidates;
    const int32 MinDistance = FMath::Max(0, MinimumSpawnDistance);
    for (int32 Y = 0; Y < MapManager->MapHeight; ++Y)
    {
        for (int32 X = 0; X < MapManager->MapWidth; ++X)
        {
            const FIntPoint Candidate(X, Y);
            FTileData TileData;
            if (!MapManager->GetTileData(X, Y, TileData) ||
                !TileData.bEnemySpawnAllowed ||
                TileData.TileType == ETileType::Extraction ||
                Candidate == GameMode->CurrentPlayerCoord ||
                HasEnemyAt(Candidate) ||
                FMath::Abs(Candidate.X - GameMode->CurrentPlayerCoord.X) +
                    FMath::Abs(Candidate.Y - GameMode->CurrentPlayerCoord.Y) < MinDistance ||
                !MapManager->FindPath(GameMode->CurrentPlayerCoord, Candidate).Num())
            {
                continue;
            }

            Candidates.Add(Candidate);
        }
    }

    if (Candidates.Num() == 0)
    {
        return false;
    }

    const int32 CandidateIndex = SpawnRandomStream.RandRange(0, Candidates.Num() - 1);
    return SpawnEnemyAt(ScheduledEnemyDefinition, Candidates[CandidateIndex]);
}

void UEnemyManagerComponent::AdvanceEnemyMovement()
{
    for (int32 EnemyIndex = 0; EnemyIndex < EnemyInstances.Num(); ++EnemyIndex)
    {
        FEnemyWorldInstance& Instance = EnemyInstances[EnemyIndex];
        if (!Instance.bAlive || Instance.BehaviorProfile != EEnemyBehaviorProfile::RandomWander ||
            Instance.NextMoveWorldTick > RaidWorldTick)
        {
            continue;
        }

        MoveRandomWanderEnemy(EnemyIndex);
    }
}

bool UEnemyManagerComponent::MoveRandomWanderEnemy(int32 EnemyIndex)
{
    if (!EnemyInstances.IsValidIndex(EnemyIndex))
    {
        return false;
    }

    const AGridGameMode* GameMode = Cast<AGridGameMode>(GetOwner());
    UMapManagerComponent* MapManager = GameMode ? GameMode->MapManagerComponent : nullptr;
    FEnemyWorldInstance& Instance = EnemyInstances[EnemyIndex];
    if (!GameMode || !MapManager || !Instance.bAlive)
    {
        return false;
    }

    const FIntPoint CurrentCoordinate = Instance.Coordinate;
    const FIntPoint Neighbors[4] = {
        FIntPoint(CurrentCoordinate.X, CurrentCoordinate.Y - 1),
        FIntPoint(CurrentCoordinate.X, CurrentCoordinate.Y + 1),
        FIntPoint(CurrentCoordinate.X - 1, CurrentCoordinate.Y),
        FIntPoint(CurrentCoordinate.X + 1, CurrentCoordinate.Y)
    };

    TArray<FIntPoint> Candidates;
    for (const FIntPoint Candidate : Neighbors)
    {
        if (Candidate == GameMode->CurrentPlayerCoord ||
            HasEnemyAt(Candidate) ||
            !MapManager->CanMoveBetween(CurrentCoordinate, Candidate))
        {
            continue;
        }

        Candidates.Add(Candidate);
    }

    Instance.NextMoveWorldTick = RaidWorldTick + 1;
    if (Candidates.Num() == 0)
    {
        return false;
    }

    const FIntPoint NewCoordinate = Candidates[SpawnRandomStream.RandRange(0, Candidates.Num() - 1)];
    OccupiedTiles.Remove(CurrentCoordinate);
    OccupiedTiles.Add(NewCoordinate, Instance.InstanceID);
    Instance.Coordinate = NewCoordinate;
    Instance.WorldState = EEnemyWorldState::Wandering;

    if (NewCoordinate.X > CurrentCoordinate.X)
    {
        Instance.Facing = EGridFacingDirection::East;
    }
    else if (NewCoordinate.X < CurrentCoordinate.X)
    {
        Instance.Facing = EGridFacingDirection::West;
    }
    else if (NewCoordinate.Y > CurrentCoordinate.Y)
    {
        Instance.Facing = EGridFacingDirection::South;
    }
    else
    {
        Instance.Facing = EGridFacingDirection::North;
    }

    return true;
}

void UEnemyManagerComponent::EvaluateDetectionAndContact()
{
    AGridGameMode* GameMode = Cast<AGridGameMode>(GetOwner());
    if (!GameMode || !GameMode->CombatComponent || GameMode->CombatComponent->bHasActiveEnemy)
    {
        return;
    }

    for (FEnemyWorldInstance& Instance : EnemyInstances)
    {
        if (!Instance.bAlive || Instance.WorldState == EEnemyWorldState::InCombat)
        {
            continue;
        }

        if (DoesPlayerSuspectEnemy(Instance) && Instance.KnowledgeState == EEnemyKnowledgeState::Hidden)
        {
            Instance.KnowledgeState = EEnemyKnowledgeState::Suspected;
        }

        if (!DoesEnemyDetectPlayer(Instance))
        {
            continue;
        }

        Instance.KnowledgeState = EEnemyKnowledgeState::Revealed;
        Instance.bRevealedToPlayer = true;
        Instance.WorldState = EEnemyWorldState::InCombat;
        if (GameMode->PlayerPosture == EPlayerRaidPosture::Ambushing)
        {
            GameMode->PlayerPosture = EPlayerRaidPosture::Normal;
        }
        ActiveEnemyInstanceID = Instance.InstanceID;
        GameMode->CombatComponent->SpawnEnemy(Instance.Definition);

        if (!GameMode->CombatComponent->bHasActiveEnemy)
        {
            Instance.KnowledgeState = EEnemyKnowledgeState::Suspected;
            Instance.bRevealedToPlayer = false;
            Instance.WorldState = EEnemyWorldState::Idle;
            ActiveEnemyInstanceID = NAME_None;
        }
        return;
    }
}

void UEnemyManagerComponent::SyncCombatContact()
{
    if (ActiveEnemyInstanceID.IsNone())
    {
        return;
    }

    const AGridGameMode* GameMode = Cast<AGridGameMode>(GetOwner());
    if (!GameMode || (GameMode->CombatComponent && GameMode->CombatComponent->bHasActiveEnemy))
    {
        return;
    }

    for (FEnemyWorldInstance& Instance : EnemyInstances)
    {
        if (Instance.InstanceID == ActiveEnemyInstanceID && Instance.bAlive)
        {
            if (GameMode->CombatComponent && GameMode->CombatComponent->CurrentEnemy.bIsDefeated)
            {
                Instance.bAlive = false;
                Instance.WorldState = EEnemyWorldState::Dead;
                OccupiedTiles.Remove(Instance.Coordinate);
            }
            else
            {
                Instance.WorldState = EEnemyWorldState::Idle;
            }
        }
    }
    ActiveEnemyInstanceID = NAME_None;
}

bool UEnemyManagerComponent::DoesEnemyDetectPlayer(const FEnemyWorldInstance& Instance) const
{
    const AGridGameMode* GameMode = Cast<AGridGameMode>(GetOwner());
    const UMapManagerComponent* MapManager = GameMode ? GameMode->MapManagerComponent : nullptr;
    if (!GameMode || !MapManager)
    {
        return false;
    }

    const int32 Distance = MapManager->GetTileDistance(Instance.Coordinate, GameMode->CurrentPlayerCoord);
    if (Distance > FMath::Max(0, Instance.Definition.VisionRangeTiles) ||
        !MapManager->HasLineOfSight(Instance.Coordinate, GameMode->CurrentPlayerCoord))
    {
        return false;
    }

    const int32 DetectionScore = Instance.Definition.DetectionPower + GameMode->PlayerPerception -
        GameMode->PlayerStealth - Distance * 10;
    return DetectionScore >= 0;
}

bool UEnemyManagerComponent::DoesPlayerSuspectEnemy(const FEnemyWorldInstance& Instance) const
{
    const AGridGameMode* GameMode = Cast<AGridGameMode>(GetOwner());
    const UMapManagerComponent* MapManager = GameMode ? GameMode->MapManagerComponent : nullptr;
    if (!GameMode || !MapManager)
    {
        return false;
    }

    const int32 Distance = MapManager->GetTileDistance(GameMode->CurrentPlayerCoord, Instance.Coordinate);
    if (Distance > FMath::Max(0, GameMode->PlayerDetectionRangeTiles) ||
        !MapManager->HasLineOfSight(GameMode->CurrentPlayerCoord, Instance.Coordinate))
    {
        return false;
    }

    const int32 DetectionScore = GameMode->PlayerDetectionPower + GameMode->PlayerPerception -
        Instance.Definition.Stealth - Distance * 10;
    return DetectionScore >= 0;
}

void UEnemyManagerComponent::ScheduleNextSpawn()
{
    const int32 IntervalMin = FMath::Max(1, FMath::Min(SpawnIntervalMinTicks, SpawnIntervalMaxTicks));
    const int32 IntervalMax = FMath::Max(IntervalMin, SpawnIntervalMaxTicks);
    NextSpawnTick = RaidWorldTick + SpawnRandomStream.RandRange(IntervalMin, IntervalMax);
}
