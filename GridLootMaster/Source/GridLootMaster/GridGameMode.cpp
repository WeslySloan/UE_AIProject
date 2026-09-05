#include "GridGameMode.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GridInventoryComponent.h"
#include "UI/MainGameUI.h"
#include "Components/WidgetSwitcher.h"
#include "Kismet/GameplayStatics.h"
#include "ItemInstance.h"
#include "EquipmentComponent.h"
#include "Map/MapManagerComponent.h"
#include "UI/MinimapWidget.h"
#include "StashSaveGame.h"
#include "CombatComponent.h"
#include "EnemyManagerComponent.h"

namespace
{
    UItemInstance* CreateSavedItem(const FStashItemRecord& Record, UDataTable* ItemDataTable, UObject* Outer)
    {
        if (!ItemDataTable) return nullptr;

        FItemData* Row = ItemDataTable->FindRow<FItemData>(Record.TemplateID, TEXT("LoadStash"));
        if (!Row) return nullptr;

        UItemInstance* Item = NewObject<UItemInstance>(Outer);
        Item->InitFromData(*Row);
        Item->InstanceID = Record.InstanceID;
        Item->CurrentStack = FMath::Clamp(Record.CurrentStack, 1, FMath::Max(1, Item->MaxStack));
        Item->CurrentAmmo = FMath::Clamp(Record.CurrentAmmo, 0, FMath::Max(0, Item->MaxAmmo));
        Item->bIsRotated = Record.bIsRotated;
        Item->bIsExamined = Record.bIsExamined;
        return Item;
    }

    UItemInstance* CreateSavedAttachedItem(const FStashAttachedItemRecord& Record, UDataTable* ItemDataTable, UObject* Outer)
    {
        if (!ItemDataTable) return nullptr;

        FItemData* Row = ItemDataTable->FindRow<FItemData>(Record.TemplateID, TEXT("LoadStashAttachment"));
        if (!Row) return nullptr;

        UItemInstance* Item = NewObject<UItemInstance>(Outer);
        Item->InitFromData(*Row);
        Item->InstanceID = Record.InstanceID;
        Item->CurrentStack = FMath::Clamp(Record.CurrentStack, 1, FMath::Max(1, Item->MaxStack));
        Item->CurrentAmmo = FMath::Clamp(Record.CurrentAmmo, 0, FMath::Max(0, Item->MaxAmmo));
        Item->bIsRotated = Record.bIsRotated;
        Item->bIsExamined = Record.bIsExamined;
        return Item;
    }

    FStashAttachedItemRecord MakeAttachedRecord(const UItemInstance* Item)
    {
        FStashAttachedItemRecord Record;
        if (!Item) return Record;

        Record.InstanceID = Item->InstanceID;
        Record.TemplateID = Item->TemplateID;
        Record.CurrentStack = Item->CurrentStack;
        Record.CurrentAmmo = Item->CurrentAmmo;
        Record.bIsRotated = Item->bIsRotated;
        Record.bIsExamined = Item->bIsExamined;
        return Record;
    }
}

AGridGameMode::AGridGameMode()
{
    InventoryComponent = CreateDefaultSubobject<UGridInventoryComponent>(TEXT("InventoryComponent"));
    LootContainerComponent = CreateDefaultSubobject<UGridInventoryComponent>(TEXT("LootContainerComponent"));
    SafeBoxComponent = CreateDefaultSubobject<UGridInventoryComponent>(TEXT("SafeBoxComponent"));
    RigComponent = CreateDefaultSubobject<UGridInventoryComponent>(TEXT("RigComponent"));
    PocketComponent = CreateDefaultSubobject<UGridInventoryComponent>(TEXT("PocketComponent"));
    StashComponent = CreateDefaultSubobject<UGridInventoryComponent>(TEXT("StashComponent"));
    EquipmentComponent = CreateDefaultSubobject<UEquipmentComponent>(TEXT("EquipmentComponent"));
    CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
    EnemyManagerComponent = CreateDefaultSubobject<UEnemyManagerComponent>(TEXT("EnemyManagerComponent"));
    MapManagerComponent = CreateDefaultSubobject<UMapManagerComponent>(TEXT("MapManagerComponent"));
    RaidState = ERaidState::Lobby;
    TotalTimeLimit = 60.0f;
    QuotaScore = 1000;
    MaxHealth = 100;
    StashSaveSlot = TEXT("GridLootMaster_Stash");
    CurrentPlayerCoord = FIntPoint(0, 0);
}

FName AGridGameMode::FindCompatibleAmmoID(const UItemInstance* Magazine) const
{
    if (!Magazine || !ItemDataTable || Magazine->CompatibleAmmo.IsEmpty()) return NAME_None;

    for (const FName& RowName : ItemDataTable->GetRowNames())
    {
        const FItemData* Row = ItemDataTable->FindRow<FItemData>(RowName, TEXT("FindCompatibleAmmo"));
        if (!Row || Row->Category != EItemCategory::Consumable) continue;

        if (Row->ItemName.Contains(Magazine->CompatibleAmmo, ESearchCase::IgnoreCase) ||
            Row->ItemID.ToString().Contains(Magazine->CompatibleAmmo, ESearchCase::IgnoreCase) ||
            RowName.ToString().Contains(Magazine->CompatibleAmmo, ESearchCase::IgnoreCase))
        {
            return RowName;
        }
    }

    return NAME_None;
}

bool AGridGameMode::ReconfigureStorageForEquipmentSlot(FName SlotID, const UItemInstance* StorageItem)
{
    UGridInventoryComponent* Storage = SlotID == TEXT("Backpack") ? InventoryComponent :
        (SlotID == TEXT("Rig") ? RigComponent : nullptr);
    if (!Storage || !StorageItem || StorageItem->StorageLayoutSpec.IsEmpty()) return false;
    TArray<FIntPoint> SectionSizes;
    if (!UGridInventoryComponent::ParseStorageLayoutSpec(StorageItem->StorageLayoutSpec, SectionSizes))
    {
        UE_LOG(LogTemp, Warning, TEXT("Rejected invalid storage layout for %s: %s"), *SlotID.ToString(), *StorageItem->StorageLayoutSpec);
        return false;
    }
    return Storage->ReconfigureSections(SectionSizes);
}

bool AGridGameMode::TryStandaloneStorageUnequip(FName SlotID, UItemInstance* Item)
{
    if (!Item || !EquipmentComponent || EquipmentComponent->GetEquippedItem(SlotID) != Item) return false;

    if (SlotID == TEXT("Rig"))
    {
        if (!RigComponent || !InventoryComponent || RigComponent->ItemInstances.Num() > 0) return false;

        int32 Section = INDEX_NONE;
        int32 X = INDEX_NONE;
        int32 Y = INDEX_NONE;
        const FIntPoint Size = Item->GetCurrentSize();
        if (!InventoryComponent->FindEmptySpaceAcrossSections(Size.X, Size.Y, Section, X, Y) ||
            !InventoryComponent->AddItemToSection(Item, Section, X, Y)) return false;

        if (!EquipmentComponent->RemoveItemByInstanceID(Item->InstanceID) || !RigComponent->DisableStorage())
        {
            const bool bRestored = InventoryComponent->RemoveItem(Item->InstanceID) && EquipmentComponent->EquipItem(SlotID, Item);
            if (!bRestored) UE_LOG(LogTemp, Error, TEXT("Rig standalone unequip rollback failed for %s"), *Item->InstanceID.ToString());
            return false;
        }
        return true;
    }

    if (SlotID == TEXT("Backpack"))
    {
        if (RaidState != ERaidState::Lobby || !InventoryComponent || !StashComponent || InventoryComponent->ItemInstances.Num() > 0) return false;

        int32 X = INDEX_NONE;
        int32 Y = INDEX_NONE;
        const FIntPoint Size = Item->GetCurrentSize();
        if (!StashComponent->FindEmptySpace(Size.X, Size.Y, X, Y) || !StashComponent->AddItem(Item, X, Y)) return false;

        if (!EquipmentComponent->RemoveItemByInstanceID(Item->InstanceID) || !InventoryComponent->DisableStorage())
        {
            const bool bRestored = StashComponent->RemoveItem(Item->InstanceID) && EquipmentComponent->EquipItem(SlotID, Item);
            if (!bRestored) UE_LOG(LogTemp, Error, TEXT("Backpack standalone unequip rollback failed for %s"), *Item->InstanceID.ToString());
            return false;
        }
        return true;
    }

    return false;
}

