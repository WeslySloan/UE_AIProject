#include "GridGameMode.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GridInventoryComponent.h"
#include "UI/MainGameUI.h"
#include "Kismet/GameplayStatics.h"

AGridGameMode::AGridGameMode()
{
    TotalTimeLimit = 60.0f;
    RemainingTime = TotalTimeLimit;
    CurrentScore = 0;
    QuotaScore = 1000;

    InventoryComponent = CreateDefaultSubobject<UGridInventoryComponent>(TEXT("GridInventoryComp"));
}

void AGridGameMode::BeginPlay()
{
    Super::BeginPlay();

    RemainingTime = TotalTimeLimit;
    CurrentScore = 0;

    // 게임 루프 타이머
    GetWorld()->GetTimerManager().SetTimer(GameTimerHandle, this, &AGridGameMode::GameTimerUpdate, 1.0f, true);
    
    // 아이템 스폰 타이머
    GetWorld()->GetTimerManager().SetTimer(SpawnTimerHandle, this, &AGridGameMode::SpawnRandomItem, SpawnDelay, true);

    // C++에서 UI 자동 생성 및 화면 표시
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        MainUI = CreateWidget<UMainGameUI>(PC, UMainGameUI::StaticClass());
        if (MainUI)
        {
            MainUI->AddToViewport();
            
            // 입력 모드를 UI 전용으로 변경
            FInputModeUIOnly InputMode;
            InputMode.SetWidgetToFocus(MainUI->TakeWidget());
            PC->SetInputMode(InputMode);
            PC->bShowMouseCursor = true;
        }
    }
}

void AGridGameMode::SpawnRandomItem()
{
    if (!MainUI) return;

    // 더미 아이템 데이터 목록
    TArray<FName> Names = { TEXT("Sword"), TEXT("Shield"), TEXT("Potion"), TEXT("Axe"), TEXT("Gem") };
    // 기본적으로 최대한 가로로 표시되도록 세로로 긴 아이템들을 가로로 눕혀서 스폰합니다.
    TArray<FIntPoint> Sizes = { FIntPoint(3,1), FIntPoint(2,2), FIntPoint(1,1), FIntPoint(3,2), FIntPoint(2,1) };
    
    int32 RandIdx = FMath::RandRange(0, Names.Num() - 1);
    
    // 고유 ID 생성 (예: Sword_1234)
    static int32 SpawnCounter = 0;
    FString UniqueName = FString::Printf(TEXT("%s_%d"), *Names[RandIdx].ToString(), SpawnCounter++);
    
    MainUI->AddItemToLootPool(FName(*UniqueName), Sizes[RandIdx], (Sizes[RandIdx].X * Sizes[RandIdx].Y) * 10);
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
        GetWorld()->GetTimerManager().ClearTimer(SpawnTimerHandle);
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
    if (CurrentScore >= QuotaScore)
    {
        if (MainUI) MainUI->ShowGameResult(true);
        GetWorld()->GetTimerManager().ClearTimer(GameTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(SpawnTimerHandle);
    }
    else if (RemainingTime <= 0.0f)
    {
        if (MainUI) MainUI->ShowGameResult(false);
    }
}
