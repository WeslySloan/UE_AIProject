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
    ScheduledEnemyDefinition.DetectionPower = 60;
}

void UEnemyManagerComponent::ResetForRaid()
{
    EnemyInstances.Empty();
    OccupiedTiles.Empty();
    ActiveEnemyInstanceID = NAME_None;
    RaidWorldTick = 0;
    NextSpawnTick = FMath::Max(0, InitialSpawnDelayTicks);
    SpawnRandomStream.Initialize(SpawnSeed);
    AmbushRandomStream.Initialize(AmbushSeed);
    AmbushReactionState = EEnemyAmbushReactionState::None;
    ActiveAmbushInstanceID = NAME_None;
}

void UEnemyManagerComponent::AdvanceWorldTick()
{
    const AGridGameMode* GameMode = Cast<AGridGameMode>(GetOwner());
    if (!GameMode || GameMode->RaidState != ERaidState::InRaid)
    {
        return;
    }

    if (AmbushReactionState != EEnemyAmbushReactionState::None)
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
    EvaluateDetectionAndContact();
    if (GameMode->CombatComponent && GameMode->CombatComponent->bHasActiveEnemy)
    {
        return;
    }
    if (AmbushReactionState != EEnemyAmbushReactionState::None)
    {
        return;
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
    NewInstance.NextMoveWorldTick = RaidWorldTick + 2;

    EnemyInstances.Add(NewInstance);
    OccupiedTiles.Add(Coordinate, NewInstance.InstanceID);
    return true;
}

bool UEnemyManagerComponent::DebugSpawnNearestScav()
{
#if UE_BUILD_SHIPPING
    return false;
#else
    AGridGameMode* GameMode = Cast<AGridGameMode>(GetOwner());
    UMapManagerComponent* MapManager = GameMode ? GameMode->MapManagerComponent : nullptr;
    if (!GameMode || GameMode->RaidState != ERaidState::InRaid || !MapManager ||
        GetAliveEnemyCount() >= FMath::Max(0, MaxAliveEnemies))
    {
        return false;
    }

    TArray<FIntPoint> Candidates;
    for (int32 Y = 0; Y < MapManager->MapHeight; ++Y)
    {
        for (int32 X = 0; X < MapManager->MapWidth; ++X)
        {
            const FIntPoint Candidate(X, Y);
            FTileData TileData;
            if (!MapManager->GetTileData(X, Y, TileData) || !TileData.bEnemySpawnAllowed ||
                TileData.TileType == ETileType::Extraction || Candidate == GameMode->CurrentPlayerCoord ||
                HasEnemyAt(Candidate) || !MapManager->FindPath(GameMode->CurrentPlayerCoord, Candidate).Num())
            {
                continue;
            }
            Candidates.Add(Candidate);
        }
    }

    Candidates.Sort([GameMode](const FIntPoint& A, const FIntPoint& B)
    {
        const int32 ADistance = FMath::Abs(A.X - GameMode->CurrentPlayerCoord.X) +
            FMath::Abs(A.Y - GameMode->CurrentPlayerCoord.Y);
        const int32 BDistance = FMath::Abs(B.X - GameMode->CurrentPlayerCoord.X) +
            FMath::Abs(B.Y - GameMode->CurrentPlayerCoord.Y);
        return ADistance < BDistance;
    });

    return Candidates.Num() > 0 && SpawnEnemyAt(ScheduledEnemyDefinition, Candidates[0]);
#endif
}

bool UEnemyManagerComponent::HasEnemyAt(FIntPoint Coordinate) const
{
    return OccupiedTiles.Contains(Coordinate);
}

bool UEnemyManagerComponent::FindDeadEnemyAt(FIntPoint Coordinate, FName& OutInstanceID) const
{
    OutInstanceID = NAME_None;
    for (const FEnemyWorldInstance& Instance : EnemyInstances)
    {
        if (!Instance.bAlive && Instance.WorldState == EEnemyWorldState::Dead && Instance.Coordinate == Coordinate &&
            (OutInstanceID.IsNone() || Instance.InstanceID.ToString() < OutInstanceID.ToString()))
        {
            OutInstanceID = Instance.InstanceID;
        }
    }
    return !OutInstanceID.IsNone();
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

        return StartEnemyContact(Instance, true);
    }

    return false;
}