void AGridGameMode::BeginPlay()
{
    Super::BeginPlay();

    InventoryComponent->InitializeGrid(5, 6); // 백팩 사이즈 (가로 5, 세로 6)
    LootContainerComponent->InitializeGrid(6, 6); 
    SafeBoxComponent->InitializeGrid(2, 2);
    RigComponent->InitializeGrid(4, 3); // Rig 사이즈 (예: 4x3)
    PocketComponent->InitializeGrid(5, 1); // Pocket 사이즈 (예: 가로 5, 세로 1)
    StashComponent->InitializeGrid(10, 10); // 영구 보관함 프로토타입 사이즈
    const bool bIsNewStash = StashSaveSlot.IsEmpty() || !UGameplayStatics::DoesSaveGameExist(StashSaveSlot, 0);
    LoadStash();
    const bool bNeedsStashSeedVersion = bIsNewStash || LoadedInitialQAMagazineSeedVersion < 1;
    const bool bHasLegacyQAMagazineSeed = StashComponent->GetItemInstance(TEXT("StashMagQA_1")) ||
        StashComponent->GetItemInstance(TEXT("StashMagQA_2")) ||
        StashComponent->GetItemInstance(TEXT("StashMagQA_3"));
    const bool bShouldSeedStash = bNeedsStashSeedVersion && !bHasLegacyQAMagazineSeed;

    auto MakeDefaultInstanceID = [this](FName PreferredID)
    {
        return MakeUniqueInstanceID(PreferredID);
    };

    // 기본 가방 인벤토리 샘플 아이템
    UItemInstance* DefBackpack = NewObject<UItemInstance>(this);
    DefBackpack->InstanceID = MakeDefaultInstanceID(TEXT("Item_DefBackpack"));
    DefBackpack->TemplateID = TEXT("DefaultBackpack");
    DefBackpack->ItemName = TEXT("Standard Backpack");
    DefBackpack->Category = EItemCategory::Backpack;
    DefBackpack->StorageLayoutSpec = TEXT("5x6");
    DefBackpack->BaseSize = FIntPoint(2, 2); // 슬롯 크기 2x2
    DefBackpack->CurrentStack = 1;
    DefBackpack->MaxStack = 1;
    EquipmentComponent->EquipItem(TEXT("Backpack"), DefBackpack);
    ReconfigureStorageForEquipmentSlot(TEXT("Backpack"), DefBackpack);

    // 기본 안전 금고 장착 (2x2)
    UItemInstance* DefSafeBox = NewObject<UItemInstance>(this);
    DefSafeBox->InstanceID = MakeDefaultInstanceID(TEXT("Item_DefSafeBox"));
    DefSafeBox->TemplateID = TEXT("DefaultSafeBox");
    DefSafeBox->Category = EItemCategory::SafeBox;
    DefSafeBox->BaseSize = FIntPoint(2, 2); // 슬롯 크기 2x2
    DefSafeBox->CurrentStack = 1;
    DefSafeBox->MaxStack = 1;
    EquipmentComponent->EquipItem(TEXT("SafeBox"), DefSafeBox);

    // 기본 체스트 리그 장착 (4x3)
    UItemInstance* DefRig = NewObject<UItemInstance>(this);
    DefRig->InstanceID = MakeDefaultInstanceID(TEXT("Item_DefRig"));
    DefRig->TemplateID = TEXT("DefaultRig");
    DefRig->ItemName = TEXT("Standard Chest Rig");
    DefRig->Category = EItemCategory::Rig;
    DefRig->StorageLayoutSpec = TEXT("4x3");
    DefRig->BaseSize = FIntPoint(2, 2); // 슬롯 모양 (예: 2x2)
    DefRig->CurrentStack = 1;
    DefRig->MaxStack = 1;
    EquipmentComponent->EquipItem(TEXT("Rig"), DefRig);
    ReconfigureStorageForEquipmentSlot(TEXT("Rig"), DefRig);

    auto CreateTestItem = [&](FName ID, FName TempID) -> UItemInstance* {
        if (!ItemDataTable) return nullptr;
        FItemData* Row = ItemDataTable->FindRow<FItemData>(TempID, TEXT("BeginPlay"));
        if (!Row) return nullptr;

        UItemInstance* Item = NewObject<UItemInstance>(this);
        Item->InstanceID = MakeDefaultInstanceID(ID);
        Item->InitFromData(*Row);
        Item->bIsExamined = true;
        Item->bIsRotated = false;
        return Item;
    };

    if (UItemInstance* Rifle = CreateTestItem(TEXT("TestRifle"), TEXT("M4A1")))
        InventoryComponent->AddItem(Rifle, 0, 0);
    UItemInstance* InitialAmmo = CreateTestItem(TEXT("TestAmmo"), TEXT("Ammo_556_M995"));
    if (InitialAmmo)
    {
        InitialAmmo->CurrentStack = FMath::Min(60, InitialAmmo->MaxStack);
        InventoryComponent->AddItem(InitialAmmo, 0, 2);
    }
    UItemInstance* InitialMagazine = CreateTestItem(TEXT("TestMag"), TEXT("Mag_M4"));
    if (InitialMagazine)
    {
        InitialMagazine->CurrentAmmo = InitialMagazine->MaxAmmo;
        InventoryComponent->AddItem(InitialMagazine, 1, 2);
    }
    if (UItemInstance* Scope = CreateTestItem(TEXT("TestScope"), TEXT("Scope_ACOG")))
        InventoryComponent->AddItem(Scope, 2, 2);
    if (UItemInstance* Silencer = CreateTestItem(TEXT("TestSilencer"), TEXT("Muzzle_556")))
        InventoryComponent->AddItem(Silencer, 2, 3);

    if (InitialMagazine)
    {
        for (int32 MagazineIndex = 0; MagazineIndex < 2; ++MagazineIndex)
        {
            const FName InstanceID(*FString::Printf(TEXT("TestMagQA_%d"), MagazineIndex + 1));
            if (UItemInstance* Magazine = CreateTestItem(InstanceID, InitialMagazine->TemplateID))
            {
                Magazine->CurrentAmmo = Magazine->MaxAmmo;
                int32 InventoryX = 0;
                int32 InventoryY = 0;
                if (InventoryComponent->FindEmptySpace(Magazine->GetCurrentSize().X, Magazine->GetCurrentSize().Y, InventoryX, InventoryY))
                {
                    InventoryComponent->AddItem(Magazine, InventoryX, InventoryY);
                }
            }
        }

        if (bShouldSeedStash)
        {
            for (int32 MagazineIndex = 0; MagazineIndex < 3; ++MagazineIndex)
            {
                const FName InstanceID(*FString::Printf(TEXT("StashMagQA_%d"), MagazineIndex + 1));
                if (UItemInstance* Magazine = CreateTestItem(InstanceID, InitialMagazine->TemplateID))
                {
                    Magazine->CurrentAmmo = Magazine->MaxAmmo;
                    int32 StashX = 0;
                    int32 StashY = 0;
                    if (StashComponent->FindEmptySpace(Magazine->GetCurrentSize().X, Magazine->GetCurrentSize().Y, StashX, StashY))
                    {
                        StashComponent->AddItem(Magazine, StashX, StashY);
                    }
                }
            }
        }

        if (bNeedsStashSeedVersion)
        {
            LoadedInitialQAMagazineSeedVersion = 1;
            SaveStash();
        }
    }

    CurrentScore = 0;
    RemainingTime = TotalTimeLimit; // 60초 게임
    CurrentHealth = MaxHealth;

    // C++에서 UI 자동 생성 및 화면 표시
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        MainUI = CreateWidget<UMainGameUI>(PC, UMainGameUI::StaticClass());
        if (MainUI)
        {
            MainUI->AddToViewport();
            MainUI->UpdateScore(CurrentScore);
            MainUI->UpdateTimer(RemainingTime);
            MainUI->UpdateHealth(CurrentHealth, MaxHealth);
            if (MainUI->RightPanelSwitcher)
            {
                MainUI->RightPanelSwitcher->SetActiveWidgetIndex(2);
            }
            MainUI->UpdateActionAvailability();
            
            // PlayerController 설정
            PC->SetShowMouseCursor(true);
            FInputModeUIOnly InputMode;
            InputMode.SetWidgetToFocus(MainUI->TakeWidget());
            PC->SetInputMode(InputMode);
        }
    }

}

