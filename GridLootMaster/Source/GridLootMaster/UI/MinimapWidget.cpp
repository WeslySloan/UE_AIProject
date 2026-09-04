#include "MinimapWidget.h"
#include "MinimapTileWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "../GridGameMode.h"
#include "../CombatComponent.h"
#include "../EnemyManagerComponent.h"
#include "Input/Reply.h"

void UMinimapWidget::NativeConstruct()
{
    Super::NativeConstruct();
}

void UMinimapWidget::InitMinimap(UMapManagerComponent* InMapManager, bool bInCompactMode)
{
    MapManager = InMapManager;
    bCompactMode = bInCompactMode;

    // 화면에 이미 붙은 위젯은 WidgetTree를 교체하지 않고 기존 Slate 위젯을 재사용합니다.
    // 레이드 재시작 때 새 트리를 할당하면 화면과 코드상의 버튼/타일이 분리될 수 있습니다.
    if (MapGridPanel && MapManager && TileWidgets.Num() == MapManager->MapGrid.Num())
    {
        if (UMinimapTileWidget** OldTile = TileWidgets.Find(CurrentPlayerCoord))
        {
            (*OldTile)->SetHasPlayer(false);
        }

        for (const FTileData& TileData : MapManager->MapGrid)
        {
            if (UMinimapTileWidget** TileWidget = TileWidgets.Find(TileData.Coordinate))
            {
                (*TileWidget)->RefreshTileData(TileData);
                (*TileWidget)->SetIsPath(false);
            }
        }

        CurrentPlayerCoord = MapManager->SpawnPoint;
        CurrentTargetCoord = CurrentPlayerCoord;
        CurrentPath.Empty();
        CurrentMoveProgress = 0;

        if (AdvanceButton)
        {
            AdvanceButton->SetVisibility(ESlateVisibility::Visible);
            AdvanceButton->SetIsEnabled(false);
        }
        UpdateAdvanceButtonText();
        if (UMinimapTileWidget** StartTile = TileWidgets.Find(CurrentPlayerCoord))
        {
            (*StartTile)->SetHasPlayer(true);
        }
        PublishMovementState();
        return;
    }

    CurrentPlayerCoord = MapManager ? MapManager->SpawnPoint : FIntPoint(0, 0); // 시작 위치
    CurrentTargetCoord = CurrentPlayerCoord;
    CurrentPath.Empty();
    CurrentMoveProgress = 0;

    if (!WidgetTree)
    {
        WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
    }

    if (!MapManager) return;

    UVerticalBox* RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    WidgetTree->RootWidget = RootBox;

    // 맵 렌더링을 위한 패널
    MapGridPanel = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
    MapGridPanel->SetSlotPadding(FMargin(1.0f));
    MapGridPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    
    if (UVerticalBoxSlot* MapSlot = Cast<UVerticalBoxSlot>(RootBox->AddChild(MapGridPanel)))
    {
        MapSlot->SetHorizontalAlignment(HAlign_Center);
        MapSlot->SetVerticalAlignment(VAlign_Center);
        MapSlot->SetPadding(FMargin(bCompactMode ? 2.0f : 20.0f));
    }

    // 전진 버튼 생성
    AdvanceButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    AdvanceButton->OnClicked.AddDynamic(this, &UMinimapWidget::OnAdvanceClicked);
    AdvanceButton->SetVisibility(ESlateVisibility::Visible);
    AdvanceButton->SetIsEnabled(false);

    AdvanceText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    AdvanceText->SetText(FText::FromString(TEXT("전진 (0/3)")));
    AdvanceButton->AddChild(AdvanceText);

    if (UVerticalBoxSlot* BtnSlot = Cast<UVerticalBoxSlot>(RootBox->AddChild(AdvanceButton)))
    {
        BtnSlot->SetHorizontalAlignment(HAlign_Center);
        BtnSlot->SetPadding(FMargin(bCompactMode ? 2.0f : 10.0f));
    }

    // 타일 위젯 생성
    TileWidgets.Empty();
    for (const FTileData& TileData : MapManager->MapGrid)
    {
        UMinimapTileWidget* TileWidget = WidgetTree->ConstructWidget<UMinimapTileWidget>(UMinimapTileWidget::StaticClass());
        TileWidget->InitTile(TileData, this, bCompactMode ? 24.0f : 64.0f);

        UUniformGridSlot* GridSlot = MapGridPanel->AddChildToUniformGrid(TileWidget, TileData.Coordinate.Y, TileData.Coordinate.X);
        GridSlot->SetHorizontalAlignment(HAlign_Fill);
        GridSlot->SetVerticalAlignment(VAlign_Fill);

        TileWidgets.Add(TileData.Coordinate, TileWidget);
    }

    // 시작 위치 표시만 수행하고, 실제 이동 이벤트는 발생시키지 않습니다.
    if (UMinimapTileWidget** StartTile = TileWidgets.Find(CurrentPlayerCoord))
    {
        (*StartTile)->SetHasPlayer(true);
    }
}

