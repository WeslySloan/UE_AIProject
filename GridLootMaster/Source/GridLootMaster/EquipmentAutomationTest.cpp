#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "EquipmentComponent.h"
#include "GridGameMode.h"
#include "ItemInstance.h"
#include "UI/EquipmentSlotWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterEquipmentStateTest,
    "GridLootMaster.Equipment.PreventsDuplicateInstanceAndSlotRegistration",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterEquipmentStateTest::RunTest(const FString& Parameters)
{
    UEquipmentComponent* Equipment = NewObject<UEquipmentComponent>();
    TestNotNull(TEXT("Equipment component is created"), Equipment);
    if (!Equipment) return false;

    UItemInstance* Item = NewObject<UItemInstance>(Equipment);
    Item->InstanceID = TEXT("EquipmentItem");
    Item->TemplateID = TEXT("TestArmor");

    TestFalse(TEXT("An item without an instance ID cannot be equipped"), Equipment->EquipItem(TEXT("Invalid"), NewObject<UItemInstance>(Equipment)));
    TestTrue(TEXT("Item equips into an empty slot"), Equipment->EquipItem(TEXT("Armor"), Item));
    TestFalse(TEXT("The same slot rejects a second item"), Equipment->EquipItem(TEXT("Armor"), NewObject<UItemInstance>(Equipment)));
    TestFalse(TEXT("The same instance cannot occupy another slot"), Equipment->EquipItem(TEXT("Helmet"), Item));
    TestEqual(TEXT("The original slot still owns the item"), Equipment->GetEquippedItem(TEXT("Armor")), Item);

    TestTrue(TEXT("Removing an equipped slot reports success"), Equipment->RemoveItemBySlotID(TEXT("Armor")));
    TestNull(TEXT("Removing the slot clears the equipped item"), Equipment->GetEquippedItem(TEXT("Armor")));
    TestTrue(TEXT("The item can be equipped again after removal"), Equipment->EquipItem(TEXT("Armor"), Item));
    TestFalse(TEXT("Removing an empty slot reports failure"), Equipment->RemoveItemBySlotID(TEXT("MissingSlot")));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterAttachmentOwnershipTest,
    "GridLootMaster.Equipment.RemovesAttachedItemFromAllWeapons",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterAttachmentOwnershipTest::RunTest(const FString& Parameters)
{
    UEquipmentComponent* Equipment = NewObject<UEquipmentComponent>();
    UItemInstance* WeaponA = NewObject<UItemInstance>(Equipment);
    UItemInstance* WeaponB = NewObject<UItemInstance>(Equipment);
    UItemInstance* Attachment = NewObject<UItemInstance>(Equipment);

    WeaponA->InstanceID = TEXT("WeaponA");
    WeaponA->Category = EItemCategory::Weapon;
    WeaponB->InstanceID = TEXT("WeaponB");
    WeaponB->Category = EItemCategory::Weapon;
    Attachment->InstanceID = TEXT("MovedSight");
    Attachment->Category = EItemCategory::Attachment;
    Attachment->AttachmentType = EAttachmentType::Sight;

    WeaponA->EquippedSight = Attachment;
    WeaponB->EquippedSight = Attachment;
    TestTrue(TEXT("First weapon registers for ownership cleanup"), Equipment->EquipItem(TEXT("Primary1"), WeaponA));
    TestTrue(TEXT("Second weapon registers for ownership cleanup"), Equipment->EquipItem(TEXT("Primary2"), WeaponB));
    TestTrue(TEXT("Removing an attached item reports success"), Equipment->RemoveAttachedItem(Attachment));
    TestNull(TEXT("First weapon no longer owns the attachment"), WeaponA->EquippedSight);
    TestNull(TEXT("Second weapon no longer owns the attachment"), WeaponB->EquippedSight);

    TestTrue(TEXT("Removing an equipped instance reports success"), Equipment->RemoveItemByInstanceID(WeaponA->InstanceID));
    TestFalse(TEXT("Removing the same equipped instance again reports failure"), Equipment->RemoveItemByInstanceID(WeaponA->InstanceID));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterEquipmentUIClearsUnavailableSourceTest,
    "GridLootMaster.Equipment.ClearsStaleItemWhenSourceUnavailable",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterEquipmentUIClearsUnavailableSourceTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for equipment UI test"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is available for equipment UI test"), GameMode);
    if (!GameMode) return false;

    UEquipmentSlotWidget* Slot = CreateWidget<UEquipmentSlotWidget>(GameWorld, UEquipmentSlotWidget::StaticClass());
    TestNotNull(TEXT("An equipment slot widget is created"), Slot);
    if (!Slot) return false;

    Slot->InitSlot(TEXT("Primary1"), EItemCategory::Weapon, TEXT("Primary"));
    UItemInstance* StaleItem = NewObject<UItemInstance>(Slot);
    StaleItem->InstanceID = TEXT("StaleEquipmentUIItem");
    StaleItem->Category = EItemCategory::Weapon;
    Slot->SetEquippedItem(StaleItem);

    UEquipmentComponent* PreviousEquipment = GameMode->EquipmentComponent;
    GameMode->EquipmentComponent = nullptr;
    Slot->OnEquipmentChanged();

    TestNull(TEXT("The equipment UI clears its stale item when the source is unavailable"), Slot->EquippedItem);

    GameMode->EquipmentComponent = PreviousEquipment;
    Slot->RemoveFromParent();
    return true;
}

#endif
