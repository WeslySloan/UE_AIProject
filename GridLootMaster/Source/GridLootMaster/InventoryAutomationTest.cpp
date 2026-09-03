#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Components/Button.h"
#include "Misc/AutomationTest.h"
#include "UObject/SoftObjectPath.h"
#include "GridGameMode.h"
#include "GridInventoryComponent.h"
#include "ItemInstance.h"
#include "UI/InspectWidget.h"
#include "UI/MainGameUI.h"
#include "UI/ContextMenuWidget.h"
#include "UI/DraggableItemWidget.h"
#include "UI/EquipmentSlotWidget.h"
#include "UI/GridBoardWidget.h"
#include "UI/ItemDragDropOperation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterPartialMergeTest,
    "GridLootMaster.Inventory.PartialMergePreservesRemainder",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterPartialMergeTest::RunTest(const FString& Parameters)
{
    UGridInventoryComponent* Inventory = NewObject<UGridInventoryComponent>();
    TestNotNull(TEXT("Inventory component is created"), Inventory);
    if (!Inventory) return false;

    Inventory->InitializeGrid(4, 4);

    UItemInstance* Target = NewObject<UItemInstance>(Inventory);
    Target->InstanceID = TEXT("TargetStack");
    Target->TemplateID = TEXT("Ammo_9x19");
    Target->BaseSize = FIntPoint(1, 1);
    Target->MaxStack = 10;
    Target->CurrentStack = 8;

    UItemInstance* Source = NewObject<UItemInstance>(Inventory);
    Source->InstanceID = TEXT("SourceStack");
    Source->TemplateID = TEXT("Ammo_9x19");
    Source->BaseSize = FIntPoint(1, 1);
    Source->MaxStack = 10;
    Source->CurrentStack = 5;

    TestTrue(TEXT("Target stack is placed"), Inventory->AddItem(Target, 0, 0));
    TestTrue(TEXT("Partial merge succeeds"), Inventory->TryMergeItem(Source, Target->InstanceID));
    TestEqual(TEXT("Target is filled to its maximum"), Target->CurrentStack, 10);
    TestEqual(TEXT("Unmerged source remainder is preserved"), Source->CurrentStack, 3);
    TestEqual(TEXT("Target remains in the inventory"), Inventory->GetItemInstance(Target->InstanceID), Target);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterSplitPreservesIconTest,
    "GridLootMaster.Inventory.SplitPreservesItemIcon",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterSplitPreservesIconTest::RunTest(const FString& Parameters)
{
    UGridInventoryComponent* Inventory = NewObject<UGridInventoryComponent>();
    TestNotNull(TEXT("Inventory component is created for split icon test"), Inventory);
    if (!Inventory) return false;

    Inventory->InitializeGrid(2, 1);

    UItemInstance* Source = NewObject<UItemInstance>(Inventory);
    Source->InstanceID = TEXT("IconSplitSource");
    Source->TemplateID = TEXT("Ammo_9x19");
    Source->ItemName = TEXT("9x19mm Ammo");
    Source->BaseSize = FIntPoint(1, 1);
    Source->MaxStack = 10;
    Source->CurrentStack = 5;
    const FSoftObjectPath ExpectedIcon(TEXT("/Game/Icons/TestIcon.TestIcon"));
    Source->ItemIcon = ExpectedIcon;
    Source->CachedDynamicIcon = NewObject<UTexture2D>(Inventory);
    TestTrue(TEXT("The source stack is placed"), Inventory->AddItem(Source, 0, 0));

    UDraggableItemWidget* Widget = NewObject<UDraggableItemWidget>();
    TestNotNull(TEXT("The source item widget is created"), Widget);
    if (!Widget) return false;
    Widget->ItemObj = Source;
    Widget->SourceInventory = Inventory;
    Widget->OnAutoSplitConfirmed(2);

    UItemInstance* SplitItem = nullptr;
    for (const TPair<FName, UItemInstance*>& Pair : Inventory->ItemInstances)
    {
        if (Pair.Value != Source)
        {
            SplitItem = Pair.Value;
            break;
        }
    }

    TestNotNull(TEXT("A new split item is registered"), SplitItem);
    if (!SplitItem) return false;
    TestEqual(TEXT("The source stack keeps its remainder"), Source->CurrentStack, 3);
    TestEqual(TEXT("The split item receives the requested amount"), SplitItem->CurrentStack, 2);
    TestEqual(TEXT("The split item preserves the source icon"),
        SplitItem->ItemIcon.ToSoftObjectPath(), ExpectedIcon);
    TestEqual(TEXT("The split item preserves the cached dynamic icon"),
        SplitItem->CachedDynamicIcon, Source->CachedDynamicIcon);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterSplitRejectsAttachedStackTest,
    "GridLootMaster.Inventory.SplitRejectsAttachedStack",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterSplitRejectsAttachedStackTest::RunTest(const FString& Parameters)
{
    UGridInventoryComponent* Inventory = NewObject<UGridInventoryComponent>();
    TestNotNull(TEXT("Inventory component is created for attached split test"), Inventory);
    if (!Inventory) return false;

    Inventory->InitializeGrid(2, 1);
    UItemInstance* Weapon = NewObject<UItemInstance>(Inventory);
    Weapon->InstanceID = TEXT("AttachedStackWeapon");
    Weapon->TemplateID = TEXT("M4A1");
    Weapon->Category = EItemCategory::Weapon;
    Weapon->BaseSize = FIntPoint(1, 1);
    Weapon->MaxStack = 2;
    Weapon->CurrentStack = 2;

    UItemInstance* Magazine = NewObject<UItemInstance>(Inventory);
    Magazine->InstanceID = TEXT("AttachedStackMagazine");
    Magazine->TemplateID = TEXT("Mag_M4");
    Magazine->Category = EItemCategory::Attachment;
    Magazine->AttachmentType = EAttachmentType::Magazine;
    Weapon->EquippedMagazine = Magazine;
    TestTrue(TEXT("The attached stack weapon is placed"), Inventory->AddItem(Weapon, 0, 0));

    UDraggableItemWidget* Widget = NewObject<UDraggableItemWidget>();
    TestNotNull(TEXT("The attached stack widget is created"), Widget);
    if (!Widget) return false;
    Widget->ItemObj = Weapon;
    Widget->SourceInventory = Inventory;
    Widget->OnAutoSplitConfirmed(1);

    TestEqual(TEXT("Attached stack splitting leaves the source stack unchanged"), Weapon->CurrentStack, 2);
    TestEqual(TEXT("Attached stack splitting does not create a second instance"), Inventory->ItemInstances.Num(), 1);
    TestEqual(TEXT("The original weapon keeps its attachment"), Weapon->EquippedMagazine, Magazine);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterRotatedFootprintTest,
    "GridLootMaster.Inventory.RotatedFootprintRespectsGridBounds",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterRotatedFootprintTest::RunTest(const FString& Parameters)
{
    UGridInventoryComponent* Inventory = NewObject<UGridInventoryComponent>();
    TestNotNull(TEXT("Inventory component is created"), Inventory);
    if (!Inventory) return false;

    Inventory->InitializeGrid(3, 2);

    UItemInstance* Item = NewObject<UItemInstance>(Inventory);
    Item->InstanceID = TEXT("RotatedItem");
    Item->TemplateID = TEXT("TestItem");
    Item->BaseSize = FIntPoint(2, 1);
    Item->bIsRotated = true;

    TestEqual(TEXT("Rotation swaps the item footprint"), Item->GetCurrentSize(), FIntPoint(1, 2));
    TestTrue(TEXT("Rotated footprint fits at the right edge"), Inventory->AddItem(Item, 2, 0));
    TestEqual(TEXT("Top rotated cell stores the item"), Inventory->GridCells[Inventory->GetIndex(2, 0)], Item->InstanceID);
    TestEqual(TEXT("Bottom rotated cell stores the item"), Inventory->GridCells[Inventory->GetIndex(2, 1)], Item->InstanceID);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterCrossInventoryMoveTest,
    "GridLootMaster.Inventory.CrossInventoryMovePreservesSourceOnTargetFailure",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterCrossInventoryMoveTest::RunTest(const FString& Parameters)
{
    UGridInventoryComponent* SourceInventory = NewObject<UGridInventoryComponent>();
    UGridInventoryComponent* TargetInventory = NewObject<UGridInventoryComponent>();
    TestNotNull(TEXT("Source inventory is created"), SourceInventory);
    TestNotNull(TEXT("Target inventory is created"), TargetInventory);
    if (!SourceInventory || !TargetInventory) return false;

    SourceInventory->InitializeGrid(2, 2);
    TargetInventory->InitializeGrid(1, 1);

    UItemInstance* SourceItem = NewObject<UItemInstance>(SourceInventory);
    SourceItem->InstanceID = TEXT("CrossMoveItem");
    SourceItem->TemplateID = TEXT("TestItem");
    SourceItem->BaseSize = FIntPoint(1, 1);

    UItemInstance* Blocker = NewObject<UItemInstance>(TargetInventory);
    Blocker->InstanceID = TEXT("TargetBlocker");
    Blocker->TemplateID = TEXT("Blocker");
    Blocker->BaseSize = FIntPoint(1, 1);

    TestTrue(TEXT("Source item is placed"), SourceInventory->AddItem(SourceItem, 0, 0));
    TestTrue(TEXT("Target blocker is placed"), TargetInventory->AddItem(Blocker, 0, 0));

    const bool bTargetAccepted = TargetInventory->AddItem(SourceItem, 0, 0);
    TestFalse(TEXT("Target rejects the item when its only cell is occupied"), bTargetAccepted);
    TestEqual(TEXT("Source item remains available after target rejection"), SourceInventory->GetItemInstance(SourceItem->InstanceID), SourceItem);
    TestEqual(TEXT("Target blocker remains intact"), TargetInventory->GetItemInstance(Blocker->InstanceID), Blocker);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterRemoveItemReportsFailureTest,
    "GridLootMaster.Inventory.RemoveItemReportsMissingPlacement",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterRemoveItemReportsFailureTest::RunTest(const FString& Parameters)
{
    UGridInventoryComponent* Inventory = NewObject<UGridInventoryComponent>();
    TestNotNull(TEXT("Inventory is created for removal result test"), Inventory);
    if (!Inventory) return false;

    Inventory->InitializeGrid(1, 1);
    UItemInstance* DetachedItem = NewObject<UItemInstance>(Inventory);
    DetachedItem->InstanceID = TEXT("DetachedItem");
    DetachedItem->TemplateID = TEXT("TestItem");
    DetachedItem->BaseSize = FIntPoint(1, 1);
    Inventory->ItemInstances.Add(DetachedItem->InstanceID, DetachedItem);

    TestFalse(TEXT("Removing an item without occupied cells reports failure"),
        Inventory->RemoveItem(DetachedItem->InstanceID));
    TestEqual(TEXT("A failed removal does not silently discard the item record"),
        Inventory->GetItemInstance(DetachedItem->InstanceID), DetachedItem);

    TestTrue(TEXT("Removing a valid placed item reports success"), Inventory->AddItem(DetachedItem, 0, 0));
    TestTrue(TEXT("A placed item can be removed"), Inventory->RemoveItem(DetachedItem->InstanceID));
    TestNull(TEXT("A removed item is no longer registered"), Inventory->GetItemInstance(DetachedItem->InstanceID));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterRejectsEmptyRemovalIDTest,
    "GridLootMaster.Inventory.RejectsEmptyRemovalID",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterRejectsEmptyRemovalIDTest::RunTest(const FString& Parameters)
{
    UGridInventoryComponent* Inventory = NewObject<UGridInventoryComponent>();
    TestNotNull(TEXT("Inventory is created for empty removal ID test"), Inventory);
    if (!Inventory) return false;

    Inventory->InitializeGrid(2, 1);
    UItemInstance* Item = NewObject<UItemInstance>(Inventory);
    Item->InstanceID = TEXT("EmptyRemovalGuardItem");
    Item->TemplateID = TEXT("TestItem");
    Item->BaseSize = FIntPoint(1, 1);
    TestTrue(TEXT("An item is placed before the invalid removal"), Inventory->AddItem(Item, 0, 0));

    TestFalse(TEXT("Removing with an empty ID is rejected"), Inventory->RemoveItem(NAME_None));
    TestEqual(TEXT("The placed item remains registered after empty ID removal"),
        Inventory->GetItemInstance(Item->InstanceID), Item);
    TestEqual(TEXT("The placed cell remains unchanged after empty ID removal"),
        Inventory->GridCells[Inventory->GetIndex(0, 0)], Item->InstanceID);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterDuplicateInstanceIDTest,
    "GridLootMaster.Inventory.RejectsDuplicateInstanceID",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterDuplicateInstanceIDTest::RunTest(const FString& Parameters)
{
    UGridInventoryComponent* Inventory = NewObject<UGridInventoryComponent>();
    TestNotNull(TEXT("Inventory is created for duplicate ID test"), Inventory);
    if (!Inventory) return false;

    Inventory->InitializeGrid(2, 1);

    UItemInstance* ExistingItem = NewObject<UItemInstance>(Inventory);
    ExistingItem->InstanceID = TEXT("DuplicateInstance");
    ExistingItem->TemplateID = TEXT("ExistingTemplate");
    ExistingItem->BaseSize = FIntPoint(1, 1);
    TestTrue(TEXT("The original item is placed"), Inventory->AddItem(ExistingItem, 0, 0));

    UItemInstance* DuplicateItem = NewObject<UItemInstance>(Inventory);
    DuplicateItem->InstanceID = ExistingItem->InstanceID;
    DuplicateItem->TemplateID = TEXT("DuplicateTemplate");
    DuplicateItem->BaseSize = FIntPoint(1, 1);

    TestFalse(TEXT("A different object with the same InstanceID is rejected"),
        Inventory->AddItem(DuplicateItem, 1, 0));
    TestEqual(TEXT("The original object remains registered"),
        Inventory->GetItemInstance(ExistingItem->InstanceID), ExistingItem);
    TestEqual(TEXT("The original grid cell remains unchanged"),
        Inventory->GridCells[Inventory->GetIndex(0, 0)], ExistingItem->InstanceID);
    TestEqual(TEXT("Only the original object remains registered"), Inventory->ItemInstances.Num(), 1);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterStaleRotationWidgetTest,
    "GridLootMaster.Inventory.StaleRotationWidgetDoesNotMutateItem",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterStaleRotationWidgetTest::RunTest(const FString& Parameters)
{
    UGridInventoryComponent* Inventory = NewObject<UGridInventoryComponent>();
    TestNotNull(TEXT("Inventory is created for stale rotation test"), Inventory);
    if (!Inventory) return false;

    Inventory->InitializeGrid(3, 2);

    UItemInstance* CurrentItem = NewObject<UItemInstance>(Inventory);
    CurrentItem->InstanceID = TEXT("RotationItem");
    CurrentItem->TemplateID = TEXT("CurrentTemplate");
    CurrentItem->BaseSize = FIntPoint(2, 1);
    TestTrue(TEXT("The current item is placed"), Inventory->AddItem(CurrentItem, 0, 0));

    UItemInstance* StaleItem = NewObject<UItemInstance>(Inventory);
    StaleItem->InstanceID = CurrentItem->InstanceID;
    StaleItem->TemplateID = TEXT("StaleTemplate");
    StaleItem->BaseSize = FIntPoint(2, 1);

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

    TestNotNull(TEXT("A game world is available for stale rotation test"), GameWorld);
    if (!GameWorld) return false;

    UDraggableItemWidget* Widget = CreateWidget<UDraggableItemWidget>(GameWorld, UDraggableItemWidget::StaticClass());
    TestNotNull(TEXT("A draggable widget is created for stale rotation test"), Widget);
    if (!Widget) return false;
    Widget->ItemObj = StaleItem;
    Widget->SourceInventory = Inventory;

    const FKeyEvent RotateEvent(EKeys::R, FModifierKeysState(), 0, false, 0, 0);
    Widget->NativeOnKeyDownForTest(FGeometry(), RotateEvent);

    TestFalse(TEXT("A stale widget does not rotate its detached item"), StaleItem->bIsRotated);
    TestFalse(TEXT("The current inventory item remains unrotated"), CurrentItem->bIsRotated);
    TestEqual(TEXT("The current item remains registered as the source object"),
        Inventory->GetItemInstance(CurrentItem->InstanceID), CurrentItem);

    Widget->RemoveFromParent();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterStaleAmmoDropTest,
    "GridLootMaster.Inventory.StaleAmmoDropDoesNotMutateMagazine",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterStaleAmmoDropTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for stale ammo drop test"), GameWorld);
    if (!GameWorld) return false;

    UGridInventoryComponent* TargetInventory = NewObject<UGridInventoryComponent>(GameWorld);
    UGridInventoryComponent* SourceInventory = NewObject<UGridInventoryComponent>(GameWorld);
    TestNotNull(TEXT("Target inventory is created for stale ammo drop test"), TargetInventory);
    TestNotNull(TEXT("Source inventory is created for stale ammo drop test"), SourceInventory);
    if (!TargetInventory || !SourceInventory) return false;

    TargetInventory->InitializeGrid(1, 1);
    SourceInventory->InitializeGrid(1, 1);

    UItemInstance* Magazine = NewObject<UItemInstance>(TargetInventory);
    Magazine->InstanceID = TEXT("StaleAmmoMagazine");
    Magazine->Category = EItemCategory::Attachment;
    Magazine->AttachmentType = EAttachmentType::Magazine;
    Magazine->BaseSize = FIntPoint(1, 1);
    Magazine->CompatibleAmmo = TEXT("5.56x45mm");
    Magazine->MaxAmmo = 10;
    Magazine->CurrentAmmo = 0;
    TestTrue(TEXT("The target magazine is placed"), TargetInventory->AddItem(Magazine, 0, 0));

    UItemInstance* CurrentAmmo = NewObject<UItemInstance>(SourceInventory);
    CurrentAmmo->InstanceID = TEXT("StaleAmmo");
    CurrentAmmo->Category = EItemCategory::Consumable;
    CurrentAmmo->ItemName = TEXT("5.56x45mm");
    CurrentAmmo->CurrentStack = 5;
    TestTrue(TEXT("The current ammo is placed in its source"), SourceInventory->AddItem(CurrentAmmo, 0, 0));

    UItemInstance* StaleAmmo = NewObject<UItemInstance>(GameWorld);
    StaleAmmo->InstanceID = CurrentAmmo->InstanceID;
    StaleAmmo->Category = EItemCategory::Consumable;
    StaleAmmo->ItemName = CurrentAmmo->ItemName;
    StaleAmmo->CurrentStack = 5;

    UGridBoardWidget* Board = CreateWidget<UGridBoardWidget>(GameWorld, UGridBoardWidget::StaticClass());
    TestNotNull(TEXT("Grid board is created for stale ammo drop test"), Board);
    if (!Board) return false;
    Board->InventoryComponent = TargetInventory;

    UItemDragDropOperation* DropOperation = NewObject<UItemDragDropOperation>(GameWorld);
    DropOperation->ItemID = StaleAmmo->InstanceID;
    DropOperation->ItemObj = StaleAmmo;
    DropOperation->SourceInventory = SourceInventory;

    FPointerEvent PointerEvent;
    FDragDropEvent DragDropEvent(PointerEvent, TSharedPtr<FDragDropOperation>());
    TestFalse(TEXT("A stale ammo drop is rejected before mutation"),
        Board->NativeOnDropForTest(FGeometry(), DragDropEvent, DropOperation));
    TestEqual(TEXT("The target magazine remains unloaded"), Magazine->CurrentAmmo, 0);
    TestEqual(TEXT("The stale ammo stack remains unchanged"), StaleAmmo->CurrentStack, 5);
    TestEqual(TEXT("The current source ammo remains registered"),
        SourceInventory->GetItemInstance(CurrentAmmo->InstanceID), CurrentAmmo);
    TestEqual(TEXT("The current source ammo remains unchanged"), CurrentAmmo->CurrentStack, 5);

    Board->RemoveFromParent();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterStaleInspectWidgetTest,
    "GridLootMaster.Inventory.StaleInspectWidgetDoesNotMutateItem",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterStaleInspectWidgetTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for stale inspect test"), GameWorld);
    if (!GameWorld) return false;

    UGridInventoryComponent* Inventory = NewObject<UGridInventoryComponent>(GameWorld);
    TestNotNull(TEXT("Inventory is created for stale inspect test"), Inventory);
    if (!Inventory) return false;
    Inventory->InitializeGrid(1, 1);

    UItemInstance* CurrentItem = NewObject<UItemInstance>(Inventory);
    CurrentItem->InstanceID = TEXT("InspectItem");
    CurrentItem->ItemName = TEXT("Current Item");
    CurrentItem->bIsExamined = false;
    TestTrue(TEXT("The current item is placed"), Inventory->AddItem(CurrentItem, 0, 0));

    UItemInstance* StaleItem = NewObject<UItemInstance>(GameWorld);
    StaleItem->InstanceID = CurrentItem->InstanceID;
    StaleItem->ItemName = TEXT("Stale Item");
    StaleItem->bIsExamined = false;

    UDraggableItemWidget* Widget = CreateWidget<UDraggableItemWidget>(GameWorld, UDraggableItemWidget::StaticClass());
    TestNotNull(TEXT("A draggable widget is created for stale inspect test"), Widget);
    if (!Widget) return false;
    Widget->SourceInventory = Inventory;
    Widget->HandleInspectItem(StaleItem);

    TestFalse(TEXT("A stale inspect action does not examine the detached item"), StaleItem->bIsExamined);
    TestFalse(TEXT("The current item remains unexamined"), CurrentItem->bIsExamined);
    TestEqual(TEXT("The current item remains registered as the source object"),
        Inventory->GetItemInstance(CurrentItem->InstanceID), CurrentItem);

    Widget->RemoveFromParent();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterStaleEquipmentInspectTest,
    "GridLootMaster.Equipment.StaleInspectMenuDoesNotMutateItem",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterStaleEquipmentInspectTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for stale equipment inspect test"), GameWorld);
    if (!GameWorld) return false;

    UItemInstance* CurrentItem = NewObject<UItemInstance>(GameWorld);
    CurrentItem->InstanceID = TEXT("EquippedInspectItem");
    CurrentItem->ItemName = TEXT("Current Equipped Item");
    CurrentItem->bIsExamined = false;

    UItemInstance* StaleItem = NewObject<UItemInstance>(GameWorld);
    StaleItem->InstanceID = CurrentItem->InstanceID;
    StaleItem->ItemName = TEXT("Stale Equipped Item");
    StaleItem->bIsExamined = false;

    UEquipmentSlotWidget* Slot = CreateWidget<UEquipmentSlotWidget>(GameWorld, UEquipmentSlotWidget::StaticClass());
    TestNotNull(TEXT("An equipment slot widget is created for stale inspect test"), Slot);
    if (!Slot) return false;
    Slot->EquippedItem = CurrentItem;
    Slot->HandleInspectItem(StaleItem);

    TestFalse(TEXT("A stale equipment inspect action does not examine the detached item"), StaleItem->bIsExamined);
    TestFalse(TEXT("The current equipped item remains unexamined"), CurrentItem->bIsExamined);

    Slot->RemoveFromParent();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterInspectFlowTest,
    "GridLootMaster.Inventory.InspectMarksItemExamined",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterInspectFlowTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for Inspect UI"), GameWorld);
    if (!GameWorld) return false;

    UItemInstance* Item = NewObject<UItemInstance>(GameWorld);
    Item->InstanceID = TEXT("InspectItem");
    Item->TemplateID = TEXT("TestItem");
    Item->ItemName = TEXT("Inspect Test Item");
    Item->BaseSize = FIntPoint(2, 1);
    Item->bIsExamined = false;

    UInspectWidget* InspectWidget = CreateWidget<UInspectWidget>(GameWorld, UInspectWidget::StaticClass());
    TestNotNull(TEXT("Inspect widget is created"), InspectWidget);
    if (!InspectWidget) return false;

    TestNotNull(TEXT("Inspect widget builds its root UI"), InspectWidget->GetRootWidget());
    InspectWidget->Setup(Item);
    TestTrue(TEXT("Inspect marks the item as examined"), Item->bIsExamined);

    InspectWidget->RemoveFromParent();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterSellStackTest,
    "GridLootMaster.Inventory.SellUsesStackValueAndPreservesUnknownItems",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterSellStackTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for the sell test"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode || !GameMode->InventoryComponent || !GameMode->ItemDataTable) return false;

    const ERaidState PreviousRaidState = GameMode->RaidState;
    const int32 PreviousScore = GameMode->CurrentScore;
    const int32 PreviousQuota = GameMode->QuotaScore;
    GameMode->RaidState = ERaidState::InRaid;
    GameMode->CurrentScore = 0;
    GameMode->QuotaScore = MAX_int32;
    GameMode->InventoryComponent->ClearInventory();

    const FItemData* SellableData = GameMode->ItemDataTable->FindRow<FItemData>(TEXT("Ammo_9x19"), TEXT("SellAutomationTest"));
    TestNotNull(TEXT("A sellable DataTable row exists"), SellableData);
    if (!SellableData)
    {
        GameMode->RaidState = PreviousRaidState;
        GameMode->CurrentScore = PreviousScore;
        GameMode->QuotaScore = PreviousQuota;
        return false;
    }

    UItemInstance* SellableItem = NewObject<UItemInstance>(GameMode);
    SellableItem->InstanceID = TEXT("SellStackAutomationItem");
    SellableItem->InitFromData(*SellableData);
    SellableItem->CurrentStack = 2;

    int32 SellX = 0;
    int32 SellY = 0;
    TestTrue(TEXT("An empty cell is available for the sellable item"),
        GameMode->InventoryComponent->FindEmptySpace(1, 1, SellX, SellY));
    if (!GameMode->InventoryComponent->AddItem(SellableItem, SellX, SellY))
    {
        GameMode->RaidState = PreviousRaidState;
        GameMode->CurrentScore = PreviousScore;
        GameMode->QuotaScore = PreviousQuota;
        return false;
    }

    UItemInstance* UnknownItem = NewObject<UItemInstance>(GameMode);
    UnknownItem->InstanceID = TEXT("SellUnknownAutomationItem");
    UnknownItem->TemplateID = TEXT("MissingTemplate");
    UnknownItem->BaseSize = FIntPoint(1, 1);

    int32 UnknownX = 0;
    int32 UnknownY = 0;
    TestTrue(TEXT("A second empty cell is available for the unknown item"),
        GameMode->InventoryComponent->FindEmptySpace(1, 1, UnknownX, UnknownY));
    TestTrue(TEXT("Unknown item is placed for preservation check"),
        GameMode->InventoryComponent->AddItem(UnknownItem, UnknownX, UnknownY));

    UMainGameUI* UI = CreateWidget<UMainGameUI>(GameWorld, UMainGameUI::StaticClass());
    TestNotNull(TEXT("Main game UI is created"), UI);
    if (UI)
    {
        UI->OnSellButtonClicked();
    }

    TestEqual(TEXT("Stack value is added once per stack unit"), GameMode->CurrentScore, SellableData->Value * 2);
    TestNull(TEXT("Sellable item is removed after selling"),
        GameMode->InventoryComponent->GetItemInstance(SellableItem->InstanceID));
    TestEqual(TEXT("Unknown-template item is preserved"),
        GameMode->InventoryComponent->GetItemInstance(UnknownItem->InstanceID), UnknownItem);

    if (UI) UI->RemoveFromParent();
    GameMode->InventoryComponent->RemoveItem(UnknownItem->InstanceID);
    GameMode->RaidState = PreviousRaidState;
    GameMode->CurrentScore = PreviousScore;
    GameMode->QuotaScore = PreviousQuota;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterSellRejectsUnplacedItemTest,
    "GridLootMaster.Inventory.SellRejectsUnplacedItem",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterSellRejectsUnplacedItemTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for the sell integrity test"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode || !GameMode->InventoryComponent || !GameMode->ItemDataTable) return false;

    const ERaidState PreviousRaidState = GameMode->RaidState;
    const int32 PreviousScore = GameMode->CurrentScore;
    GameMode->RaidState = ERaidState::InRaid;
    GameMode->CurrentScore = 0;
    GameMode->InventoryComponent->ClearInventory();

    const FItemData* SellableData = GameMode->ItemDataTable->FindRow<FItemData>(TEXT("Ammo_9x19"), TEXT("SellIntegrityAutomationTest"));
    TestNotNull(TEXT("A sellable DataTable row exists"), SellableData);
    if (!SellableData)
    {
        GameMode->RaidState = PreviousRaidState;
        GameMode->CurrentScore = PreviousScore;
        return false;
    }

    UItemInstance* UnplacedItem = NewObject<UItemInstance>(GameMode);
    UnplacedItem->InstanceID = TEXT("UnplacedSellAutomationItem");
    UnplacedItem->InitFromData(*SellableData);
    GameMode->InventoryComponent->ItemInstances.Add(UnplacedItem->InstanceID, UnplacedItem);

    UMainGameUI* UI = CreateWidget<UMainGameUI>(GameWorld, UMainGameUI::StaticClass());
    TestNotNull(TEXT("Main game UI is created"), UI);
    if (UI)
    {
        UI->OnSellButtonClicked();
        TestEqual(TEXT("Sell does not award score for an unplaced item"), GameMode->CurrentScore, 0);
        TestEqual(TEXT("Sell preserves the unplaced item when removal fails"),
            GameMode->InventoryComponent->GetItemInstance(UnplacedItem->InstanceID), UnplacedItem);

        GameMode->CurrentScore = 0;
        UI->OnSellAllButtonClicked();
        TestEqual(TEXT("Sell all does not award score for an unplaced item"), GameMode->CurrentScore, 0);
        TestEqual(TEXT("Sell all preserves the unplaced item when removal fails"),
            GameMode->InventoryComponent->GetItemInstance(UnplacedItem->InstanceID), UnplacedItem);
        UI->RemoveFromParent();
    }

    GameMode->InventoryComponent->ItemInstances.Remove(UnplacedItem->InstanceID);
    GameMode->RaidState = PreviousRaidState;
    GameMode->CurrentScore = PreviousScore;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterContextMenuAvailabilityTest,
    "GridLootMaster.Inventory.ContextMenuHidesUnavailableUnequip",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterContextMenuAvailabilityTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for context menu availability"), GameWorld);
    if (!GameWorld) return false;

    UContextMenuWidget* ContextMenu = CreateWidget<UContextMenuWidget>(GameWorld, UContextMenuWidget::StaticClass());
    TestNotNull(TEXT("Context menu is created"), ContextMenu);
    if (!ContextMenu) return false;

    ContextMenu->Setup(NewObject<UItemInstance>(ContextMenu), FVector2D::ZeroVector);
    UButton* UnequipButton = Cast<UButton>(ContextMenu->GetWidgetFromName(TEXT("UnequipButton")));
    TestNotNull(TEXT("Unequip button is available for the visibility check"), UnequipButton);
    if (!UnequipButton) return false;

    TestEqual(TEXT("Unequip is hidden for non-equipment items"), UnequipButton->GetVisibility(), ESlateVisibility::Collapsed);
    ContextMenu->RemoveFromParent();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterInitFromDataResetsStateTest,
    "GridLootMaster.Inventory.InitFromDataResetsInstanceState",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterInitFromDataResetsStateTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for InitFromData state reset"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode || !GameMode->ItemDataTable) return false;

    const FItemData* WeaponData = GameMode->ItemDataTable->FindRow<FItemData>(TEXT("M4A1"), TEXT("InitFromDataAutomationTest"));
    TestNotNull(TEXT("A weapon DataTable row exists"), WeaponData);
    if (!WeaponData) return false;

    UItemInstance* ReusedItem = NewObject<UItemInstance>(GameWorld);
    ReusedItem->bIsRotated = true;
    ReusedItem->bIsExamined = false;
    ReusedItem->EquippedSight = NewObject<UItemInstance>(ReusedItem);
    ReusedItem->EquippedMuzzle = NewObject<UItemInstance>(ReusedItem);
    ReusedItem->EquippedMagazine = NewObject<UItemInstance>(ReusedItem);

    ReusedItem->InitFromData(*WeaponData);

    TestFalse(TEXT("Data initialization clears stale rotation state"), ReusedItem->bIsRotated);
    TestTrue(TEXT("Data initialization restores the default examined state"), ReusedItem->bIsExamined);
    TestNull(TEXT("Data initialization clears a stale sight"), ReusedItem->EquippedSight);
    TestNull(TEXT("Data initialization clears a stale muzzle"), ReusedItem->EquippedMuzzle);
    TestNull(TEXT("Data initialization clears a stale magazine"), ReusedItem->EquippedMagazine);
    return true;
}

#endif
