#include "GridGameMode.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GridInventoryComponent.h"
#include "UI/MainGameUI.h"
#include "Kismet/GameplayStatics.h"

AGridGameMode::AGridGameMode()
{
    InventoryComponent = CreateDefaultSubobject<UGridInventoryComponent>(TEXT("InventoryComponent"));
    LootContainerComponent = CreateDefaultSubobject<UGridInventoryComponent>(TEXT("LootContainerComponent"));
}

void AGridGameMode::BeginPlay()
{
    Super::BeginPlay();

    InventoryComponent->InitializeGrid(6, 8);
    // 컨테이너는 4x4 크기로 임시 생성
    LootContainerComponent->InitializeGrid(4, 4); 

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

    // 더미 아이템 데이터 목록
    static const FName Names[] = { TEXT("Sword"), TEXT("Shield"), TEXT("Potion"), TEXT("Axe"), TEXT("Gem"), TEXT("Diamond") };
    static const FIntPoint Sizes[] = { FIntPoint(3, 1), FIntPoint(2, 2), FIntPoint(1, 1), FIntPoint(3, 2), FIntPoint(1, 1), FIntPoint(1, 1) };
    static const EItemRarity Rarities[] = { 
        EItemRarity::Common,    // Sword
        EItemRarity::Uncommon,  // Shield 
        EItemRarity::Common,    // Potion
        EItemRarity::Legendary, // Axe
        EItemRarity::Rare,      // Gem
        EItemRarity::Mythic     // Diamond
    };
    
    static int32 SpawnCounter = 0;

    // 4x4 상자 안에 3개의 아이템을 랜덤으로 욱여넣기 시도
    int32 Attempts = 10;
    int32 AddedItems = 0;

    while(Attempts > 0 && AddedItems < 3)
    {
        Attempts--;
        int32 RandIdx = FMath::RandRange(0, 5);
        int32 RandX = FMath::RandRange(0, 3);
        int32 RandY = FMath::RandRange(0, 3);
        
        FString UniqueName = FString::Printf(TEXT("%s_%d"), *Names[RandIdx].ToString(), SpawnCounter);
        FName ItemID = FName(*UniqueName);
        
        // bIsExamined = false 로 스폰! (실루엣 상태)
        if (LootContainerComponent->AddItem(ItemID, RandX, RandY, Sizes[RandIdx].X, Sizes[RandIdx].Y, Rarities[RandIdx], false))
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
        // 강제로 해당 아이템의 식별 상태를 true로 변경
        bool* bExaminedRef = LootContainerComponent->ItemExaminedMap.Find(TargetItem);
        if (bExaminedRef)
        {
            *bExaminedRef = true;
            // UI를 다시 그리도록 브로드캐스트
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
