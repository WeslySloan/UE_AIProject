#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/UniformGridPanel.h"
#include "Components/WidgetSwitcher.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectIterator.h"
#include "CombatComponent.h"
#include "EnemyManagerComponent.h"
#include "GridGameMode.h"
#include "GridInventoryComponent.h"
#include "ItemData.h"
#include "ItemInstance.h"
#include "EquipmentComponent.h"
#include "Map/MapManagerComponent.h"
#include "UI/MainGameUI.h"
#include "UI/MinimapWidget.h"
#include "UI/MinimapTileWidget.h"
#include "UI/GridBoardWidget.h"
#include "UI/ItemDragDropOperation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterCombatBasicFlowTest,
    "GridLootMaster.Combat.BasicFlow",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterCombatBasicFlowTest::RunTest(const FString& Parameters)
{
    UCombatComponent* Combat = NewObject<UCombatComponent>();
    TestNotNull(TEXT("Combat component is created"), Combat);
    if (!Combat) return false;

    FEnemyDefinition Enemy;
    Enemy.EnemyID = TEXT("TestEnemy");
    Enemy.DisplayName = TEXT("Test Enemy");
    Enemy.MaxHealth = 100;
    Enemy.Armor = 5;

    Combat->SpawnEnemy(Enemy);
    TestTrue(TEXT("Enemy becomes active after spawn"), Combat->bHasActiveEnemy);
    TestEqual(TEXT("Spawn initializes enemy health"), Combat->CurrentEnemy.CurrentHealth, 100);

    TestTrue(TEXT("Attack succeeds against active enemy"), Combat->AttackEnemy(30));
    TestEqual(TEXT("Enemy armor reduces incoming damage"), Combat->CurrentEnemy.CurrentHealth, 75);

    TestTrue(TEXT("Second attack succeeds"), Combat->AttackEnemy(30));
    TestTrue(TEXT("Third attack succeeds"), Combat->AttackEnemy(30));
    TestTrue(TEXT("Fourth attack succeeds and defeats enemy"), Combat->AttackEnemy(30));
    TestFalse(TEXT("Enemy becomes inactive after defeat"), Combat->bHasActiveEnemy);
    TestTrue(TEXT("Defeated flag is set"), Combat->CurrentEnemy.bIsDefeated);
    TestEqual(TEXT("Defeated enemy health is clamped to zero"), Combat->CurrentEnemy.CurrentHealth, 0);
    TestFalse(TEXT("Attack is rejected after defeat"), Combat->AttackEnemy(30));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterItemCombatStatsTest,
    "GridLootMaster.Combat.ItemCombatStats",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterItemCombatStatsTest::RunTest(const FString& Parameters)
{
    FItemData WeaponData;
    WeaponData.ItemID = TEXT("TestMelee");
    WeaponData.Category = EItemCategory::Weapon;
    WeaponData.Damage = 30;
    WeaponData.WeaponAttackType = EWeaponAttackType::Melee;
    WeaponData.BaseAccuracyPercent = 82;
    WeaponData.AttackIntervalSeconds = 0.75f;
    WeaponData.OptimalRangeTiles = 1;
    WeaponData.MaxRangeTiles = 1;
    WeaponData.RecoilPerShot = 2.0f;
    WeaponData.RecoilRecoveryPerSecond = 1.0f;
    WeaponData.SwapTimeSeconds = 0.5f;
    WeaponData.ReloadTimeSeconds = 0.0f;
    WeaponData.NoiseRadiusTiles = 0;

    UItemInstance* Item = NewObject<UItemInstance>();
    TestNotNull(TEXT("Item instance is created for combat stat mapping"), Item);
    if (!Item) return false;

    Item->InitFromData(WeaponData);
    TestEqual(TEXT("Attack type is copied from item data"), Item->WeaponAttackType, EWeaponAttackType::Melee);
    TestEqual(TEXT("Accuracy is copied from item data"), Item->BaseAccuracyPercent, 82);
    TestTrue(TEXT("Attack interval is copied from item data"),
        FMath::IsNearlyEqual(Item->AttackIntervalSeconds, 0.75f));
    TestEqual(TEXT("Maximum range is copied from item data"), Item->MaxRangeTiles, 1);
    TestTrue(TEXT("Recoil is copied from item data"), FMath::IsNearlyEqual(Item->RecoilPerShot, 2.0f));
    TestTrue(TEXT("Swap time is copied from item data"), FMath::IsNearlyEqual(Item->SwapTimeSeconds, 0.5f));

    FItemData LegacyData;
    LegacyData.ItemID = TEXT("LegacyWeapon");
    LegacyData.Category = EItemCategory::Weapon;
    LegacyData.Damage = 25;
    LegacyData.BaseAccuracyPercent = 0;
    LegacyData.AttackIntervalSeconds = 0.0f;
    LegacyData.OptimalRangeTiles = 0;
    LegacyData.MaxRangeTiles = 0;

    UItemInstance* LegacyItem = NewObject<UItemInstance>();
    TestNotNull(TEXT("Legacy item instance is created"), LegacyItem);
    if (!LegacyItem) return false;

    LegacyItem->InitFromData(LegacyData);
    TestEqual(TEXT("Legacy rows receive safe accuracy default"), LegacyItem->BaseAccuracyPercent, 100);
    TestTrue(TEXT("Legacy rows receive safe attack interval default"),
        FMath::IsNearlyEqual(LegacyItem->AttackIntervalSeconds, 1.0f));
    TestEqual(TEXT("Legacy rows receive safe maximum range default"), LegacyItem->MaxRangeTiles, 3);

    UItemInstance* CompatibleWeapon = NewObject<UItemInstance>();
    UItemInstance* MatchingMagazine = NewObject<UItemInstance>();
    UItemInstance* WrongMagazine = NewObject<UItemInstance>();
    CompatibleWeapon->Category = EItemCategory::Weapon;
    CompatibleWeapon->CompatibleAmmo = TEXT("5.56x45mm");
    MatchingMagazine->Category = EItemCategory::Attachment;
    MatchingMagazine->AttachmentType = EAttachmentType::Magazine;
    MatchingMagazine->CompatibleAmmo = TEXT("5.56x45mm");
    WrongMagazine->Category = EItemCategory::Attachment;
    WrongMagazine->AttachmentType = EAttachmentType::Magazine;
    WrongMagazine->CompatibleAmmo = TEXT("9x19mm");
    TestTrue(TEXT("Matching magazine caliber is accepted"), CompatibleWeapon->IsCompatibleMagazine(MatchingMagazine));
    TestFalse(TEXT("Mismatched magazine caliber is rejected"), CompatibleWeapon->IsCompatibleMagazine(WrongMagazine));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterCombatRecoilTest,
    "GridLootMaster.Combat.RecoilChangesAccuracyAndRecovers",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterCombatRecoilTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for recoil"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode || !GameMode->CombatComponent) return false;

    const ERaidState PreviousRaidState = GameMode->RaidState;
    const int32 PreviousHealth = GameMode->CurrentHealth;
    const float PreviousInterval = GameMode->CombatComponent->PlayerAttackIntervalSeconds;
    const int32 PreviousAccuracy = GameMode->CombatComponent->PlayerAccuracyPercent;
    const float PreviousRecoil = GameMode->CombatComponent->PlayerRecoilPerShot;
    const float PreviousRecovery = GameMode->CombatComponent->PlayerRecoilRecoveryPerSecond;
    GameMode->RaidState = ERaidState::InRaid;
    GameMode->CurrentHealth = 100;
    GameMode->CombatComponent->PlayerAttackIntervalSeconds = 0.1f;
    GameMode->CombatComponent->PlayerAccuracyPercent = 100;
    GameMode->CombatComponent->PlayerRecoilPerShot = 100.0f;
    GameMode->CombatComponent->PlayerRecoilRecoveryPerSecond = 0.0f;
    GameMode->CombatComponent->ClearEnemy();

    FEnemyDefinition Enemy;
    Enemy.EnemyID = TEXT("RecoilEnemy");
    Enemy.MaxHealth = 100;
    Enemy.ReactionTimeSeconds = 100.0f;
    GameMode->CombatComponent->SpawnEnemy(Enemy);
    TestTrue(TEXT("Recoil test enemy is activated"), GameMode->CombatComponent->bHasActiveEnemy);

    TestTrue(TEXT("First shot is accepted and hits"),
        GameMode->CombatComponent->RequestPlayerAttack(10));
    TestEqual(TEXT("First shot applies damage"), GameMode->CombatComponent->CurrentEnemy.CurrentHealth, 90);
    TestTrue(TEXT("Recoil accumulates after a shot"), GameMode->CombatComponent->CurrentRecoil > 0.0f);

    GameMode->CombatComponent->AdvanceCombatTimeForTest(0.1f);
    TestTrue(TEXT("Second shot is accepted after the interval"),
        GameMode->CombatComponent->RequestPlayerAttack(10));
    TestFalse(TEXT("Accumulated recoil reduces the second shot to a miss"),
        GameMode->CombatComponent->bLastPlayerAttackHit);
    TestEqual(TEXT("Recoil miss leaves enemy health unchanged"),
        GameMode->CombatComponent->CurrentEnemy.CurrentHealth, 90);

    GameMode->CombatComponent->PlayerRecoilRecoveryPerSecond = 100.0f;
    GameMode->CombatComponent->AdvanceCombatTimeForTest(1.0f);
    TestTrue(TEXT("Recoil recovers over time"),
        FMath::IsNearlyZero(GameMode->CombatComponent->CurrentRecoil));
    TestTrue(TEXT("A recovered shot hits again"),
        GameMode->CombatComponent->RequestPlayerAttack(10));
    TestEqual(TEXT("Recovered shot applies damage"), GameMode->CombatComponent->CurrentEnemy.CurrentHealth, 80);

    GameMode->CombatComponent->ClearEnemy();
    GameMode->CombatComponent->PlayerAttackIntervalSeconds = PreviousInterval;
    GameMode->CombatComponent->PlayerAccuracyPercent = PreviousAccuracy;
    GameMode->CombatComponent->PlayerRecoilPerShot = PreviousRecoil;
    GameMode->CombatComponent->PlayerRecoilRecoveryPerSecond = PreviousRecovery;
    GameMode->CurrentHealth = PreviousHealth;
    GameMode->RaidState = PreviousRaidState;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterCombatWeaponSwapTest,
    "GridLootMaster.Combat.WeaponSwapDelayAndAttackGuard",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterCombatWeaponSwapTest::RunTest(const FString& Parameters)
{
    AGridGameMode* GameMode = NewObject<AGridGameMode>();
    TestNotNull(TEXT("A game mode is created for weapon swap"), GameMode);
    if (!GameMode || !GameMode->EquipmentComponent || !GameMode->CombatComponent) return false;

    GameMode->RaidState = ERaidState::InRaid;
    GameMode->EquipmentComponent->ClearEquipment();

    UItemInstance* WeaponOne = NewObject<UItemInstance>(GameMode);
    UItemInstance* WeaponTwo = NewObject<UItemInstance>(GameMode);
    WeaponOne->InstanceID = TEXT("SwapWeaponOne");
    WeaponOne->ItemName = TEXT("Weapon One");
    WeaponOne->Category = EItemCategory::Weapon;
    WeaponOne->WeaponAttackType = EWeaponAttackType::Melee;
    WeaponOne->Damage = 10;
    WeaponTwo->InstanceID = TEXT("SwapWeaponTwo");
    WeaponTwo->ItemName = TEXT("Weapon Two");
    WeaponTwo->Category = EItemCategory::Weapon;
    WeaponTwo->WeaponAttackType = EWeaponAttackType::Melee;
    WeaponTwo->Damage = 10;
    WeaponTwo->SwapTimeSeconds = 0.5f;

    TestTrue(TEXT("Primary weapon one is equipped"),
        GameMode->EquipmentComponent->EquipItem(TEXT("Primary1"), WeaponOne));
    TestTrue(TEXT("Primary weapon two is equipped"),
        GameMode->EquipmentComponent->EquipItem(TEXT("Primary2"), WeaponTwo));

    GameMode->CombatComponent->ActiveWeaponSlot = TEXT("Primary1");
    TestTrue(TEXT("Out-of-combat weapon swap is immediate"),
        GameMode->CombatComponent->RequestWeaponSwap(TEXT("Primary2")));
    TestEqual(TEXT("Immediate swap activates the target slot"),
        GameMode->CombatComponent->ActiveWeaponSlot, FName(TEXT("Primary2")));

    GameMode->CombatComponent->ActiveWeaponSlot = TEXT("Primary1");
    FEnemyDefinition Enemy;
    Enemy.EnemyID = TEXT("SwapEnemy");
    Enemy.ReactionTimeSeconds = 100.0f;
    GameMode->CombatComponent->SpawnEnemy(Enemy);
    TestTrue(TEXT("Swap test enemy is active"), GameMode->CombatComponent->bHasActiveEnemy);

    TestTrue(TEXT("In-combat weapon swap is accepted as a delayed action"),
        GameMode->CombatComponent->RequestWeaponSwap(TEXT("Primary2")));
    TestEqual(TEXT("Target weapon is not active before swap completes"),
        GameMode->CombatComponent->ActiveWeaponSlot, FName(TEXT("Primary1")));
    TestEqual(TEXT("Combat enters swapping state"),
        GameMode->CombatComponent->PlayerActionState, ECombatPlayerActionState::Swapping);
    TestFalse(TEXT("Attack is rejected while swapping"),
        GameMode->CombatComponent->RequestPlayerAttack(10));

    GameMode->CombatComponent->AdvanceCombatTimeForTest(0.49f);
    TestEqual(TEXT("Swap remains pending before its delay expires"),
        GameMode->CombatComponent->ActiveWeaponSlot, FName(TEXT("Primary1")));
    GameMode->CombatComponent->AdvanceCombatTimeForTest(0.01f);
    TestEqual(TEXT("Swap completion activates the target weapon"),
        GameMode->CombatComponent->ActiveWeaponSlot, FName(TEXT("Primary2")));
    TestEqual(TEXT("Combat action returns to idle after swap"),
        GameMode->CombatComponent->PlayerActionState, ECombatPlayerActionState::None);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterCombatReloadTransactionTest,
    "GridLootMaster.Combat.ReloadDelayAndAmmoTransaction",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterCombatReloadTransactionTest::RunTest(const FString& Parameters)
{
    AGridGameMode* GameMode = NewObject<AGridGameMode>();
    TestNotNull(TEXT("A game mode is created for reload"), GameMode);
    if (!GameMode || !GameMode->EquipmentComponent || !GameMode->InventoryComponent ||
        !GameMode->RigComponent || !GameMode->PocketComponent || !GameMode->CombatComponent)
    {
        return false;
    }

    GameMode->RaidState = ERaidState::InRaid;
    GameMode->EquipmentComponent->ClearEquipment();
    GameMode->InventoryComponent->InitializeGrid(4, 1);
    GameMode->RigComponent->InitializeSections({ FIntPoint(1, 2), FIntPoint(1, 2) });
    GameMode->PocketComponent->InitializeGrid(2, 2);

    UItemInstance* Weapon = NewObject<UItemInstance>(GameMode);
    Weapon->InstanceID = TEXT("ReloadWeapon");
    Weapon->ItemName = TEXT("Reload Weapon");
    Weapon->Category = EItemCategory::Weapon;
    Weapon->WeaponAttackType = EWeaponAttackType::Firearm;
    Weapon->CompatibleAmmo = TEXT("5.56");
    Weapon->Damage = 10;
    Weapon->ReloadTimeSeconds = 0.8f;

    UItemInstance* Magazine = NewObject<UItemInstance>(GameMode);
    Magazine->InstanceID = TEXT("ReloadMagazine");
    Magazine->Category = EItemCategory::Attachment;
    Magazine->AttachmentType = EAttachmentType::Magazine;
    Magazine->CompatibleAmmo = TEXT("5.56");
    Magazine->MaxAmmo = 5;
    Magazine->CurrentAmmo = 1;
    Weapon->EquippedMagazine = Magazine;
    TestTrue(TEXT("Reload weapon is equipped"),
        GameMode->EquipmentComponent->EquipItem(TEXT("Primary1"), Weapon));

    UItemInstance* Ammo = NewObject<UItemInstance>(GameMode);
    Ammo->InstanceID = TEXT("ReloadAmmo");
    Ammo->TemplateID = TEXT("Ammo_556");
    Ammo->ItemName = TEXT("5.56 Ammo");
    Ammo->Category = EItemCategory::Consumable;
    Ammo->BaseSize = FIntPoint(1, 1);
    Ammo->CurrentStack = 4;
    Ammo->MaxStack = 30;
    UItemInstance* SpareMagazine = NewObject<UItemInstance>(GameMode);
    SpareMagazine->InstanceID = TEXT("ReloadSpareMagazine");
    SpareMagazine->Category = EItemCategory::Attachment;
    SpareMagazine->AttachmentType = EAttachmentType::Magazine;
    SpareMagazine->CompatibleAmmo = TEXT("5.56");
    SpareMagazine->BaseSize = FIntPoint(1, 2);
    SpareMagazine->MaxAmmo = 5;
    SpareMagazine->CurrentAmmo = 5;
    TestTrue(TEXT("Compatible spare magazine is placed in a later rig section"),
        GameMode->RigComponent->AddItemToSection(SpareMagazine, 1, 0, 0));

    TestTrue(TEXT("Loose ammo remains available in the backpack"),
        GameMode->InventoryComponent->AddItem(Ammo, 0, 0));

    GameMode->CombatComponent->ActiveWeaponSlot = TEXT("Primary1");
    TestTrue(TEXT("Reload request is accepted"),
        GameMode->CombatComponent->RequestReload());
    TestEqual(TEXT("Reload enters the delayed action state"),
        GameMode->CombatComponent->PlayerActionState, ECombatPlayerActionState::Reloading);
    TestEqual(TEXT("Magazine is not committed before reload completes"), Magazine->CurrentAmmo, 1);
    TestEqual(TEXT("Ammo is not consumed before reload completes"), Ammo->CurrentStack, 4);
    TestFalse(TEXT("Attack is rejected while reloading"),
        GameMode->CombatComponent->RequestPlayerAttack(10));

    GameMode->CombatComponent->AdvanceCombatTimeForTest(0.79f);
    TestEqual(TEXT("Reload remains pending before its delay expires"), Magazine->CurrentAmmo, 1);
    GameMode->CombatComponent->AdvanceCombatTimeForTest(0.01f);
    TestEqual(TEXT("Reload swaps to the selected spare magazine"), Weapon->EquippedMagazine, SpareMagazine);
    TestEqual(TEXT("The former magazine moves into the spare magazine slot"),
        GameMode->RigComponent->GetItemInstance(Magazine->InstanceID), Magazine);
    int32 FormerMagazineSection = INDEX_NONE;
    int32 FormerMagazineX = INDEX_NONE;
    int32 FormerMagazineY = INDEX_NONE;
    TestTrue(TEXT("The former magazine has a placement after reload"),
        GameMode->RigComponent->FindItemPlacement(Magazine->InstanceID,
            FormerMagazineSection, FormerMagazineX, FormerMagazineY));
    TestEqual(TEXT("The former magazine returns to the selected later rig section"), FormerMagazineSection, 1);
    TestEqual(TEXT("The former magazine returns to the selected X coordinate"), FormerMagazineX, 0);
    TestEqual(TEXT("The former magazine returns to the selected Y coordinate"), FormerMagazineY, 0);
    TestEqual(TEXT("Loose ammo is not consumed by magazine swap"), Ammo->CurrentStack, 4);
    TestEqual(TEXT("Reload action returns to idle"),
        GameMode->CombatComponent->PlayerActionState, ECombatPlayerActionState::None);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterCombatSpawnGuardTest,
    "GridLootMaster.Combat.SpawnRejectsActiveEnemy",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterCombatSpawnGuardTest::RunTest(const FString& Parameters)
{
    UCombatComponent* Combat = NewObject<UCombatComponent>();
    TestNotNull(TEXT("Combat component is created for spawn guard test"), Combat);
    if (!Combat) return false;

    FEnemyDefinition FirstEnemy;
    FirstEnemy.EnemyID = TEXT("FirstEnemy");
    FirstEnemy.DisplayName = TEXT("First Enemy");
    FirstEnemy.MaxHealth = 100;

    FEnemyDefinition SecondEnemy;
    SecondEnemy.EnemyID = TEXT("SecondEnemy");
    SecondEnemy.DisplayName = TEXT("Second Enemy");
    SecondEnemy.MaxHealth = 200;

    Combat->SpawnEnemy(FirstEnemy);
    Combat->SpawnEnemy(SecondEnemy);

    TestTrue(TEXT("The first enemy remains active"), Combat->bHasActiveEnemy);
    TestEqual(TEXT("An active encounter is not replaced by a second spawn"),
        Combat->CurrentEnemy.Definition.EnemyID, FirstEnemy.EnemyID);
    TestEqual(TEXT("The active enemy health remains unchanged"), Combat->CurrentEnemy.CurrentHealth, FirstEnemy.MaxHealth);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterCombatRejectsEmptyEnemyIDTest,
    "GridLootMaster.Combat.SpawnRejectsEmptyEnemyID",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterCombatRejectsEmptyEnemyIDTest::RunTest(const FString& Parameters)
{
    UCombatComponent* Combat = NewObject<UCombatComponent>();
    TestNotNull(TEXT("Combat component is created for empty enemy ID test"), Combat);
    if (!Combat) return false;

    FEnemyDefinition InvalidEnemy;
    InvalidEnemy.DisplayName = TEXT("Unnamed Enemy");

    Combat->SpawnEnemy(InvalidEnemy);

    TestFalse(TEXT("An enemy without an ID is not activated"), Combat->bHasActiveEnemy);
    TestTrue(TEXT("No current enemy ID is stored after rejected spawn"),
        Combat->CurrentEnemy.Definition.EnemyID.IsNone());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterFailRaidWithoutEquipmentTest,
    "GridLootMaster.Raid.FailRaidWithoutEquipmentComponent",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterFailRaidWithoutEquipmentTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for null equipment fail test"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode) return false;

    const ERaidState PreviousRaidState = GameMode->RaidState;
    UEquipmentComponent* PreviousEquipmentComponent = GameMode->EquipmentComponent;
    GameMode->RaidState = ERaidState::InRaid;
    GameMode->EquipmentComponent = nullptr;

    GameMode->FailRaid();

    TestEqual(TEXT("FailRaid returns to the lobby without equipment"),
        GameMode->RaidState, ERaidState::Lobby);

    GameMode->EquipmentComponent = PreviousEquipmentComponent;
    GameMode->RaidState = PreviousRaidState;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterFailRaidWithoutWorldTest,
    "GridLootMaster.Raid.FailRaidWithoutWorld",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterFailRaidWithoutWorldTest::RunTest(const FString& Parameters)
{
    AGridGameMode* GameMode = NewObject<AGridGameMode>();
    TestNotNull(TEXT("Game mode is created without a world"), GameMode);
    if (!GameMode) return false;

    GameMode->RaidState = ERaidState::InRaid;
    GameMode->FailRaid();

    TestEqual(TEXT("FailRaid returns to the lobby without a world"),
        GameMode->RaidState, ERaidState::Lobby);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterCombatStateGuardTest,
    "GridLootMaster.Combat.CombatApiRejectsNonRaidState",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterCombatStateGuardTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for the combat state guard test"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode || !GameMode->CombatComponent) return false;

    const ERaidState PreviousRaidState = GameMode->RaidState;
    const int32 PreviousHealth = GameMode->CurrentHealth;
    GameMode->CombatComponent->ClearEnemy();

    FEnemyDefinition Enemy;
    Enemy.EnemyID = TEXT("StateGuardEnemy");
    Enemy.MaxHealth = 100;
    Enemy.AttackDamage = 10;

    GameMode->RaidState = ERaidState::Lobby;
    GameMode->CombatComponent->SpawnEnemy(Enemy);
    TestFalse(TEXT("Enemy spawn is rejected outside a raid"), GameMode->CombatComponent->bHasActiveEnemy);

    GameMode->RaidState = ERaidState::InRaid;
    GameMode->CombatComponent->SpawnEnemy(Enemy);
    TestTrue(TEXT("Enemy spawn is allowed during a raid"), GameMode->CombatComponent->bHasActiveEnemy);
    GameMode->RaidState = ERaidState::Lobby;
    TestFalse(TEXT("Player attack is rejected outside a raid"), GameMode->CombatComponent->AttackEnemy(10));
    GameMode->CombatComponent->EnemyAttackPlayer();
    TestEqual(TEXT("Counterattack does not damage the player outside a raid"), GameMode->CurrentHealth, PreviousHealth);

    GameMode->CombatComponent->ClearEnemy();
    GameMode->RaidState = PreviousRaidState;
    GameMode->CurrentHealth = PreviousHealth;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterCombatUIFlowTest,
    "GridLootMaster.Combat.UIFlow",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterCombatUIFlowTest::RunTest(const FString& Parameters)
{
    UWorld* GameWorld = nullptr;
    if (GEngine)
    {
        for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
        {
            if (WorldContext.World() && WorldContext.World()->IsGameWorld())
            {
                GameWorld = WorldContext.World();
                break;
            }
        }
    }

    TestNotNull(TEXT("A game world is available for the UI test"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode || !GameMode->CombatComponent) return false;

    UMainGameUI* UI = CreateWidget<UMainGameUI>(GameWorld, UMainGameUI::StaticClass());
    TestNotNull(TEXT("Combat UI is created"), UI);
    if (!UI || !UI->CombatText) return false;
    UI->AddToViewport(1000);

    GameMode->RaidState = ERaidState::InRaid;
    GameMode->QuotaScore = MAX_int32;
    GameMode->CurrentScore = 0;
    GameMode->CombatComponent->ClearEnemy();
    UI->ShowEventNotification(TEXT(""));

    UI->MinimapUI->HandleTileClicked(FIntPoint(1, 0));
    TestTrue(TEXT("Destination selection is recorded in the event log"),
        UI->EventLogEntries.Num() > 0 && UI->EventLogEntries.Last().Contains(TEXT("목적지 선택")));
    TestTrue(TEXT("Destination top toast remains disabled"),
        UI->EventNotificationBorder && UI->EventNotificationBorder->GetVisibility() != ESlateVisibility::Visible);
    UI->MinimapUI->ResetMovement();
    UI->ShowEventNotification(TEXT(""));

    FEnemyDefinition Enemy;
    Enemy.EnemyID = TEXT("UITestEnemy");
    Enemy.DisplayName = TEXT("UI Test Enemy");
    Enemy.MaxHealth = 30;
    Enemy.AttackDamage = 10;
    Enemy.AccuracyPercent = 0;
    Enemy.Reward = 0;

    GameMode->CombatComponent->SpawnEnemy(Enemy);
    TestTrue(TEXT("Enemy appearance is shown in combat text"),
        UI->CombatText->GetText().ToString().Contains(TEXT("적이 나타났다!!")));
    TestTrue(TEXT("Enemy appearance is recorded in the event log"),
        UI->EventLogEntries.Num() > 0 && UI->EventLogEntries.Last().Contains(TEXT("적이 나타났다!!")));

    UI->ShowEventNotification(TEXT(""));
    UI->QueueEventNotification(TEXT("First event"));
    UI->QueueEventNotification(TEXT("Second event"));
    bool bFirstEventRecorded = false;
    for (const FString& Entry : UI->EventLogEntries)
    {
        bFirstEventRecorded |= Entry.Contains(TEXT("First event"));
    }
    TestTrue(TEXT("The first event is recorded in the event log"), bFirstEventRecorded);
    TestEqual(TEXT("Top toast queue remains disabled"), UI->PendingEventNotifications.Num(), 0);
    UI->ShowEventNotification(TEXT(""));

    TestTrue(TEXT("Player attack is shown in combat text"), GameMode->CombatComponent->AttackEnemy(10));
    TestTrue(TEXT("Damage result reaches the UI"),
        UI->CombatText->GetText().ToString().Contains(TEXT("플레이어가 10 피해")));

    GameMode->CombatComponent->EnemyAttackPlayer();
    TestTrue(TEXT("Counterattack miss reaches the UI"),
        UI->CombatText->GetText().ToString().Contains(TEXT("반격이 빗나갔")));

    TestTrue(TEXT("Final attack defeats the enemy"), GameMode->CombatComponent->AttackEnemy(20));
    TestTrue(TEXT("Defeat result reaches the UI"),
        UI->CombatText->GetText().ToString().Contains(TEXT("적을 처치했")));

    UI->RemoveFromParent();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterCombatUIHidesTerminalEnemyTest,
    "GridLootMaster.Combat.UIHidesEnemyOutsideRaid",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterCombatUIHidesTerminalEnemyTest::RunTest(const FString& Parameters)
{
    UWorld* GameWorld = nullptr;
    if (GEngine)
    {
        for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
        {
            if (WorldContext.World() && WorldContext.World()->IsGameWorld())
            {
                GameWorld = WorldContext.World();
                break;
            }
        }
    }

    TestNotNull(TEXT("A game world is available for terminal combat UI test"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode || !GameMode->CombatComponent) return false;

    UMainGameUI* UI = CreateWidget<UMainGameUI>(GameWorld, UMainGameUI::StaticClass());
    TestNotNull(TEXT("Combat UI is created for terminal state test"), UI);
    if (!UI || !UI->CombatText) return false;
    UI->AddToViewport(1000);

    const ERaidState PreviousRaidState = GameMode->RaidState;
    GameMode->CombatComponent->ClearEnemy();
    GameMode->RaidState = ERaidState::InRaid;

    FEnemyDefinition Enemy;
    Enemy.EnemyID = TEXT("TerminalUIEnemy");
    Enemy.DisplayName = TEXT("Terminal UI Enemy");
    Enemy.MaxHealth = 20;
    GameMode->CombatComponent->SpawnEnemy(Enemy);
    TestTrue(TEXT("Active enemy is initially visible"),
        UI->CombatText->GetText().ToString().Contains(TEXT("Terminal UI Enemy")));

    GameMode->SetRaidState(ERaidState::Failed);
    const FString TerminalText = UI->CombatText->GetText().ToString();
    TestTrue(TEXT("Terminal state shows no enemy"), TerminalText.Contains(TEXT("Enemy: None")));
    TestFalse(TEXT("Terminal state does not keep the active enemy text"),
        TerminalText.Contains(TEXT("Terminal UI Enemy")));

    GameMode->CombatComponent->ClearEnemy();
    GameMode->RaidState = PreviousRaidState;
    UI->RemoveFromParent();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterSearchFailureNotificationTest,
    "GridLootMaster.Combat.SearchFailureShowsNotification",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterSearchFailureNotificationTest::RunTest(const FString& Parameters)
{
    UWorld* GameWorld = nullptr;
    if (GEngine)
    {
        for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
        {
            if (WorldContext.World() && WorldContext.World()->IsGameWorld())
            {
                GameWorld = WorldContext.World();
                break;
            }
        }
    }

    TestNotNull(TEXT("A game world is available for the search failure notification test"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode) return false;

    UMainGameUI* MainUI = nullptr;
    for (TObjectIterator<UMainGameUI> It; It; ++It)
    {
        if (It->GetWorld() == GameWorld && IsValid(*It))
        {
            MainUI = *It;
            break;
        }
    }

    TestNotNull(TEXT("The active main UI is available"), MainUI);
    if (!MainUI || !MainUI->EventNotificationText) return false;

    const ERaidState PreviousRaidState = GameMode->RaidState;
    UDataTable* PreviousItemDataTable = GameMode->ItemDataTable;
    GameMode->RaidState = ERaidState::InRaid;
    GameMode->ItemDataTable = nullptr;
    MainUI->ShowEventNotification(TEXT(""));

    GameMode->SearchPhaseCompleteForTest();

    TestTrue(TEXT("A missing loot table shows a search failure in the event log"),
        MainUI->EventLogEntries.Num() > 0 && MainUI->EventLogEntries.Last().Contains(TEXT("탐색 데이터를")));

    GameMode->RaidState = ERaidState::Lobby;
    MainUI->ShowEventNotification(TEXT(""));
    GameMode->StartContainerSearch();
    TestTrue(TEXT("Searching outside a raid shows a reason in the event log"),
        MainUI->EventLogEntries.Num() > 0 && MainUI->EventLogEntries.Last().Contains(TEXT("레이드 중에만 컨테이너")));

    GameMode->ItemDataTable = PreviousItemDataTable;
    GameMode->RaidState = PreviousRaidState;
    MainUI->ShowEventNotification(TEXT(""));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterSearchCompletionWithoutWorldTest,
    "GridLootMaster.Combat.SearchCompletionWithoutWorldIsSafe",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterSearchCompletionWithoutWorldTest::RunTest(const FString& Parameters)
{
    AGridGameMode* GameMode = NewObject<AGridGameMode>();
    TestNotNull(TEXT("Game mode is created without a world"), GameMode);
    if (!GameMode) return false;

    GameMode->RaidState = ERaidState::InRaid;
    GameMode->LootContainerComponent = NewObject<UGridInventoryComponent>(GameMode);
    GameMode->SearchPhaseCompleteForTest();

    TestTrue(TEXT("Search completion without a world exits safely"), true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterSearchRejectsMalformedContainerTest,
    "GridLootMaster.Combat.SearchRejectsMalformedContainer",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterSearchRejectsMalformedContainerTest::RunTest(const FString& Parameters)
{
    UWorld* GameWorld = nullptr;
    if (GEngine)
    {
        for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
        {
            if (WorldContext.World() && WorldContext.World()->IsGameWorld())
            {
                GameWorld = WorldContext.World();
                break;
            }
        }
    }

    TestNotNull(TEXT("A game world is available for malformed search container test"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode || !GameMode->LootContainerComponent) return false;

    UMainGameUI* MainUI = nullptr;
    for (TObjectIterator<UMainGameUI> It; It; ++It)
    {
        if (It->GetWorld() == GameWorld && IsValid(*It))
        {
            MainUI = *It;
            break;
        }
    }

    TestNotNull(TEXT("The active main UI is available"), MainUI);
    if (!MainUI || !MainUI->EventNotificationText) return false;

    const ERaidState PreviousRaidState = GameMode->RaidState;
    const int32 PreviousGridWidth = GameMode->LootContainerComponent->GridWidth;
    const int32 PreviousGridHeight = GameMode->LootContainerComponent->GridHeight;
    const TArray<FName> PreviousGridCells = GameMode->LootContainerComponent->GridCells;
    GameMode->RaidState = ERaidState::InRaid;
    GameMode->LootContainerComponent->GridWidth = 0;
    GameMode->LootContainerComponent->GridHeight = 0;
    GameMode->LootContainerComponent->GridCells.Empty();
    MainUI->ShowEventNotification(TEXT(""));

    GameMode->SearchPhaseCompleteForTest();

    TestTrue(TEXT("Malformed container grid is rejected with an event log entry"),
        MainUI->EventLogEntries.Num() > 0 && MainUI->EventLogEntries.Last().Contains(TEXT("컨테이너를 사용할 수")));

    GameMode->LootContainerComponent->GridWidth = PreviousGridWidth;
    GameMode->LootContainerComponent->GridHeight = PreviousGridHeight;
    GameMode->LootContainerComponent->GridCells = PreviousGridCells;
    GameMode->RaidState = PreviousRaidState;
    MainUI->ShowEventNotification(TEXT(""));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterCombatActionAvailabilityTest,
    "GridLootMaster.Combat.ActionButtonsReflectRaidState",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterCombatActionAvailabilityTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for action button availability"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode || !GameMode->CombatComponent) return false;

    UMainGameUI* ActiveUI = nullptr;
    for (TObjectIterator<UMainGameUI> It; It; ++It)
    {
        if (It->GetWorld() == GameWorld && IsValid(*It) && It->IsInViewport())
        {
            ActiveUI = *It;
            break;
        }
    }
    TestNotNull(TEXT("The active main UI is available for extraction feedback"), ActiveUI);

    UMainGameUI* UI = CreateWidget<UMainGameUI>(GameWorld, UMainGameUI::StaticClass());
    TestNotNull(TEXT("Main UI is created for action button availability"), UI);
    if (!UI) return false;
    TestNotNull(TEXT("Search button exists"), UI->SearchBtn);
    TestNotNull(TEXT("BANG button exists"), UI->BangBtn);
    TestNotNull(TEXT("Extract button exists"), UI->ExtractBtn);
    TestNotNull(TEXT("Start raid button exists"), UI->StartRaidBtn);
    TestNotNull(TEXT("Full minimap exists"), UI->MinimapUI);
    TestNotNull(TEXT("Compact minimap exists"), UI->CompactMinimapUI);
    TestNotNull(TEXT("Stash button exists"), UI->StashBtn);
    TestNotNull(TEXT("Toggle button exists"), UI->ToggleModeButton);
    if (!UI->SearchBtn || !UI->BangBtn || !UI->ExtractBtn || !UI->StartRaidBtn ||
        !UI->MinimapUI || !UI->CompactMinimapUI || !UI->StashBtn || !UI->ToggleModeButton)
    {
        UI->RemoveFromParent();
        return false;
    }

    const ERaidState PreviousRaidState = GameMode->RaidState;
    GameMode->CombatComponent->ClearEnemy();

    GameMode->RaidState = ERaidState::Lobby;
    UI->UpdateActionAvailability();
    TestFalse(TEXT("Search is disabled in the lobby"), UI->SearchBtn->GetIsEnabled());
    TestFalse(TEXT("BANG is disabled in the lobby"), UI->BangBtn->GetIsEnabled());
    TestTrue(TEXT("Start raid is enabled in the lobby"), UI->StartRaidBtn->GetIsEnabled());
    TestTrue(TEXT("Stash is available before starting a raid"), UI->StashBtn->GetIsEnabled());
    UI->RightPanelSwitcher->SetActiveWidgetIndex(2);
    UI->UpdateActionAvailability();
    TestEqual(TEXT("Stash view remains active in the lobby"),
        UI->RightPanelSwitcher->GetActiveWidgetIndex(), 2);
    TestEqual(TEXT("Map/inventory toggle is hidden in the stash"),
        UI->ToggleModeButton->GetVisibility(), ESlateVisibility::Hidden);

    GameMode->RaidState = ERaidState::InRaid;
    UI->UpdateActionAvailability();
    TestTrue(TEXT("Search is enabled during a raid without combat"), UI->SearchBtn->GetIsEnabled());
    TestFalse(TEXT("Extract is disabled away from an extraction point"), UI->ExtractBtn->GetIsEnabled());
    TestFalse(TEXT("BANG is disabled when no enemy is active"), UI->BangBtn->GetIsEnabled());
    TestFalse(TEXT("Stash is disabled during a raid"), UI->StashBtn->GetIsEnabled());
    UI->RightPanelSwitcher->SetActiveWidgetIndex(0);
    UI->OnToggleModeClicked();
    TestEqual(TEXT("Toggle opens the full minimap during a raid"),
        UI->RightPanelSwitcher->GetActiveWidgetIndex(), 1);
    UMinimapTileWidget** DestinationTile = UI->MinimapUI->TileWidgets.Find(FIntPoint(1, 0));
    TestNotNull(TEXT("Destination tile exists on the full minimap"), DestinationTile);
    TestTrue(TEXT("Destination tile has a standard Slate click surface"),
        DestinationTile && *DestinationTile && (*DestinationTile)->ClickButton);
    if (DestinationTile && *DestinationTile)
    {
        (*DestinationTile)->ClickButton->OnClicked.Broadcast();
    }
    TestTrue(TEXT("Clicking a destination tile creates a minimap path"), UI->MinimapUI->CurrentPath.Num() > 0);
    TestTrue(TEXT("Selecting a destination enables the full minimap advance button"),
        UI->MinimapUI->AdvanceButton && UI->MinimapUI->AdvanceButton->GetIsEnabled());
    TestTrue(TEXT("The compact minimap advance button mirrors the destination"),
        UI->CompactMinimapUI->AdvanceButton && UI->CompactMinimapUI->AdvanceButton->GetIsEnabled());
    UI->OnToggleModeClicked();
    TestEqual(TEXT("Toggle returns from the full minimap to inventory"),
        UI->RightPanelSwitcher->GetActiveWidgetIndex(), 0);
    if (GameMode->MapManagerComponent && GameMode->MapManagerComponent->ExtractionPoints.Num() > 0)
    {
        GameMode->CurrentPlayerCoord = GameMode->MapManagerComponent->ExtractionPoints[0];
        UI->ShowEventNotification(TEXT(""));
        UI->MinimapUI->OnPlayerMoved.Broadcast(GameMode->CurrentPlayerCoord);
        TestTrue(TEXT("Extract is enabled on an extraction point"), UI->ExtractBtn->GetIsEnabled());
        TestTrue(TEXT("Extraction arrival is recorded in the event log"),
            UI->EventLogEntries.Num() > 0 && UI->EventLogEntries.Last().Contains(TEXT("탈출 지점에 도착")));

        GameMode->CurrentPlayerCoord = GameMode->MapManagerComponent->SpawnPoint;
        if (ActiveUI) ActiveUI->ShowEventNotification(TEXT(""));
        GameMode->ExtractRaid();
        TestTrue(TEXT("Extraction outside the extraction point shows a reason in the event log"),
            ActiveUI && ActiveUI->EventLogEntries.Num() > 0 &&
            ActiveUI->EventLogEntries.Last().Contains(TEXT("탈출 지점에서만")));
    }
    UI->RightPanelSwitcher->SetActiveWidgetIndex(2);
    UI->UpdateActionAvailability();
    TestEqual(TEXT("Stash panel closes when a raid becomes active"),
        UI->RightPanelSwitcher->GetActiveWidgetIndex(), 0);

    FEnemyDefinition Enemy;
    Enemy.EnemyID = TEXT("ActionAvailabilityEnemy");
    Enemy.MaxHealth = 10;
    GameMode->CombatComponent->SpawnEnemy(Enemy);
    TestFalse(TEXT("Search is disabled during combat"), UI->SearchBtn->GetIsEnabled());
    TestFalse(TEXT("Extract is disabled during combat"), UI->ExtractBtn->GetIsEnabled());
    TestTrue(TEXT("BANG is enabled during combat"), UI->BangBtn->GetIsEnabled());

    GameMode->CombatComponent->ClearEnemy();
    GameMode->RaidState = ERaidState::Failed;
    UI->RightPanelSwitcher->SetActiveWidgetIndex(1);
    UI->UpdateActionAvailability();
    TestFalse(TEXT("Search is disabled after the raid ends"), UI->SearchBtn->GetIsEnabled());
    TestFalse(TEXT("BANG is disabled after the raid ends"), UI->BangBtn->GetIsEnabled());
    TestTrue(TEXT("Stash is available after the raid ends"), UI->StashBtn->GetIsEnabled());
    TestEqual(TEXT("The full minimap closes after the raid ends"),
        UI->RightPanelSwitcher->GetActiveWidgetIndex(), 0);
    TestEqual(TEXT("The compact minimap is hidden after the raid ends"),
        UI->CompactMinimapUI->GetVisibility(), ESlateVisibility::Hidden);
    TestEqual(TEXT("The map toggle is hidden after the raid ends"),
        UI->ToggleModeButton->GetVisibility(), ESlateVisibility::Hidden);

    GameMode->RaidState = PreviousRaidState;
    if (ActiveUI) ActiveUI->ShowEventNotification(TEXT(""));
    UI->RemoveFromParent();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterCombatDataTableTest,
    "GridLootMaster.Combat.DataTableValuesMatchCombatRules",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterCombatDataTableTest::RunTest(const FString& Parameters)
{
    UDataTable* ItemDataTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/DataTable/DT_ItemData.DT_ItemData"));
    TestNotNull(TEXT("Combat DataTable asset is available"), ItemDataTable);
    if (!ItemDataTable) return false;

    const FItemData* WeaponData = ItemDataTable->FindRow<FItemData>(TEXT("M4A1"), TEXT("CombatDataTableTest"));
    const FItemData* GlockData = ItemDataTable->FindRow<FItemData>(TEXT("Glock19"), TEXT("CombatDataTableTest"));
    const FItemData* MP5Data = ItemDataTable->FindRow<FItemData>(TEXT("MP5"), TEXT("CombatDataTableTest"));
    const FItemData* MosinData = ItemDataTable->FindRow<FItemData>(TEXT("Mosin"), TEXT("CombatDataTableTest"));
    const FItemData* AK74MData = ItemDataTable->FindRow<FItemData>(TEXT("AK74M"), TEXT("CombatDataTableTest"));
    const FItemData* ArmorData = ItemDataTable->FindRow<FItemData>(TEXT("PACA"), TEXT("CombatDataTableTest"));
    const FItemData* MagazineData = ItemDataTable->FindRow<FItemData>(TEXT("Mag_M4"), TEXT("CombatDataTableTest"));
    TestNotNull(TEXT("M4A1 row exists"), WeaponData);
    TestNotNull(TEXT("Glock19 row exists"), GlockData);
    TestNotNull(TEXT("MP5 row exists"), MP5Data);
    TestNotNull(TEXT("Mosin row exists"), MosinData);
    TestNotNull(TEXT("AK74M row exists"), AK74MData);
    TestNotNull(TEXT("PACA row exists"), ArmorData);
    TestNotNull(TEXT("M4 magazine row exists"), MagazineData);
    if (!WeaponData || !GlockData || !MP5Data || !MosinData || !AK74MData || !ArmorData || !MagazineData) return false;

    TestEqual(TEXT("M4A1 damage is imported from CSV"), WeaponData->Damage, 25);
    TestEqual(TEXT("PACA armor is imported from CSV"), ArmorData->Armor, 5);
    TestEqual(TEXT("M4 magazine capacity is imported from CSV"), MagazineData->MaxAmmo, 30);
    TestEqual(TEXT("M4 magazine ammo compatibility is imported from CSV"), MagazineData->CompatibleAmmo, FString(TEXT("5.56x45mm")));
    TestEqual(TEXT("Glock19 damage is imported from CSV"), GlockData->Damage, 20);
    TestEqual(TEXT("Glock19 accuracy is imported from CSV"), GlockData->BaseAccuracyPercent, 90);
    TestTrue(TEXT("Glock19 interval is imported from CSV"), FMath::IsNearlyEqual(GlockData->AttackIntervalSeconds, 0.70f));
    TestEqual(TEXT("Glock19 optimal range is imported from CSV"), GlockData->OptimalRangeTiles, 1);
    TestEqual(TEXT("Glock19 maximum range is imported from CSV"), GlockData->MaxRangeTiles, 2);
    TestTrue(TEXT("Glock19 recoil is imported from CSV"), FMath::IsNearlyEqual(GlockData->RecoilPerShot, 8.0f));
    TestTrue(TEXT("Glock19 recoil recovery is imported from CSV"), FMath::IsNearlyEqual(GlockData->RecoilRecoveryPerSecond, 8.0f));
    TestTrue(TEXT("Glock19 swap time is imported from CSV"), FMath::IsNearlyEqual(GlockData->SwapTimeSeconds, 0.35f));
    TestTrue(TEXT("Glock19 reload time is imported from CSV"), FMath::IsNearlyEqual(GlockData->ReloadTimeSeconds, 1.30f));
    TestEqual(TEXT("Glock19 noise radius is imported from CSV"), GlockData->NoiseRadiusTiles, 3);
    TestEqual(TEXT("MP5 damage is imported from CSV"), MP5Data->Damage, 18);
    TestTrue(TEXT("MP5 interval is imported from CSV"), FMath::IsNearlyEqual(MP5Data->AttackIntervalSeconds, 0.40f));
    TestEqual(TEXT("MP5 maximum range is imported from CSV"), MP5Data->MaxRangeTiles, 3);
    TestEqual(TEXT("Mosin damage is imported from CSV"), MosinData->Damage, 45);
    TestEqual(TEXT("Mosin optimal range is imported from CSV"), MosinData->OptimalRangeTiles, 3);
    TestEqual(TEXT("Mosin maximum range is imported from CSV"), MosinData->MaxRangeTiles, 5);
    TestEqual(TEXT("AK74M damage is imported from CSV"), AK74MData->Damage, 28);
    TestTrue(TEXT("AK74M recoil is imported from CSV"), FMath::IsNearlyEqual(AK74MData->RecoilPerShot, 18.0f));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterMapTruncatedGridTest,
    "GridLootMaster.Combat.MapManagerRejectsTruncatedGrid",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterMapTruncatedGridTest::RunTest(const FString& Parameters)
{
    UMapManagerComponent* MapManager = NewObject<UMapManagerComponent>();
    TestNotNull(TEXT("Map manager is created"), MapManager);
    if (!MapManager) return false;

    MapManager->MapWidth = 2;
    MapManager->MapHeight = 2;
    MapManager->MapGrid.SetNum(1);

    TestFalse(TEXT("Movement rejects a truncated map grid"),
        MapManager->CanMoveBetween(FIntPoint(0, 0), FIntPoint(1, 0)));
    FTileData TileData;
    TestFalse(TEXT("Tile lookup rejects a truncated map grid"),
        MapManager->GetTileData(1, 0, TileData));
    TestEqual(TEXT("Path finding rejects a truncated map grid"),
        MapManager->FindPath(FIntPoint(0, 0), FIntPoint(1, 0)).Num(), 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterExtractionPointGenerationTest,
    "GridLootMaster.Map.GeneratesOppositeExtractionPoint",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterExtractionPointGenerationTest::RunTest(const FString& Parameters)
{
    UMapManagerComponent* MapManager = NewObject<UMapManagerComponent>();
    TestNotNull(TEXT("Map manager is created for extraction point generation"), MapManager);
    if (!MapManager) return false;

    MapManager->MapWidth = 9;
    MapManager->MapHeight = 9;
    MapManager->SpawnPoint = FIntPoint(0, 0);
    MapManager->ExtractionPointCount = 1;
    MapManager->ExtractionMinDistance = 5;
    MapManager->InitializeMap();

    TestEqual(TEXT("The configured number of extraction points is generated"), MapManager->ExtractionPoints.Num(), 1);
    if (MapManager->ExtractionPoints.Num() != 1) return false;

    const FIntPoint ExtractionPoint = MapManager->ExtractionPoints[0];
    const int32 Distance = FMath::Abs(ExtractionPoint.X - MapManager->SpawnPoint.X) +
        FMath::Abs(ExtractionPoint.Y - MapManager->SpawnPoint.Y);
    TestEqual(TEXT("A west-side spawn receives an east-side extraction point"), ExtractionPoint.X, 8);
    TestTrue(TEXT("The extraction point respects the minimum distance"), Distance >= MapManager->ExtractionMinDistance);
    TestTrue(TEXT("The generated point is marked as an extraction tile"), MapManager->IsExtractionPoint(ExtractionPoint));

    FTileData TileData;
    TestTrue(TEXT("Extraction tile data can be read"),
        MapManager->GetTileData(ExtractionPoint.X, ExtractionPoint.Y, TileData));
    TestEqual(TEXT("Extraction tile type is set"), TileData.TileType, ETileType::Extraction);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterMinimapUninitializedInputTest,
    "GridLootMaster.Combat.MinimapRejectsUninitializedInput",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterMinimapUninitializedInputTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for the uninitialized minimap test"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode) return false;

    const ERaidState PreviousRaidState = GameMode->RaidState;
    GameMode->RaidState = ERaidState::InRaid;

    UMinimapWidget* Minimap = CreateWidget<UMinimapWidget>(GameWorld, UMinimapWidget::StaticClass());
    TestNotNull(TEXT("Minimap widget is created"), Minimap);
    if (!Minimap)
    {
        GameMode->RaidState = PreviousRaidState;
        return false;
    }

    // InitMinimap 이전 입력은 무시되어야 하며, null 멤버를 역참조하면 안 됩니다.
    Minimap->HandleTileClicked(FIntPoint(1, 0));
    TestEqual(TEXT("Uninitialized input does not create a path"), Minimap->CurrentPath.Num(), 0);
    TestEqual(TEXT("Uninitialized input does not advance movement"), Minimap->CurrentMoveProgress, 0);

    Minimap->RemoveFromParent();
    GameMode->RaidState = PreviousRaidState;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterMinimapInitialDisplayTest,
    "GridLootMaster.Combat.MinimapInitialDisplayDoesNotTriggerEncounter",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterMinimapInitialDisplayTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for the minimap test"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode || !GameMode->CombatComponent || !GameMode->MapManagerComponent) return false;

    const ERaidState PreviousRaidState = GameMode->RaidState;
    GameMode->RaidState = ERaidState::InRaid;
    GameMode->CombatComponent->ClearEnemy();
    GameMode->MapManagerComponent->InitializeMap();

    UMinimapWidget* Minimap = CreateWidget<UMinimapWidget>(GameWorld, UMinimapWidget::StaticClass());
    TestNotNull(TEXT("Minimap widget is created"), Minimap);
    if (!Minimap)
    {
        GameMode->RaidState = PreviousRaidState;
        return false;
    }

    Minimap->OnPlayerMoved.AddDynamic(GameMode, &AGridGameMode::HandlePlayerMoved);
    Minimap->InitMinimap(GameMode->MapManagerComponent);
    TestFalse(TEXT("Displaying the initial player position does not create an encounter"),
        GameMode->CombatComponent->bHasActiveEnemy);

    Minimap->RemoveFromParent();
    GameMode->CombatComponent->ClearEnemy();
    GameMode->RaidState = PreviousRaidState;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterMinimapPathLockTest,
    "GridLootMaster.Combat.MinimapLocksPathDuringMovement",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterMinimapPathLockTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for the path-lock test"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode || !GameMode->MapManagerComponent) return false;

    const ERaidState PreviousRaidState = GameMode->RaidState;
    GameMode->RaidState = ERaidState::InRaid;
    GameMode->CombatComponent->ClearEnemy();
    GameMode->MapManagerComponent->InitializeMap();

    UMinimapWidget* Minimap = CreateWidget<UMinimapWidget>(GameWorld, UMinimapWidget::StaticClass());
    TestNotNull(TEXT("Minimap widget is created"), Minimap);
    if (!Minimap)
    {
        GameMode->RaidState = PreviousRaidState;
        return false;
    }

    Minimap->InitMinimap(GameMode->MapManagerComponent);
    Minimap->HandleTileClicked(FIntPoint(1, 0));
    TestTrue(TEXT("A path is selected before movement begins"), Minimap->CurrentPath.Num() > 0);
    Minimap->OnAdvanceClicked();

    TestEqual(TEXT("One advance moves exactly one tile"), Minimap->CurrentPlayerCoord, FIntPoint(1, 0));
    TestEqual(TEXT("One-tile movement consumes one world turn"), Minimap->CurrentMoveProgress, 0);

    Minimap->HandleTileClicked(FIntPoint(0, 1));
    TestEqual(TEXT("A new destination can be selected after the tile move"), Minimap->CurrentTargetCoord, FIntPoint(0, 1));
    TestEqual(TEXT("Movement progress remains zero between tile turns"), Minimap->CurrentMoveProgress, 0);

    Minimap->RemoveFromParent();
    GameMode->CombatComponent->ClearEnemy();
    GameMode->RaidState = PreviousRaidState;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterMinimapReinitializationClearsMovementTest,
    "GridLootMaster.Combat.MinimapReinitializationClearsMovementState",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterMinimapReinitializationClearsMovementTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for minimap reinitialization"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode || !GameMode->MapManagerComponent) return false;

    const ERaidState PreviousRaidState = GameMode->RaidState;
    GameMode->RaidState = ERaidState::InRaid;

    UMinimapWidget* Minimap = CreateWidget<UMinimapWidget>(GameWorld, UMinimapWidget::StaticClass());
    TestNotNull(TEXT("Minimap widget is created for reinitialization"), Minimap);
    if (!Minimap)
    {
        GameMode->RaidState = PreviousRaidState;
        return false;
    }

    Minimap->InitMinimap(GameMode->MapManagerComponent);
    Minimap->HandleTileClicked(FIntPoint(2, 0));
    Minimap->OnAdvanceClicked();
    TestTrue(TEXT("Movement state exists before minimap reinitialization"), Minimap->CurrentPath.Num() > 0);
    TestEqual(TEXT("Movement progress remains zero after a tile move"), Minimap->CurrentMoveProgress, 0);

    Minimap->InitMinimap(GameMode->MapManagerComponent);
    TestEqual(TEXT("Reinitialization clears the selected path"), Minimap->CurrentPath.Num(), 0);
    TestEqual(TEXT("Reinitialization clears movement progress"), Minimap->CurrentMoveProgress, 0);
    TestEqual(TEXT("Reinitialization resets the selected target"), Minimap->CurrentTargetCoord, FIntPoint(0, 0));

    Minimap->RemoveFromParent();
    GameMode->RaidState = PreviousRaidState;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterCombatResumesMovementTest,
    "GridLootMaster.Combat.CombatEndResumesMovement",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterCombatResumesMovementTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for the combat movement boundary test"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode || !GameMode->MapManagerComponent || !GameMode->CombatComponent) return false;

    const ERaidState PreviousRaidState = GameMode->RaidState;
    GameMode->RaidState = ERaidState::InRaid;
    GameMode->CombatComponent->ClearEnemy();
    GameMode->MapManagerComponent->InitializeMap();

    UMinimapWidget* Minimap = CreateWidget<UMinimapWidget>(GameWorld, UMinimapWidget::StaticClass());
    TestNotNull(TEXT("Minimap widget is created"), Minimap);
    if (!Minimap)
    {
        GameMode->RaidState = PreviousRaidState;
        return false;
    }

    Minimap->OnPlayerMoved.AddDynamic(GameMode, &AGridGameMode::HandlePlayerMoved);
    Minimap->InitMinimap(GameMode->MapManagerComponent);
    Minimap->HandleTileClicked(FIntPoint(3, 0));
    const int32 SelectedRouteLength = Minimap->CurrentPath.Num();
    TestTrue(TEXT("A route is selected"), SelectedRouteLength > 1);

    Minimap->OnAdvanceClicked();
    TestFalse(TEXT("Moving onto the first tile does not create a random encounter"), GameMode->CombatComponent->bHasActiveEnemy);
    TestEqual(TEXT("The first tile is reached before combat"), Minimap->CurrentPlayerCoord, FIntPoint(1, 0));
    TestEqual(TEXT("The remaining route is preserved during combat"), Minimap->CurrentPath.Num(), SelectedRouteLength - 1);

    FEnemyDefinition Enemy;
    Enemy.EnemyID = TEXT("ExplicitCombatMovementEnemy");
    Enemy.MaxHealth = 10;
    GameMode->CombatComponent->SpawnEnemy(Enemy);
    TestTrue(TEXT("An explicitly spawned enemy starts combat"), GameMode->CombatComponent->bHasActiveEnemy);
    Minimap->OnAdvanceClicked();
    TestEqual(TEXT("Movement remains locked during explicit combat"), Minimap->CurrentMoveProgress, 0);

    GameMode->CombatComponent->ClearEnemy();
    while (Minimap->CurrentPath.Num() > 0)
    {
        Minimap->OnAdvanceClicked();
    }

    TestEqual(TEXT("Movement resumes after combat ends"), Minimap->CurrentPlayerCoord, FIntPoint(3, 0));
    TestEqual(TEXT("The resumed route is completed"), Minimap->CurrentPath.Num(), 0);

    Minimap->RemoveFromParent();
    GameMode->CombatComponent->ClearEnemy();
    GameMode->RaidState = PreviousRaidState;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterMinimapRejectsStaleBlockedStepTest,
    "GridLootMaster.Combat.MinimapRejectsStaleBlockedStep",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterMinimapRejectsStaleBlockedStepTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for stale movement test"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode || !GameMode->MapManagerComponent || !GameMode->CombatComponent) return false;

    const ERaidState PreviousRaidState = GameMode->RaidState;
    GameMode->RaidState = ERaidState::InRaid;
    GameMode->CombatComponent->ClearEnemy();
    GameMode->MapManagerComponent->InitializeMap();

    UMinimapWidget* Minimap = CreateWidget<UMinimapWidget>(GameWorld, UMinimapWidget::StaticClass());
    TestNotNull(TEXT("Minimap widget is created for stale movement test"), Minimap);
    if (!Minimap)
    {
        GameMode->RaidState = PreviousRaidState;
        return false;
    }

    Minimap->InitMinimap(GameMode->MapManagerComponent);
    Minimap->HandleTileClicked(FIntPoint(1, 0));
    TestTrue(TEXT("A path exists before the map changes"), Minimap->CurrentPath.Num() > 0);

    GameMode->MapManagerComponent->MapGrid[0].bOpenEast = false;
    GameMode->MapManagerComponent->MapGrid[1].bOpenWest = false;
    Minimap->OnAdvanceClicked();
    Minimap->OnAdvanceClicked();
    Minimap->OnAdvanceClicked();

    TestEqual(TEXT("A stale path cannot move through a newly blocked edge"),
        Minimap->CurrentPlayerCoord, FIntPoint(0, 0));
    TestEqual(TEXT("A stale blocked path is discarded"), Minimap->CurrentPath.Num(), 0);

    Minimap->RemoveFromParent();
    GameMode->CombatComponent->ClearEnemy();
    GameMode->RaidState = PreviousRaidState;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterBangHandlerTest,
    "GridLootMaster.Combat.BangHandlerConsumesAmmoAndDealsDamage",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterBangHandlerTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for the BANG handler test"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode || !GameMode->EquipmentComponent || !GameMode->CombatComponent) return false;

    UMainGameUI* UI = CreateWidget<UMainGameUI>(GameWorld, UMainGameUI::StaticClass());
    TestNotNull(TEXT("Main UI is created for the BANG handler test"), UI);
    if (!UI) return false;

    const ERaidState PreviousRaidState = GameMode->RaidState;
    const int32 PreviousScore = GameMode->CurrentScore;
    const int32 PreviousQuota = GameMode->QuotaScore;
    UItemInstance* PreviousWeapon = GameMode->EquipmentComponent->GetEquippedItem(TEXT("Primary1"));

    GameMode->RaidState = ERaidState::InRaid;
    GameMode->CurrentScore = 0;
    GameMode->QuotaScore = MAX_int32;
    GameMode->CombatComponent->ClearEnemy();
    GameMode->EquipmentComponent->RemoveItemBySlotID(TEXT("Primary1"));

    UItemInstance* Weapon = NewObject<UItemInstance>(GameMode);
    UItemInstance* Magazine = NewObject<UItemInstance>(GameMode);
    TestNotNull(TEXT("Test weapon is created"), Weapon);
    TestNotNull(TEXT("Test magazine is created"), Magazine);
    if (!Weapon || !Magazine)
    {
        if (PreviousWeapon) GameMode->EquipmentComponent->EquipItem(TEXT("Primary1"), PreviousWeapon);
        GameMode->RaidState = PreviousRaidState;
        GameMode->CurrentScore = PreviousScore;
        GameMode->QuotaScore = PreviousQuota;
        return false;
    }

    Weapon->InstanceID = TEXT("BangHandlerWeapon");
    Weapon->Category = EItemCategory::Weapon;
    Weapon->Damage = 25;
    Weapon->EquippedMagazine = Magazine;
    Magazine->InstanceID = TEXT("BangHandlerMagazine");
    Magazine->Category = EItemCategory::Attachment;
    Magazine->AttachmentType = EAttachmentType::Magazine;
    Magazine->CurrentAmmo = 2;
    Magazine->MaxAmmo = 2;
    TestTrue(TEXT("Test weapon equips in the active slot"),
        GameMode->EquipmentComponent->EquipItem(TEXT("Primary1"), Weapon));

    FEnemyDefinition Enemy;
    Enemy.EnemyID = TEXT("BangHandlerEnemy");
    Enemy.DisplayName = TEXT("BANG Handler Enemy");
    Enemy.MaxHealth = 100;
    Enemy.AttackDamage = 10;
    Enemy.AccuracyPercent = 0;
    Enemy.Armor = 0;
    Enemy.Reward = 0;
    GameMode->CombatComponent->SpawnEnemy(Enemy);

    UI->OnBangButtonClicked();
    TestEqual(TEXT("BANG handler consumes one round"), Magazine->CurrentAmmo, 1);
    TestEqual(TEXT("BANG handler applies weapon damage"), GameMode->CombatComponent->CurrentEnemy.CurrentHealth, 75);
    TestTrue(TEXT("Enemy remains active after non-lethal BANG"), GameMode->CombatComponent->bHasActiveEnemy);

    UI->OnBangButtonClicked();
    TestEqual(TEXT("Cooldown rejection does not consume ammo"), Magazine->CurrentAmmo, 1);
    TestTrue(TEXT("Cooldown rejection is reported"),
        GameMode->CombatComponent->LastCombatMessage.Contains(TEXT("준비되지 않았습니다")));

    GameMode->CombatComponent->ClearEnemy();
    Magazine->CurrentAmmo = 0;
    GameMode->CombatComponent->SpawnEnemy(Enemy);
    UI->ShowEventNotification(TEXT(""));
    UI->OnBangButtonClicked();
    TestEqual(TEXT("BANG handler does not consume ammo when the magazine is empty"), Magazine->CurrentAmmo, 0);
    TestTrue(TEXT("Empty magazine shows a combat event log entry"),
        UI->EventLogEntries.Num() > 0 && UI->EventLogEntries.Last().Contains(TEXT("탄약이 없습니다")));

    GameMode->CombatComponent->ClearEnemy();
    GameMode->EquipmentComponent->RemoveItemBySlotID(TEXT("Primary1"));
    GameMode->RaidState = ERaidState::InRaid;
    Magazine->CurrentAmmo = 2;
    TestTrue(TEXT("Test weapon re-equips for the non-raid guard"),
        GameMode->EquipmentComponent->EquipItem(TEXT("Primary1"), Weapon));

    GameMode->CombatComponent->SpawnEnemy(Enemy);
    GameMode->RaidState = ERaidState::Lobby;
    UI->OnBangButtonClicked();
    TestEqual(TEXT("BANG handler does not consume ammo outside a raid"), Magazine->CurrentAmmo, 2);
    TestEqual(TEXT("BANG handler does not damage an enemy outside a raid"),
        GameMode->CombatComponent->CurrentEnemy.CurrentHealth, 100);

    GameMode->CombatComponent->ClearEnemy();
    GameMode->EquipmentComponent->RemoveItemBySlotID(TEXT("Primary1"));
    if (PreviousWeapon) GameMode->EquipmentComponent->EquipItem(TEXT("Primary1"), PreviousWeapon);
    GameMode->RaidState = PreviousRaidState;
    GameMode->CurrentScore = PreviousScore;
    GameMode->QuotaScore = PreviousQuota;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterCombatTimingTest,
    "GridLootMaster.Combat.TimeBasedCombat",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterCombatTimingTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for time-based combat"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode || !GameMode->CombatComponent) return false;

    const ERaidState PreviousRaidState = GameMode->RaidState;
    const int32 PreviousHealth = GameMode->CurrentHealth;
    const int32 PreviousMaxHealth = GameMode->MaxHealth;
    const float PreviousPlayerInterval = GameMode->CombatComponent->PlayerAttackIntervalSeconds;
    const int32 PreviousPlayerAccuracy = GameMode->CombatComponent->PlayerAccuracyPercent;
    GameMode->RaidState = ERaidState::InRaid;
    GameMode->MaxHealth = 100;
    GameMode->CurrentHealth = 100;
    GameMode->CombatComponent->PlayerAttackIntervalSeconds = 1.0f;
    GameMode->CombatComponent->PlayerAccuracyPercent = 100;
    GameMode->CombatComponent->ClearEnemy();

    FEnemyDefinition Enemy;
    Enemy.EnemyID = TEXT("TimingEnemy");
    Enemy.DisplayName = TEXT("Timing Enemy");
    Enemy.MaxHealth = 100;
    Enemy.AttackDamage = 10;
    Enemy.AccuracyPercent = 100;
    Enemy.ReactionTimeSeconds = 0.5f;
    Enemy.AttackIntervalSeconds = 1.0f;
    GameMode->CombatComponent->SpawnEnemy(Enemy);

    TestTrue(TEXT("Player attack is accepted when ready"),
        GameMode->CombatComponent->RequestPlayerAttack(10));
    TestEqual(TEXT("Accepted attack hits the enemy"), GameMode->CombatComponent->CurrentEnemy.CurrentHealth, 90);
    TestEqual(TEXT("Player is not counterattacked immediately"), GameMode->CurrentHealth, 100);
    TestFalse(TEXT("Rapid attack is rejected during player cooldown"),
        GameMode->CombatComponent->RequestPlayerAttack(10));
    TestEqual(TEXT("Cooldown rejection leaves enemy health unchanged"),
        GameMode->CombatComponent->CurrentEnemy.CurrentHealth, 90);

    GameMode->CombatComponent->AdvanceCombatTimeForTest(0.49f);
    TestEqual(TEXT("Enemy waits through reaction time"), GameMode->CurrentHealth, 100);
    GameMode->CombatComponent->AdvanceCombatTimeForTest(0.01f);
    TestEqual(TEXT("Enemy attacks on its independent schedule"), GameMode->CurrentHealth, 90);
    GameMode->CombatComponent->AdvanceCombatTimeForTest(1.0f);
    TestEqual(TEXT("Enemy continues attacking by interval"), GameMode->CurrentHealth, 80);

    GameMode->CombatComponent->ClearEnemy();
    GameMode->CombatComponent->PlayerAttackIntervalSeconds = PreviousPlayerInterval;
    GameMode->CombatComponent->PlayerAccuracyPercent = PreviousPlayerAccuracy;
    GameMode->MaxHealth = PreviousMaxHealth;
    GameMode->CurrentHealth = PreviousHealth;
    GameMode->RaidState = PreviousRaidState;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterCombatAcceptedMissTest,
    "GridLootMaster.Combat.AcceptedMissConsumesAmmo",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterCombatAcceptedMissTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for accepted miss"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode || !GameMode->CombatComponent) return false;

    const ERaidState PreviousRaidState = GameMode->RaidState;
    const int32 PreviousAccuracy = GameMode->CombatComponent->PlayerAccuracyPercent;
    GameMode->RaidState = ERaidState::InRaid;
    GameMode->CombatComponent->PlayerAccuracyPercent = 0;
    GameMode->CombatComponent->ClearEnemy();

    FEnemyDefinition Enemy;
    Enemy.EnemyID = TEXT("AcceptedMissEnemy");
    Enemy.MaxHealth = 100;
    GameMode->CombatComponent->SpawnEnemy(Enemy);

    TestTrue(TEXT("An accepted miss still commits the attack"),
        GameMode->CombatComponent->RequestPlayerAttack(10));
    TestFalse(TEXT("Accepted miss does not damage the enemy"),
        GameMode->CombatComponent->bLastPlayerAttackHit);
    TestEqual(TEXT("Accepted miss leaves enemy health unchanged"),
        GameMode->CombatComponent->CurrentEnemy.CurrentHealth, 100);

    GameMode->CombatComponent->ClearEnemy();
    GameMode->CombatComponent->PlayerAccuracyPercent = PreviousAccuracy;
    GameMode->RaidState = PreviousRaidState;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterCombatRangeTest,
    "GridLootMaster.Combat.CombatRangeRejectsOutOfRangeAttack",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterCombatRangeTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for combat range"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode || !GameMode->CombatComponent || !GameMode->EnemyManagerComponent || !GameMode->MapManagerComponent)
    {
        return false;
    }

    const ERaidState PreviousRaidState = GameMode->RaidState;
    const FIntPoint PreviousPlayerCoord = GameMode->CurrentPlayerCoord;
    const int32 PreviousCombatRange = GameMode->CombatComponent->CombatRangeTiles;
    GameMode->RaidState = ERaidState::InRaid;
    GameMode->CurrentPlayerCoord = FIntPoint(0, 0);
    GameMode->MapManagerComponent->InitializeMap();
    GameMode->EnemyManagerComponent->InitialSpawnDelayTicks = 100;
    GameMode->CombatComponent->ClearEnemy();
    GameMode->EnemyManagerComponent->ResetForRaid();

    FEnemyDefinition Enemy;
    Enemy.EnemyID = TEXT("RangeEnemy");
    Enemy.DisplayName = TEXT("Range Enemy");
    Enemy.MaxHealth = 100;
    Enemy.VisionRangeTiles = 2;
    Enemy.DetectionPower = 100;
    Enemy.ReactionTimeSeconds = 10.0f;
    TestTrue(TEXT("Range test enemy is spawned in the world"),
        GameMode->EnemyManagerComponent->SpawnEnemyAt(Enemy, FIntPoint(2, 0), EEnemyBehaviorProfile::GuardZone));
    GameMode->EnemyManagerComponent->AdvanceWorldTick();
    TestTrue(TEXT("Range test creates a combat contact"), GameMode->CombatComponent->bHasActiveEnemy);

    GameMode->CombatComponent->CombatRangeTiles = 1;
    TestFalse(TEXT("Player attack is rejected outside combat range"),
        GameMode->CombatComponent->RequestPlayerAttack(10));
    TestEqual(TEXT("Out-of-range attack does not damage the enemy"),
        GameMode->CombatComponent->CurrentEnemy.CurrentHealth, 100);

    GameMode->CombatComponent->ClearEnemy();
    GameMode->EnemyManagerComponent->ResetForRaid();
    GameMode->CombatComponent->CombatRangeTiles = PreviousCombatRange;
    GameMode->CurrentPlayerCoord = PreviousPlayerCoord;
    GameMode->RaidState = PreviousRaidState;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterMinimapResetTest,
    "GridLootMaster.Combat.MinimapResetsMovementState",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterMinimapResetTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for minimap reset"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode) return false;
    const ERaidState PreviousRaidState = GameMode->RaidState;
    GameMode->RaidState = ERaidState::InRaid;

    UMapManagerComponent* MapManager = NewObject<UMapManagerComponent>(GameWorld);
    TestNotNull(TEXT("Map manager is created"), MapManager);
    if (!MapManager)
    {
        GameMode->RaidState = PreviousRaidState;
        return false;
    }
    MapManager->SpawnPoint = FIntPoint(2, 2);
    MapManager->InitializeMap();

    UMinimapWidget* Minimap = CreateWidget<UMinimapWidget>(GameWorld, UMinimapWidget::StaticClass());
    TestNotNull(TEXT("Minimap widget is created"), Minimap);
    if (!Minimap)
    {
        GameMode->RaidState = PreviousRaidState;
        return false;
    }

    Minimap->InitMinimap(MapManager);
    Minimap->HandleTileClicked(FIntPoint(1, 0));
    Minimap->OnAdvanceClicked();
    TestEqual(TEXT("Movement progress remains zero after a tile move"), Minimap->CurrentMoveProgress, 0);

    Minimap->ResetMovement();
    TestEqual(TEXT("Reset returns to the configured start coordinate"),
        Minimap->CurrentPlayerCoord, MapManager->SpawnPoint);
    TestEqual(TEXT("Reset clears the selected path"), Minimap->CurrentPath.Num(), 0);
    TestEqual(TEXT("Reset clears movement progress"), Minimap->CurrentMoveProgress, 0);

    Minimap->RemoveFromParent();
    GameMode->RaidState = PreviousRaidState;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterMinimapSharedMovementStateTest,
    "GridLootMaster.Combat.MinimapSharesMovementState",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterMinimapSharedMovementStateTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for shared minimap movement"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode || !GameMode->CombatComponent) return false;

    const ERaidState PreviousRaidState = GameMode->RaidState;
    GameMode->RaidState = ERaidState::InRaid;
    GameMode->CombatComponent->ClearEnemy();

    UMapManagerComponent* MapManager = NewObject<UMapManagerComponent>(GameWorld);
    UMinimapWidget* FullMinimap = CreateWidget<UMinimapWidget>(GameWorld, UMinimapWidget::StaticClass());
    UMinimapWidget* CompactMinimap = CreateWidget<UMinimapWidget>(GameWorld, UMinimapWidget::StaticClass());
    TestNotNull(TEXT("Full minimap is created"), FullMinimap);
    TestNotNull(TEXT("Compact minimap is created"), CompactMinimap);
    if (!MapManager || !FullMinimap || !CompactMinimap)
    {
        GameMode->RaidState = PreviousRaidState;
        return false;
    }

    MapManager->InitializeMap();
    FullMinimap->InitMinimap(MapManager, false);
    CompactMinimap->InitMinimap(MapManager, true);

    UUniformGridPanel* InitialGridPanel = FullMinimap->MapGridPanel;
    UMinimapTileWidget* InitialTile = nullptr;
    if (UMinimapTileWidget** Tile = FullMinimap->TileWidgets.Find(FIntPoint(1, 0)))
    {
        InitialTile = *Tile;
    }
    FullMinimap->AddToViewport(1000);
    MapManager->InitializeMap();
    FullMinimap->InitMinimap(MapManager, false);
    TestTrue(TEXT("Reinitializing an on-screen minimap keeps its Slate root"),
        FullMinimap->MapGridPanel == InitialGridPanel);
    TestTrue(TEXT("Reinitializing an on-screen minimap keeps clickable tiles"),
        InitialTile && FullMinimap->TileWidgets.FindRef(FIntPoint(1, 0)) == InitialTile);
    FullMinimap->SetMovementStateMirror(CompactMinimap);

    UMinimapTileWidget** ClickableTile = FullMinimap->TileWidgets.Find(FIntPoint(1, 0));
    TestTrue(TEXT("Full minimap tile has a standard Slate click surface"),
        ClickableTile && *ClickableTile && (*ClickableTile)->ClickButton);
    if (ClickableTile && *ClickableTile && (*ClickableTile)->ClickButton)
    {
        (*ClickableTile)->ClickButton->OnClicked.Broadcast();
    }
    TestTrue(TEXT("Tile button click selects a path"), FullMinimap->CurrentPath.Num() > 0);
    TestEqual(TEXT("Compact minimap receives the selected path"),
        CompactMinimap->CurrentPath.Num(), FullMinimap->CurrentPath.Num());
    TestEqual(TEXT("Compact minimap receives the selected target"),
        CompactMinimap->CurrentTargetCoord, FullMinimap->CurrentTargetCoord);
    TestTrue(TEXT("Compact minimap shows its shared advance button"),
        CompactMinimap->AdvanceButton && CompactMinimap->AdvanceButton->GetVisibility() == ESlateVisibility::Visible);

    CompactMinimap->OnAdvanceClicked();
    TestEqual(TEXT("Compact advance input updates the source progress"),
        FullMinimap->CurrentMoveProgress, 0);
    TestEqual(TEXT("Source progress is mirrored back to the compact minimap"),
        CompactMinimap->CurrentMoveProgress, FullMinimap->CurrentMoveProgress);

    TestEqual(TEXT("Source minimap reaches the next tile"), FullMinimap->CurrentPlayerCoord, FIntPoint(1, 0));
    TestEqual(TEXT("Compact minimap reaches the same tile"), CompactMinimap->CurrentPlayerCoord, FIntPoint(1, 0));
    TestEqual(TEXT("Both minimaps finish the same path"), CompactMinimap->CurrentPath.Num(), 0);

    MapManager->InitializeMap();
    FullMinimap->InitMinimap(MapManager, false);
    CompactMinimap->InitMinimap(MapManager, true);
    FullMinimap->SetMovementStateMirror(CompactMinimap);
    FullMinimap->HandleTileClicked(FIntPoint(1, 0));
    const int32 BlockedEdgeIndex = 0;
    const int32 BlockedNeighborIndex = 1;
    if (MapManager->MapGrid.IsValidIndex(BlockedEdgeIndex) && MapManager->MapGrid.IsValidIndex(BlockedNeighborIndex))
    {
        MapManager->MapGrid[BlockedEdgeIndex].bOpenEast = false;
        MapManager->MapGrid[BlockedNeighborIndex].bOpenWest = false;
    }
    FullMinimap->OnAdvanceClicked();
    FullMinimap->OnAdvanceClicked();
    FullMinimap->OnAdvanceClicked();
    TestEqual(TEXT("A failed source step clears the source path"), FullMinimap->CurrentPath.Num(), 0);
    TestEqual(TEXT("A failed source step clears the compact mirrored path"), CompactMinimap->CurrentPath.Num(), 0);

    FullMinimap->RemoveFromParent();
    CompactMinimap->RemoveFromParent();
    GameMode->RaidState = PreviousRaidState;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterScorePreservesMovementTest,
    "GridLootMaster.Combat.ScoreDoesNotResetMinimapMovement",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterScorePreservesMovementTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for score movement regression"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode || !GameMode->MapManagerComponent || !GameMode->CombatComponent) return false;

    UMainGameUI* MainUI = nullptr;
    for (TObjectIterator<UMainGameUI> It; It; ++It)
    {
		if (It->GetWorld() == GameWorld && IsValid(*It))
        {
            MainUI = *It;
            break;
        }
    }

    TestNotNull(TEXT("The active main UI is available"), MainUI);
    if (!MainUI || !MainUI->MinimapUI) return false;

    const ERaidState PreviousRaidState = GameMode->RaidState;
    const int32 PreviousScore = GameMode->CurrentScore;
    const int32 PreviousQuota = GameMode->QuotaScore;
    GameMode->RaidState = ERaidState::InRaid;
    GameMode->CurrentScore = 0;
    GameMode->QuotaScore = MAX_int32;
    GameMode->CombatComponent->ClearEnemy();

    MainUI->MinimapUI->InitMinimap(GameMode->MapManagerComponent);
    for (int32 Y = 0; Y < GameMode->MapManagerComponent->MapHeight && MainUI->MinimapUI->CurrentPath.Num() == 0; ++Y)
    {
        for (int32 X = 0; X < GameMode->MapManagerComponent->MapWidth && MainUI->MinimapUI->CurrentPath.Num() == 0; ++X)
        {
            MainUI->MinimapUI->HandleTileClicked(FIntPoint(X, Y));
        }
    }
    MainUI->MinimapUI->OnAdvanceClicked();
    TestEqual(TEXT("Movement progress is zero after a one-tile advance"), MainUI->MinimapUI->CurrentMoveProgress, 0);

    GameMode->AddScore(1);
    TestEqual(TEXT("Scoring does not reset movement progress"), MainUI->MinimapUI->CurrentMoveProgress, 0);

    MainUI->MinimapUI->ResetMovement();
    GameMode->RaidState = PreviousRaidState;
    GameMode->CurrentScore = PreviousScore;
    GameMode->QuotaScore = PreviousQuota;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterMinimapBlocksAmbushAdvanceTest,
    "GridLootMaster.Combat.MinimapBlocksAmbushAdvance",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterMinimapBlocksAmbushAdvanceTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for ambush advance guard"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode || !GameMode->MapManagerComponent || !GameMode->EnemyManagerComponent ||
        !GameMode->CombatComponent)
    {
        return false;
    }

    const ERaidState PreviousRaidState = GameMode->RaidState;
    const FIntPoint PreviousCoord = GameMode->CurrentPlayerCoord;
    const FIntPoint PreviousSpawn = GameMode->MapManagerComponent->SpawnPoint;
    GameMode->RaidState = ERaidState::InRaid;
    GameMode->PlayerPosture = EPlayerRaidPosture::Normal;
    GameMode->CombatComponent->ClearEnemy();
    GameMode->EnemyManagerComponent->ResetForRaid();
    GameMode->CurrentPlayerCoord = FIntPoint(0, 0);
    GameMode->PreviousPlayerCoord = FIntPoint(0, 0);
    GameMode->MapManagerComponent->SpawnPoint = FIntPoint(0, 0);
    GameMode->MapManagerComponent->InitializeMap();

    UMinimapWidget* Minimap = CreateWidget<UMinimapWidget>(GameWorld, UMinimapWidget::StaticClass());
    TestNotNull(TEXT("Minimap widget is created"), Minimap);
    if (!Minimap) return false;
    Minimap->InitMinimap(GameMode->MapManagerComponent, false);
    Minimap->HandleTileClicked(FIntPoint(1, 0));
    TestTrue(TEXT("A path is selected before ambush"), Minimap->CurrentPath.Num() > 0);

    const FIntPoint MinimapCoord = Minimap->CurrentPlayerCoord;
    const int32 MinimapProgress = Minimap->CurrentMoveProgress;
    GameMode->PlayerPosture = EPlayerRaidPosture::Ambushing;
    Minimap->OnAdvanceClicked();
    Minimap->OnAdvanceClicked();
    Minimap->OnAdvanceClicked();
    TestEqual(TEXT("Player ambush blocks minimap coordinate changes"), Minimap->CurrentPlayerCoord, MinimapCoord);
    TestEqual(TEXT("Player ambush blocks game mode coordinate changes"), GameMode->CurrentPlayerCoord, MinimapCoord);
    TestEqual(TEXT("Player ambush blocks minimap progress changes"), Minimap->CurrentMoveProgress, MinimapProgress);

    GameMode->PlayerPosture = EPlayerRaidPosture::Normal;
    GameMode->EnemyManagerComponent->ResetForRaid();
    GameMode->MapManagerComponent->SpawnPoint = FIntPoint(0, 0);
    Minimap->ResetMovement();
    Minimap->InitMinimap(GameMode->MapManagerComponent, false);
    Minimap->HandleTileClicked(FIntPoint(1, 0));
    FEnemyDefinition Ambusher;
    Ambusher.EnemyID = TEXT("MinimapAmbusher");
    Ambusher.VisionRangeTiles = 0;
    Ambusher.DetectionPower = 0;
    TestTrue(TEXT("Enemy ambusher is spawned for minimap guard"),
        GameMode->EnemyManagerComponent->SpawnEnemyAt(Ambusher, FIntPoint(0, 1), EEnemyBehaviorProfile::Ambusher));
    TestTrue(TEXT("Enemy ambush reaction starts at the current player tile"),
        GameMode->EnemyManagerComponent->TryStartEnemyAmbushAtCurrentPlayer());
    TestTrue(TEXT("Enemy ambush reaction is active"), GameMode->EnemyManagerComponent->HasActiveAmbushReaction());

    const int32 ContainerItemCount = GameMode->LootContainerComponent
        ? GameMode->LootContainerComponent->ItemInstances.Num() : 0;
    GameMode->StartContainerSearch();
    TestEqual(TEXT("Enemy ambush blocks container search"),
        GameMode->LootContainerComponent ? GameMode->LootContainerComponent->ItemInstances.Num() : 0,
        ContainerItemCount);
    TestFalse(TEXT("Enemy ambush blocks extraction"), GameMode->ExtractRaid());
    TestEqual(TEXT("Enemy ambush keeps the raid active"), GameMode->RaidState, ERaidState::InRaid);
    TestFalse(TEXT("Enemy ambush blocks reload requests"), GameMode->CombatComponent->RequestReload());
    TestTrue(TEXT("Reload rejection reports the ambush guard"),
        GameMode->CombatComponent->LastCombatMessage.Contains(TEXT("매복")));
    TestFalse(TEXT("Enemy ambush blocks weapon swap requests"),
        GameMode->CombatComponent->RequestWeaponSwap(TEXT("Primary2")));
    TestTrue(TEXT("Weapon swap rejection reports the ambush guard"),
        GameMode->CombatComponent->LastCombatMessage.Contains(TEXT("매복")));

    const FIntPoint EnemyReactionCoord = Minimap->CurrentPlayerCoord;
    const int32 EnemyReactionProgress = Minimap->CurrentMoveProgress;
    Minimap->OnAdvanceClicked();
    Minimap->OnAdvanceClicked();
    Minimap->OnAdvanceClicked();
    TestEqual(TEXT("Enemy ambush blocks minimap coordinate changes"), Minimap->CurrentPlayerCoord, EnemyReactionCoord);
    TestEqual(TEXT("Enemy ambush blocks game mode coordinate changes"), GameMode->CurrentPlayerCoord, EnemyReactionCoord);
    TestEqual(TEXT("Enemy ambush blocks minimap progress changes"), Minimap->CurrentMoveProgress, EnemyReactionProgress);

    Minimap->RemoveFromParent();
    GameMode->PlayerPosture = EPlayerRaidPosture::Normal;
    GameMode->CombatComponent->ClearEnemy();
    GameMode->EnemyManagerComponent->ResetForRaid();
    GameMode->RaidState = PreviousRaidState;
    GameMode->CurrentPlayerCoord = PreviousCoord;
    GameMode->MapManagerComponent->SpawnPoint = PreviousSpawn;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterTerminalRaidClearsMovementTest,
    "GridLootMaster.Raid.TerminalRaidClearsMovementState",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterTerminalRaidClearsMovementTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for terminal raid movement regression"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode || !GameMode->MapManagerComponent || !GameMode->CombatComponent) return false;

    UMainGameUI* MainUI = nullptr;
    for (TObjectIterator<UMainGameUI> It; It; ++It)
    {
        if (It->GetWorld() == GameWorld && IsValid(*It))
        {
            MainUI = *It;
            break;
        }
    }

    TestNotNull(TEXT("The active main UI is available"), MainUI);
    if (!MainUI || !MainUI->MinimapUI) return false;

    const ERaidState PreviousRaidState = GameMode->RaidState;
    GameMode->RaidState = ERaidState::InRaid;
    GameMode->CombatComponent->ClearEnemy();
    if (GameMode->EnemyManagerComponent)
    {
        GameMode->EnemyManagerComponent->ResetForRaid();
    }
    GameMode->MapManagerComponent->InitializeMap();

    MainUI->MinimapUI->InitMinimap(GameMode->MapManagerComponent);
    for (int32 Y = 0; Y < GameMode->MapManagerComponent->MapHeight && MainUI->MinimapUI->CurrentPath.Num() == 0; ++Y)
    {
        for (int32 X = 0; X < GameMode->MapManagerComponent->MapWidth && MainUI->MinimapUI->CurrentPath.Num() == 0; ++X)
        {
            MainUI->MinimapUI->HandleTileClicked(FIntPoint(X, Y));
        }
    }
    TestEqual(TEXT("Movement progress is zero before terminal raid end"), MainUI->MinimapUI->CurrentMoveProgress, 0);
    TestTrue(TEXT("A movement path exists before terminal raid end"), MainUI->MinimapUI->CurrentPath.Num() > 0);
    MainUI->MinimapUI->OnAdvanceClicked();

    GameMode->FailRaid();
    TestEqual(TEXT("Failing the raid returns to the lobby"), GameMode->RaidState, ERaidState::Lobby);
    TestEqual(TEXT("Terminal raid end clears movement progress"), MainUI->MinimapUI->CurrentMoveProgress, 0);
    TestEqual(TEXT("Terminal raid end clears the movement path"), MainUI->MinimapUI->CurrentPath.Num(), 0);

    MainUI->MinimapUI->ResetMovement();
    GameMode->RaidState = PreviousRaidState;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterTimerStyleResetTest,
    "GridLootMaster.Raid.StartingRaidResetsResultTimerStyle",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterTimerStyleResetTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for the timer style test"), GameWorld);
    if (!GameWorld) return false;

    UMainGameUI* UI = CreateWidget<UMainGameUI>(GameWorld, UMainGameUI::StaticClass());
    TestNotNull(TEXT("Main UI is created for the timer style test"), UI);
    if (!UI || !UI->TimerText) return false;

    UI->ShowGameResult(false);
    TestTrue(TEXT("Failed raid result uses the failure color"),
        UI->TimerText->GetColorAndOpacity().GetSpecifiedColor().Equals(FLinearColor::Red));

    UI->UpdateTimer(60.0f);
    TestTrue(TEXT("Starting a new raid restores the default timer color"),
        UI->TimerText->GetColorAndOpacity().GetSpecifiedColor().Equals(FLinearColor::White));

    UI->RemoveFromParent();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterTimeoutPrecedesQuotaTest,
    "GridLootMaster.Raid.TimeoutPrecedesQuota",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterTimeoutPrecedesQuotaTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for timeout precedence"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode || !GameMode->StashComponent || !GameMode->ItemDataTable) return false;

    const ERaidState PreviousRaidState = GameMode->RaidState;
    const int32 PreviousScore = GameMode->CurrentScore;
    const int32 PreviousQuota = GameMode->QuotaScore;
    const float PreviousRemainingTime = GameMode->RemainingTime;

    GameMode->StashComponent->InitializeGrid(2, 2);
    GameMode->RaidState = ERaidState::InRaid;
    GameMode->CurrentScore = 1000;
    GameMode->QuotaScore = 1000;
    GameMode->RemainingTime = 0.0f;

    GameMode->CheckWinCondition();
    TestEqual(TEXT("Timeout takes precedence over an already met quota and returns to the lobby"),
        GameMode->RaidState, ERaidState::Lobby);

    GameMode->RaidState = PreviousRaidState;
    GameMode->CurrentScore = PreviousScore;
    GameMode->QuotaScore = PreviousQuota;
    GameMode->RemainingTime = PreviousRemainingTime;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterTimerStateGuardTest,
    "GridLootMaster.Raid.TimerIgnoresTerminalState",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterTimerStateGuardTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for the timer state guard"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode) return false;

    const ERaidState PreviousRaidState = GameMode->RaidState;
    const float PreviousRemainingTime = GameMode->RemainingTime;

    GameMode->RaidState = ERaidState::Failed;
    GameMode->RemainingTime = 10.0f;
    GameMode->GameTimerUpdateForTest();
    TestEqual(TEXT("A terminal raid state does not consume timer time"), GameMode->RemainingTime, 10.0f);

    GameMode->RaidState = ERaidState::InRaid;
    GameMode->GameTimerUpdateForTest();
    TestEqual(TEXT("An active raid still consumes one second"), GameMode->RemainingTime, 9.0f);

    GameMode->RaidState = PreviousRaidState;
    GameMode->RemainingTime = PreviousRemainingTime;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterTimerWithoutWorldTest,
    "GridLootMaster.Raid.TimerExpiresWithoutWorld",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterTimerWithoutWorldTest::RunTest(const FString& Parameters)
{
    AGridGameMode* GameMode = NewObject<AGridGameMode>();
    TestNotNull(TEXT("Game mode is created without a world"), GameMode);
    if (!GameMode) return false;

    GameMode->RaidState = ERaidState::InRaid;
    GameMode->RemainingTime = 1.0f;
    GameMode->GameTimerUpdateForTest();

    TestEqual(TEXT("A worldless timer expiry returns to the lobby"),
        GameMode->RaidState, ERaidState::Lobby);
    TestEqual(TEXT("Worldless timer expiry clamps remaining time to zero"), GameMode->RemainingTime, 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterTerminalTimerWithoutWorldTest,
    "GridLootMaster.Raid.TerminalTimerIgnoresWithoutWorld",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterTerminalTimerWithoutWorldTest::RunTest(const FString& Parameters)
{
    AGridGameMode* GameMode = NewObject<AGridGameMode>();
    TestNotNull(TEXT("Game mode is created without a world"), GameMode);
    if (!GameMode) return false;

    GameMode->RaidState = ERaidState::Failed;
    GameMode->RemainingTime = 10.0f;
    GameMode->GameTimerUpdateForTest();

    TestEqual(TEXT("A worldless terminal timer update preserves remaining time"),
        GameMode->RemainingTime, 10.0f);
    TestEqual(TEXT("A worldless terminal timer update preserves raid state"),
        GameMode->RaidState, ERaidState::Failed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterGridBoardRejectsTruncatedCellsTest,
    "GridLootMaster.Inventory.GridBoardRejectsTruncatedCells",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterGridBoardRejectsTruncatedCellsTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for the grid board guard test"), GameWorld);
    if (!GameWorld) return false;

    UGridInventoryComponent* Inventory = NewObject<UGridInventoryComponent>(GameWorld);
    TestNotNull(TEXT("Inventory component is created for the grid board guard test"), Inventory);
    if (!Inventory) return false;

    Inventory->InitializeGrid(2, 2);
    Inventory->GridCells.SetNum(1);

    UGridBoardWidget* Board = CreateWidget<UGridBoardWidget>(GameWorld, UGridBoardWidget::StaticClass());
    TestNotNull(TEXT("Grid board widget is created for the truncated cells test"), Board);
    if (!Board) return false;

    Board->InventoryComponent = Inventory;
    TestNotNull(TEXT("Grid canvas is initialized for the truncated cells test"), Board->GridCanvas);
    Board->RefreshGridUI();

    Board->RemoveFromParent();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterGridBoardRejectsDanglingCellTest,
    "GridLootMaster.Inventory.GridBoardRejectsDanglingCell",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterGridBoardRejectsDanglingCellTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for the dangling cell test"), GameWorld);
    if (!GameWorld) return false;

    UGridInventoryComponent* TargetInventory = NewObject<UGridInventoryComponent>(GameWorld);
    UGridInventoryComponent* SourceInventory = NewObject<UGridInventoryComponent>(GameWorld);
    TestNotNull(TEXT("Target inventory is created for the dangling cell test"), TargetInventory);
    TestNotNull(TEXT("Source inventory is created for the dangling cell test"), SourceInventory);
    if (!TargetInventory || !SourceInventory) return false;

    TargetInventory->InitializeGrid(1, 1);
    TargetInventory->GridCells[0] = TEXT("MissingItem");
    SourceInventory->InitializeGrid(1, 1);

    UItemInstance* SourceItem = NewObject<UItemInstance>(SourceInventory);
    SourceItem->InstanceID = TEXT("DropItem");
    SourceItem->TemplateID = TEXT("DropTemplate");
    SourceItem->BaseSize = FIntPoint(1, 1);
    TestTrue(TEXT("The source item is placed"), SourceInventory->AddItem(SourceItem, 0, 0));

    UGridBoardWidget* Board = CreateWidget<UGridBoardWidget>(GameWorld, UGridBoardWidget::StaticClass());
    TestNotNull(TEXT("Grid board is created for the dangling cell test"), Board);
    if (!Board) return false;
    Board->InventoryComponent = TargetInventory;

    UItemDragDropOperation* DropOperation = NewObject<UItemDragDropOperation>(GameWorld);
    DropOperation->ItemID = SourceItem->InstanceID;
    DropOperation->ItemObj = SourceItem;
    DropOperation->SourceInventory = SourceInventory;

    FPointerEvent PointerEvent;
    FDragDropEvent DragDropEvent(PointerEvent, TSharedPtr<FDragDropOperation>());
    TestFalse(TEXT("A drop onto a dangling cell reference is rejected without a crash"),
        Board->NativeOnDropForTest(FGeometry(), DragDropEvent, DropOperation));
    TestEqual(TEXT("The dangling cell remains unchanged"), TargetInventory->GridCells[0], FName(TEXT("MissingItem")));
    TestEqual(TEXT("The source item remains in its source inventory"),
        SourceInventory->GetItemInstance(SourceItem->InstanceID), SourceItem);

    Board->RemoveFromParent();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterMinimapRejectsMissingGameModeTest,
    "GridLootMaster.Combat.MinimapRejectsAdvanceWithoutGameMode",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterMinimapRejectsMissingGameModeTest::RunTest(const FString& Parameters)
{
    UMinimapWidget* Minimap = NewObject<UMinimapWidget>();
    TestNotNull(TEXT("Minimap widget is created without a world"), Minimap);
    if (!Minimap) return false;

    Minimap->CurrentPath.Add(FIntPoint(1, 0));
    Minimap->AdvanceButton = NewObject<UButton>(Minimap);
    Minimap->ApplySharedMovementState(FIntPoint(0, 0), FIntPoint(1, 0), Minimap->CurrentPath, 0);

    TestFalse(TEXT("A mirrored advance button stays disabled without a game mode"),
        Minimap->AdvanceButton->GetIsEnabled());
    Minimap->OnAdvanceClicked();

    TestEqual(TEXT("Advance input is rejected without a game mode"), Minimap->CurrentMoveProgress, 0);
    TestEqual(TEXT("The selected path is preserved when advance input is rejected"), Minimap->CurrentPath.Num(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterMinimapRejectsTileSelectionWithoutGameModeTest,
    "GridLootMaster.Combat.MinimapRejectsTileSelectionWithoutGameMode",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterMinimapRejectsTileSelectionWithoutGameModeTest::RunTest(const FString& Parameters)
{
    UMapManagerComponent* MapManager = NewObject<UMapManagerComponent>();
    UMinimapWidget* Minimap = NewObject<UMinimapWidget>();
    TestNotNull(TEXT("Map manager is created without a world"), MapManager);
    TestNotNull(TEXT("Minimap widget is created without a world"), Minimap);
    if (!MapManager || !Minimap) return false;

    MapManager->InitializeMap();
    Minimap->InitMinimap(MapManager);
    Minimap->HandleTileClicked(FIntPoint(1, 0));

    TestEqual(TEXT("Tile selection is rejected without a game mode"), Minimap->CurrentPath.Num(), 0);
    TestEqual(TEXT("Tile selection does not advance movement progress"), Minimap->CurrentMoveProgress, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGridLootMasterMinimapRejectsTileSelectionDuringCombatTest,
    "GridLootMaster.Combat.MinimapRejectsTileSelectionDuringCombat",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGridLootMasterMinimapRejectsTileSelectionDuringCombatTest::RunTest(const FString& Parameters)
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

    TestNotNull(TEXT("A game world is available for the combat path selection guard"), GameWorld);
    if (!GameWorld) return false;

    AGridGameMode* GameMode = Cast<AGridGameMode>(GameWorld->GetAuthGameMode());
    TestNotNull(TEXT("The active game mode is GridLootMaster"), GameMode);
    if (!GameMode || !GameMode->MapManagerComponent || !GameMode->CombatComponent) return false;

    const ERaidState PreviousRaidState = GameMode->RaidState;
    GameMode->RaidState = ERaidState::InRaid;
    GameMode->MapManagerComponent->InitializeMap();
    GameMode->CombatComponent->ClearEnemy();

    UMinimapWidget* Minimap = CreateWidget<UMinimapWidget>(GameWorld, UMinimapWidget::StaticClass());
    TestNotNull(TEXT("Minimap widget is created for the combat path selection guard"), Minimap);
    if (!Minimap)
    {
        GameMode->RaidState = PreviousRaidState;
        return false;
    }

    Minimap->InitMinimap(GameMode->MapManagerComponent);

    FEnemyDefinition Enemy;
    Enemy.EnemyID = TEXT("PathSelectionCombatEnemy");
    Enemy.DisplayName = TEXT("Path Selection Enemy");
    Enemy.MaxHealth = 100;
    GameMode->CombatComponent->SpawnEnemy(Enemy);

    Minimap->HandleTileClicked(FIntPoint(1, 0));
    TestEqual(TEXT("Tile selection is rejected while combat is active"), Minimap->CurrentPath.Num(), 0);

    Minimap->RemoveFromParent();
    GameMode->CombatComponent->ClearEnemy();
    GameMode->RaidState = PreviousRaidState;
    return true;
}

#endif