bool UEnemyManagerComponent::HasActiveAmbushReaction() const
{
    return AmbushReactionState != EEnemyAmbushReactionState::None;
}

FName UEnemyManagerComponent::GetActiveAmbushInstanceID() const
{
    return ActiveAmbushInstanceID;
}

bool UEnemyManagerComponent::TryStartEnemyAmbushAtCurrentPlayer()
{
    AGridGameMode* GameMode = Cast<AGridGameMode>(GetOwner());
    if (!GameMode || !GameMode->CombatComponent || GameMode->CombatComponent->bHasActiveEnemy ||
        HasActiveAmbushReaction())
    {
        return false;
    }

    for (FEnemyWorldInstance& Instance : EnemyInstances)
    {
        if (TryStartEnemyAmbush(Instance))
        {
            return true;
        }
    }
    return false;
}

bool UEnemyManagerComponent::TryStartEnemyAmbush(FEnemyWorldInstance& Instance)
{
    const AGridGameMode* GameMode = Cast<AGridGameMode>(GetOwner());
    const UMapManagerComponent* MapManager = GameMode ? GameMode->MapManagerComponent : nullptr;
    if (!GameMode || !MapManager || GameMode->RaidState != ERaidState::InRaid ||
        !Instance.bAlive || Instance.BehaviorProfile != EEnemyBehaviorProfile::Ambusher ||
        Instance.KnowledgeState != EEnemyKnowledgeState::Hidden ||
        MapManager->GetTileDistance(GameMode->CurrentPlayerCoord, Instance.Coordinate) >
            FMath::Max(0, Instance.Definition.AmbushRangeTiles) ||
        !MapManager->HasLineOfSight(GameMode->CurrentPlayerCoord, Instance.Coordinate))
    {
        return false;
    }

    Instance.WorldState = EEnemyWorldState::Ambushing;
    AmbushReactionState = EEnemyAmbushReactionState::WaitingForPlayerChoice;
    ActiveAmbushInstanceID = Instance.InstanceID;
    return true;
}

bool UEnemyManagerComponent::StartEnemyContact(FEnemyWorldInstance& Instance, bool bGrantPlayerInitiative)
{
    AGridGameMode* GameMode = Cast<AGridGameMode>(GetOwner());
    if (!GameMode || !GameMode->CombatComponent || GameMode->CombatComponent->bHasActiveEnemy)
    {
        return false;
    }

    Instance.KnowledgeState = EEnemyKnowledgeState::Revealed;
    Instance.bRevealedToPlayer = true;
    Instance.WorldState = EEnemyWorldState::InCombat;
    ActiveEnemyInstanceID = Instance.InstanceID;
    GameMode->CombatComponent->SpawnEnemy(Instance.Definition, bGrantPlayerInitiative);
    if (GameMode->CombatComponent->bHasActiveEnemy)
    {
        return true;
    }

    Instance.WorldState = EEnemyWorldState::Idle;
    ActiveEnemyInstanceID = NAME_None;
    return false;
}

bool UEnemyManagerComponent::ResolveAmbushAttack(float DamageMultiplier)
{
    for (FEnemyWorldInstance& Instance : EnemyInstances)
    {
        if (Instance.InstanceID == ActiveAmbushInstanceID && Instance.bAlive)
        {
            const bool bStarted = StartEnemyContact(Instance);
            AmbushReactionState = EEnemyAmbushReactionState::None;
            ActiveAmbushInstanceID = NAME_None;
            if (bStarted)
            {
                CastChecked<AGridGameMode>(GetOwner())->CombatComponent->EnemyAttackPlayer(DamageMultiplier);
            }
            return bStarted;
        }
    }
    return false;
}

int32 UEnemyManagerComponent::GetCoverValueAt(FIntPoint Coordinate) const
{
    const AGridGameMode* GameMode = Cast<AGridGameMode>(GetOwner());
    const UMapManagerComponent* MapManager = GameMode ? GameMode->MapManagerComponent : nullptr;
    FTileData TileData;
    if (!MapManager || !MapManager->GetTileData(Coordinate.X, Coordinate.Y, TileData))
    {
        return 0;
    }

    int32 ClosedEdges = 0;
    if (Coordinate.X > 0 && !TileData.bOpenWest) ++ClosedEdges;
    if (Coordinate.X + 1 < MapManager->MapWidth && !TileData.bOpenEast) ++ClosedEdges;
    if (Coordinate.Y > 0 && !TileData.bOpenNorth) ++ClosedEdges;
    if (Coordinate.Y + 1 < MapManager->MapHeight && !TileData.bOpenSouth) ++ClosedEdges;
    return ClosedEdges == 0 ? 0 : ClosedEdges == 1 ? 25 : ClosedEdges == 2 ? 50 : 75;
}