FReply UMinimapWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();
        for (const TPair<FIntPoint, UMinimapTileWidget*>& Pair : TileWidgets)
        {
            if (Pair.Value && Pair.Value->GetCachedGeometry().IsUnderLocation(ScreenPosition))
            {
                HandleTileClicked(Pair.Key);
                return FReply::Handled();
            }
        }
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UMinimapWidget::SetMovementStateMirror(UMinimapWidget* InMirror)
{
    MovementStateMirror = InMirror;
    if (MovementStateMirror)
    {
        MovementStateMirror->MovementStateSource = this;
        MovementStateMirror->ApplySharedMovementState(
            CurrentPlayerCoord, CurrentTargetCoord, CurrentPath, CurrentMoveProgress);
    }
}

void UMinimapWidget::ApplySharedMovementState(FIntPoint InPlayerCoord, FIntPoint InTargetCoord,
    const TArray<FIntPoint>& InPath, int32 InMoveProgress)
{
    if (UMinimapTileWidget** OldTile = TileWidgets.Find(CurrentPlayerCoord))
    {
        (*OldTile)->SetHasPlayer(false);
    }

    CurrentPlayerCoord = InPlayerCoord;
    CurrentTargetCoord = InTargetCoord;
    CurrentPath = InPath;
    CurrentMoveProgress = InMoveProgress;

    if (UMinimapTileWidget** NewTile = TileWidgets.Find(CurrentPlayerCoord))
    {
        (*NewTile)->SetHasPlayer(true);
    }

    if (AdvanceButton)
    {
        AdvanceButton->SetVisibility(ESlateVisibility::Visible);
        bool bCanAdvance = false;
        if (CurrentPath.Num() > 0)
        {
            if (UWorld* World = GetWorld())
            {
                if (AGridGameMode* GM = Cast<AGridGameMode>(World->GetAuthGameMode()))
                {
                    bCanAdvance = GM->RaidState == ERaidState::InRaid &&
                        (!GM->CombatComponent || !GM->CombatComponent->bHasActiveEnemy);
                }
            }
        }
        AdvanceButton->SetIsEnabled(bCanAdvance);
    }
    UpdateAdvanceButtonText();
    UpdatePathHighlight();
}

void UMinimapWidget::PublishMovementState()
{
    if (MovementStateMirror)
    {
        MovementStateMirror->ApplySharedMovementState(
            CurrentPlayerCoord, CurrentTargetCoord, CurrentPath, CurrentMoveProgress);
    }
}

void UMinimapWidget::ResetMovement()
{
    if (MovementStateSource)
    {
        MovementStateSource->ResetMovement();
        return;
    }

    if (UMinimapTileWidget** OldTile = TileWidgets.Find(CurrentPlayerCoord))
    {
        (*OldTile)->SetHasPlayer(false);
    }

    CurrentPlayerCoord = MapManager ? MapManager->SpawnPoint : FIntPoint(0, 0);
    CurrentTargetCoord = CurrentPlayerCoord;
    CurrentPath.Empty();
    CurrentMoveProgress = 0;

    if (AdvanceButton)
    {
        AdvanceButton->SetVisibility(ESlateVisibility::Visible);
        AdvanceButton->SetIsEnabled(false);
    }
    UpdateAdvanceButtonText();
    UpdatePathHighlight();

    if (UMinimapTileWidget** StartTile = TileWidgets.Find(CurrentPlayerCoord))
    {
        (*StartTile)->SetHasPlayer(true);
    }
    PublishMovementState();
}

void UMinimapWidget::HandleTileClicked(FIntPoint ClickedCoord)
{
    if (MovementStateSource)
    {
        MovementStateSource->HandleTileClicked(ClickedCoord);
        return;
    }

    AGridGameMode* GM = nullptr;
    if (UWorld* World = GetWorld())
    {
        GM = Cast<AGridGameMode>(World->GetAuthGameMode());
    }
    if (!GM || GM->RaidState != ERaidState::InRaid ||
        (GM->CombatComponent && GM->CombatComponent->bHasActiveEnemy) ||
        GM->PlayerPosture == EPlayerRaidPosture::Ambushing ||
        (GM->EnemyManagerComponent && GM->EnemyManagerComponent->HasActiveAmbushReaction()))
    {
        NotifyMovementMessage(!GM
            ? TEXT("레이드 상태를 확인할 수 없습니다.")
            : GM->RaidState != ERaidState::InRaid
                ? TEXT("레이드 중에만 이동할 수 있습니다.")
                : TEXT("전투 중에는 이동할 수 없습니다."));
        return;
    }

    if (CurrentMoveProgress > 0)
    {
        // 이동 도중에는 현재 경로를 끝까지 유지합니다.
        NotifyMovementMessage(TEXT("현재 이동을 완료한 뒤 목적지를 변경할 수 있습니다."));
        return;
    }

    if (!MapManager || !AdvanceButton)
    {
        NotifyMovementMessage(TEXT("맵이 아직 준비되지 않았습니다."));
        return;
    }

    // A* 길찾기로 경로 계산
    CurrentPath = MapManager->FindPath(CurrentPlayerCoord, ClickedCoord);
    
    if (CurrentPath.Num() > 0)
    {
        CurrentTargetCoord = ClickedCoord;
        CurrentMoveProgress = 0;
        AdvanceButton->SetVisibility(ESlateVisibility::Visible);
        AdvanceButton->SetIsEnabled(true);
        UpdateAdvanceButtonText();
        NotifyMovementMessage(FString::Printf(TEXT("목적지 선택: (%d, %d)"), ClickedCoord.X, ClickedCoord.Y));
    }
    else
    {
        AdvanceButton->SetVisibility(ESlateVisibility::Visible);
        AdvanceButton->SetIsEnabled(false);
        NotifyMovementMessage(TEXT("해당 타일로 이동할 수 없습니다."));
    }

    UpdatePathHighlight();
    PublishMovementState();
}

