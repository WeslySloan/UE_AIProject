#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatComponent.h"
#include "EnemyManagerComponent.generated.h"

UENUM()
enum class EEnemyBehaviorProfile : uint8
{
    RandomWander,
    Patrol,
    GuardZone,
    BossPatrol,
    Ambusher
};

UENUM()
enum class EEnemyWorldState : uint8
{
    Idle,
    Wandering,
    Patrolling,
    Guarding,
    Investigating,
    Ambushing,
    InCombat,
    Dead
};

UENUM()
enum class EEnemyKnowledgeState : uint8
{
    Hidden,
    Suspected,
    Revealed
};

UENUM()
enum class EEnemyAmbushReactionState : uint8
{
    None,
    WaitingForPlayerChoice
};

UENUM()
enum class EGridFacingDirection : uint8
{
    North,
    South,
    East,
    West
};

USTRUCT()
struct GRIDLOOTMASTER_API FEnemyWorldInstance
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, Category = "Enemy World")
    FName InstanceID;

    UPROPERTY(VisibleAnywhere, Category = "Enemy World")
    FEnemyDefinition Definition;

    UPROPERTY(VisibleAnywhere, Category = "Enemy World")
    FIntPoint Coordinate = FIntPoint::ZeroValue;

    UPROPERTY(VisibleAnywhere, Category = "Enemy World")
    FIntPoint HomeCoordinate = FIntPoint::ZeroValue;

    UPROPERTY(VisibleAnywhere, Category = "Enemy World")
    EEnemyBehaviorProfile BehaviorProfile = EEnemyBehaviorProfile::RandomWander;

    UPROPERTY(VisibleAnywhere, Category = "Enemy World")
    EEnemyWorldState WorldState = EEnemyWorldState::Idle;

    UPROPERTY(VisibleAnywhere, Category = "Enemy World|Knowledge")
    EEnemyKnowledgeState KnowledgeState = EEnemyKnowledgeState::Hidden;

    UPROPERTY(VisibleAnywhere, Category = "Enemy World")
    EGridFacingDirection Facing = EGridFacingDirection::South;

    UPROPERTY(VisibleAnywhere, Category = "Enemy World")
    int32 NextMoveWorldTick = 0;

    UPROPERTY(VisibleAnywhere, Category = "Enemy World")
    int32 PatrolIndex = 0;

    UPROPERTY(VisibleAnywhere, Category = "Enemy World")
    bool bAlive = true;

    UPROPERTY(VisibleAnywhere, Category = "Enemy World")
    bool bRevealedToPlayer = false;
};

UCLASS(ClassGroup = (Custom))
class GRIDLOOTMASTER_API UEnemyManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UEnemyManagerComponent();

    UPROPERTY(EditAnywhere, Category = "Enemy World|Spawn")
    int32 InitialSpawnDelayTicks = 2;

    UPROPERTY(EditAnywhere, Category = "Enemy World|Spawn")
    int32 SpawnIntervalMinTicks = 3;

    UPROPERTY(EditAnywhere, Category = "Enemy World|Spawn")
    int32 SpawnIntervalMaxTicks = 5;

    UPROPERTY(EditAnywhere, Category = "Enemy World|Spawn")
    int32 MaxAliveEnemies = 3;

    UPROPERTY(EditAnywhere, Category = "Enemy World|Spawn")
    int32 MinimumSpawnDistance = 3;

    UPROPERTY(EditAnywhere, Category = "Enemy World|Spawn")
    int32 SpawnSeed = 1337;

    UPROPERTY(EditAnywhere, Category = "Enemy World|Ambush")
    int32 AmbushSeed = 7331;

    UPROPERTY(EditAnywhere, Category = "Enemy World|Spawn")
    FEnemyDefinition ScheduledEnemyDefinition;

    UPROPERTY(VisibleAnywhere, Category = "Enemy World")
    int32 RaidWorldTick = 0;

    UPROPERTY(VisibleAnywhere, Category = "Enemy World|Spawn")
    int32 NextSpawnTick = 0;

    void ResetForRaid();
    void AdvanceWorldTick();

    bool SpawnEnemyAt(const FEnemyDefinition& EnemyDefinition, FIntPoint Coordinate,
        EEnemyBehaviorProfile BehaviorProfile = EEnemyBehaviorProfile::RandomWander);

    bool HasEnemyAt(FIntPoint Coordinate) const;

    int32 GetAliveEnemyCount() const;

    int32 GetNextSpawnTick() const;

    FName GetActiveEnemyInstanceID() const;

    bool GetActiveEnemyCoordinate(FIntPoint& OutCoordinate) const;

    bool FindPlayerAmbushTarget(FName& OutInstanceID) const;

    bool StartPlayerAmbushContact(FName InstanceID);

    bool HasActiveAmbushReaction() const;
    FName GetActiveAmbushInstanceID() const;
    bool RequestAmbushSearch();
    bool RequestAmbushCover();
    bool RequestAmbushFlee();
    bool TryStartEnemyAmbushAtCurrentPlayer();

    const TArray<FEnemyWorldInstance>& GetEnemyInstances() const;

private:
    bool IsValidSpawnCoordinate(FIntPoint Coordinate) const;
    FName MakeUniqueInstanceID(FName PreferredID) const;
    bool TrySpawnScheduledEnemy();
    void AdvanceEnemyMovement();
    bool MoveRandomWanderEnemy(int32 EnemyIndex);
    void EvaluateDetectionAndContact();
    bool TryStartEnemyAmbush(FEnemyWorldInstance& Instance);
    bool StartEnemyContact(FEnemyWorldInstance& Instance, bool bGrantPlayerInitiative = false);
    bool ResolveAmbushAttack(float DamageMultiplier);
    int32 GetCoverValueAt(FIntPoint Coordinate) const;
    int32 RollAmbushPercent();
    void SyncCombatContact();
    bool DoesEnemyDetectPlayer(const FEnemyWorldInstance& Instance) const;
    bool DoesPlayerSuspectEnemy(const FEnemyWorldInstance& Instance) const;
    void ScheduleNextSpawn();

    UPROPERTY()
    TArray<FEnemyWorldInstance> EnemyInstances;

    TMap<FIntPoint, FName> OccupiedTiles;
    FRandomStream SpawnRandomStream;
    FRandomStream AmbushRandomStream;

    UPROPERTY(VisibleAnywhere, Category = "Enemy World|Contact")
    FName ActiveEnemyInstanceID;

    UPROPERTY(VisibleAnywhere, Category = "Enemy World|Ambush")
    EEnemyAmbushReactionState AmbushReactionState = EEnemyAmbushReactionState::None;

    UPROPERTY(VisibleAnywhere, Category = "Enemy World|Ambush")
    FName ActiveAmbushInstanceID;

#if WITH_DEV_AUTOMATION_TESTS
public:
    int32 ForcedAmbushRollForTest = -1;
#endif
};