FName AGridGameMode::MakeUniqueInstanceID(FName PreferredID) const
{
    if (PreferredID == NAME_None)
    {
        return PreferredID;
    }

    auto ItemUsesID = [](const UItemInstance* Item, FName ID)
    {
        return Item && (Item->InstanceID == ID ||
            (Item->EquippedSight && Item->EquippedSight->InstanceID == ID) ||
            (Item->EquippedMuzzle && Item->EquippedMuzzle->InstanceID == ID) ||
            (Item->EquippedMagazine && Item->EquippedMagazine->InstanceID == ID));
    };
    auto InventoryUsesID = [&](const UGridInventoryComponent* Inventory, FName ID)
    {
        if (!Inventory) return false;
        for (const TPair<FName, UItemInstance*>& Pair : Inventory->ItemInstances)
        {
            if (Pair.Key == ID || ItemUsesID(Pair.Value, ID)) return true;
        }
        return Inventory->GridCells.Contains(ID);
    };
    auto IsUsed = [&](FName ID)
    {
        const UGridInventoryComponent* Inventories[] = {
            InventoryComponent, RigComponent, PocketComponent, SafeBoxComponent, LootContainerComponent, StashComponent };
        for (const UGridInventoryComponent* Inventory : Inventories)
        {
            if (InventoryUsesID(Inventory, ID)) return true;
        }
        for (const TPair<FName, UGridInventoryComponent*>& Pair : CorpseLootInventories)
        {
            if (InventoryUsesID(Pair.Value, ID)) return true;
        }
        if (EquipmentComponent)
        {
            for (const TPair<FName, UItemInstance*>& Pair : EquipmentComponent->EquippedItems)
            {
                if (ItemUsesID(Pair.Value, ID)) return true;
            }
        }
        return false;
    };

    FName Candidate = PreferredID;
    while (IsUsed(Candidate))
    {
        Candidate = FName(*FString::Printf(TEXT("%s_%s"), *PreferredID.ToString(), *FGuid::NewGuid().ToString()));
    }

    return Candidate;
}

void AGridGameMode::HandlePlayerMoved(FIntPoint NewCoordinate)
{
    if (RaidState != ERaidState::InRaid || !CombatComponent || CombatComponent->bHasActiveEnemy ||
        PlayerPosture == EPlayerRaidPosture::Ambushing ||
        (EnemyManagerComponent && EnemyManagerComponent->HasActiveAmbushReaction()))
    {
        return;
    }

    if (MapManagerComponent && CurrentPlayerCoord != NewCoordinate &&
        MapManagerComponent->CanMoveBetween(CurrentPlayerCoord, NewCoordinate))
    {
        PreviousPlayerCoord = CurrentPlayerCoord;
        CurrentPlayerCoord = NewCoordinate;
        InvalidateContainerSearchIfPlayerLeftTile();
        InvalidateCorpseSearchIfPlayerLeftTile();
        if (EnemyManagerComponent)
        {
            EnemyManagerComponent->TryStartEnemyAmbushAtCurrentPlayer();
        }
    }
}

void AGridGameMode::RefreshEnemyWorldUI()
{
    if (MainUI)
    {
        MainUI->RefreshEnemyMarkers();
        MainUI->UpdateActionAvailability();
    }
    OnGameStateChanged.Broadcast();
}

bool AGridGameMode::HasDeadBodyAtCurrentPlayerCoord() const
{
    FName InstanceID = NAME_None;
    return FindSearchableDeadBodyAt(CurrentPlayerCoord, InstanceID);
}

bool AGridGameMode::FindSearchableDeadBodyAt(FIntPoint Coordinate, FName& OutInstanceID) const
{
    OutInstanceID = NAME_None;
    if (!EnemyManagerComponent) return false;

    for (const FEnemyWorldInstance& Enemy : EnemyManagerComponent->GetEnemyInstances())
    {
        if (Enemy.bAlive || Enemy.WorldState != EEnemyWorldState::Dead || Enemy.Coordinate != Coordinate) continue;
        const UGridInventoryComponent* const* Loot = CorpseLootInventories.Find(Enemy.InstanceID);
        if (Loot && (!*Loot || (*Loot)->ItemInstances.Num() == 0)) continue;
        if (OutInstanceID.IsNone() || Enemy.InstanceID.ToString() < OutInstanceID.ToString())
        {
            OutInstanceID = Enemy.InstanceID;
        }
    }
    return !OutInstanceID.IsNone();
}

const TMap<FName, UGridInventoryComponent*>& AGridGameMode::GetCorpseLootInventories() const
{
    return CorpseLootInventories;
}

bool AGridGameMode::FindCorpseLootInventory(FName ItemID, UGridInventoryComponent*& OutInventory) const
{
    OutInventory = nullptr;
    for (const TPair<FName, UGridInventoryComponent*>& Pair : CorpseLootInventories)
    {
        if (Pair.Value && Pair.Value->GetItemInstance(ItemID))
        {
            OutInventory = Pair.Value;
            return true;
        }
    }
    return false;
}

int32 AGridGameMode::GetCorpseLootGenerationCount(FName EnemyInstanceID) const
{
    return CorpseLootGenerationCounts.FindRef(EnemyInstanceID);
}

void AGridGameMode::ClearCorpseLoot()
{
    CorpseLootInventories.Empty();
    CorpseLootGenerationCounts.Empty();
    ActiveCorpseInstanceID = NAME_None;
    ActiveCorpseSearchCoord = FIntPoint::ZeroValue;
    if (MainUI) MainUI->ClearCorpseLootView();
}

bool AGridGameMode::EnsureCorpseLootGenerated(FName EnemyInstanceID)
{
    if (EnemyInstanceID.IsNone()) return false;
    if (CorpseLootInventories.Contains(EnemyInstanceID)) return true;

    UGridInventoryComponent* CorpseInventory = NewObject<UGridInventoryComponent>(this);
    if (!CorpseInventory) return false;
    CorpseInventory->InitializeGrid(6, 6);
    CorpseLootInventories.Add(EnemyInstanceID, CorpseInventory);
    CorpseLootGenerationCounts.Add(EnemyInstanceID, 1);

    if (!ItemDataTable) return true;
    TArray<FItemData*> AllItems;
    ItemDataTable->GetAllRows<FItemData>(TEXT("CorpseLoot"), AllItems);
    int32 TotalWeight = 0;
    for (FItemData* ItemData : AllItems)
    {
        if (ItemData && ItemData->DropWeight > 0) TotalWeight += ItemData->DropWeight;
    }
    if (TotalWeight <= 0) return true;

    for (int32 Index = 0; Index < 5; ++Index)
    {
        int32 Roll = FMath::RandRange(1, TotalWeight);
        FItemData* Selected = nullptr;
        for (FItemData* ItemData : AllItems)
        {
            if (ItemData && ItemData->DropWeight > 0 && (Roll -= ItemData->DropWeight) <= 0)
            {
                Selected = ItemData;
                break;
            }
        }
        if (!Selected) continue;
        UItemInstance* Item = NewObject<UItemInstance>(CorpseInventory);
        Item->InstanceID = MakeUniqueInstanceID(FName(*FString::Printf(TEXT("Corpse_%s_%d"), *EnemyInstanceID.ToString(), Index)));
        Item->InitFromData(*Selected);
        Item->bIsExamined = false;
        int32 X = INDEX_NONE;
        int32 Y = INDEX_NONE;
        if (CorpseInventory->FindEmptySpace(Item->GetCurrentSize().X, Item->GetCurrentSize().Y, X, Y))
        {
            CorpseInventory->AddItem(Item, X, Y);
        }
    }
    return true;
}

