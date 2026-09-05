#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"
#include "GridGameMode.h"
#include "GridInventoryComponent.h"
#include "EquipmentComponent.h"
#include "EnemyManagerComponent.h"
#include "ItemInstance.h"
#include "Map/MapManagerComponent.h"
#include "UI/DraggableItemWidget.h"
#include "UI/ModSlotWidget.h"
#include "UI/ItemDragDropOperation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGridLootMasterFreezeItemIdentityTest,
    "GridLootMaster.SystemFreeze.ItemIdentityAcrossOwners",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterFreezeItemIdentityTest::RunTest(const FString& Parameters)
{
    AGridGameMode* GM = NewObject<AGridGameMode>();
    const TArray<UGridInventoryComponent*> Inventories = {
        GM->InventoryComponent, GM->RigComponent, GM->PocketComponent,
        GM->SafeBoxComponent, GM->LootContainerComponent, GM->StashComponent };
    for (int32 Index = 0; Index < Inventories.Num(); ++Index)
    {
        UGridInventoryComponent* Inventory = Inventories[Index];
        Inventory->InitializeGrid(2, 2);
        UItemInstance* Item = NewObject<UItemInstance>(GM);
        Item->InstanceID = FName(*FString::Printf(TEXT("Retained_%d"), Index));
        TestTrue(TEXT("Retained item is placed"), Inventory->AddItem(Item, 0, 0));
        TestTrue(TEXT("Generated identity avoids every inventory"),
            GM->MakeUniqueInstanceID(Item->InstanceID) != Item->InstanceID);
        Item->EquippedMagazine = NewObject<UItemInstance>(GM);
        Item->EquippedMagazine->InstanceID = FName(*FString::Printf(TEXT("RetainedMag_%d"), Index));
        TestTrue(TEXT("Generated identity avoids stored attachments"),
            GM->MakeUniqueInstanceID(Item->EquippedMagazine->InstanceID) != Item->EquippedMagazine->InstanceID);
    }
    UItemInstance* Weapon = NewObject<UItemInstance>(GM);
    Weapon->InstanceID = TEXT("EquippedWeapon");
    Weapon->EquippedSight = NewObject<UItemInstance>(GM);
    Weapon->EquippedSight->InstanceID = TEXT("EquippedSight");
    TestTrue(TEXT("Weapon is equipped"), GM->EquipmentComponent->EquipItem(TEXT("Primary1"), Weapon));
    TestTrue(TEXT("Equipped identity is reserved"), GM->MakeUniqueInstanceID(Weapon->InstanceID) != Weapon->InstanceID);
    TestTrue(TEXT("Equipped attachment identity is reserved"),
        GM->MakeUniqueInstanceID(Weapon->EquippedSight->InstanceID) != Weapon->EquippedSight->InstanceID);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGridLootMasterFreezeCorpseIdentityTest,
    "GridLootMaster.SystemFreeze.CorpseIdentityAcrossRaids",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterFreezeCorpseIdentityTest::RunTest(const FString& Parameters)
{
    AGridGameMode* GM = NewObject<AGridGameMode>();
    GM->ItemDataTable = NewObject<UDataTable>(GM);
    GM->ItemDataTable->RowStruct = FItemData::StaticStruct();
    FItemData Data;
    Data.ItemID = TEXT("FreezeLoot");
    Data.Size = FIntPoint(1, 1);
    Data.MaxStack = 1;
    Data.DropWeight = 1;
    GM->ItemDataTable->AddRow(Data.ItemID, Data);
    GM->StashComponent->InitializeGrid(10, 10);
    GM->PocketComponent->InitializeGrid(5, 1);
    GM->StashSaveSlot = FString::Printf(TEXT("GridLootMaster_Test_%s"), *FGuid::NewGuid().ToString());
    GM->SetRaidStartPointForTest(FIntPoint(0, 0));
    TSet<FName> RetainedIDs;
    for (int32 Raid = 0; Raid < 2; ++Raid)
    {
        if (!TestTrue(TEXT("Raid starts from lobby"), GM->StartRaid())) break;
        FEnemyDefinition Enemy;
        Enemy.EnemyID = TEXT("FreezeScav");
        if (!TestTrue(TEXT("Scav spawns"), GM->EnemyManagerComponent->SpawnEnemyAt(Enemy, FIntPoint(1, 0)))) break;
        TestTrue(TEXT("Scav dies"), GM->EnemyManagerComponent->MarkEnemyDeadForTest(Enemy.EnemyID));
        GM->CurrentPlayerCoord = FIntPoint(1, 0);
        if (!TestTrue(TEXT("Production corpse loot is generated"), GM->RequestSearchDeadBody())) break;
        UGridInventoryComponent* Corpse = GM->GetCorpseLootInventories().FindRef(Enemy.EnemyID);
        if (!TestNotNull(TEXT("Corpse inventory exists"), Corpse)) break;
        TestEqual(TEXT("Single-row loot fixture generates five items"), Corpse->ItemInstances.Num(), 5);
        TArray<UItemInstance*> Items;
        Corpse->ItemInstances.GenerateValueArray(Items);
        for (int32 Index = 0; Index < Items.Num(); ++Index)
        {
            UItemInstance* Item = Items[Index];
            TestFalse(TEXT("Next raid loot never reuses a retained identity"), RetainedIDs.Contains(Item->InstanceID));
            if (Raid == 0)
            {
                RetainedIDs.Add(Item->InstanceID);
                UGridInventoryComponent* Destination = Index == 0 ? GM->PocketComponent : GM->StashComponent;
                TestTrue(TEXT("Loot can be retained"), Destination->AddItem(Item, Index, 0));
                TestTrue(TEXT("Loot is removed from corpse"), Corpse->RemoveItem(Item->InstanceID));
            }
        }
        GM->CurrentPlayerCoord = GM->MapManagerComponent->ExtractionPoints[0];
        TestTrue(TEXT("Extraction returns to lobby"), GM->ExtractRaid());
    }
    UGameplayStatics::DeleteGameInSlot(GM->StashSaveSlot, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGridLootMasterFreezeRotationTest,
    "GridLootMaster.SystemFreeze.RotationUpdatesSectionFootprint",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterFreezeRotationTest::RunTest(const FString& Parameters)
{
    UWorld* GameWorld = nullptr;
    if (GEngine)
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.World() && Context.World()->IsGameWorld()) { GameWorld = Context.World(); break; }
        }
    }
    if (!TestNotNull(TEXT("A game world is required for the real widget key handler"), GameWorld)) return false;
    UGridInventoryComponent* Inventory = NewObject<UGridInventoryComponent>();
    Inventory->InitializeSections({ FIntPoint(3, 3), FIntPoint(3, 3) });
    UItemInstance* Item = NewObject<UItemInstance>(Inventory);
    Item->InstanceID = TEXT("RotatingItem");
    Item->BaseSize = FIntPoint(2, 1);
    TestTrue(TEXT("Item starts in a secondary section"), Inventory->AddItemToSection(Item, 1, 0, 0));
    UDraggableItemWidget* Widget = CreateWidget<UDraggableItemWidget>(GameWorld, UDraggableItemWidget::StaticClass());
    if (!TestNotNull(TEXT("Rotation widget exists"), Widget)) return false;
    Widget->ItemObj = Item;
    Widget->SourceInventory = Inventory;
    const FKeyEvent RotateEvent(EKeys::R, FModifierKeysState(), 0, false, 0, 0);
    Widget->NativeOnKeyDownForTest(FGeometry(), RotateEvent);
    TestTrue(TEXT("Item rotates"), Item->bIsRotated);
    TestEqual(TEXT("Old footprint is released"), Inventory->GetCellItemID(1, 1, 0), NAME_None);
    TestEqual(TEXT("New footprint is occupied"), Inventory->GetCellItemID(1, 0, 1), Item->InstanceID);
    TestFalse(TEXT("Another item cannot overlap the rotated item"), Inventory->CheckItemFitInSection(NAME_None, 1, 0, 1, 1, 1));
    TestEqual(TEXT("Other section remains empty"), Inventory->GetCellItemID(0, 0, 0), NAME_None);
    UItemInstance* Blocker = NewObject<UItemInstance>(Inventory);
    Blocker->InstanceID = TEXT("RotationBlocker");
    TestTrue(TEXT("Freed cell accepts another item"), Inventory->AddItemToSection(Blocker, 1, 1, 0));
    Widget->NativeOnKeyDownForTest(FGeometry(), RotateEvent);
    TestTrue(TEXT("Blocked rotation preserves orientation"), Item->bIsRotated);
    TestEqual(TEXT("Blocked rotation preserves occupied cells"), Inventory->GetCellItemID(1, 0, 1), Item->InstanceID);
    Widget->RemoveFromParent();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGridLootMasterFreezeTerminalLootTest,
    "GridLootMaster.SystemFreeze.TerminalInvalidatesCorpseAndStorage",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterFreezeTerminalLootTest::RunTest(const FString& Parameters)
{
    for (bool bExtract : { false, true })
    {
        AGridGameMode* GM = NewObject<AGridGameMode>();
        GM->RaidState = ERaidState::InRaid;
        GM->MapManagerComponent->InitializeMap();
        GM->InventoryComponent->InitializeGrid(2, 2);
        GM->RigComponent->InitializeGrid(2, 2);
        FItemData Data;
        Data.ItemID = TEXT("TerminalLoot");
        TestTrue(TEXT("Corpse has pending loot"), GM->SeedCorpseLootForTest(TEXT("TerminalCorpse"), Data));
        UGridInventoryComponent* RetainedCorpse = GM->GetCorpseLootInventories().FindRef(TEXT("TerminalCorpse"));
        if (!TestNotNull(TEXT("Drag can retain its source inventory"), RetainedCorpse)) return false;
        if (bExtract)
        {
            GM->CurrentPlayerCoord = GM->MapManagerComponent->ExtractionPoints[0];
            TestTrue(TEXT("Extraction completes"), GM->ExtractRaid());
        }
        else
        {
            GM->FailRaid();
            TestEqual(TEXT("Lost backpack has no usable storage"), GM->InventoryComponent->GetSectionCount(), 0);
            TestEqual(TEXT("Lost rig has no usable storage"), GM->RigComponent->GetSectionCount(), 0);
        }
        TestEqual(TEXT("Terminal raid invalidates even retained corpse references"), RetainedCorpse->ItemInstances.Num(), 0);
        TestEqual(TEXT("Terminal raid returns to lobby"), GM->RaidState, ERaidState::Lobby);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGridLootMasterFreezeModCancelTest,
    "GridLootMaster.SystemFreeze.CancelModDragFromStoredWeapon",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterFreezeModCancelTest::RunTest(const FString& Parameters)
{
    UWorld* GameWorld = nullptr;
    if (GEngine)
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.World() && Context.World()->IsGameWorld()) { GameWorld = Context.World(); break; }
        }
    }
    AGridGameMode* GM = GameWorld ? Cast<AGridGameMode>(GameWorld->GetAuthGameMode()) : nullptr;
    if (!TestNotNull(TEXT("A real GridGameMode game world is required"), GM)) return false;
    UGridInventoryComponent* OriginalBackpack = GM->InventoryComponent;
    UGridInventoryComponent* OriginalStash = GM->StashComponent;
    GM->InventoryComponent = NewObject<UGridInventoryComponent>(GM);
    GM->InventoryComponent->InitializeGrid(1, 1);
    GM->StashComponent = NewObject<UGridInventoryComponent>(GM);
    GM->StashComponent->InitializeGrid(1, 1);
    UItemInstance* Weapon = NewObject<UItemInstance>(GM);
    Weapon->InstanceID = TEXT("StoredModWeapon");
    Weapon->Category = EItemCategory::Weapon;
    UItemInstance* Mod = NewObject<UItemInstance>(GM);
    Mod->InstanceID = TEXT("CancelledSight");
    Mod->Category = EItemCategory::Attachment;
    Mod->AttachmentType = EAttachmentType::Sight;
    UItemInstance* Blocker = NewObject<UItemInstance>(GM);
    Blocker->InstanceID = TEXT("FullBackpack");
    TestTrue(TEXT("Backpack is full"), GM->InventoryComponent->AddItem(Blocker, 0, 0));
    TestTrue(TEXT("Unequipped weapon is in stash"), GM->StashComponent->AddItem(Weapon, 0, 0));
    UModSlotWidget* Slot = CreateWidget<UModSlotWidget>(GameWorld, UModSlotWidget::StaticClass());
    if (TestNotNull(TEXT("Inspect mod slot exists"), Slot))
    {
        Slot->Setup(Weapon, EAttachmentType::Sight);
        UItemDragDropOperation* Operation = NewObject<UItemDragDropOperation>();
        Operation->ItemObj = Mod;
        Operation->ItemID = Mod->InstanceID;
        Operation->SourceModSlot = Slot;
        FPointerEvent PointerEvent;
        FDragDropEvent Event(PointerEvent, TSharedPtr<FDragDropOperation>());
        Slot->CancelDragForTest(Event, Operation);
        TestEqual(TEXT("Cancelled attachment returns to the stored weapon even with a full backpack"), Weapon->EquippedSight, Mod);
        TestEqual(TEXT("Cancellation preserves backpack contents"), GM->InventoryComponent->GetItemInstance(Blocker->InstanceID), Blocker);
        Slot->RemoveFromParent();
    }
    GM->InventoryComponent = OriginalBackpack;
    GM->StashComponent = OriginalStash;
    return true;
}

#endif