void UMinimapWidget::NotifyMovementMessage(const FString& Message)
{
    if (!Message.IsEmpty())
    {
        OnMovementMessage.Broadcast(Message);
    }
}

void UMinimapWidget::OnAdvanceClicked()
{
    AGridGameMode* GM = nullptr;
    if (UWorld* World = GetWorld())
    {
        GM = Cast<AGridGameMode>(World->GetAuthGameMode());
    }
    if (!GM || GM->RaidState != ERaidState::InRaid ||
        (GM->CombatComponent && GM->CombatComponent->bHasActiveEnemy) ||
        GM->PlayerPosture == EPlayerRaidPosture::Ambushing ||
        (GM->EnemyManagerComponent && GM->EnemyManagerComponent->HasActiveAmbushReaction()))
    {
        return;
    }

    if (MovementStateSource)
    {
        MovementStateSource->OnAdvanceClicked();
        return;
    }

    if (CurrentPath.Num() == 0) return;

    bool bWorldTickCommitted = false;
    CurrentMoveProgress++;
    bWorldTickCommitted = true;
    if (CurrentMoveProgress >= 3)
    {
        // 3턴 경과 시 다음 칸으로 이동
        CurrentMoveProgress = 0;
        if (!MovePlayerTo(CurrentPath[0])) // Path의 첫번째가 다음 칸
        {
            bWorldTickCommitted = false;
            CurrentPath.Empty();
            CurrentTargetCoord = CurrentPlayerCoord;
            if (AdvanceButton)
            {
                AdvanceButton->SetVisibility(ESlateVisibility::Visible);
                AdvanceButton->SetIsEnabled(false);
            }
            UpdatePathHighlight();
            UpdateAdvanceButtonText();
            NotifyMovementMessage(TEXT("이동 경로가 막혀 목적지를 다시 선택해야 합니다."));
            PublishMovementState();
            return;
        }
        CurrentPath.RemoveAt(0);

        if (CurrentPath.Num() == 0)
        {
            // 최종 도착
            AdvanceButton->SetVisibility(ESlateVisibility::Visible);
            AdvanceButton->SetIsEnabled(false);
        }
        UpdatePathHighlight();
    }
    
    UpdateAdvanceButtonText();
    PublishMovementState();

    if (bWorldTickCommitted)
    {
        GM->AdvanceRaidWorldTick();
    }
}

void UMinimapWidget::UpdateAdvanceButtonText()
{
    if (AdvanceText)
    {
        const FString TextStr = CurrentPath.Num() == 0
            ? TEXT("목적지 선택 후 전진")
            : FString::Printf(TEXT("전진 (%d/3)"), CurrentMoveProgress);
        AdvanceText->SetText(FText::FromString(TextStr));
    }
}

void UMinimapWidget::UpdatePathHighlight()
{
    // 모든 타일 하이라이트 초기화
    for (auto& Pair : TileWidgets)
    {
        Pair.Value->SetIsPath(false);
    }

    // 새로운 경로 하이라이트
    for (const FIntPoint& PathCoord : CurrentPath)
    {
        if (UMinimapTileWidget** FoundTile = TileWidgets.Find(PathCoord))
        {
            (*FoundTile)->SetIsPath(true);
        }
    }
}

bool UMinimapWidget::MovePlayerTo(FIntPoint NewCoord)
{
    if (!MapManager || !MapManager->CanMoveBetween(CurrentPlayerCoord, NewCoord))
    {
        return false;
    }

    // 기존 위치 플레이어 아이콘 숨김
    if (UMinimapTileWidget** OldTile = TileWidgets.Find(CurrentPlayerCoord))
    {
        (*OldTile)->SetHasPlayer(false);
    }

    CurrentPlayerCoord = NewCoord;

    // 새 위치 플레이어 아이콘 표시
    if (UMinimapTileWidget** NewTile = TileWidgets.Find(CurrentPlayerCoord))
    {
        (*NewTile)->SetHasPlayer(true);
    }

    OnPlayerMoved.Broadcast(CurrentPlayerCoord);
    PublishMovementState();
    return true;
}