void AGridGameMode::BindLootInventoryToUI(UGridInventoryComponent* Inventory)
{
    if (MainUI) MainUI->SetLootInventory(Inventory);
}

bool AGridGameMode::RequestSearchDeadBody()
{
    if (RaidState != ERaidState::InRaid || !EnemyManagerComponent ||
        (CombatComponent && CombatComponent->bHasActiveEnemy) || PlayerPosture == EPlayerRaidPosture::Ambushing ||
        EnemyManagerComponent->HasActiveAmbushReaction())
    {
        return false;
    }

    if (UWorld* World = GetWorld())
    {
        if (World->GetTimerManager().IsTimerActive(SearchPhaseTimer) ||
            World->GetTimerManager().IsTimerActive(ExamineTimer) || ItemsToExamine.Num() > 0)
        {
            return false;
        }
    }

    FName InstanceID = NAME_None;
    if (!FindSearchableDeadBodyAt(CurrentPlayerCoord, InstanceID) ||
        !EnsureCorpseLootGenerated(InstanceID))
    {
        return false;
    }
    ActiveCorpseInstanceID = InstanceID;
    ActiveCorpseSearchCoord = CurrentPlayerCoord;
    if (UGridInventoryComponent* const* Inventory = CorpseLootInventories.Find(InstanceID))
    {
        ActiveExamineInventory = *Inventory;
        ItemsToExamine.Empty();
        for (const TPair<FName, UItemInstance*>& Pair : (*Inventory)->ItemInstances)
        {
            if (Pair.Value && !Pair.Value->bIsExamined) ItemsToExamine.Add(Pair.Key);
        }
        ItemsToExamine.Sort([Inventory](const FName& A, const FName& B)
        {
            int32 ASection = INDEX_NONE, AX = INDEX_NONE, AY = INDEX_NONE;
            int32 BSection = INDEX_NONE, BX = INDEX_NONE, BY = INDEX_NONE;
            (*Inventory)->FindItemPlacement(A, ASection, AX, AY);
            (*Inventory)->FindItemPlacement(B, BSection, BX, BY);
            if (ASection != BSection) return ASection < BSection;
            if (AY != BY) return AY < BY;
            if (AX != BX) return AX < BX;
            return A.ToString() < B.ToString();
        });
        if (ItemsToExamine.Num() > 0)
        {
            if (UWorld* World = GetWorld())
            {
                World->GetTimerManager().SetTimer(ExamineTimer, this, &AGridGameMode::ProcessNextExamine, 0.5f, true);
            }
        }
        BindLootInventoryToUI(*Inventory);
    }
    if (MainUI) MainUI->QueueEventNotification(TEXT("시체를 수색 중입니다."));
    return true;
}

void AGridGameMode::InvalidateCorpseSearchIfPlayerLeftTile()
{
    if (ActiveCorpseInstanceID.IsNone() || CurrentPlayerCoord == ActiveCorpseSearchCoord) return;
    if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(ExamineTimer);
    ItemsToExamine.Empty();
    ActiveExamineInventory = nullptr;
    ActiveCorpseInstanceID = NAME_None;
    ActiveCorpseSearchCoord = FIntPoint::ZeroValue;
    if (MainUI) MainUI->ClearCorpseLootView();
}

bool AGridGameMode::MovePlayerDuringCombat(FIntPoint NewCoordinate)
{
    if (RaidState != ERaidState::InRaid || !CombatComponent || !CombatComponent->bHasActiveEnemy ||
        !MapManagerComponent || !MapManagerComponent->CanMoveBetween(CurrentPlayerCoord, NewCoordinate) ||
        !EnemyManagerComponent)
    {
        return false;
    }

    PreviousPlayerCoord = CurrentPlayerCoord;
    CurrentPlayerCoord = NewCoordinate;
    InvalidateContainerSearchIfPlayerLeftTile();
    InvalidateCorpseSearchIfPlayerLeftTile();
    if (MainUI)
    {
        if (MainUI->MinimapUI) MainUI->MinimapUI->SetPlayerCoordinateForCombat(NewCoordinate);
        if (MainUI->CompactMinimapUI) MainUI->CompactMinimapUI->SetPlayerCoordinateForCombat(NewCoordinate);
    }
    OnGameStateChanged.Broadcast();
    return true;
}

bool AGridGameMode::RequestPlayerCardinalMove(FIntPoint Delta)
{
    if (RaidState != ERaidState::InRaid || !CombatComponent || CombatComponent->bHasActiveEnemy ||
        PlayerPosture == EPlayerRaidPosture::Ambushing ||
        (EnemyManagerComponent && EnemyManagerComponent->HasActiveAmbushReaction()) ||
        (Delta.X != 0 && Delta.Y != 0) || FMath::Abs(Delta.X) + FMath::Abs(Delta.Y) != 1 ||
        !MapManagerComponent)
    {
        return false;
    }

    const FIntPoint CandidateCoord = CurrentPlayerCoord + Delta;
    if (!MapManagerComponent->CanMoveBetween(CurrentPlayerCoord, CandidateCoord)) return false;

    PreviousPlayerCoord = CurrentPlayerCoord;
    CurrentPlayerCoord = CandidateCoord;
    InvalidateContainerSearchIfPlayerLeftTile();
    InvalidateCorpseSearchIfPlayerLeftTile();
    if (MainUI)
    {
        if (MainUI->MinimapUI) MainUI->MinimapUI->SetPlayerCoordinateForCombat(CurrentPlayerCoord);
        if (MainUI->CompactMinimapUI) MainUI->CompactMinimapUI->SetPlayerCoordinateForCombat(CurrentPlayerCoord);
    }
    OnGameStateChanged.Broadcast();
    AdvanceRaidWorldTick();
    return true;
}

void AGridGameMode::AdvanceRaidWorldTick()
{
    if (RaidState != ERaidState::InRaid ||
        (CombatComponent && CombatComponent->bHasActiveEnemy) ||
        !EnemyManagerComponent || EnemyManagerComponent->HasActiveAmbushReaction())
    {
        return;
    }

    EnemyManagerComponent->AdvanceWorldTick();
}

bool AGridGameMode::RequestAmbushSearch()
{
    return RaidState == ERaidState::InRaid && EnemyManagerComponent && CombatComponent &&
        !CombatComponent->bHasActiveEnemy && EnemyManagerComponent->RequestAmbushSearch();
}

bool AGridGameMode::RequestAmbushCover()
{
    return RaidState == ERaidState::InRaid && EnemyManagerComponent && CombatComponent &&
        !CombatComponent->bHasActiveEnemy && EnemyManagerComponent->RequestAmbushCover();
}

bool AGridGameMode::RequestAmbushFlee()
{
    return RaidState == ERaidState::InRaid && EnemyManagerComponent && CombatComponent &&
        !CombatComponent->bHasActiveEnemy && EnemyManagerComponent->RequestAmbushFlee();
}

bool AGridGameMode::TryRestorePreviousPlayerCoord()
{
    if (RaidState != ERaidState::InRaid || !MapManagerComponent ||
        PreviousPlayerCoord == CurrentPlayerCoord ||
        MapManagerComponent->GetTileDistance(CurrentPlayerCoord, PreviousPlayerCoord) != 1 ||
        !MapManagerComponent->CanMoveBetween(CurrentPlayerCoord, PreviousPlayerCoord))
    {
        return false;
    }

    CurrentPlayerCoord = PreviousPlayerCoord;
    InvalidateContainerSearchIfPlayerLeftTile();
    InvalidateCorpseSearchIfPlayerLeftTile();
    return true;
}

