#include "GridGameMode.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GridInventoryComponent.h"
#include "UI/MainGameUI.h"
#include "Kismet/GameplayStatics.h"
#include "ItemInstance.h"
#include "EquipmentComponent.h"
#include "Map/MapManagerComponent.h"

AGridGameMode::AGridGameMode()
{
    InventoryComponent = CreateDefaultSubobject<UGridInventoryComponent>(TEXT("InventoryComponent"));
    LootContainerComponent = CreateDefaultSubobject<UGridInventoryComponent>(TEXT("LootContainerComponent"));
    SafeBoxComponent = CreateDefaultSubobject<UGridInventoryComponent>(TEXT("SafeBoxComponent"));
    RigComponent = CreateDefaultSubobject<UGridInventoryComponent>(TEXT("RigComponent"));
    PocketComponent = CreateDefaultSubobject<UGridInventoryComponent>(TEXT("PocketComponent"));
    EquipmentComponent = CreateDefaultSubobject<UEquipmentComponent>(TEXT("EquipmentComponent"));
    MapManagerComponent = CreateDefaultSubobject<UMapManagerComponent>(TEXT("MapManagerComponent"));
}

void AGridGameMode::BeginPlay()
{
    Super::BeginPlay();

    InventoryComponent->InitializeGrid(5, 5); // 백팩 사이즈 (예: 5x5)
    LootContainerComponent->InitializeGrid(6, 6); 
    SafeBoxComponent->InitializeGrid(2, 2);
    RigComponent->InitializeGrid(4, 3); // Rig 사이즈 (예: 4x3)
    PocketComponent->InitializeGrid(5, 1); // Pocket 사이즈 (예: 가로 5, 세로 1)

    // 기본 가방 아이템 장착 (6x8 인벤토리 역할)
    UItemInstance* DefBackpack = NewObject<UItemInstance>(this);
    DefBackpack->InstanceID = TEXT("Item_DefBackpack");
    DefBackpack->TemplateID = TEXT("DefaultBackpack");
    DefBackpack->Category = EItemCategory::Backpack;
    DefBackpack->BaseSize = FIntPoint(2, 2); // 슬롯 크기 2x2
    DefBackpack->CurrentStack = 1;
    DefBackpack->MaxStack = 1;
    EquipmentComponent->EquipItem(TEXT("Backpack"), DefBackpack);

    // 기본 안전 금고 장착 (2x2)
    UItemInstance* DefSafeBox = NewObject<UItemInstance>(this);
    DefSafeBox->InstanceID = TEXT("Item_DefSafeBox");
    DefSafeBox->TemplateID = TEXT("DefaultSafeBox");
    DefSafeBox->Category = EItemCategory::SafeBox;
    DefSafeBox->BaseSize = FIntPoint(2, 2); // 슬롯 크기 2x2
    DefSafeBox->CurrentStack = 1;
    DefSafeBox->MaxStack = 1;
    EquipmentComponent->EquipItem(TEXT("SafeBox"), DefSafeBox);

    // 기본 체스트 리그 장착 (4x3)
    UItemInstance* DefRig = NewObject<UItemInstance>(this);
    DefRig->InstanceID = TEXT("Item_DefRig");
    DefRig->TemplateID = TEXT("DefaultRig");
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
        Item->InstanceID = ID;
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
    TotalTimeLimit = 60.0f;
    RemainingTime = TotalTimeLimit; // 60초 게임
    QuotaScore = 1000;

    // C++에서 UI 자동 생성 및 화면 표시
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        MainUI = CreateWidget<UMainGameUI>(PC, UMainGameUI::StaticClass());
        if (MainUI)
        {
            MainUI->AddToViewport();
            MainUI->UpdateScore(CurrentScore);
            MainUI->UpdateTimer(RemainingTime);
            
            // PlayerController 설정
            PC->SetShowMouseCursor(true);
            FInputModeUIOnly InputMode;
            InputMode.SetWidgetToFocus(MainUI->TakeWidget());
            PC->SetInputMode(InputMode);
        }
    }

    GetWorldTimerManager().SetTimer(GameTimerHandle, this, &AGridGameMode::GameTimerUpdate, 1.0f, true);
}

