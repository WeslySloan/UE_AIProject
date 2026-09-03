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

void AGridGameMode::BeginPlay()
{
    Super::BeginPlay();

    InventoryComponent->InitializeGrid(5, 6); // 백팩 사이즈 (가로 5, 세로 6)
    LootContainerComponent->InitializeGrid(6, 6); 
    SafeBoxComponent->InitializeGrid(2, 2);
    RigComponent->InitializeGrid(4, 3); // Rig 사이즈 (예: 4x3)
    PocketComponent->InitializeGrid(5, 1); // Pocket 사이즈 (예: 가로 5, 세로 1)
    StashComponent->InitializeGrid(10, 10); // 영구 보관함 프로토타입 사이즈
    LoadStash();

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
    DefBackpack->BaseSize = FIntPoint(2, 2); // 슬롯 크기 2x2
    DefBackpack->CurrentStack = 1;
    DefBackpack->MaxStack = 1;
    EquipmentComponent->EquipItem(TEXT("Backpack"), DefBackpack);

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
    DefRig->BaseSize = FIntPoint(2, 2); // 슬롯 모양 (예: 2x2)
    DefRig->CurrentStack = 1;
    DefRig->MaxStack = 1;
    EquipmentComponent->EquipItem(TEXT("Rig"), DefRig);

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
    if (UItemInstance* Ammo = CreateTestItem(TEXT("TestAmmo"), TEXT("Ammo_556_M995")))
        InventoryComponent->AddItem(Ammo, 0, 2);
    if (UItemInstance* Mag = CreateTestItem(TEXT("TestMag"), TEXT("Mag_M4")))
        InventoryComponent->AddItem(Mag, 1, 2);
    if (UItemInstance* Scope = CreateTestItem(TEXT("TestScope"), TEXT("Scope_ACOG")))
        InventoryComponent->AddItem(Scope, 2, 2);
    if (UItemInstance* Silencer = CreateTestItem(TEXT("TestSilencer"), TEXT("Muzzle_556")))
        InventoryComponent->AddItem(Silencer, 2, 3);

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
    if (PreferredID == NAME_None || !StashComponent)
    {
        return PreferredID;
    }

    FName Candidate = PreferredID;
    while (StashComponent->GetItemInstance(Candidate) || StashComponent->GridCells.Contains(Candidate))
    {
        Candidate = FName(*FString::Printf(TEXT("%s_%s"), *PreferredID.ToString(), *FGuid::NewGuid().ToString()));
    }

    return Candidate;
}

void AGridGameMode::HandlePlayerMoved(FIntPoint NewCoordinate)
{
    if (RaidState != ERaidState::InRaid || !CombatComponent || CombatComponent->bHasActiveEnemy)
    {
        return;
    }

    CurrentPlayerCoord = NewCoordinate;

    const int32 Chance = FMath::Clamp(EncounterChancePercent, 0, 100);
    if (FMath::RandRange(1, 100) > Chance)
    {
        return;
    }

    FEnemyDefinition EncounterEnemy;
    EncounterEnemy.EnemyID = FName(*FString::Printf(TEXT("Scav_%d_%d"), NewCoordinate.X, NewCoordinate.Y));
    EncounterEnemy.DisplayName = TEXT("Scavenger");
    EncounterEnemy.MaxHealth = 100;
    EncounterEnemy.AttackDamage = 10;
    EncounterEnemy.AccuracyPercent = 75;
    EncounterEnemy.Armor = 0;
    EncounterEnemy.Reward = 100;
    CombatComponent->SpawnEnemy(EncounterEnemy);
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

    // 기존 템 지우기
    LootContainerComponent->ClearInventory();
    
    MainUI->QueueEventNotification(TEXT("컨테이너를 탐색 중입니다."));

    // 1초 후 OnSearchPhaseComplete 호출
    World->GetTimerManager().SetTimer(SearchPhaseTimer, this, &AGridGameMode::OnSearchPhaseComplete, 1.0f, false);
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
        return;
    }

    if (!LootContainerComponent || !MainUI)
    {
        ItemsToExamine.Empty();
        World->GetTimerManager().ClearTimer(ExamineTimer);
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
        FName ItemID = FName(*UniqueName);
        
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
    if (!World) return;

    if (RaidState != ERaidState::InRaid ||
        (CombatComponent && CombatComponent->bHasActiveEnemy))
    {
        ItemsToExamine.Empty();
        World->GetTimerManager().ClearTimer(ExamineTimer);
        return;
    }

    if (ItemsToExamine.Num() == 0)
    {
        World->GetTimerManager().ClearTimer(ExamineTimer);
        return;
    }

    // 큐에서 아이템 하나 빼기
    FName TargetItem = ItemsToExamine[0];
    ItemsToExamine.RemoveAt(0);

    if (LootContainerComponent)
    {
        // 인스턴스의 식별 상태를 true로 변경
        if (UItemInstance* ItemObj = LootContainerComponent->GetItemInstance(TargetItem))
        {
            ItemObj->bIsExamined = true;
            ItemObj->OnItemModified.Broadcast();
            // UI 갱신
            LootContainerComponent->OnInventoryChanged.Broadcast();
        }
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
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(SearchPhaseTimer);
        World->GetTimerManager().ClearTimer(ExamineTimer);
    }
    ItemsToExamine.Empty();

    CurrentScore = 0;
    RemainingTime = TotalTimeLimit;
    CurrentHealth = MaxHealth;
    MapManagerComponent->InitializeMap();
    CurrentPlayerCoord = MapManagerComponent->SpawnPoint;
    if (MainUI)
    {
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
    if (MainUI && MainUI->MinimapUI) MainUI->MinimapUI->ResetMovement();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(GameTimerHandle);
        World->GetTimerManager().ClearTimer(SearchPhaseTimer);
        World->GetTimerManager().ClearTimer(ExamineTimer);
    }
    ItemsToExamine.Empty();
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
    if (MainUI && MainUI->MinimapUI) MainUI->MinimapUI->ResetMovement();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(GameTimerHandle);
        World->GetTimerManager().ClearTimer(SearchPhaseTimer);
        World->GetTimerManager().ClearTimer(ExamineTimer);
    }
    ItemsToExamine.Empty();
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