bool AGridGameMode::RequestPlayerAmbush()
{
    if (RaidState != ERaidState::InRaid || !EnemyManagerComponent || !CombatComponent ||
        CombatComponent->bHasActiveEnemy || PlayerPosture == EPlayerRaidPosture::Ambushing)
    {
        return false;
    }

    if (MainUI && MainUI->MinimapUI && MainUI->MinimapUI->CurrentMoveProgress > 0)
    {
        return false;
    }

    PlayerPosture = EPlayerRaidPosture::Ambushing;
    OnGameStateChanged.Broadcast();
    AdvanceRaidWorldTick();
    return true;
}

bool AGridGameMode::RequestAmbushWait()
{
    if (RaidState != ERaidState::InRaid || PlayerPosture != EPlayerRaidPosture::Ambushing ||
        !CombatComponent || CombatComponent->bHasActiveEnemy)
    {
        return false;
    }

    AdvanceRaidWorldTick();
    return true;
}

bool AGridGameMode::RequestAmbushCancel()
{
    if (RaidState != ERaidState::InRaid || PlayerPosture != EPlayerRaidPosture::Ambushing)
    {
        return false;
    }

    PlayerPosture = EPlayerRaidPosture::Normal;
    OnGameStateChanged.Broadcast();
    return true;
}

bool AGridGameMode::RequestAmbushLetPass()
{
    if (RaidState != ERaidState::InRaid || PlayerPosture != EPlayerRaidPosture::Ambushing ||
        !CombatComponent || CombatComponent->bHasActiveEnemy)
    {
        return false;
    }

    PlayerPosture = EPlayerRaidPosture::Normal;
    OnGameStateChanged.Broadcast();
    AdvanceRaidWorldTick();
    return true;
}

bool AGridGameMode::RequestAmbushAssault()
{
    if (RaidState != ERaidState::InRaid || PlayerPosture != EPlayerRaidPosture::Ambushing ||
        !CombatComponent || CombatComponent->bHasActiveEnemy || !EnemyManagerComponent)
    {
        return false;
    }

    FName TargetInstanceID = NAME_None;
    if (!EnemyManagerComponent->FindPlayerAmbushTarget(TargetInstanceID) ||
        !EnemyManagerComponent->StartPlayerAmbushContact(TargetInstanceID))
    {
        return false;
    }

    PlayerPosture = EPlayerRaidPosture::Normal;
    OnGameStateChanged.Broadcast();
    return true;
}

void AGridGameMode::StartContainerSearch()
{
    if (RaidState != ERaidState::InRaid)
    {
        if (MainUI) MainUI->QueueEventNotification(TEXT("레이드 중에만 컨테이너를 탐색할 수 있습니다."));
        return;
    }

    if (!LootContainerComponent)
    {
        if (MainUI) MainUI->QueueEventNotification(TEXT("탐색 컨테이너를 사용할 수 없습니다."));
        return;
    }

    if (!MainUI) return;

    ActiveCorpseInstanceID = NAME_None;
    ActiveCorpseSearchCoord = FIntPoint::ZeroValue;
    MainUI->SetLootInventory(LootContainerComponent);

    if (PlayerPosture == EPlayerRaidPosture::Ambushing ||
        (EnemyManagerComponent && EnemyManagerComponent->HasActiveAmbushReaction()))
    {
        MainUI->QueueEventNotification(TEXT("매복 중에는 컨테이너를 탐색할 수 없습니다."));
        return;
    }

    if (CombatComponent && CombatComponent->bHasActiveEnemy)
    {
        MainUI->QueueEventNotification(TEXT("전투 중에는 컨테이너를 탐색할 수 없습니다."));
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        MainUI->QueueEventNotification(TEXT("탐색을 시작할 수 없습니다."));
        return;
    }

    if (World->GetTimerManager().IsTimerActive(SearchPhaseTimer) ||
        World->GetTimerManager().IsTimerActive(ExamineTimer) || ItemsToExamine.Num() > 0)
    {
        MainUI->QueueEventNotification(TEXT("컨테이너를 이미 탐색 중입니다."));
        return;
    }

    // 기존 타이머 취소
    World->GetTimerManager().ClearTimer(SearchPhaseTimer);
    World->GetTimerManager().ClearTimer(ExamineTimer);
    ItemsToExamine.Empty();
    ActiveExamineInventory = nullptr;
    ActiveContainerSearchCoord = CurrentPlayerCoord;
    bHasActiveContainerSearch = true;

    // 기존 템 지우기
    LootContainerComponent->ClearInventory();
    
    MainUI->QueueEventNotification(TEXT("컨테이너를 탐색 중입니다."));

    // 1초 후 OnSearchPhaseComplete 호출
    World->GetTimerManager().SetTimer(SearchPhaseTimer, this, &AGridGameMode::OnSearchPhaseComplete, 1.0f, false);
}

void AGridGameMode::InvalidateContainerSearchIfPlayerLeftTile()
{
    if (!bHasActiveContainerSearch || CurrentPlayerCoord == ActiveContainerSearchCoord) return;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(SearchPhaseTimer);
        World->GetTimerManager().ClearTimer(ExamineTimer);
    }
    ItemsToExamine.Empty();
    ActiveExamineInventory = nullptr;
    if (LootContainerComponent) LootContainerComponent->ClearInventory();
    bHasActiveContainerSearch = false;
    ActiveContainerSearchCoord = FIntPoint::ZeroValue;
}

void AGridGameMode::OnSearchPhaseComplete()
{
    UWorld* World = GetWorld();
    if (!World) return;

    if (RaidState != ERaidState::InRaid ||
        (CombatComponent && CombatComponent->bHasActiveEnemy))
    {
        ItemsToExamine.Empty();
        World->GetTimerManager().ClearTimer(ExamineTimer);
        ActiveExamineInventory = nullptr;
        return;
    }

    if (!LootContainerComponent || !MainUI)
    {
        ItemsToExamine.Empty();
        World->GetTimerManager().ClearTimer(ExamineTimer);
        ActiveExamineInventory = nullptr;
        return;
    }

    const int64 ExpectedCellCount = static_cast<int64>(LootContainerComponent->GridWidth) *
        LootContainerComponent->GridHeight;
    if (LootContainerComponent->GridWidth <= 0 || LootContainerComponent->GridHeight <= 0 ||
        ExpectedCellCount > MAX_int32 || LootContainerComponent->GridCells.Num() != ExpectedCellCount)
    {
        UE_LOG(LogTemp, Warning, TEXT("Loot container grid shape is invalid."));
        MainUI->QueueEventNotification(TEXT("컨테이너를 사용할 수 없습니다."));
        return;
    }

    if (!ItemDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("ItemDataTable is not assigned in GridGameMode!"));
        MainUI->QueueEventNotification(TEXT("탐색 데이터를 불러오지 못했습니다."));
        return;
    }

    TArray<FItemData*> AllItems;
    ItemDataTable->GetAllRows<FItemData>(TEXT("Loot"), AllItems);
    if (AllItems.Num() == 0)
    {
        MainUI->QueueEventNotification(TEXT("탐색 가능한 아이템이 없습니다."));
        return;
    }

    int32 TotalWeight = 0;
    for (FItemData* ItemData : AllItems)
    {
        if (ItemData && ItemData->DropWeight > 0)
        {
            TotalWeight += ItemData->DropWeight;
        }
    }
    if (TotalWeight <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("ItemDataTable has no loot entries with a positive DropWeight."));
        MainUI->QueueEventNotification(TEXT("탐색 가능한 아이템이 없습니다."));
        return;
    }

    ActiveExamineInventory = LootContainerComponent;

    static int32 SpawnCounter = 0;

    // 컨테이너 그리드 안에 5개의 아이템을 랜덤으로 배치 시도
    int32 Attempts = 20;
    int32 AddedItems = 0;

    while(Attempts > 0 && AddedItems < 5)
    {
        Attempts--;
        int32 RandX = FMath::RandRange(0, LootContainerComponent->GridWidth - 1);
        int32 RandY = FMath::RandRange(0, LootContainerComponent->GridHeight - 1);
        
        int32 RandWeight = FMath::RandRange(1, TotalWeight);
        FItemData* SelectedItem = nullptr;
        for (FItemData* ItemData : AllItems)
        {
            if (ItemData && ItemData->DropWeight > 0)
            {
                RandWeight -= ItemData->DropWeight;
                if (RandWeight <= 0)
                {
                    SelectedItem = ItemData;
                    break;
                }
            }
        }
        
        if (!SelectedItem) continue;

        FString UniqueName = FString::Printf(TEXT("%s_%d"), *SelectedItem->ItemID.ToString(), SpawnCounter);
        FName ItemID = MakeUniqueInstanceID(FName(*UniqueName));
        
        // 아이템 인스턴스 생성
        UItemInstance* NewItem = NewObject<UItemInstance>(this);
        NewItem->InstanceID = ItemID;
        NewItem->InitFromData(*SelectedItem);
        NewItem->bIsExamined = false;

        // 스폰!
        if (LootContainerComponent->AddItem(NewItem, RandX, RandY))
        {
            SpawnCounter++;
            AddedItems++;
            ItemsToExamine.Add(ItemID);
        }
    }

    // 아이템이 무사히 생성되었으면, 0.5초마다 하나씩 까보는 타이머 시작
    if (ItemsToExamine.Num() > 0)
    {
        MainUI->QueueEventNotification(TEXT("탐색 완료. 아이템을 분석 중입니다."));
        World->GetTimerManager().SetTimer(ExamineTimer, this, &AGridGameMode::ProcessNextExamine, 0.5f, true);
    }
    else
    {
        MainUI->QueueEventNotification(TEXT("컨테이너에서 아이템을 찾지 못했습니다."));
    }
}

