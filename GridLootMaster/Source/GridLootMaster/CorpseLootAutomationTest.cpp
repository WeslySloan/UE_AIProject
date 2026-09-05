#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "GridGameMode.h"
#include "EnemyManagerComponent.h"
#include "GridInventoryComponent.h"
#include "ItemData.h"
#include "ItemInstance.h"
#include "Map/MapManagerComponent.h"
#include "CombatComponent.h"


namespace
{
    bool PrepareDeadScav(AGridGameMode* GameMode, FName& OutInstanceID)
    {
        if (!GameMode || !GameMode->EnemyManagerComponent || !GameMode->MapManagerComponent) return false;
        GameMode->RaidState = ERaidState::InRaid;
        GameMode->CurrentPlayerCoord = FIntPoint(0, 0);
        GameMode->MapManagerComponent->SpawnPoint = FIntPoint(0, 0);
        GameMode->MapManagerComponent->InitializeMap();
        GameMode->EnemyManagerComponent->ResetForRaid();
        FEnemyDefinition Enemy;
        Enemy.EnemyID = TEXT("CorpseScav");
        Enemy.DisplayName = TEXT("Corpse Scav");
        if (!GameMode->EnemyManagerComponent->SpawnEnemyAt(Enemy, FIntPoint(1, 0))) return false;
        OutInstanceID = GameMode->EnemyManagerComponent->GetEnemyInstances()[0].InstanceID;
        const bool bMarkedDead = GameMode->EnemyManagerComponent->MarkEnemyDeadForTest(OutInstanceID);
        GameMode->CurrentPlayerCoord = FIntPoint(1, 0);
        return bMarkedDead;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterCorpseLootLifecycleTest,
    "GridLootMaster.Corpse.PersistentPerEnemyLifecycle",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterCorpseLootLifecycleTest::RunTest(const FString& Parameters)
{
    AGridGameMode* GameMode = NewObject<AGridGameMode>();
    FName CorpseID = NAME_None;
    TestTrue(TEXT("A dead scav can be prepared without a world tick"), PrepareDeadScav(GameMode, CorpseID));
    if (!GameMode || CorpseID.IsNone()) return false;

    const FIntPoint DeathCoord = GameMode->EnemyManagerComponent->GetEnemyInstances()[0].Coordinate;
    FItemData ItemData;
    ItemData.ItemID = TEXT("CorpseTestItem");
    ItemData.ItemName = TEXT("Corpse Test Item");
    ItemData.Size = FIntPoint(1, 1);
    ItemData.MaxStack = 1;
    TestTrue(TEXT("Corpse owns its generated inventory"), GameMode->SeedCorpseLootForTest(CorpseID, ItemData));
    TestTrue(TEXT("Dead body search is available on the same tile"), GameMode->RequestSearchDeadBody());
    TestEqual(TEXT("Corpse unexamined items are queued"), GameMode->GetPendingExamineCountForTest(), 1);
    TestEqual(TEXT("Corpse loot is generated exactly once"), GameMode->GetCorpseLootGenerationCount(CorpseID), 1);

    UGridInventoryComponent* CorpseInventory = nullptr;
    const FName CorpseItemID(*FString::Printf(TEXT("TestCorpse_%s"), *CorpseID.ToString()));
    TestTrue(TEXT("Corpse item is discoverable by its own inventory"),
        GameMode->FindCorpseLootInventory(CorpseItemID, CorpseInventory));
    TestNotNull(TEXT("Corpse inventory remains valid"), CorpseInventory);
    if (CorpseInventory)
    {
        const FName ItemID = CorpseItemID;
        TestFalse(TEXT("Corpse item starts unexamined"), CorpseInventory->GetItemInstance(ItemID)->bIsExamined);
        GameMode->ProcessNextExamineForTest();
        TestTrue(TEXT("Corpse examination uses the corpse inventory"), CorpseInventory->GetItemInstance(ItemID)->bIsExamined);
        TestTrue(TEXT("Taking an item removes it from corpse remaining loot"), CorpseInventory->RemoveItem(ItemID));
        GameMode->CurrentPlayerCoord = FIntPoint(2, 0);
        GameMode->InvalidateCorpseSearchIfPlayerLeftTile();
        TestTrue(TEXT("Leaving the corpse tile invalidates active search"), !GameMode->RequestSearchDeadBody());
        GameMode->CurrentPlayerCoord = DeathCoord;
        TestFalse(TEXT("An empty corpse no longer claims the search action"), GameMode->RequestSearchDeadBody());
        TestFalse(TEXT("Empty corpse allows the generic search action to be selected"),
            GameMode->HasDeadBodyAtCurrentPlayerCoord());
        TestEqual(TEXT("Re-search does not regenerate corpse loot"), GameMode->GetCorpseLootGenerationCount(CorpseID), 1);
        TestNull(TEXT("Taken item does not respawn"), CorpseInventory->GetItemInstance(ItemID));
    }

    // SpawnEnemyAt intentionally rejects the player's current tile.
    // Move the synthetic test player away before spawning another enemy on the corpse tile.
    GameMode->CurrentPlayerCoord = FIntPoint(0, 0);

    FEnemyDefinition SecondEnemy;
    SecondEnemy.EnemyID = TEXT("CorpseScavB");
    TestTrue(TEXT("A second dead scav can share the raid"),
        GameMode->EnemyManagerComponent->SpawnEnemyAt(SecondEnemy, FIntPoint(1, 0)));
    const FName SecondCorpseID = GameMode->EnemyManagerComponent->GetEnemyInstances().Last().InstanceID;
    TestTrue(TEXT("The second scav becomes a separate corpse"),
        GameMode->EnemyManagerComponent->MarkEnemyDeadForTest(SecondCorpseID));
    GameMode->CurrentPlayerCoord = FIntPoint(1, 0);
    TestTrue(TEXT("The second corpse receives remaining loot"), GameMode->SeedCorpseLootForTest(SecondCorpseID, ItemData));
    TestTrue(TEXT("Second corpse can be searched independently"), GameMode->RequestSearchDeadBody());
    TestTrue(TEXT("Two corpse inventories are independent"),
        GameMode->GetCorpseLootInventories().FindRef(CorpseID) != GameMode->GetCorpseLootInventories().FindRef(SecondCorpseID));

    GameMode->FailRaid();
    TestEqual(TEXT("Raid failure clears corpse state"), GameMode->GetCorpseLootInventories().Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterCorpseSearchGuardsTest,
    "GridLootMaster.Corpse.SearchGuards",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterCorpseSearchGuardsTest::RunTest(const FString& Parameters)
{
    AGridGameMode* GameMode = NewObject<AGridGameMode>();
    FName CorpseID = NAME_None;
    TestTrue(TEXT("Dead scav setup succeeds"), PrepareDeadScav(GameMode, CorpseID));
    if (!GameMode) return false;

    GameMode->CombatComponent->bHasActiveEnemy = true;
    TestFalse(TEXT("Active combat rejects corpse search"), GameMode->RequestSearchDeadBody());
    GameMode->CombatComponent->bHasActiveEnemy = false;
    GameMode->PlayerPosture = EPlayerRaidPosture::Ambushing;
    TestFalse(TEXT("Incompatible ambush posture rejects corpse search"), GameMode->RequestSearchDeadBody());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterAliveEnemyWinsCorpseMarkerPriorityTest,
    "GridLootMaster.Corpse.AliveEnemyMarkerPriority",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterAliveEnemyWinsCorpseMarkerPriorityTest::RunTest(const FString& Parameters)
{
    AGridGameMode* GameMode = NewObject<AGridGameMode>();
    FName CorpseID = NAME_None;
    TestTrue(TEXT("Dead scav setup succeeds"), PrepareDeadScav(GameMode, CorpseID));
    if (!GameMode || !GameMode->EnemyManagerComponent) return false;

    // The corpse tile itself is not occupied by the dead enemy, but SpawnEnemyAt
    // still rejects spawning directly on the player's tile. Move the test player away.
    GameMode->CurrentPlayerCoord = FIntPoint(0, 0);

    FEnemyDefinition AliveEnemy;
    AliveEnemy.EnemyID = TEXT("AliveOnCorpseTile");
    TestTrue(TEXT("Alive enemy can share a corpse tile"),
        GameMode->EnemyManagerComponent->SpawnEnemyAt(AliveEnemy, FIntPoint(1, 0)));
    FName FoundCorpseID = NAME_None;
    TestTrue(TEXT("Dead corpse remains discoverable on the shared tile"),
        GameMode->EnemyManagerComponent->FindDeadEnemyAt(FIntPoint(1, 0), FoundCorpseID));
    TestEqual(TEXT("The dead corpse identity is preserved"), FoundCorpseID, CorpseID);
    TestEqual(TEXT("The alive enemy remains in the alive count"),
        GameMode->EnemyManagerComponent->GetAliveEnemyCount(), 1);
    return true;
}

#endif
