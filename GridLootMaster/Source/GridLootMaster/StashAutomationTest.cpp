#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Kismet/GameplayStatics.h"
#include "GridGameMode.h"
#include "GridInventoryComponent.h"
#include "EquipmentComponent.h"
#include "CombatComponent.h"
#include "ItemData.h"
#include "ItemInstance.h"
#include "Map/MapManagerComponent.h"
#include "UI/MainGameUI.h"
#include "Misc/Guid.h"
#include "StashSaveGame.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterStashValidationTest,
    "GridLootMaster.Stash.RejectsUnroundtrippableAttachment",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterStashValidationTest::RunTest(const FString& Parameters)
{
    AGridGameMode* GameMode = NewObject<AGridGameMode>();
    TestNotNull(TEXT("Game mode is created"), GameMode);
    if (!GameMode) return false;

    TestEqual(TEXT("Default raid time is configured on the game mode"), GameMode->TotalTimeLimit, 60.0f);
    TestEqual(TEXT("Default quota is configured on the game mode"), GameMode->QuotaScore, 1000);
    TestEqual(TEXT("Default health is configured on the game mode"), GameMode->MaxHealth, 100);

    UDataTable* ItemDataTable = NewObject<UDataTable>(GameMode);
    ItemDataTable->RowStruct = FItemData::StaticStruct();

    FItemData WeaponData;
    WeaponData.ItemID = TEXT("TestWeapon");
    WeaponData.Category = EItemCategory::Weapon;
    WeaponData.Size = FIntPoint(1, 1);
    WeaponData.MaxStack = 1;
    ItemDataTable->AddRow(TEXT("TestWeapon"), WeaponData);
    GameMode->ItemDataTable = ItemDataTable;

    GameMode->StashComponent->InitializeGrid(2, 2);
    UItemInstance* Weapon = NewObject<UItemInstance>(GameMode);
    Weapon->InitFromData(WeaponData);
    Weapon->InstanceID = TEXT("StashWeapon");
    Weapon->TemplateID = TEXT("TestWeapon");

    UItemInstance* InvalidAttachment = NewObject<UItemInstance>(GameMode);
    InvalidAttachment->InstanceID = TEXT("MissingAttachment");
    InvalidAttachment->TemplateID = TEXT("TestWeapon");
    InvalidAttachment->Category = EItemCategory::Attachment;
    InvalidAttachment->AttachmentType = EAttachmentType::Sight;
    Weapon->EquippedSight = InvalidAttachment;

    TestTrue(TEXT("Weapon is placed in the stash"), GameMode->StashComponent->AddItem(Weapon, 0, 0));
    TestFalse(TEXT("Stash save rejects an attachment whose template has the wrong category"), GameMode->SaveStash());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterStashRejectsMalformedGridTest,
    "GridLootMaster.Stash.RejectsMalformedGridShape",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterStashRejectsMalformedGridTest::RunTest(const FString& Parameters)
{
    AGridGameMode* GameMode = NewObject<AGridGameMode>();
    TestNotNull(TEXT("Game mode is created for malformed stash test"), GameMode);
    if (!GameMode) return false;

    UDataTable* ItemDataTable = NewObject<UDataTable>(GameMode);
    ItemDataTable->RowStruct = FItemData::StaticStruct();

    FItemData ItemData;
    ItemData.ItemID = TEXT("MalformedGridItem");
    ItemData.ItemName = TEXT("Malformed Grid Item");
    ItemData.Category = EItemCategory::Valuable;
    ItemData.Size = FIntPoint(1, 1);
    ItemData.MaxStack = 1;
    ItemDataTable->AddRow(TEXT("MalformedGridItem"), ItemData);
    GameMode->ItemDataTable = ItemDataTable;

    GameMode->StashComponent->InitializeGrid(2, 2);
    GameMode->StashComponent->GridCells.SetNum(5);
    UItemInstance* Item = NewObject<UItemInstance>(GameMode);
    Item->InitFromData(ItemData);
    Item->InstanceID = TEXT("MalformedGridInstance");
    GameMode->StashComponent->ItemInstances.Add(Item->InstanceID, Item);
    GameMode->StashComponent->GridCells[4] = Item->InstanceID;

    TestFalse(TEXT("Stash save rejects a grid cell array with an invalid shape"), GameMode->SaveStash());
    TestEqual(TEXT("Malformed stash item remains available after rejected save"),
        GameMode->StashComponent->GetItemInstance(Item->InstanceID), Item);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterStashSerializationTest,
    "GridLootMaster.Stash.SaveGameRoundTripPreservesState",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterStashSerializationTest::RunTest(const FString& Parameters)
{
    const FString SlotName = FString::Printf(TEXT("GridLootMaster_Test_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    UStashSaveGame* SaveGame = Cast<UStashSaveGame>(UGameplayStatics::CreateSaveGameObject(UStashSaveGame::StaticClass()));
    TestNotNull(TEXT("Stash save object is created"), SaveGame);
    if (!SaveGame) return false;

    SaveGame->GridWidth = 10;
    SaveGame->GridHeight = 10;

    FStashItemRecord Record;
    Record.InstanceID = TEXT("SavedWeapon");
    Record.TemplateID = TEXT("M4A1");
    Record.GridX = 2;
    Record.GridY = 3;
    Record.CurrentAmmo = 17;
    Record.bIsRotated = true;
    Record.bIsExamined = false;
    Record.bHasEquippedMagazine = true;
    Record.EquippedMagazine.InstanceID = TEXT("SavedMagazine");
    Record.EquippedMagazine.TemplateID = TEXT("Mag_M4");
    Record.EquippedMagazine.CurrentAmmo = 12;
    SaveGame->Items.Add(Record);

    const bool bSaved = UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, 0);
    TestTrue(TEXT("Stash save is written to the isolated test slot"), bSaved);
    if (!bSaved)
    {
        UGameplayStatics::DeleteGameInSlot(SlotName, 0);
        return false;
    }

    UStashSaveGame* LoadedSaveGame = Cast<UStashSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
    TestNotNull(TEXT("Stash save can be loaded after the write"), LoadedSaveGame);
    if (LoadedSaveGame)
    {
        TestEqual(TEXT("Grid width survives the round trip"), LoadedSaveGame->GridWidth, 10);
        TestEqual(TEXT("Grid height survives the round trip"), LoadedSaveGame->GridHeight, 10);
        TestEqual(TEXT("One stash item survives the round trip"), LoadedSaveGame->Items.Num(), 1);
        if (LoadedSaveGame->Items.Num() == 1)
        {
            const FStashItemRecord& LoadedRecord = LoadedSaveGame->Items[0];
            TestEqual(TEXT("Item instance ID survives the round trip"), LoadedRecord.InstanceID, FName(TEXT("SavedWeapon")));
            TestEqual(TEXT("Item position survives the round trip"), LoadedRecord.GridX, 2);
            TestEqual(TEXT("Item ammo survives the round trip"), LoadedRecord.CurrentAmmo, 17);
            TestTrue(TEXT("Attachment presence survives the round trip"), LoadedRecord.bHasEquippedMagazine);
            TestEqual(TEXT("Attachment instance ID survives the round trip"), LoadedRecord.EquippedMagazine.InstanceID, FName(TEXT("SavedMagazine")));
            TestEqual(TEXT("Attachment ammo survives the round trip"), LoadedRecord.EquippedMagazine.CurrentAmmo, 12);
        }
    }

    UGameplayStatics::DeleteGameInSlot(SlotName, 0);
    return LoadedSaveGame != nullptr;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterExtractionPreservesCarriedItemsTest,
    "GridLootMaster.Stash.ExtractionPreservesCarriedItems",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterExtractionPreservesCarriedItemsTest::RunTest(const FString& Parameters)
{
    AGridGameMode* GameMode = NewObject<AGridGameMode>();
    TestNotNull(TEXT("Game mode is created"), GameMode);
    if (!GameMode) return false;

    GameMode->RaidState = ERaidState::InRaid;
    GameMode->ItemDataTable = nullptr;
    GameMode->InventoryComponent->InitializeGrid(2, 2);
    GameMode->StashComponent->InitializeGrid(2, 2);
    GameMode->MapManagerComponent->InitializeMap();
    TestTrue(TEXT("An extraction point exists for the rollback extraction test"),
        GameMode->MapManagerComponent->ExtractionPoints.Num() > 0);
    if (GameMode->MapManagerComponent->ExtractionPoints.Num() == 0) return false;
    GameMode->CurrentPlayerCoord = GameMode->MapManagerComponent->ExtractionPoints[0];

    UItemInstance* Item = NewObject<UItemInstance>(GameMode);
    Item->InstanceID = TEXT("RollbackItem");
    Item->TemplateID = TEXT("MissingTemplate");
    Item->BaseSize = FIntPoint(1, 1);
    TestTrue(TEXT("Raid item is placed in the source inventory"), GameMode->InventoryComponent->AddItem(Item, 0, 0));

    TestTrue(TEXT("Extraction succeeds without moving carried items to the stash"), GameMode->ExtractRaid());
    TestEqual(TEXT("Successful extraction returns to the lobby"), GameMode->RaidState, ERaidState::Lobby);
    TestEqual(TEXT("Source inventory keeps the item after successful extraction"),
        GameMode->InventoryComponent->GetItemInstance(Item->InstanceID), Item);
    TestNull(TEXT("Successful extraction does not move the item to the stash"),
        GameMode->StashComponent->GetItemInstance(Item->InstanceID));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterExtractionWithoutWorldTest,
    "GridLootMaster.Stash.ExtractionWithoutWorld",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterExtractionWithoutWorldTest::RunTest(const FString& Parameters)
{
    const FString SaveSlot = FString::Printf(TEXT("GridLootMaster_Test_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));

    AGridGameMode* GameMode = NewObject<AGridGameMode>();
    TestNotNull(TEXT("Game mode is created without a world"), GameMode);
    if (!GameMode) return false;
    GameMode->StashSaveSlot = SaveSlot;

    GameMode->ItemDataTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/DataTable/DT_ItemData.DT_ItemData"));
    TestNotNull(TEXT("Item DataTable is available for extraction"), GameMode->ItemDataTable);
    if (!GameMode->ItemDataTable) return false;

    GameMode->RaidState = ERaidState::InRaid;
    GameMode->StashComponent->InitializeGrid(1, 1);
    GameMode->MapManagerComponent->InitializeMap();
    TestTrue(TEXT("An extraction point exists for the worldless extraction test"),
        GameMode->MapManagerComponent->ExtractionPoints.Num() > 0);
    if (GameMode->MapManagerComponent->ExtractionPoints.Num() == 0) return false;
    GameMode->CurrentPlayerCoord = GameMode->MapManagerComponent->ExtractionPoints[0];

    const bool bExtracted = GameMode->ExtractRaid();
    TestTrue(TEXT("Extraction succeeds without a world when the stash is valid"), bExtracted);
    TestEqual(TEXT("Extraction returns to the lobby without a world"),
        GameMode->RaidState, ERaidState::Lobby);
    TestTrue(TEXT("A new raid can start after extraction"), GameMode->StartRaid());
    TestEqual(TEXT("Starting the next raid returns to InRaid"), GameMode->RaidState, ERaidState::InRaid);

    UGameplayStatics::DeleteGameInSlot(SaveSlot, 0);
    return bExtracted;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterFailureReturnsToLobbyTest,
    "GridLootMaster.Stash.FailureReturnsToLobbyAndAllowsNextRaid",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterFailureReturnsToLobbyTest::RunTest(const FString& Parameters)
{
    const FString SaveSlot = FString::Printf(TEXT("GridLootMaster_Test_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));

    AGridGameMode* GameMode = NewObject<AGridGameMode>();
    TestNotNull(TEXT("Game mode is created for the failure recovery test"), GameMode);
    if (!GameMode) return false;
    GameMode->StashSaveSlot = SaveSlot;

    GameMode->ItemDataTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/DataTable/DT_ItemData.DT_ItemData"));
    TestNotNull(TEXT("Item DataTable is available for the failure recovery test"), GameMode->ItemDataTable);
    if (!GameMode->ItemDataTable) return false;

    GameMode->StashComponent->InitializeGrid(1, 1);
    GameMode->MapManagerComponent->InitializeMap();
    GameMode->RaidState = ERaidState::InRaid;

    GameMode->FailRaid();
    TestEqual(TEXT("A failed raid returns to the lobby"), GameMode->RaidState, ERaidState::Lobby);
    TestTrue(TEXT("The next raid can start after a failed raid"), GameMode->StartRaid());
    TestEqual(TEXT("The next raid enters the active state"), GameMode->RaidState, ERaidState::InRaid);

    UGameplayStatics::DeleteGameInSlot(SaveSlot, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterSafeBoxExtractionTest,
    "GridLootMaster.Stash.ExtractionPreservesSafeBox",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterSafeBoxExtractionTest::RunTest(const FString& Parameters)
{
    UWorld* GameWorld = nullptr;
    if (GEngine)
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.World() && Context.World()->IsGameWorld())
            {
                GameWorld = Context.World();
                break;
            }
        }
    }

    TestNotNull(TEXT("A game world is available for SafeBox extraction"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode || !GameMode->ItemDataTable || !GameMode->SafeBoxComponent ||
        !GameMode->StashComponent || !GameMode->EquipmentComponent || !GameMode->CombatComponent) return false;

    const FItemData* SafeBoxItemData = GameMode->ItemDataTable->FindRow<FItemData>(TEXT("Ammo_9x19"), TEXT("SafeBoxExtractionTest"));
    TestNotNull(TEXT("A valid SafeBox item row exists"), SafeBoxItemData);
    if (!SafeBoxItemData) return false;

    GameMode->RaidState = ERaidState::InRaid;
    GameMode->CombatComponent->ClearEnemy();
    GameMode->InventoryComponent->ClearInventory();
    GameMode->LootContainerComponent->ClearInventory();
    GameMode->RigComponent->ClearInventory();
    GameMode->PocketComponent->ClearInventory();
    GameMode->EquipmentComponent->ClearEquipment();
    GameMode->StashComponent->InitializeGrid(2, 2);
    GameMode->SafeBoxComponent->InitializeGrid(1, 1);
    GameMode->MapManagerComponent->InitializeMap();
    TestTrue(TEXT("An extraction point exists for SafeBox extraction"),
        GameMode->MapManagerComponent->ExtractionPoints.Num() > 0);
    if (GameMode->MapManagerComponent->ExtractionPoints.Num() == 0) return false;
    GameMode->CurrentPlayerCoord = GameMode->MapManagerComponent->ExtractionPoints[0];

    UItemInstance* SafeBoxItem = NewObject<UItemInstance>(GameMode);
    SafeBoxItem->InstanceID = TEXT("SafeBoxExtractionItem");
    SafeBoxItem->InitFromData(*SafeBoxItemData);
    TestTrue(TEXT("SafeBox item is placed before extraction"),
        GameMode->SafeBoxComponent->AddItem(SafeBoxItem, 0, 0));

    const bool bExtracted = GameMode->ExtractRaid();
    TestTrue(TEXT("Extraction succeeds with a SafeBox item"), bExtracted);
    TestEqual(TEXT("SafeBox item remains in the SafeBox after extraction"),
        GameMode->SafeBoxComponent->GetItemInstance(SafeBoxItem->InstanceID), SafeBoxItem);
    TestNull(TEXT("SafeBox item is not moved to the stash automatically"),
        GameMode->StashComponent->GetItemInstance(SafeBoxItem->InstanceID));

    return bExtracted;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterExtractionRequiresPointTest,
    "GridLootMaster.Stash.ExtractionRequiresExtractionPoint",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterExtractionRequiresPointTest::RunTest(const FString& Parameters)
{
    AGridGameMode* GameMode = NewObject<AGridGameMode>();
    TestNotNull(TEXT("Game mode is created for extraction point validation"), GameMode);
    if (!GameMode) return false;

    UDataTable* ItemDataTable = NewObject<UDataTable>(GameMode);
    ItemDataTable->RowStruct = FItemData::StaticStruct();
    GameMode->ItemDataTable = ItemDataTable;
    GameMode->StashComponent->InitializeGrid(2, 2);
    GameMode->MapManagerComponent->InitializeMap();
    GameMode->RaidState = ERaidState::InRaid;
    GameMode->CurrentPlayerCoord = GameMode->MapManagerComponent->SpawnPoint;

    TestFalse(TEXT("Extraction is rejected away from an extraction point"), GameMode->ExtractRaid());
    TestEqual(TEXT("Raid remains active away from an extraction point"), GameMode->RaidState, ERaidState::InRaid);

    TestTrue(TEXT("The map generated an extraction point"), GameMode->MapManagerComponent->ExtractionPoints.Num() > 0);
    if (GameMode->MapManagerComponent->ExtractionPoints.Num() == 0) return false;
    GameMode->CurrentPlayerCoord = GameMode->MapManagerComponent->ExtractionPoints[0];
    TestTrue(TEXT("Extraction succeeds at an extraction point"), GameMode->ExtractRaid());
    TestEqual(TEXT("Extraction returns to the lobby"), GameMode->RaidState, ERaidState::Lobby);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterStashReloadTest,
    "GridLootMaster.Stash.SaveAndReloadPreservesItemState",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterStashReloadTest::RunTest(const FString& Parameters)
{
    const FString SaveSlot = FString::Printf(TEXT("GridLootMaster_Test_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));

    UWorld* GameWorld = nullptr;
    if (GEngine)
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.World() && Context.World()->IsGameWorld())
            {
                GameWorld = Context.World();
                break;
            }
        }
    }

    TestNotNull(TEXT("A game world is available for Stash reload"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode || !GameMode->ItemDataTable || !GameMode->StashComponent) return false;
    GameMode->StashSaveSlot = SaveSlot;

    const FItemData* ItemData = GameMode->ItemDataTable->FindRow<FItemData>(TEXT("Junk_Bolts"), TEXT("StashReloadTest"));
    TestNotNull(TEXT("A valid Stash item row exists"), ItemData);
    if (!ItemData) return false;

    GameMode->StashComponent->InitializeGrid(4, 4);
    UItemInstance* Item = NewObject<UItemInstance>(GameMode);
    Item->InstanceID = TEXT("StashReloadItem");
    Item->InitFromData(*ItemData);
    Item->CurrentStack = 1;
    Item->bIsRotated = true;
    Item->bIsExamined = false;
    TestTrue(TEXT("Stash item is placed before saving"), GameMode->StashComponent->AddItem(Item, 1, 1));

    const bool bSaved = GameMode->SaveStash();
    TestTrue(TEXT("Stash is saved through the GameMode API"), bSaved);
    if (!bSaved)
    {
        UGameplayStatics::DeleteGameInSlot(SaveSlot, 0);
        return false;
    }

    GameMode->RaidState = ERaidState::InRaid;
    GameMode->StashComponent->ClearInventory();
    GameMode->RaidState = ERaidState::Lobby;
    TestTrue(TEXT("Stash is empty before reload"), GameMode->StashComponent->ItemInstances.Num() == 0);

    const bool bLoaded = GameMode->LoadStash();
    TestTrue(TEXT("Stash reload succeeds after the in-memory reset"), bLoaded);
    UItemInstance* ReloadedItem = GameMode->StashComponent->GetItemInstance(Item->InstanceID);
    TestNotNull(TEXT("Saved item exists after Stash reload"), ReloadedItem);
    if (ReloadedItem)
    {
        TestEqual(TEXT("Reloaded item keeps its saved X coordinate"),
            GameMode->StashComponent->GridCells.IndexOfByKey(ReloadedItem->InstanceID) % GameMode->StashComponent->GridWidth, 1);
        TestEqual(TEXT("Reloaded item keeps its saved Y coordinate"),
            GameMode->StashComponent->GridCells.IndexOfByKey(ReloadedItem->InstanceID) / GameMode->StashComponent->GridWidth, 1);
        TestTrue(TEXT("Reloaded item keeps its rotation"), ReloadedItem->bIsRotated);
        TestFalse(TEXT("Reloaded item keeps its examined state"), ReloadedItem->bIsExamined);
    }

    UGameplayStatics::DeleteGameInSlot(SaveSlot, 0);
    return bLoaded && ReloadedItem != nullptr;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterStashAutosaveTest,
    "GridLootMaster.Stash.AutosavesInventoryChangesInLobby",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterStashAutosaveTest::RunTest(const FString& Parameters)
{
    UWorld* GameWorld = nullptr;
    if (GEngine)
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.World() && Context.World()->IsGameWorld())
            {
                GameWorld = Context.World();
                break;
            }
        }
    }

    TestNotNull(TEXT("A game world is available for Stash autosave"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode || !GameMode->ItemDataTable || !GameMode->StashComponent) return false;

    const FItemData* ItemData = GameMode->ItemDataTable->FindRow<FItemData>(TEXT("Junk_Bolts"), TEXT("StashAutosaveTest"));
    TestNotNull(TEXT("A valid Stash item row exists for autosave"), ItemData);
    if (!ItemData) return false;

    const FString SaveSlot = FString::Printf(TEXT("GridLootMaster_Test_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    GameMode->StashSaveSlot = SaveSlot;
    GameMode->RaidState = ERaidState::Lobby;
    GameMode->StashComponent->InitializeGrid(2, 2);

    UMainGameUI* UI = CreateWidget<UMainGameUI>(GameWorld, UMainGameUI::StaticClass());
    TestNotNull(TEXT("Main UI is created for Stash autosave"), UI);
    if (!UI)
    {
        UGameplayStatics::DeleteGameInSlot(SaveSlot, 0);
        return false;
    }

    UItemInstance* Item = NewObject<UItemInstance>(GameMode);
    Item->InstanceID = TEXT("StashAutosaveItem");
    Item->InitFromData(*ItemData);
    const bool bAdded = GameMode->StashComponent->AddItem(Item, 1, 1);
    TestTrue(TEXT("Adding an item to the lobby Stash succeeds"), bAdded);

    UStashSaveGame* SavedGame = Cast<UStashSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
    TestNotNull(TEXT("Stash changes are saved automatically"), SavedGame);
    bool bFoundItem = false;
    if (SavedGame)
    {
        for (const FStashItemRecord& Record : SavedGame->Items)
        {
            if (Record.InstanceID == Item->InstanceID)
            {
                bFoundItem = true;
                TestEqual(TEXT("Autosaved item keeps its X coordinate"), Record.GridX, 1);
                TestEqual(TEXT("Autosaved item keeps its Y coordinate"), Record.GridY, 1);
                break;
            }
        }
    }
    TestTrue(TEXT("The changed Stash item is present in the autosave"), bFoundItem);

    UGameplayStatics::DeleteGameInSlot(SaveSlot, 0);
    UI->RemoveFromParent();
    return bAdded && SavedGame != nullptr && bFoundItem;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterStartRaidTest,
    "GridLootMaster.Raid.LobbyCanStartRaid",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterStartRaidTest::RunTest(const FString& Parameters)
{
    const FString SaveSlot = FString::Printf(TEXT("GridLootMaster_Test_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));

    AGridGameMode* GameMode = NewObject<AGridGameMode>();
    TestNotNull(TEXT("Game mode is created"), GameMode);
    if (!GameMode) return false;
    GameMode->StashSaveSlot = SaveSlot;

    UDataTable* ItemDataTable = NewObject<UDataTable>(GameMode);
    ItemDataTable->RowStruct = FItemData::StaticStruct();
    FItemData TestItemData;
    TestItemData.ItemID = TEXT("StartRaidTestItem");
    TestItemData.Category = EItemCategory::Valuable;
    TestItemData.Size = FIntPoint(1, 1);
    TestItemData.MaxStack = 1;
    ItemDataTable->AddRow(TestItemData.ItemID, TestItemData);

    GameMode->ItemDataTable = ItemDataTable;
    GameMode->StashComponent->InitializeGrid(2, 2);
    GameMode->RaidState = ERaidState::Lobby;
    GameMode->TotalTimeLimit = 45.0f;
    GameMode->MaxHealth = 100;
    GameMode->CurrentScore = 25;
    GameMode->RemainingTime = 3.0f;
    GameMode->CurrentHealth = 1;
    GameMode->CombatComponent->LastCombatMessage = TEXT("Previous raid combat message");

    const bool bStarted = GameMode->StartRaid();
    TestTrue(TEXT("Lobby starts a raid"), bStarted);
    TestEqual(TEXT("Raid state changes to InRaid"), GameMode->RaidState, ERaidState::InRaid);
    TestEqual(TEXT("Raid score resets at start"), GameMode->CurrentScore, 0);
    TestEqual(TEXT("Raid timer resets at start"), GameMode->RemainingTime, 45.0f);
    TestEqual(TEXT("Player health resets at start"), GameMode->CurrentHealth, 100);
    TestTrue(TEXT("Previous raid combat message is cleared at start"),
        GameMode->CombatComponent->LastCombatMessage.IsEmpty());

    UGameplayStatics::DeleteGameInSlot(SaveSlot, 0);
    return bStarted;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterUniqueInstanceIDTest,
    "GridLootMaster.Stash.UniqueInstanceIDAvoidsSavedCollision",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterUniqueInstanceIDTest::RunTest(const FString& Parameters)
{
    AGridGameMode* GameMode = NewObject<AGridGameMode>();
    TestNotNull(TEXT("Game mode is created for instance ID collision test"), GameMode);
    if (!GameMode || !GameMode->StashComponent) return false;

    GameMode->StashComponent->InitializeGrid(2, 2);

    UItemInstance* SavedItem = NewObject<UItemInstance>(GameMode);
    SavedItem->InstanceID = TEXT("Item_DefBackpack");
    SavedItem->TemplateID = TEXT("SavedBackpack");
    SavedItem->BaseSize = FIntPoint(1, 1);
    TestTrue(TEXT("Existing saved item is placed in the stash"),
        GameMode->StashComponent->AddItem(SavedItem, 0, 0));

    const FName UniqueID = GameMode->MakeUniqueInstanceID(TEXT("Item_DefBackpack"));
    TestTrue(TEXT("A saved ID collision receives a different instance ID"),
        UniqueID != SavedItem->InstanceID && UniqueID != NAME_None);
    TestFalse(TEXT("The generated ID is not already occupied by the stash"),
        GameMode->StashComponent->GridCells.Contains(UniqueID));
    return true;
}

#endif