void AGridGameMode::ProcessNextExamine()
{
    UWorld* World = GetWorld();

    if (RaidState != ERaidState::InRaid ||
        (CombatComponent && CombatComponent->bHasActiveEnemy))
    {
        ItemsToExamine.Empty();
        if (World) World->GetTimerManager().ClearTimer(ExamineTimer);
        ActiveExamineInventory = nullptr;
        return;
    }

    if (ItemsToExamine.Num() == 0)
    {
        if (World) World->GetTimerManager().ClearTimer(ExamineTimer);
        ActiveExamineInventory = nullptr;
        return;
    }

    // 큐에서 아이템 하나 빼기
    FName TargetItem = ItemsToExamine[0];
    ItemsToExamine.RemoveAt(0);

    if (ActiveExamineInventory)
    {
        // 인스턴스의 식별 상태를 true로 변경
        if (UItemInstance* ItemObj = ActiveExamineInventory->GetItemInstance(TargetItem))
        {
            ItemObj->bIsExamined = true;
            ItemObj->OnItemModified.Broadcast();
            // UI 갱신
            ActiveExamineInventory->OnInventoryChanged.Broadcast();
        }
    }

    if (ItemsToExamine.Num() == 0)
    {
        if (World) World->GetTimerManager().ClearTimer(ExamineTimer);
        ActiveExamineInventory = nullptr;
    }
}

void AGridGameMode::GameTimerUpdate()
{
    if (RaidState != ERaidState::InRaid)
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(GameTimerHandle);
        }
        return;
    }

    RemainingTime -= 1.0f;

    if (RemainingTime <= 0.0f)
    {
        RemainingTime = 0.0f;
    }

    if (MainUI)
    {
        MainUI->UpdateTimer(RemainingTime);
    }

    if (RemainingTime <= 0.0f)
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(GameTimerHandle);
        }
        CheckWinCondition();
    }
}

#if WITH_DEV_AUTOMATION_TESTS
void AGridGameMode::GameTimerUpdateForTest()
{
    GameTimerUpdate();
}

void AGridGameMode::SearchPhaseCompleteForTest()
{
    OnSearchPhaseComplete();
}

int32 AGridGameMode::GetPendingExamineCountForTest() const
{
    return ItemsToExamine.Num();
}

void AGridGameMode::ProcessNextExamineForTest()
{
    ProcessNextExamine();
}

bool AGridGameMode::SeedCorpseLootForTest(FName EnemyInstanceID, const FItemData& ItemData)
{
    if (!EnsureCorpseLootGenerated(EnemyInstanceID)) return false;
    UGridInventoryComponent* const* Inventory = CorpseLootInventories.Find(EnemyInstanceID);
    if (!Inventory || !*Inventory) return false;
    UItemInstance* Item = NewObject<UItemInstance>(*Inventory);
    Item->InstanceID = FName(*FString::Printf(TEXT("TestCorpse_%s"), *EnemyInstanceID.ToString()));
    Item->InitFromData(ItemData);
    Item->bIsExamined = false;
    int32 X = INDEX_NONE;
    int32 Y = INDEX_NONE;
    return (*Inventory)->FindEmptySpace(Item->GetCurrentSize().X, Item->GetCurrentSize().Y, X, Y) &&
        (*Inventory)->AddItem(Item, X, Y);
}
#endif

void AGridGameMode::AddScore(int32 Amount)
{
    if (RaidState != ERaidState::InRaid || Amount == 0)
    {
        return;
    }

    CurrentScore += Amount;
    
    if (MainUI)
    {
        MainUI->UpdateScore(CurrentScore);
    }
    
    CheckWinCondition();
}

void AGridGameMode::SetRaidState(ERaidState NewState)
{
    if (RaidState == NewState) return;

    RaidState = NewState;
    OnGameStateChanged.Broadcast();
}

bool AGridGameMode::StartRaid()
{
    if (RaidState != ERaidState::Lobby || !StashComponent || !ItemDataTable || !MapManagerComponent)
    {
        return false;
    }

    if (!SaveStash())
    {
        return false;
    }

    if (LootContainerComponent) LootContainerComponent->ClearInventory();
    if (CombatComponent)
    {
        CombatComponent->ClearEnemy();
        CombatComponent->LastCombatMessage.Empty();
    }
    if (EnemyManagerComponent) EnemyManagerComponent->ResetForRaid();
    ClearCorpseLoot();
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(SearchPhaseTimer);
        World->GetTimerManager().ClearTimer(ExamineTimer);
    }
    ItemsToExamine.Empty();
    bHasActiveContainerSearch = false;
    ActiveContainerSearchCoord = FIntPoint::ZeroValue;

    CurrentScore = 0;
    RemainingTime = TotalTimeLimit;
    CurrentHealth = MaxHealth;
    PlayerPosture = EPlayerRaidPosture::Normal;
#if WITH_DEV_AUTOMATION_TESTS
    if (bHasForcedRaidStartPointForTest)
    {
        MapManagerComponent->SpawnPoint = ForcedRaidStartPointForTest;
    }
    else
#endif
    {
        MapManagerComponent->SpawnPoint = FMath::RandBool() ? FIntPoint(0, 0) : FIntPoint(8, 8);
    }
    MapManagerComponent->InitializeMap();
    CurrentPlayerCoord = MapManagerComponent->SpawnPoint;
    PreviousPlayerCoord = CurrentPlayerCoord;
    if (MainUI)
    {
        MainUI->LastDisplayedCombatMessage.Empty();
        MainUI->ShowEventNotification(TEXT(""));
        MainUI->RefreshMinimaps(MapManagerComponent);
    }
    SetRaidState(ERaidState::InRaid);

    if (MainUI)
    {
        MainUI->UpdateScore(CurrentScore);
        MainUI->UpdateTimer(RemainingTime);
        MainUI->UpdateHealth(CurrentHealth, MaxHealth);
        if (MainUI->RightPanelSwitcher)
        {
            MainUI->RightPanelSwitcher->SetActiveWidgetIndex(0);
        }
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(GameTimerHandle, this, &AGridGameMode::GameTimerUpdate, 1.0f, true);
    }
    return true;
}