void AGridGameMode::StartContainerSearch()
{
    if (!LootContainerComponent || !MainUI) return;

    // 기존 타이머 취소
    GetWorldTimerManager().ClearTimer(SearchPhaseTimer);
    GetWorldTimerManager().ClearTimer(ExamineTimer);
    ItemsToExamine.Empty();

    // 기존 템 지우기
    LootContainerComponent->ClearInventory();
    
    // (임시 시각 피드백) UI에 "Searching..." 을 띄우는 건 생략하고, 그냥 빈 그리드로 1초 대기
    // 실제 게임에서는 Progress Bar 위젯을 노출하는 처리를 여기에 추가

    // 1초 후 OnSearchPhaseComplete 호출
    GetWorldTimerManager().SetTimer(SearchPhaseTimer, this, &AGridGameMode::OnSearchPhaseComplete, 1.0f, false);
}

void AGridGameMode::OnSearchPhaseComplete()
{
    if (!LootContainerComponent) return;

    if (!ItemDataTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("ItemDataTable is not assigned in GridGameMode!"));
        return;
    }

    TArray<FItemData*> AllItems;
    ItemDataTable->GetAllRows<FItemData>(TEXT("Loot"), AllItems);
    if (AllItems.Num() == 0) return;

    int32 TotalWeight = 0;
    for (FItemData* ItemData : AllItems)
    {
        if (ItemData) TotalWeight += ItemData->DropWeight;
    }

    static int32 SpawnCounter = 0;

    // 6x6 상자 안에 5개의 아이템을 랜덤으로 욱여넣기 시도
    int32 Attempts = 20;
    int32 AddedItems = 0;

    while(Attempts > 0 && AddedItems < 5)
    {
        Attempts--;
        int32 RandX = FMath::RandRange(0, 5); 
        int32 RandY = FMath::RandRange(0, 5);
        
        int32 RandWeight = FMath::RandRange(0, TotalWeight);
        FItemData* SelectedItem = nullptr;
        for (FItemData* ItemData : AllItems)
        {
            if (ItemData)
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
        GetWorldTimerManager().SetTimer(ExamineTimer, this, &AGridGameMode::ProcessNextExamine, 0.5f, true);
    }
}

void AGridGameMode::ProcessNextExamine()
{
    if (ItemsToExamine.Num() == 0)
    {
        GetWorldTimerManager().ClearTimer(ExamineTimer);
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
            // UI 갱신
            LootContainerComponent->OnInventoryChanged.Broadcast();
        }
    }
}

void AGridGameMode::GameTimerUpdate()
{
    RemainingTime -= 1.0f;
    
    if (MainUI)
    {
        MainUI->UpdateTimer(RemainingTime);
    }
    
    if (RemainingTime <= 0.0f)
    {
        RemainingTime = 0.0f;
        GetWorld()->GetTimerManager().ClearTimer(GameTimerHandle);
        CheckWinCondition();
    }
}

void AGridGameMode::AddScore(int32 Amount)
{
    CurrentScore += Amount;
    
    if (MainUI)
    {
        MainUI->UpdateScore(CurrentScore);
    }
    
    CheckWinCondition();
}

void AGridGameMode::CheckWinCondition()
{
    // QuotaScore 대신 하드코딩 1000
    if (CurrentScore >= 1000)
    {
        if (MainUI) MainUI->ShowGameResult(true);
        GetWorld()->GetTimerManager().ClearTimer(GameTimerHandle);
    }
    else if (RemainingTime <= 0.0f)
    {
        if (MainUI) MainUI->ShowGameResult(false);
    }
}