int32 UEnemyManagerComponent::RollAmbushPercent()
{
#if WITH_DEV_AUTOMATION_TESTS
    if (ForcedAmbushRollForTest >= 0)
    {
        return ForcedAmbushRollForTest;
    }
#endif
    return AmbushRandomStream.RandRange(1, 100);
}

bool UEnemyManagerComponent::RequestAmbushSearch()
{
    if (AmbushReactionState != EEnemyAmbushReactionState::WaitingForPlayerChoice)
    {
        return false;
    }
    for (FEnemyWorldInstance& Instance : EnemyInstances)
    {
        if (Instance.InstanceID != ActiveAmbushInstanceID || !Instance.bAlive) continue;
        const AGridGameMode* GameMode = Cast<AGridGameMode>(GetOwner());
        const int32 Chance = FMath::Clamp(50 + GameMode->PlayerPerception - Instance.Definition.Stealth -
            Instance.Definition.AmbushPower, 10, 90);
        if (RollAmbushPercent() <= Chance)
        {
            AmbushReactionState = EEnemyAmbushReactionState::None;
            ActiveAmbushInstanceID = NAME_None;
            return StartEnemyContact(Instance);
        }
        return ResolveAmbushAttack(1.0f);
    }
    return false;
}

bool UEnemyManagerComponent::RequestAmbushCover()
{
    if (AmbushReactionState != EEnemyAmbushReactionState::WaitingForPlayerChoice)
    {
        return false;
    }
    for (FEnemyWorldInstance& Instance : EnemyInstances)
    {
        if (Instance.InstanceID == ActiveAmbushInstanceID && Instance.bAlive)
        {
            AGridGameMode* GameMode = Cast<AGridGameMode>(GetOwner());
            return ResolveAmbushAttack((100.0f - GetCoverValueAt(GameMode->CurrentPlayerCoord)) / 100.0f);
        }
    }
    return false;
}

bool UEnemyManagerComponent::RequestAmbushFlee()
{
    if (AmbushReactionState != EEnemyAmbushReactionState::WaitingForPlayerChoice)
    {
        return false;
    }
    for (FEnemyWorldInstance& Instance : EnemyInstances)
    {
        if (Instance.InstanceID != ActiveAmbushInstanceID || !Instance.bAlive) continue;
        AGridGameMode* GameMode = Cast<AGridGameMode>(GetOwner());
        const int32 Chance = FMath::Clamp(50 + GameMode->PlayerMobility - Instance.Definition.AmbushPower, 10, 90);
        if (RollAmbushPercent() <= Chance && GameMode->TryRestorePreviousPlayerCoord())
        {
            Instance.WorldState = EEnemyWorldState::Idle;
            AmbushReactionState = EEnemyAmbushReactionState::None;
            ActiveAmbushInstanceID = NAME_None;
            return true;
        }
        return ResolveAmbushAttack(1.0f);
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

    Instance.NextMoveWorldTick = RaidWorldTick + 2;
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

        if (TryStartEnemyAmbush(Instance))
        {
            return;
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

    AGridGameMode* GameMode = Cast<AGridGameMode>(GetOwner());
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
                GameMode->RefreshEnemyWorldUI();
            }
            else
            {
                Instance.WorldState = EEnemyWorldState::Idle;
            }
        }
    }
    ActiveEnemyInstanceID = NAME_None;
}

#if WITH_DEV_AUTOMATION_TESTS
bool UEnemyManagerComponent::MarkEnemyDeadForTest(FName InstanceID)
{
    for (FEnemyWorldInstance& Instance : EnemyInstances)
    {
        if (Instance.InstanceID == InstanceID && Instance.bAlive)
        {
            Instance.bAlive = false;
            Instance.WorldState = EEnemyWorldState::Dead;
            OccupiedTiles.Remove(Instance.Coordinate);
            return true;
        }
    }
    return false;
}
#endif

void UEnemyManagerComponent::EndCombatContact()
{
    SyncCombatContact();
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

    const int32 DetectionScore = Instance.Definition.DetectionPower -
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