bool AGridGameMode::SaveStash()
{
    if (!StashComponent || !ItemDataTable || StashComponent->GridWidth <= 0 || StashComponent->GridHeight <= 0)
    {
        return false;
    }

    const int64 ExpectedCellCount = static_cast<int64>(StashComponent->GridWidth) * StashComponent->GridHeight;
    if (ExpectedCellCount > MAX_int32 || StashComponent->GridCells.Num() != ExpectedCellCount)
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to save stash because the grid shape is invalid."));
        return false;
    }

    UStashSaveGame* SaveGame = Cast<UStashSaveGame>(UGameplayStatics::CreateSaveGameObject(UStashSaveGame::StaticClass()));
    if (!SaveGame) return false;

    SaveGame->GridWidth = StashComponent->GridWidth;
    SaveGame->GridHeight = StashComponent->GridHeight;
    SaveGame->InitialQAMagazineSeedVersion = LoadedInitialQAMagazineSeedVersion;
    TSet<FName> SavedInstanceIDs;

    for (const TPair<FName, UItemInstance*>& Pair : StashComponent->ItemInstances)
    {
        UItemInstance* Item = Pair.Value;
        if (!Item || Item->InstanceID == NAME_None || SavedInstanceIDs.Contains(Item->InstanceID) ||
            !ItemDataTable->FindRow<FItemData>(Item->TemplateID, TEXT("SaveStash")))
        {
            UE_LOG(LogTemp, Warning, TEXT("Failed to save stash because an item has an invalid identity or template."));
            return false;
        }
        SavedInstanceIDs.Add(Item->InstanceID);

        int32 CellIndex = StashComponent->GridCells.IndexOfByKey(Item->InstanceID);
        if (!StashComponent->GridCells.IsValidIndex(CellIndex))
        {
            UE_LOG(LogTemp, Warning, TEXT("Failed to save stash because item %s has no valid grid cell."), *Item->InstanceID.ToString());
            return false;
        }

        const FIntPoint ItemSize = Item->GetCurrentSize();
        const int32 GridX = CellIndex % StashComponent->GridWidth;
        const int32 GridY = CellIndex / StashComponent->GridWidth;
        if (GridX + ItemSize.X > StashComponent->GridWidth || GridY + ItemSize.Y > StashComponent->GridHeight)
        {
            UE_LOG(LogTemp, Warning, TEXT("Failed to save stash because item %s exceeds the grid bounds."), *Item->InstanceID.ToString());
            return false;
        }
        int32 OccupiedCellCount = 0;
        for (const FName& CellItemID : StashComponent->GridCells)
        {
            if (CellItemID == Item->InstanceID)
            {
                ++OccupiedCellCount;
            }
        }

        if (ItemSize.X <= 0 || ItemSize.Y <= 0 ||
            OccupiedCellCount != ItemSize.X * ItemSize.Y)
        {
            UE_LOG(LogTemp, Warning, TEXT("Failed to save stash because item %s has an invalid occupied footprint."), *Item->InstanceID.ToString());
            return false;
        }

        for (int32 ItemY = GridY; ItemY < GridY + ItemSize.Y; ++ItemY)
        {
            for (int32 ItemX = GridX; ItemX < GridX + ItemSize.X; ++ItemX)
            {
                const int32 FootprintIndex = StashComponent->GetIndex(ItemX, ItemY);
                if (!StashComponent->GridCells.IsValidIndex(FootprintIndex) ||
                    StashComponent->GridCells[FootprintIndex] != Item->InstanceID)
                {
                    UE_LOG(LogTemp, Warning, TEXT("Failed to save stash because item %s has a mismatched occupied footprint."), *Item->InstanceID.ToString());
                    return false;
                }
            }
        }

        FStashItemRecord Record;
        Record.InstanceID = Item->InstanceID;
        Record.TemplateID = Item->TemplateID;
        Record.GridX = GridX;
        Record.GridY = GridY;
        Record.CurrentStack = Item->CurrentStack;
        Record.CurrentAmmo = Item->CurrentAmmo;
        Record.bIsRotated = Item->bIsRotated;
        Record.bIsExamined = Item->bIsExamined;

        auto SaveAttachedItem = [&](const UItemInstance* AttachedItem, EAttachmentType ExpectedType, FStashAttachedItemRecord& OutRecord)
        {
            const FItemData* AttachedData = AttachedItem && ItemDataTable
                ? ItemDataTable->FindRow<FItemData>(AttachedItem->TemplateID, TEXT("SaveStashAttachment"))
                : nullptr;
            if (Item->Category != EItemCategory::Weapon || !AttachedItem ||
                AttachedItem->InstanceID == NAME_None || SavedInstanceIDs.Contains(AttachedItem->InstanceID) ||
                AttachedItem->Category != EItemCategory::Attachment || AttachedItem->AttachmentType != ExpectedType ||
                !AttachedData || AttachedData->Category != EItemCategory::Attachment ||
                AttachedData->AttachmentType != ExpectedType)
            {
                return false;
            }

            OutRecord = MakeAttachedRecord(AttachedItem);
            SavedInstanceIDs.Add(AttachedItem->InstanceID);
            return true;
        };

        if (Item->EquippedSight)
        {
            if (!SaveAttachedItem(Item->EquippedSight, EAttachmentType::Sight, Record.EquippedSight)) return false;
            Record.bHasEquippedSight = true;
        }
        if (Item->EquippedMuzzle)
        {
            if (!SaveAttachedItem(Item->EquippedMuzzle, EAttachmentType::Muzzle, Record.EquippedMuzzle)) return false;
            Record.bHasEquippedMuzzle = true;
        }
        if (Item->EquippedMagazine)
        {
            if (!SaveAttachedItem(Item->EquippedMagazine, EAttachmentType::Magazine, Record.EquippedMagazine)) return false;
            Record.bHasEquippedMagazine = true;
        }

        SaveGame->Items.Add(Record);
    }

    if (StashSaveSlot.IsEmpty() || !UGameplayStatics::SaveGameToSlot(SaveGame, StashSaveSlot, 0))
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to save GridLootMaster stash."));
        return false;
    }

    return true;
}

