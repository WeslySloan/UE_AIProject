#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "CombatComponent.h"
#include "EnemyManagerComponent.h"
#include "GridGameMode.h"
#include "GridInventoryComponent.h"
#include "ItemData.h"
#include "Map/MapManagerComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterEnemyWorldSpawnValidationTest,
    "GridLootMaster.EnemyWorld.SpawnValidation",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterEnemyWorldSpawnValidationTest::RunTest(const FString& Parameters)
{
    AGridGameMode* GameMode = NewObject<AGridGameMode>();
    TestNotNull(TEXT("Game mode is created for enemy world validation"), GameMode);
    if (!GameMode || !GameMode->EnemyManagerComponent || !GameMode->MapManagerComponent)
    {
        return false;
    }

    GameMode->MapManagerComponent->InitializeMap();
    GameMode->RaidState = ERaidState::InRaid;
    GameMode->CurrentPlayerCoord = FIntPoint(0, 0);
    GameMode->EnemyManagerComponent->ResetForRaid();

    FEnemyDefinition Enemy;
    Enemy.EnemyID = TEXT("WorldScav");
    Enemy.DisplayName = TEXT("World Scav");
    Enemy.MaxHealth = 100;

    TestTrue(TEXT("A valid non-player tile accepts an enemy"),
        GameMode->EnemyManagerComponent->SpawnEnemyAt(Enemy, FIntPoint(4, 4)));
    TestFalse(TEXT("The player tile rejects an enemy"),
        GameMode->EnemyManagerComponent->SpawnEnemyAt(Enemy, GameMode->CurrentPlayerCoord));
    TestFalse(TEXT("An occupied tile rejects a duplicate enemy"),
        GameMode->EnemyManagerComponent->SpawnEnemyAt(Enemy, FIntPoint(4, 4)));
    TestFalse(TEXT("An out-of-bounds negative tile is rejected"),
        GameMode->EnemyManagerComponent->SpawnEnemyAt(Enemy, FIntPoint(-1, 0)));
    TestFalse(TEXT("The invalid 9,9 tile is rejected"),
        GameMode->EnemyManagerComponent->SpawnEnemyAt(Enemy, FIntPoint(9, 9)));

    TestTrue(TEXT("A second valid tile accepts another enemy"),
        GameMode->EnemyManagerComponent->SpawnEnemyAt(Enemy, FIntPoint(4, 5)));
    TestEqual(TEXT("Two world enemies are alive"),
        GameMode->EnemyManagerComponent->GetAliveEnemyCount(), 2);
    TestNotEqual(TEXT("World enemy instance IDs are unique"),
        GameMode->EnemyManagerComponent->GetEnemyInstances()[0].InstanceID,
        GameMode->EnemyManagerComponent->GetEnemyInstances()[1].InstanceID);

    GameMode->EnemyManagerComponent->ResetForRaid();
    TestEqual(TEXT("Reset clears all world enemies"),
        GameMode->EnemyManagerComponent->GetEnemyInstances().Num(), 0);
    TestFalse(TEXT("Reset clears tile occupancy"),
        GameMode->EnemyManagerComponent->HasEnemyAt(FIntPoint(4, 4)));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterEnemyWorldRaidLifecycleTest,
    "GridLootMaster.EnemyWorld.RaidLifecycleReset",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterEnemyWorldRaidLifecycleTest::RunTest(const FString& Parameters)
{
    const FString SaveSlot = FString::Printf(TEXT("GridLootMaster_EnemyWorld_%s"), *FGuid::NewGuid().ToString());

    AGridGameMode* GameMode = NewObject<AGridGameMode>();
    TestNotNull(TEXT("Game mode is created for enemy world lifecycle"), GameMode);
    if (!GameMode || !GameMode->EnemyManagerComponent || !GameMode->MapManagerComponent || !GameMode->StashComponent)
    {
        return false;
    }

    UDataTable* ItemDataTable = NewObject<UDataTable>(GameMode);
    ItemDataTable->RowStruct = FItemData::StaticStruct();
    FItemData TestItemData;
    TestItemData.ItemID = TEXT("EnemyWorldLifecycleItem");
    TestItemData.Category = EItemCategory::Valuable;
    TestItemData.Size = FIntPoint(1, 1);
    TestItemData.MaxStack = 1;
    ItemDataTable->AddRow(TestItemData.ItemID, TestItemData);

    GameMode->StashSaveSlot = SaveSlot;
    GameMode->ItemDataTable = ItemDataTable;
    GameMode->StashComponent->InitializeGrid(2, 2);
    GameMode->MapManagerComponent->InitializeMap();
    GameMode->RaidState = ERaidState::InRaid;
    GameMode->CurrentPlayerCoord = FIntPoint(0, 0);

    FEnemyDefinition Enemy;
    Enemy.EnemyID = TEXT("LifecycleScav");
    Enemy.DisplayName = TEXT("Lifecycle Scav");

    TestTrue(TEXT("Enemy is present before starting the next raid"),
        GameMode->EnemyManagerComponent->SpawnEnemyAt(Enemy, FIntPoint(4, 4)));
    GameMode->RaidState = ERaidState::Lobby;

    TestTrue(TEXT("A configured lobby can start a raid"), GameMode->StartRaid());
    TestEqual(TEXT("Starting a raid resets the enemy world"),
        GameMode->EnemyManagerComponent->GetEnemyInstances().Num(), 0);

    TestTrue(TEXT("Enemy can be added during the raid"),
        GameMode->EnemyManagerComponent->SpawnEnemyAt(Enemy, FIntPoint(4, 4)));
    const bool bHasExtractionPoint = GameMode->MapManagerComponent->ExtractionPoints.Num() > 0;
    TestTrue(TEXT("An extraction point is available for lifecycle cleanup"), bHasExtractionPoint);
    if (bHasExtractionPoint)
    {
        GameMode->CurrentPlayerCoord = GameMode->MapManagerComponent->ExtractionPoints[0];
        TestTrue(TEXT("Extraction succeeds at the selected extraction point"), GameMode->ExtractRaid());
        TestEqual(TEXT("Successful extraction resets the enemy world"),
            GameMode->EnemyManagerComponent->GetEnemyInstances().Num(), 0);
    }

    UGameplayStatics::DeleteGameInSlot(SaveSlot, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterEnemyWorldTickSpawnSchedulerTest,
    "GridLootMaster.EnemyWorld.WorldTickSpawnScheduler",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterEnemyWorldTickSpawnSchedulerTest::RunTest(const FString& Parameters)
{
    AGridGameMode* GameMode = NewObject<AGridGameMode>();
    TestNotNull(TEXT("Game mode is created for world tick scheduler"), GameMode);
    if (!GameMode || !GameMode->EnemyManagerComponent || !GameMode->MapManagerComponent)
    {
        return false;
    }

    GameMode->MapManagerComponent->InitializeMap();
    GameMode->RaidState = ERaidState::InRaid;
    GameMode->CurrentPlayerCoord = FIntPoint(0, 0);

    UEnemyManagerComponent* EnemyManager = GameMode->EnemyManagerComponent;
    EnemyManager->InitialSpawnDelayTicks = 2;
    EnemyManager->SpawnIntervalMinTicks = 3;
    EnemyManager->SpawnIntervalMaxTicks = 3;
    EnemyManager->MaxAliveEnemies = 3;
    EnemyManager->MinimumSpawnDistance = 3;
    EnemyManager->SpawnSeed = 4242;
    EnemyManager->ResetForRaid();

    GameMode->AdvanceRaidWorldTick();
    TestEqual(TEXT("The first advance consumes one world tick"), EnemyManager->RaidWorldTick, 1);
    TestEqual(TEXT("Initial safe ticks prevent an early spawn"), EnemyManager->GetAliveEnemyCount(), 0);

    GameMode->AdvanceRaidWorldTick();
    TestEqual(TEXT("The scheduled spawn occurs on the due tick"), EnemyManager->GetAliveEnemyCount(), 1);
    TestEqual(TEXT("The next spawn is scheduled after the configured interval"),
        EnemyManager->GetNextSpawnTick(), 5);

    if (EnemyManager->GetEnemyInstances().Num() == 1)
    {
        const FIntPoint SpawnedCoordinate = EnemyManager->GetEnemyInstances()[0].Coordinate;
        const int32 Distance = FMath::Abs(SpawnedCoordinate.X - GameMode->CurrentPlayerCoord.X) +
            FMath::Abs(SpawnedCoordinate.Y - GameMode->CurrentPlayerCoord.Y);
        TestTrue(TEXT("Scheduled spawn respects the minimum player distance"), Distance >= 3);
        TestFalse(TEXT("Scheduled spawn does not use an extraction tile"),
            GameMode->MapManagerComponent->IsExtractionPoint(SpawnedCoordinate));
        TestFalse(TEXT("World spawn does not start combat before contact"),
            GameMode->CombatComponent && GameMode->CombatComponent->bHasActiveEnemy);

        EnemyManager->ResetForRaid();
        GameMode->AdvanceRaidWorldTick();
        GameMode->AdvanceRaidWorldTick();
        TestEqual(TEXT("The same seed selects the same first spawn coordinate"),
            EnemyManager->GetEnemyInstances()[0].Coordinate, SpawnedCoordinate);
    }

    EnemyManager->MaxAliveEnemies = 1;
    EnemyManager->SpawnIntervalMinTicks = 1;
    EnemyManager->SpawnIntervalMaxTicks = 1;
    EnemyManager->MinimumSpawnDistance = 0;
    EnemyManager->ResetForRaid();
    for (int32 Tick = 0; Tick < 12; ++Tick)
    {
        GameMode->AdvanceRaidWorldTick();
    }
    TestEqual(TEXT("The scheduler respects the maximum alive enemy cap"),
        EnemyManager->GetAliveEnemyCount(), 1);

    EnemyManager->MaxAliveEnemies = 3;
    EnemyManager->MinimumSpawnDistance = 100;
    EnemyManager->ResetForRaid();
    GameMode->AdvanceRaidWorldTick();
    GameMode->AdvanceRaidWorldTick();
    TestEqual(TEXT("No enemy spawns when every candidate violates the distance rule"),
        EnemyManager->GetAliveEnemyCount(), 0);

    GameMode->RaidState = ERaidState::Lobby;
    EnemyManager->ResetForRaid();
    GameMode->AdvanceRaidWorldTick();
    TestEqual(TEXT("World tick does not advance outside a raid"), EnemyManager->RaidWorldTick, 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterEnemyWorldRandomWanderTest,
    "GridLootMaster.EnemyWorld.RandomWander",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterEnemyWorldRandomWanderTest::RunTest(const FString& Parameters)
{
    AGridGameMode* GameMode = NewObject<AGridGameMode>();
    TestNotNull(TEXT("Game mode is created for random wander"), GameMode);
    if (!GameMode || !GameMode->EnemyManagerComponent || !GameMode->MapManagerComponent)
    {
        return false;
    }

    GameMode->MapManagerComponent->InitializeMap();
    GameMode->RaidState = ERaidState::InRaid;
    GameMode->CurrentPlayerCoord = FIntPoint(0, 0);

    UEnemyManagerComponent* EnemyManager = GameMode->EnemyManagerComponent;
    EnemyManager->InitialSpawnDelayTicks = 100;
    EnemyManager->MaxAliveEnemies = 3;
    EnemyManager->SpawnSeed = 2026;
    EnemyManager->ResetForRaid();

    FEnemyDefinition Enemy;
    Enemy.EnemyID = TEXT("WanderScav");
    Enemy.DisplayName = TEXT("Wander Scav");
    const FIntPoint InitialCoordinate(4, 4);
    TestTrue(TEXT("A wander enemy is spawned on a valid tile"),
        EnemyManager->SpawnEnemyAt(Enemy, InitialCoordinate));

    GameMode->AdvanceRaidWorldTick();
    TestEqual(TEXT("The wander enemy remains registered after its move tick"),
        EnemyManager->GetEnemyInstances().Num(), 1);

    if (EnemyManager->GetEnemyInstances().Num() == 1)
    {
        const FEnemyWorldInstance& Instance = EnemyManager->GetEnemyInstances()[0];
        const int32 ManhattanDistance = FMath::Abs(Instance.Coordinate.X - InitialCoordinate.X) +
            FMath::Abs(Instance.Coordinate.Y - InitialCoordinate.Y);
        TestTrue(TEXT("Random wander moves at most one tile"), ManhattanDistance <= 1);
        TestTrue(TEXT("Random wander selects a reachable neighbor"),
            Instance.Coordinate == InitialCoordinate ||
            GameMode->MapManagerComponent->CanMoveBetween(InitialCoordinate, Instance.Coordinate));
        TestFalse(TEXT("Random wander does not move onto the player tile"),
            Instance.Coordinate == GameMode->CurrentPlayerCoord);
        TestTrue(TEXT("The new tile is occupied by the same enemy"),
            EnemyManager->HasEnemyAt(Instance.Coordinate));
        TestFalse(TEXT("The old tile is released after movement"),
            EnemyManager->HasEnemyAt(InitialCoordinate));
        TestEqual(TEXT("A moved wander enemy enters Wandering state"),
            Instance.WorldState, EEnemyWorldState::Wandering);
    }

    EnemyManager->ResetForRaid();
    EnemyManager->SpawnSeed = 7;
    TestTrue(TEXT("The first enemy is spawned for occupancy testing"),
        EnemyManager->SpawnEnemyAt(Enemy, FIntPoint(4, 4)));
    TestTrue(TEXT("The second enemy is spawned for occupancy testing"),
        EnemyManager->SpawnEnemyAt(Enemy, FIntPoint(4, 5)));
    for (int32 Tick = 0; Tick < 8; ++Tick)
    {
        const TArray<FEnemyWorldInstance>& BeforeMove = EnemyManager->GetEnemyInstances();
        const FIntPoint FirstBefore = BeforeMove[0].Coordinate;
        const FIntPoint SecondBefore = BeforeMove[1].Coordinate;
        GameMode->AdvanceRaidWorldTick();
        const TArray<FEnemyWorldInstance>& AfterMove = EnemyManager->GetEnemyInstances();

        TestFalse(TEXT("Two wander enemies never occupy the same tile"),
            AfterMove[0].Coordinate == AfterMove[1].Coordinate);
        if (AfterMove[0].Coordinate != FirstBefore)
        {
            TestTrue(TEXT("First enemy movement respects the wall graph"),
                GameMode->MapManagerComponent->CanMoveBetween(FirstBefore, AfterMove[0].Coordinate));
        }
        if (AfterMove[1].Coordinate != SecondBefore)
        {
            TestTrue(TEXT("Second enemy movement respects the wall graph"),
                GameMode->MapManagerComponent->CanMoveBetween(SecondBefore, AfterMove[1].Coordinate));
        }
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterMapLineOfSightTest,
    "GridLootMaster.EnemyWorld.MapDistanceAndLOS",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterMapLineOfSightTest::RunTest(const FString& Parameters)
{
    UMapManagerComponent* MapManager = NewObject<UMapManagerComponent>();
    TestNotNull(TEXT("Map manager is created for distance and LOS"), MapManager);
    if (!MapManager)
    {
        return false;
    }

    MapManager->InitializeMap();
    TestEqual(TEXT("Tile distance uses Chebyshev distance"),
        MapManager->GetTileDistance(FIntPoint(0, 0), FIntPoint(3, 3)), 3);
    TestTrue(TEXT("A clear cardinal route has line of sight"),
        MapManager->HasLineOfSight(FIntPoint(0, 0), FIntPoint(0, 2)));
    TestFalse(TEXT("A closed wall blocks line of sight"),
        MapManager->HasLineOfSight(FIntPoint(2, 0), FIntPoint(3, 0)));
    TestFalse(TEXT("A corner blocked by a wall does not allow diagonal LOS"),
        MapManager->HasLineOfSight(FIntPoint(2, 0), FIntPoint(3, 1)));
    TestFalse(TEXT("Invalid coordinates have no line of sight"),
        MapManager->HasLineOfSight(FIntPoint(0, 0), FIntPoint(9, 9)));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterEnemyWorldDetectionContactTest,
    "GridLootMaster.EnemyWorld.DetectionContact",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterEnemyWorldDetectionContactTest::RunTest(const FString& Parameters)
{
    AGridGameMode* GameMode = NewObject<AGridGameMode>();
    TestNotNull(TEXT("Game mode is created for detection contact"), GameMode);
    if (!GameMode || !GameMode->EnemyManagerComponent || !GameMode->MapManagerComponent ||
        !GameMode->CombatComponent)
    {
        return false;
    }

    GameMode->MapManagerComponent->InitializeMap();
    GameMode->RaidState = ERaidState::InRaid;
    GameMode->CurrentPlayerCoord = FIntPoint(0, 0);
    UEnemyManagerComponent* EnemyManager = GameMode->EnemyManagerComponent;
    EnemyManager->InitialSpawnDelayTicks = 100;
    EnemyManager->ResetForRaid();

    FEnemyDefinition DistantEnemy;
    DistantEnemy.EnemyID = TEXT("DistantEnemy");
    DistantEnemy.VisionRangeTiles = 2;
    DistantEnemy.DetectionPower = 100;
    TestTrue(TEXT("A distant guard enemy is spawned"),
        EnemyManager->SpawnEnemyAt(DistantEnemy, FIntPoint(4, 0), EEnemyBehaviorProfile::GuardZone));
    GameMode->AdvanceRaidWorldTick();
    TestFalse(TEXT("An enemy outside vision range does not start contact"),
        GameMode->CombatComponent->bHasActiveEnemy);
    TestEqual(TEXT("A hidden distant enemy is not revealed exactly"),
        EnemyManager->GetEnemyInstances()[0].KnowledgeState, EEnemyKnowledgeState::Hidden);
    TestFalse(TEXT("A hidden distant enemy has no exact reveal flag"),
        EnemyManager->GetEnemyInstances()[0].bRevealedToPlayer);

    EnemyManager->ResetForRaid();
    FEnemyDefinition HiddenEnemy;
    HiddenEnemy.EnemyID = TEXT("HiddenEnemy");
    HiddenEnemy.VisionRangeTiles = 0;
    HiddenEnemy.DetectionPower = 0;
    HiddenEnemy.Stealth = 0;
    GameMode->PlayerDetectionPower = 100;
    TestTrue(TEXT("A nearby enemy is spawned for suspected knowledge"),
        EnemyManager->SpawnEnemyAt(HiddenEnemy, FIntPoint(2, 0), EEnemyBehaviorProfile::GuardZone));
    GameMode->AdvanceRaidWorldTick();
    TestFalse(TEXT("Player suspicion alone does not start combat"),
        GameMode->CombatComponent->bHasActiveEnemy);
    TestEqual(TEXT("Player detection marks the enemy as suspected"),
        EnemyManager->GetEnemyInstances()[0].KnowledgeState, EEnemyKnowledgeState::Suspected);
    TestFalse(TEXT("Suspected knowledge does not reveal the exact enemy"),
        EnemyManager->GetEnemyInstances()[0].bRevealedToPlayer);

    EnemyManager->ResetForRaid();
    FEnemyDefinition ContactEnemy;
    ContactEnemy.EnemyID = TEXT("ContactEnemy");
    ContactEnemy.VisionRangeTiles = 2;
    ContactEnemy.DetectionPower = 100;
    TestTrue(TEXT("A guard enemy is spawned for normal contact"),
        EnemyManager->SpawnEnemyAt(ContactEnemy, FIntPoint(2, 0), EEnemyBehaviorProfile::GuardZone));
    GameMode->AdvanceRaidWorldTick();
    TestTrue(TEXT("Clear LOS within vision starts normal contact"),
        GameMode->CombatComponent->bHasActiveEnemy);
    TestEqual(TEXT("Contact starts combat with the correct enemy ID"),
        GameMode->CombatComponent->CurrentEnemy.Definition.EnemyID, ContactEnemy.EnemyID);
    TestEqual(TEXT("Contact stores the active world enemy ID"),
        EnemyManager->GetActiveEnemyInstanceID(), EnemyManager->GetEnemyInstances()[0].InstanceID);
    TestEqual(TEXT("Contact reveals the enemy knowledge state"),
        EnemyManager->GetEnemyInstances()[0].KnowledgeState, EEnemyKnowledgeState::Revealed);

    const FIntPoint ContactCoordinate = EnemyManager->GetEnemyInstances()[0].Coordinate;
    const int32 ContactTick = EnemyManager->RaidWorldTick;
    GameMode->AdvanceRaidWorldTick();
    TestEqual(TEXT("World simulation is paused while combat is active"),
        EnemyManager->RaidWorldTick, ContactTick);
    TestEqual(TEXT("The contacted enemy remains at its world coordinate during combat"),
        EnemyManager->GetEnemyInstances()[0].Coordinate, ContactCoordinate);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterPlayerAmbushFlowTest,
    "GridLootMaster.EnemyWorld.PlayerAmbushFlow",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterPlayerAmbushFlowTest::RunTest(const FString& Parameters)
{
    AGridGameMode* GameMode = NewObject<AGridGameMode>();
    TestNotNull(TEXT("Game mode is created for player ambush"), GameMode);
    if (!GameMode || !GameMode->EnemyManagerComponent || !GameMode->MapManagerComponent ||
        !GameMode->CombatComponent)
    {
        return false;
    }

    GameMode->MapManagerComponent->InitializeMap();
    GameMode->CurrentPlayerCoord = FIntPoint(0, 0);
    GameMode->RaidState = ERaidState::Lobby;
    TestFalse(TEXT("Player ambush is rejected outside a raid"), GameMode->RequestPlayerAmbush());

    GameMode->RaidState = ERaidState::InRaid;
    FEnemyDefinition CombatEnemy;
    CombatEnemy.EnemyID = TEXT("AmbushCombatEnemy");
    GameMode->CombatComponent->SpawnEnemy(CombatEnemy);
    TestFalse(TEXT("Player ambush is rejected during combat"), GameMode->RequestPlayerAmbush());
    GameMode->CombatComponent->ClearEnemy();

    UEnemyManagerComponent* EnemyManager = GameMode->EnemyManagerComponent;
    EnemyManager->InitialSpawnDelayTicks = 100;
    EnemyManager->ResetForRaid();
    GameMode->PlayerDetectionPower = 100;
    GameMode->PlayerDetectionRangeTiles = 2;

    FEnemyDefinition HiddenApproachEnemy;
    HiddenApproachEnemy.EnemyID = TEXT("AmbushApproachEnemy");
    HiddenApproachEnemy.VisionRangeTiles = 0;
    HiddenApproachEnemy.DetectionPower = 0;
    HiddenApproachEnemy.Stealth = 0;
    TestTrue(TEXT("An enemy is placed in the player detection range"),
        EnemyManager->SpawnEnemyAt(HiddenApproachEnemy, FIntPoint(2, 0), EEnemyBehaviorProfile::GuardZone));

    TestTrue(TEXT("Player ambush starts in a raid"), GameMode->RequestPlayerAmbush());
    TestEqual(TEXT("Player enters ambushing posture"),
        GameMode->PlayerPosture, EPlayerRaidPosture::Ambushing);
    TestEqual(TEXT("Starting ambush advances the world by one tick"), EnemyManager->RaidWorldTick, 1);
    TestEqual(TEXT("Nearby enemy becomes a suspected ambush target"),
        EnemyManager->GetEnemyInstances()[0].KnowledgeState, EEnemyKnowledgeState::Suspected);

    const int32 TickAfterAmbushStart = EnemyManager->RaidWorldTick;
    TestTrue(TEXT("Ambush wait is accepted"), GameMode->RequestAmbushWait());
    TestEqual(TEXT("Ambush wait advances the world tick"),
        EnemyManager->RaidWorldTick, TickAfterAmbushStart + 1);
    TestEqual(TEXT("Waiting keeps the player in ambushing posture"),
        GameMode->PlayerPosture, EPlayerRaidPosture::Ambushing);

    TestTrue(TEXT("Ambush assault starts a player-initiated contact"),
        GameMode->RequestAmbushAssault());
    TestTrue(TEXT("Ambush assault starts combat"), GameMode->CombatComponent->bHasActiveEnemy);
    TestTrue(TEXT("Ambush assault grants player initiative"),
        GameMode->CombatComponent->bPlayerHasInitiative);
    TestEqual(TEXT("Assault ends the ambushing posture"),
        GameMode->PlayerPosture, EPlayerRaidPosture::Normal);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterPlayerAmbushDetectionBreakTest,
    "GridLootMaster.EnemyWorld.PlayerAmbushDetectionBreaks",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterPlayerAmbushDetectionBreakTest::RunTest(const FString& Parameters)
{
    AGridGameMode* GameMode = NewObject<AGridGameMode>();
    TestNotNull(TEXT("Game mode is created for ambush detection break"), GameMode);
    if (!GameMode || !GameMode->EnemyManagerComponent || !GameMode->MapManagerComponent ||
        !GameMode->CombatComponent)
    {
        return false;
    }

    GameMode->MapManagerComponent->InitializeMap();
    GameMode->RaidState = ERaidState::InRaid;
    GameMode->CurrentPlayerCoord = FIntPoint(0, 0);
    GameMode->EnemyManagerComponent->InitialSpawnDelayTicks = 100;
    GameMode->EnemyManagerComponent->ResetForRaid();

    FEnemyDefinition AlertEnemy;
    AlertEnemy.EnemyID = TEXT("AmbushDetectionEnemy");
    AlertEnemy.VisionRangeTiles = 2;
    AlertEnemy.DetectionPower = 100;
    TestTrue(TEXT("A detecting enemy is placed near the player"),
        GameMode->EnemyManagerComponent->SpawnEnemyAt(AlertEnemy, FIntPoint(2, 0), EEnemyBehaviorProfile::GuardZone));

    TestTrue(TEXT("Ambush starts before detection"), GameMode->RequestPlayerAmbush());
    TestTrue(TEXT("Enemy detection breaks ambush and starts normal contact"),
        GameMode->CombatComponent->bHasActiveEnemy);
    TestEqual(TEXT("Detection returns player posture to normal"),
        GameMode->PlayerPosture, EPlayerRaidPosture::Normal);
    TestFalse(TEXT("Detection contact does not grant player ambush initiative"),
        GameMode->CombatComponent->bPlayerHasInitiative);
    return true;
}

#endif