bool AGridGameMode::LoadStash()
{
    if (!StashComponent || !ItemDataTable) return false;

    if (StashSaveSlot.IsEmpty() || !UGameplayStatics::DoesSaveGameExist(StashSaveSlot, 0))
    {
        return true;
    }

    UStashSaveGame* SaveGame = Cast<UStashSaveGame>(UGameplayStatics::LoadGameFromSlot(StashSaveSlot, 0));
    if (!SaveGame) return false;

    constexpr int32 MaxStashGridDimension = 10;
    if (SaveGame->GridWidth < 1 || SaveGame->GridWidth > MaxStashGridDimension ||
        SaveGame->GridHeight < 1 || SaveGame->GridHeight > MaxStashGridDimension)
    {
        UE_LOG(LogTemp, Warning, TEXT("Skipped stash load because saved grid dimensions are invalid: %dx%d."),
            SaveGame->GridWidth, SaveGame->GridHeight);
        return false;
    }

    const int32 SavedWidth = SaveGame->GridWidth;
    const int32 SavedHeight = SaveGame->GridHeight;
    UGridInventoryComponent* LoadedStash = NewObject<UGridInventoryComponent>(this);
    if (!LoadedStash) return false;
    LoadedStash->InitializeGrid(SavedWidth, SavedHeight);
    TSet<FName> LoadedInstanceIDs;
    bool bAllItemsLoaded = true;

    for (const FStashItemRecord& Record : SaveGame->Items)
    {
        if (Record.InstanceID == NAME_None || LoadedInstanceIDs.Contains(Record.InstanceID))
        {
            UE_LOG(LogTemp, Warning, TEXT("Skipped invalid or duplicate stash item InstanceID %s."), *Record.InstanceID.ToString());
            bAllItemsLoaded = false;
            continue;
        }

        UItemInstance* Item = CreateSavedItem(Record, ItemDataTable, this);
        if (!Item || !LoadedStash->AddItem(Item, Record.GridX, Record.GridY))
        {
            UE_LOG(LogTemp, Warning, TEXT("Failed to restore stash item %s."), *Record.TemplateID.ToString());
            bAllItemsLoaded = false;
            continue;
        }
        LoadedInstanceIDs.Add(Record.InstanceID);

        auto RestoreAttachedItem = [&](const FStashAttachedItemRecord& AttachedRecord, UItemInstance*& OutAttachedItem, EAttachmentType ExpectedType)
        {
            if (Item->Category != EItemCategory::Weapon ||
                AttachedRecord.InstanceID == NAME_None || LoadedInstanceIDs.Contains(AttachedRecord.InstanceID))
            {
                bAllItemsLoaded = false;
                return;
            }

            UItemInstance* RestoredItem = CreateSavedAttachedItem(AttachedRecord, ItemDataTable, this);
            if (RestoredItem && RestoredItem->Category == EItemCategory::Attachment &&
                RestoredItem->AttachmentType == ExpectedType)
            {
                OutAttachedItem = RestoredItem;
                LoadedInstanceIDs.Add(AttachedRecord.InstanceID);
            }
            else
            {
                OutAttachedItem = nullptr;
                bAllItemsLoaded = false;
            }
        };

        if (Record.bHasEquippedSight)
        {
            RestoreAttachedItem(Record.EquippedSight, Item->EquippedSight, EAttachmentType::Sight);
        }
        if (Record.bHasEquippedMuzzle)
        {
            RestoreAttachedItem(Record.EquippedMuzzle, Item->EquippedMuzzle, EAttachmentType::Muzzle);
        }
        if (Record.bHasEquippedMagazine)
        {
            RestoreAttachedItem(Record.EquippedMagazine, Item->EquippedMagazine, EAttachmentType::Magazine);
        }
    }

    if (!bAllItemsLoaded) return false;

    LoadedInitialQAMagazineSeedVersion = SaveGame->InitialQAMagazineSeedVersion;

    StashComponent->GridWidth = LoadedStash->GridWidth;
    StashComponent->GridHeight = LoadedStash->GridHeight;
    StashComponent->GridCells = MoveTemp(LoadedStash->GridCells);
    StashComponent->ItemInstances = MoveTemp(LoadedStash->ItemInstances);
    StashComponent->OnInventoryChanged.Broadcast();
    return true;
}

bool AGridGameMode::ExtractRaid()
{
    if (RaidState != ERaidState::InRaid)
    {
        if (MainUI) MainUI->QueueEventNotification(TEXT("레이드 중에만 탈출할 수 있습니다."));
        return false;
    }

    if (!StashComponent || !EquipmentComponent)
    {
        if (MainUI) MainUI->QueueEventNotification(TEXT("탈출 준비가 완료되지 않았습니다."));
        return false;
    }

    if (PlayerPosture == EPlayerRaidPosture::Ambushing ||
        (EnemyManagerComponent && EnemyManagerComponent->HasActiveAmbushReaction()))
    {
        if (MainUI) MainUI->QueueEventNotification(TEXT("매복 중에는 탈출할 수 없습니다."));
        return false;
    }

    if (!IsAtExtractionPoint())
    {
        if (MainUI) MainUI->QueueEventNotification(TEXT("탈출 지점에서만 탈출할 수 있습니다."));
        return false;
    }

    if (CombatComponent && CombatComponent->bHasActiveEnemy)
    {
        if (MainUI) MainUI->QueueEventNotification(TEXT("전투 중에는 탈출할 수 없습니다."));
        return false;
    }

    // 추출은 레이드 종료만 처리합니다. 장비/휴대품은 플레이어가 Stash에서 직접 정리할 때까지 유지합니다.
    if (LootContainerComponent) LootContainerComponent->ClearInventory();
    if (CombatComponent) CombatComponent->ClearEnemy();
    if (EnemyManagerComponent) EnemyManagerComponent->ResetForRaid();
    ClearCorpseLoot();
    if (MainUI && MainUI->MinimapUI) MainUI->MinimapUI->ResetMovement();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(GameTimerHandle);
        World->GetTimerManager().ClearTimer(SearchPhaseTimer);
        World->GetTimerManager().ClearTimer(ExamineTimer);
    }
    ItemsToExamine.Empty();
    bHasActiveContainerSearch = false;
    ActiveContainerSearchCoord = FIntPoint::ZeroValue;
    SetRaidState(ERaidState::Lobby);
    if (MainUI)
    {
        MainUI->ShowGameResult(true);
        if (MainUI->RightPanelSwitcher)
        {
            MainUI->RightPanelSwitcher->SetActiveWidgetIndex(2);
        }
        MainUI->UpdateActionAvailability();
    }
    return true;
}

bool AGridGameMode::IsAtExtractionPoint() const
{
    return RaidState == ERaidState::InRaid && MapManagerComponent &&
        MapManagerComponent->IsExtractionPoint(CurrentPlayerCoord);
}

void AGridGameMode::FailRaid()
{
    if (RaidState != ERaidState::InRaid) return;

    if (InventoryComponent) InventoryComponent->ClearInventory();
    if (LootContainerComponent) LootContainerComponent->ClearInventory();
    if (RigComponent) RigComponent->ClearInventory();
    if (PocketComponent) PocketComponent->ClearInventory();

    const FName CarriedEquipmentSlots[] =
    {
        TEXT("Backpack"), TEXT("Rig"), TEXT("Helmet"), TEXT("Armor"),
        TEXT("Primary1"), TEXT("Primary2")
    };
    if (EquipmentComponent)
    {
        for (const FName SlotID : CarriedEquipmentSlots)
        {
            EquipmentComponent->RemoveItemBySlotID(SlotID);
        }
    }
    if (CombatComponent) CombatComponent->ClearEnemy();
    if (EnemyManagerComponent) EnemyManagerComponent->ResetForRaid();
    ClearCorpseLoot();
    if (MainUI && MainUI->MinimapUI) MainUI->MinimapUI->ResetMovement();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(GameTimerHandle);
        World->GetTimerManager().ClearTimer(SearchPhaseTimer);
        World->GetTimerManager().ClearTimer(ExamineTimer);
    }
    ItemsToExamine.Empty();
    bHasActiveContainerSearch = false;
    ActiveContainerSearchCoord = FIntPoint::ZeroValue;
    // 실패 결과를 표시한 뒤 다음 출격을 준비할 수 있도록 로비로 복귀합니다.
    SetRaidState(ERaidState::Lobby);
    if (MainUI) MainUI->ShowGameResult(false);
}

void AGridGameMode::ApplyPlayerDamage(int32 DamageAmount)
{
    if (RaidState != ERaidState::InRaid || DamageAmount <= 0) return;

    int32 TotalArmor = 0;
    if (EquipmentComponent)
    {
        const FName DefensiveSlots[] = { TEXT("Armor"), TEXT("Helmet") };
        for (const FName SlotID : DefensiveSlots)
        {
            if (UItemInstance* DefensiveItem = EquipmentComponent->GetEquippedItem(SlotID))
            {
                TotalArmor += FMath::Max(0, DefensiveItem->Armor);
            }
        }
    }

    const int32 AppliedDamage = FMath::Max(1, DamageAmount - TotalArmor);
    CurrentHealth = FMath::Max(0, CurrentHealth - AppliedDamage);
    if (MainUI)
    {
        MainUI->UpdateHealth(CurrentHealth, MaxHealth);
    }
    if (CurrentHealth <= 0)
    {
        FailRaid();
    }
}

#if WITH_DEV_AUTOMATION_TESTS
void AGridGameMode::SetRaidStartPointForTest(FIntPoint StartPoint)
{
    bHasForcedRaidStartPointForTest = StartPoint == FIntPoint(0, 0) || StartPoint == FIntPoint(8, 8);
    ForcedRaidStartPointForTest = StartPoint;
}
#endif

void AGridGameMode::CheckWinCondition()
{
    if (RemainingTime <= 0.0f && RaidState == ERaidState::InRaid)
    {
        FailRaid();
        return;
    }

    if (CurrentScore >= QuotaScore)
    {
        ExtractRaid();
    }
}
