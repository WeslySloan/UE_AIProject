#include "MainGameUI.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/WidgetSwitcher.h"
#include "MinimapWidget.h"
#include "GridBoardWidget.h"
#include "DraggableItemWidget.h"
#include "EquipmentSlotWidget.h"
#include "../GridGameMode.h"
#include "../GridInventoryComponent.h"
#include "../EquipmentComponent.h"
#include "../CombatComponent.h"
#include "../Map/MapManagerComponent.h"
#include "../ItemInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

bool UMainGameUI::Initialize()
{
    if (!Super::Initialize()) return false;

    SetIsFocusable(true);
    ActiveWeaponSlot = TEXT("Primary1");

    if (WidgetTree && !WidgetTree->RootWidget)
    {
        // 1. Root: Canvas Panel
        UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
        WidgetTree->RootWidget = RootCanvas;

        // 2. 전체 레이아웃 (가로 3분할)
        UHorizontalBox* HBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MainLayout"));
        UCanvasPanelSlot* HBoxSlot = RootCanvas->AddChildToCanvas(HBox);
        HBoxSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
        HBoxSlot->SetOffsets(FMargin(50.0f, 50.0f, 50.0f, 50.0f));

        AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));

        auto CreateEquipSlotEx = [&](UEquipmentSlotWidget*& OutSlot, FName SlotID, EItemCategory Category, const FString& Name, float Width, float Height) -> USizeBox*
        {
            OutSlot = WidgetTree->ConstructWidget<UEquipmentSlotWidget>(UEquipmentSlotWidget::StaticClass());
            OutSlot->InitSlot(SlotID, Category, Name);
            
            USizeBox* SBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
            SBox->SetWidthOverride(Width);
            SBox->SetHeightOverride(Height);
            SBox->AddChild(OutSlot);

            if (GM && GM->EquipmentComponent)
            {
                GM->EquipmentComponent->OnEquipmentChanged.AddDynamic(OutSlot, &UEquipmentSlotWidget::OnEquipmentChanged);
                OutSlot->OnEquipmentChanged();
            }
            return SBox;
        };

        auto AddToVertical = [](UVerticalBox* Parent, UWidget* Child, float PaddingBottom = 5.0f) {
            UVerticalBoxSlot* VSlot = Parent->AddChildToVerticalBox(Child);
            VSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
            VSlot->SetHorizontalAlignment(HAlign_Left);
            VSlot->SetPadding(FMargin(0, 0, 0, PaddingBottom));
        };

        auto AddToHorizontal = [](UHorizontalBox* Parent, UWidget* Child, float PaddingRight = 10.0f) {
            UHorizontalBoxSlot* HSlot = Parent->AddChildToHorizontalBox(Child);
            HSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
            HSlot->SetPadding(FMargin(0, 0, PaddingRight, 0));
        };

        // === 1. 왼쪽 패널 (캐릭터 장비 슬롯) ===
        LeftPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LeftPanel"));
        UHorizontalBoxSlot* LeftPanelSlot = HBox->AddChildToHorizontalBox(LeftPanel);
        LeftPanelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        LeftPanelSlot->SetPadding(FMargin(0, 0, 40, 0));

        // --- 왼쪽 패널: 헬멧, 방어구, 무기 ---
        AddToVertical(LeftPanel, CreateEquipSlotEx(HelmetSlot, TEXT("Helmet"), EItemCategory::Helmet, TEXT("Helmet"), 192.0f, 192.0f));
        AddToVertical(LeftPanel, CreateEquipSlotEx(ArmorSlot, TEXT("Armor"), EItemCategory::Armor, TEXT("Armor"), 192.0f, 320.0f));
        AddToVertical(LeftPanel, CreateEquipSlotEx(WeaponSlot1, TEXT("Primary1"), EItemCategory::Weapon, TEXT("Primary Weapon 1"), 320.0f, 128.0f));
        AddToVertical(LeftPanel, CreateEquipSlotEx(WeaponSlot2, TEXT("Primary2"), EItemCategory::Weapon, TEXT("Primary Weapon 2"), 320.0f, 128.0f));

        // === 2. 중앙 패널 (Rig, Pocket, Backpack, SafeBox) ===
        UVerticalBox* MiddlePanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MiddlePanel"));
        UHorizontalBoxSlot* MiddlePanelSlot = HBox->AddChildToHorizontalBox(MiddlePanel);
        MiddlePanelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

        // 1. Rig Row
        UHorizontalBox* RigRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        AddToVertical(MiddlePanel, RigRow, 20.0f);
        AddToHorizontal(RigRow, CreateEquipSlotEx(RigSlot, TEXT("Rig"), EItemCategory::Rig, TEXT("Chest Rig"), 128.0f, 64.0f)); // 2x1 장비 슬롯
        RigBoard = WidgetTree->ConstructWidget<UGridBoardWidget>(UGridBoardWidget::StaticClass(), TEXT("RigBoard"));
        AddToHorizontal(RigRow, RigBoard);

        // 2. Pocket Row (No Slot)
        UHorizontalBox* PocketRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        AddToVertical(MiddlePanel, PocketRow, 20.0f);
        // Spacer for alignment
        USizeBox* Spacer = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        Spacer->SetWidthOverride(128.0f); // Rig 슬롯 위치 맞춤
        AddToHorizontal(PocketRow, Spacer);
        PocketBoard = WidgetTree->ConstructWidget<UGridBoardWidget>(UGridBoardWidget::StaticClass(), TEXT("PocketBoard"));
        AddToHorizontal(PocketRow, PocketBoard);

        // 3. Backpack Row
        UHorizontalBox* BackpackRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        AddToVertical(MiddlePanel, BackpackRow, 20.0f);
        AddToHorizontal(BackpackRow, CreateEquipSlotEx(BackpackSlot, TEXT("Backpack"), EItemCategory::Backpack, TEXT("Backpack"), 128.0f, 192.0f));
        GridBoard = WidgetTree->ConstructWidget<UGridBoardWidget>(UGridBoardWidget::StaticClass(), TEXT("GridBoard"));
        AddToHorizontal(BackpackRow, GridBoard);

        // 4. SafeBox Row
        UHorizontalBox* SafeBoxRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        AddToVertical(MiddlePanel, SafeBoxRow, 20.0f);
        AddToHorizontal(SafeBoxRow, CreateEquipSlotEx(SafeBoxSlot, TEXT("SafeBox"), EItemCategory::SafeBox, TEXT("SafeBox"), 128.0f, 128.0f));
        SafeBoxBoard = WidgetTree->ConstructWidget<UGridBoardWidget>(UGridBoardWidget::StaticClass(), TEXT("SafeBoxBoard"));
        AddToHorizontal(SafeBoxRow, SafeBoxBoard);

        // Toggle 버튼 (상단 중앙 쯤 표시 배치)
        ToggleModeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ToggleModeButton"));
        UTextBlock* ToggleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        ToggleText->SetText(FText::FromString(TEXT("Toggle Map/Inv")));
        
        // 폰트 크기 축소 및 검은색 텍스트로 가독성 확보
        FSlateFontInfo ToggleFont = ToggleText->GetFont();
        ToggleFont.Size = 16; 
        ToggleText->SetFont(ToggleFont);
        ToggleText->SetColorAndOpacity(FLinearColor::Black);
        
        ToggleModeButton->AddChild(ToggleText);
        ToggleModeButton->OnClicked.AddDynamic(this, &UMainGameUI::OnToggleModeClicked);
        
        UCanvasPanelSlot* ToggleSlot = RootCanvas->AddChildToCanvas(ToggleModeButton);
        ToggleSlot->SetAnchors(FAnchors(0.5f, 0.0f));
        ToggleSlot->SetAlignment(FVector2D(0.5f, 0.0f));
        ToggleSlot->SetPosition(FVector2D(0.0f, 15.0f));
        ToggleSlot->SetSize(FVector2D(200.0f, 40.0f)); // 버튼 넉넉하게 고정 크기 할당

        // 상단 이벤트 알림 영역
        EventNotificationBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("EventNotificationBorder"));
        EventNotificationBorder->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.04f, 0.85f));
        EventNotificationText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EventNotificationText"));
        EventNotificationText->SetColorAndOpacity(FLinearColor::White);
        FSlateFontInfo NotificationFont = EventNotificationText->GetFont();
        NotificationFont.Size = 20;
        EventNotificationText->SetFont(NotificationFont);
        EventNotificationText->SetJustification(ETextJustify::Center);
        EventNotificationBorder->AddChild(EventNotificationText);
        UCanvasPanelSlot* NotificationSlot = RootCanvas->AddChildToCanvas(EventNotificationBorder);
        NotificationSlot->SetAnchors(FAnchors(0.5f, 0.0f));
        NotificationSlot->SetAlignment(FVector2D(0.5f, 0.0f));
        NotificationSlot->SetPosition(FVector2D(0.0f, 65.0f));
        NotificationSlot->SetSize(FVector2D(700.0f, 48.0f));
        NotificationSlot->SetZOrder(15);
        EventNotificationBorder->SetVisibility(ESlateVisibility::Hidden);

        // === 3. 오른쪽 패널 (상태바 + 루팅 컨테이너 + 버튼) ===
        RightPanelSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("RightPanelSwitcher"));
        UHorizontalBoxSlot* RightPanelSlot = HBox->AddChildToHorizontalBox(RightPanelSwitcher);
        RightPanelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        RightPanelSlot->SetPadding(FMargin(40, 0, 0, 0));

        UVerticalBox* RightPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RightPanel"));
        RightPanelSwitcher->AddChild(RightPanel); // Index 0: Inventory View

        MinimapUI = WidgetTree->ConstructWidget<UMinimapWidget>(UMinimapWidget::StaticClass(), TEXT("MinimapUI"));
        RightPanelSwitcher->AddChild(MinimapUI); // Index 1: Minimap View

        CompactMinimapUI = WidgetTree->ConstructWidget<UMinimapWidget>(UMinimapWidget::StaticClass(), TEXT("CompactMinimapUI"));
        UCanvasPanelSlot* CompactMinimapSlot = RootCanvas->AddChildToCanvas(CompactMinimapUI);
        CompactMinimapSlot->SetAnchors(FAnchors(0.5f, 1.0f));
        CompactMinimapSlot->SetAlignment(FVector2D(0.5f, 1.0f));
        CompactMinimapSlot->SetPosition(FVector2D(0.0f, -15.0f));
        CompactMinimapSlot->SetSize(FVector2D(250.0f, 285.0f));
        CompactMinimapSlot->SetZOrder(20);
        CompactMinimapUI->SetVisibility(ESlateVisibility::Hidden);

        UVerticalBox* StashPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StashPanel"));
        RightPanelSwitcher->AddChild(StashPanel); // Index 2: Stash View

        UTextBlock* StashTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        StashTitle->SetText(FText::FromString(TEXT("STASH")));
        StashPanel->AddChildToVerticalBox(StashTitle);

        StashBoard = WidgetTree->ConstructWidget<UGridBoardWidget>(UGridBoardWidget::StaticClass(), TEXT("StashBoard"));
        UVerticalBoxSlot* StashBoardSlot = StashPanel->AddChildToVerticalBox(StashBoard);
        StashBoardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        StashBoardSlot->SetHorizontalAlignment(HAlign_Left);

        StartRaidBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("StartRaidButton"));
        UTextBlock* StartRaidText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        StartRaidText->SetText(FText::FromString(TEXT("START RAID")));
        StartRaidText->SetColorAndOpacity(FLinearColor::Black);
        StartRaidBtn->AddChild(StartRaidText);
        StartRaidBtn->OnClicked.AddDynamic(this, &UMainGameUI::OnStartRaidClicked);
        StashPanel->AddChildToVerticalBox(StartRaidBtn);

        TimerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TimerText"));
        TimerText->SetText(FText::FromString(TEXT("Time: 60s")));
        RightPanel->AddChildToVerticalBox(TimerText);

        ScoreText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ScoreText"));
        ScoreText->SetText(FText::FromString(TEXT("Score: 0 / 1000")));
        RightPanel->AddChildToVerticalBox(ScoreText);

        HealthText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HealthText"));
        HealthText->SetText(FText::FromString(TEXT("HP: 100 / 100")));
        RightPanel->AddChildToVerticalBox(HealthText);

        CombatText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CombatText"));
        CombatText->SetText(FText::FromString(TEXT("Enemy: None")));
        RightPanel->AddChildToVerticalBox(CombatText);

        UBorder* Spacer1 = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Spacer1->SetBrushColor(FLinearColor::Transparent);
        UVerticalBoxSlot* SpacerSlot1 = RightPanel->AddChildToVerticalBox(Spacer1);
        SpacerSlot1->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        SpacerSlot1->SetPadding(FMargin(0, 20));

        UTextBlock* PoolTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        PoolTitle->SetText(FText::FromString(TEXT("Loot Container")));
        RightPanel->AddChildToVerticalBox(PoolTitle);

        ContainerBoard = WidgetTree->ConstructWidget<UGridBoardWidget>(UGridBoardWidget::StaticClass(), TEXT("ContainerBoard"));
        UVerticalBoxSlot* PoolSlot = RightPanel->AddChildToVerticalBox(ContainerBoard);
        PoolSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        PoolSlot->SetHorizontalAlignment(HAlign_Left);

        SearchBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SearchBtn"));
        UTextBlock* SearchBtnText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        SearchBtnText->SetText(FText::FromString(TEXT("SEARCH CONTAINER")));
        SearchBtnText->SetColorAndOpacity(FLinearColor::Black);
        SearchBtn->AddChild(SearchBtnText);
        UVerticalBoxSlot* SearchSlot = RightPanel->AddChildToVerticalBox(SearchBtn);
        SearchSlot->SetPadding(FMargin(0, 10, 0, 0));
        SearchSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

        StashBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("StashButton"));
        UTextBlock* StashBtnText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        StashBtnText->SetText(FText::FromString(TEXT("OPEN STASH")));
        StashBtnText->SetColorAndOpacity(FLinearColor::Black);
        StashBtn->AddChild(StashBtnText);
        StashBtn->OnClicked.AddDynamic(this, &UMainGameUI::OnStashButtonClicked);
        UVerticalBoxSlot* StashBtnSlot = RightPanel->AddChildToVerticalBox(StashBtn);
        StashBtnSlot->SetPadding(FMargin(0, 10, 0, 0));
        StashBtnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

        ExtractBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ExtractButton"));
        UTextBlock* ExtractBtnText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        ExtractBtnText->SetText(FText::FromString(TEXT("EXTRACT RAID")));
        ExtractBtnText->SetColorAndOpacity(FLinearColor::Black);
        ExtractBtn->AddChild(ExtractBtnText);
        ExtractBtn->OnClicked.AddDynamic(this, &UMainGameUI::OnExtractButtonClicked);
        UVerticalBoxSlot* ExtractBtnSlot = RightPanel->AddChildToVerticalBox(ExtractBtn);
        ExtractBtnSlot->SetPadding(FMargin(0, 10, 0, 0));
        ExtractBtnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

        // Sell Button
        SellBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SellButton"));
        UTextBlock* SellBtnText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        SellBtnText->SetText(FText::FromString(TEXT("SELL BAG")));
        SellBtnText->SetColorAndOpacity(FLinearColor::Black);
        SellBtn->AddChild(SellBtnText);
        SellBtn->OnClicked.AddDynamic(this, &UMainGameUI::OnSellButtonClicked);
        UVerticalBoxSlot* SellSlot = RightPanel->AddChildToVerticalBox(SellBtn);
        SellSlot->SetPadding(FMargin(0, 20, 0, 0));
        SellSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

        // Sell All Button
        SellAllBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SellAllButton"));
        UTextBlock* SellAllBtnText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        SellAllBtnText->SetText(FText::FromString(TEXT("SELL ALL")));
        SellAllBtnText->SetColorAndOpacity(FLinearColor::Black);
        SellAllBtn->AddChild(SellAllBtnText);
        SellAllBtn->OnClicked.AddDynamic(this, &UMainGameUI::OnSellAllButtonClicked);
        UVerticalBoxSlot* SellAllSlot = RightPanel->AddChildToVerticalBox(SellAllBtn);
        SellAllSlot->SetPadding(FMargin(0, 10, 0, 0));
        SellAllSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

        // Bang (Shoot) Button
        BangBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("BangButton"));
        UTextBlock* BangBtnText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        BangBtnText->SetText(FText::FromString(TEXT("BANG!")));
        BangBtnText->SetColorAndOpacity(FLinearColor::Black);
        BangBtn->AddChild(BangBtnText);
        BangBtn->OnClicked.AddDynamic(this, &UMainGameUI::OnBangButtonClicked);
        UVerticalBoxSlot* BangSlot = RightPanel->AddChildToVerticalBox(BangBtn);
        BangSlot->SetPadding(FMargin(0, 10, 0, 0));
        BangSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

        // --- 인벤토리/장비 컴포넌트 연결 ---
        if (GM)
        {
            if (GM->InventoryComponent)
            {
                GridBoard->InventoryComponent = GM->InventoryComponent;
                GM->InventoryComponent->OnInventoryChanged.AddDynamic(GridBoard, &UGridBoardWidget::RefreshGridUI);
                GridBoard->RefreshGridUI();
            }

            if (GM->LootContainerComponent)
            {
                ContainerBoard->InventoryComponent = GM->LootContainerComponent;
                GM->LootContainerComponent->OnInventoryChanged.AddDynamic(ContainerBoard, &UGridBoardWidget::RefreshGridUI);
                ContainerBoard->RefreshGridUI();
            }

            if (GM->SafeBoxComponent)
            {
                SafeBoxBoard->InventoryComponent = GM->SafeBoxComponent;
                GM->SafeBoxComponent->OnInventoryChanged.AddDynamic(SafeBoxBoard, &UGridBoardWidget::RefreshGridUI);
                SafeBoxBoard->RefreshGridUI();
            }

            if (GM->RigComponent)
            {
                RigBoard->InventoryComponent = GM->RigComponent;
                GM->RigComponent->OnInventoryChanged.AddDynamic(RigBoard, &UGridBoardWidget::RefreshGridUI);
                RigBoard->RefreshGridUI();
            }

            if (GM->PocketComponent)
            {
                PocketBoard->InventoryComponent = GM->PocketComponent;
                GM->PocketComponent->OnInventoryChanged.AddDynamic(PocketBoard, &UGridBoardWidget::RefreshGridUI);
                PocketBoard->RefreshGridUI();
            }

            if (GM->StashComponent)
            {
                StashBoard->InventoryComponent = GM->StashComponent;
                GM->StashComponent->OnInventoryChanged.AddUniqueDynamic(StashBoard, &UGridBoardWidget::RefreshGridUI);
                GM->StashComponent->OnInventoryChanged.AddUniqueDynamic(this, &UMainGameUI::OnStashInventoryChanged);
                StashBoard->RefreshGridUI();
            }

            if (GM->MapManagerComponent) RefreshMinimaps(GM->MapManagerComponent);

            if (GM->CombatComponent)
            {
                GM->CombatComponent->OnCombatStateChanged.AddDynamic(this, &UMainGameUI::UpdateCombatUI);
                UpdateCombatUI();
            }

            GM->OnGameStateChanged.AddDynamic(this, &UMainGameUI::UpdateActionAvailability);
            GM->OnGameStateChanged.AddDynamic(this, &UMainGameUI::UpdateCombatUI);
            UpdateActionAvailability();

            SearchBtn->OnClicked.AddDynamic(GM, &AGridGameMode::StartContainerSearch);
        }
    }
    return true;
}

void UMainGameUI::RefreshMinimaps(UMapManagerComponent* InMapManager)
{
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (!InMapManager || !MinimapUI || !CompactMinimapUI) return;

    MinimapUI->InitMinimap(InMapManager, false);
    CompactMinimapUI->InitMinimap(InMapManager, true);
    MinimapUI->OnMovementMessage.AddUniqueDynamic(this, &UMainGameUI::QueueEventNotification);
    MinimapUI->SetMovementStateMirror(CompactMinimapUI);

    if (GM)
    {
        MinimapUI->OnPlayerMoved.AddUniqueDynamic(GM, &AGridGameMode::HandlePlayerMoved);
    }
    MinimapUI->OnPlayerMoved.AddUniqueDynamic(this, &UMainGameUI::OnMinimapPlayerMoved);
}

void UMainGameUI::OnMinimapPlayerMoved(FIntPoint NewCoordinate)
{
    UpdateActionAvailability();

    if (AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        if (GM->IsAtExtractionPoint())
        {
            QueueEventNotification(TEXT("탈출 지점에 도착했습니다."));
        }
    }
}

void UMainGameUI::OnStashInventoryChanged()
{
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM || GM->RaidState == ERaidState::InRaid)
    {
        return;
    }

    if (!GM->SaveStash())
    {
        QueueEventNotification(TEXT("창고 저장에 실패했습니다."));
    }
}

void UMainGameUI::UpdateActionAvailability()
{
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    const bool bInRaid = GM && GM->RaidState == ERaidState::InRaid;
    const bool bInCombat = bInRaid && GM->CombatComponent && GM->CombatComponent->bHasActiveEnemy;
    const bool bAtExtractionPoint = bInRaid && GM->IsAtExtractionPoint();

    if (SearchBtn) SearchBtn->SetIsEnabled(bInRaid && !bInCombat);
    if (ExtractBtn) ExtractBtn->SetIsEnabled(bAtExtractionPoint && !bInCombat);
    if (SellBtn) SellBtn->SetIsEnabled(bInRaid && !bInCombat);
    if (SellAllBtn) SellAllBtn->SetIsEnabled(bInRaid && !bInCombat);
    if (BangBtn) BangBtn->SetIsEnabled(bInCombat);
    if (MinimapUI && MinimapUI->AdvanceButton)
    {
        MinimapUI->AdvanceButton->SetIsEnabled(bInRaid && !bInCombat && MinimapUI->CurrentPath.Num() > 0);
    }
    if (CompactMinimapUI && CompactMinimapUI->AdvanceButton)
    {
        CompactMinimapUI->AdvanceButton->SetIsEnabled(bInRaid && !bInCombat && CompactMinimapUI->CurrentPath.Num() > 0);
    }
    if (CompactMinimapUI) CompactMinimapUI->SetVisibility(bInRaid ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
    if (StartRaidBtn) StartRaidBtn->SetIsEnabled(GM && GM->RaidState == ERaidState::Lobby);
    if (StashBtn) StashBtn->SetIsEnabled(GM && GM->RaidState != ERaidState::InRaid);
    if (GM && GM->RaidState == ERaidState::InRaid && RightPanelSwitcher &&
        RightPanelSwitcher->GetActiveWidgetIndex() == 2)
    {
        RightPanelSwitcher->SetActiveWidgetIndex(0);
    }
    if (GM && GM->RaidState != ERaidState::InRaid && RightPanelSwitcher &&
        RightPanelSwitcher->GetActiveWidgetIndex() == 1)
    {
        RightPanelSwitcher->SetActiveWidgetIndex(0);
    }
    if (ToggleModeButton)
    {
        const bool bIsStashVisible = RightPanelSwitcher && RightPanelSwitcher->GetActiveWidgetIndex() == 2;
        ToggleModeButton->SetVisibility(bInRaid && !bIsStashVisible
            ? ESlateVisibility::Visible
            : ESlateVisibility::Hidden);
    }
}

void UMainGameUI::OnToggleModeClicked()
{
    if (AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        if (GM->RaidState != ERaidState::InRaid) return;
    }
    else
    {
        return;
    }

    if (RightPanelSwitcher)
    {
        int32 CurrentIdx = RightPanelSwitcher->GetActiveWidgetIndex();
        if (CurrentIdx == 2)
        {
            if (AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this)))
            {
                if (!GM->SaveStash()) return;
            }
            RightPanelSwitcher->SetActiveWidgetIndex(0);
            UpdateActionAvailability();
            return;
        }
        RightPanelSwitcher->SetActiveWidgetIndex(CurrentIdx == 0 ? 1 : 0);
    }
}

void UMainGameUI::OnStashButtonClicked()
{
    if (!RightPanelSwitcher) return;

    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM || GM->RaidState == ERaidState::InRaid) return;
    if (!GM->SaveStash()) return;

    RightPanelSwitcher->SetActiveWidgetIndex(2);
    UpdateActionAvailability();
}

void UMainGameUI::OnStartRaidClicked()
{
    if (AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        if (GM->StartRaid() && RightPanelSwitcher)
        {
            RightPanelSwitcher->SetActiveWidgetIndex(0);
        }
    }
}

void UMainGameUI::OnExtractButtonClicked()
{
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (GM) GM->ExtractRaid();
}

FReply UMainGameUI::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    FKey Key = InKeyEvent.GetKey();
    if (Key == EKeys::One)
    {
        ActiveWeaponSlot = TEXT("Primary1");
        if (WeaponSlot1) WeaponSlot1->SetHighlight(true);
        if (WeaponSlot2) WeaponSlot2->SetHighlight(false);
        return FReply::Handled();
    }
    else if (Key == EKeys::Two)
    {
        ActiveWeaponSlot = TEXT("Primary2");
        if (WeaponSlot1) WeaponSlot1->SetHighlight(false);
        if (WeaponSlot2) WeaponSlot2->SetHighlight(true);
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UMainGameUI::UpdateScore(int32 NewScore)
{
    if (ScoreText)
    {
        int32 ScoreQuota = 1000;
        if (AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this)))
        {
            ScoreQuota = GM->QuotaScore;
        }
        ScoreText->SetText(FText::FromString(FString::Printf(TEXT("Score: %d / %d"), NewScore, ScoreQuota)));
    }
}

void UMainGameUI::UpdateTimer(float RemainingTime)
{
	if (TimerText)
	{
		TimerText->SetColorAndOpacity(FLinearColor::White);
		TimerText->SetText(FText::FromString(FString::Printf(TEXT("Time: %d s"), FMath::FloorToInt(RemainingTime))));
	}
}

void UMainGameUI::UpdateHealth(int32 NewHealth, int32 NewMaxHealth)
{
    if (HealthText)
    {
        HealthText->SetText(FText::FromString(FString::Printf(TEXT("HP: %d / %d"), NewHealth, NewMaxHealth)));
    }
}

void UMainGameUI::UpdateCombatUI()
{
    UpdateActionAvailability();
    if (!CombatText) return;

    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM || !GM->CombatComponent || GM->RaidState != ERaidState::InRaid ||
        GM->CombatComponent->CurrentEnemy.Definition.EnemyID.IsNone())
    {
        const FString Message = GM && GM->CombatComponent && GM->RaidState == ERaidState::InRaid
            ? GM->CombatComponent->LastCombatMessage
            : TEXT("");
        CombatText->SetText(FText::FromString(Message.IsEmpty() ? TEXT("Enemy: None") : FString::Printf(TEXT("Enemy: None\n%s"), *Message)));
        QueueEventNotification(Message);
        return;
    }

    const FEnemyInstanceData& Enemy = GM->CombatComponent->CurrentEnemy;
    const FString StateText = GM->CombatComponent->bHasActiveEnemy ?
        FString::Printf(TEXT("HP: %d / %d"), Enemy.CurrentHealth, Enemy.Definition.MaxHealth) :
        TEXT("DEFEATED");
    const FString Message = GM->CombatComponent->LastCombatMessage;
    const FString CombatStatus = FString::Printf(TEXT("Enemy: %s (%s)"), *Enemy.Definition.DisplayName, *StateText);
    CombatText->SetText(FText::FromString(Message.IsEmpty() ? CombatStatus : FString::Printf(TEXT("%s\n%s"), *CombatStatus, *Message)));
    QueueEventNotification(Message);
}

void UMainGameUI::ShowEventNotification(FString Message)
{
    if (!EventNotificationText) return;

    PendingEventNotifications.Empty();
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(EventNotificationTimerHandle);
    }
    EventNotificationText->SetText(FText::FromString(Message));
    const ESlateVisibility NotificationVisibility = Message.IsEmpty()
        ? ESlateVisibility::Hidden
        : ESlateVisibility::Visible;
    EventNotificationText->SetVisibility(NotificationVisibility);
    if (EventNotificationBorder)
    {
        EventNotificationBorder->SetVisibility(NotificationVisibility);
    }
}

void UMainGameUI::QueueEventNotification(FString Message)
{
    if (Message.IsEmpty() || !EventNotificationText) return;

    PendingEventNotifications.Add(Message);
    if (EventNotificationText->GetVisibility() == ESlateVisibility::Visible)
    {
        return;
    }

    EventNotificationText->SetText(FText::FromString(PendingEventNotifications[0]));
    EventNotificationText->SetVisibility(ESlateVisibility::Visible);
    if (EventNotificationBorder)
    {
        EventNotificationBorder->SetVisibility(ESlateVisibility::Visible);
    }
    PendingEventNotifications.RemoveAt(0);

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            EventNotificationTimerHandle, this, &UMainGameUI::OnEventNotificationTimerExpired, 2.5f, false);
    }
}

void UMainGameUI::OnEventNotificationTimerExpired()
{
    if (!EventNotificationText) return;

    if (PendingEventNotifications.Num() == 0)
    {
        EventNotificationText->SetVisibility(ESlateVisibility::Hidden);
        if (EventNotificationBorder)
        {
            EventNotificationBorder->SetVisibility(ESlateVisibility::Hidden);
        }
        return;
    }

    EventNotificationText->SetText(FText::FromString(PendingEventNotifications[0]));
    PendingEventNotifications.RemoveAt(0);
    if (EventNotificationBorder)
    {
        EventNotificationBorder->SetVisibility(ESlateVisibility::Visible);
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            EventNotificationTimerHandle, this, &UMainGameUI::OnEventNotificationTimerExpired, 2.5f, false);
    }
}

void UMainGameUI::ShowGameResult(bool bIsWin)
{
    if (TimerText)
    {
        FString ResultStr = bIsWin ? TEXT("YOU WIN!") : TEXT("GAME OVER!");
        TimerText->SetText(FText::FromString(ResultStr));
        TimerText->SetColorAndOpacity(bIsWin ? FLinearColor::Green : FLinearColor::Red);
    }
    QueueEventNotification(bIsWin ? TEXT("RAID EXTRACTED") : TEXT("RAID FAILED"));
}

void UMainGameUI::OnSellButtonClicked()
{
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (GM && GM->RaidState == ERaidState::InRaid && GM->InventoryComponent && GM->ItemDataTable)
    {
        int32 TotalValue = 0;
        TArray<TPair<FName, int32>> SellableItems;
        for (const TPair<FName, UItemInstance*>& Pair : GM->InventoryComponent->ItemInstances)
        {
            if (UItemInstance* Item = Pair.Value)
            {
                if (const FItemData* ItemData = GM->ItemDataTable->FindRow<FItemData>(Item->TemplateID, TEXT("Sell")))
                {
                    SellableItems.Add(TPair<FName, int32>(Pair.Key, ItemData->Value * Item->CurrentStack));
                }
            }
        }

        for (const TPair<FName, int32>& SellableItem : SellableItems)
        {
            if (GM->InventoryComponent->RemoveItem(SellableItem.Key))
            {
                TotalValue += SellableItem.Value;
            }
        }

        if (TotalValue > 0)
        {
            GM->AddScore(TotalValue);
        }
    }
}

void UMainGameUI::OnSellAllButtonClicked()
{
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM || GM->RaidState != ERaidState::InRaid || !GM->ItemDataTable) return;

    int32 TotalValue = 0;
    auto SellGrid = [&](UGridInventoryComponent* Inv) {
        if (!Inv) return;

        TArray<TPair<FName, int32>> SellableItems;
        for (const TPair<FName, UItemInstance*>& Pair : Inv->ItemInstances)
        {
            if (UItemInstance* Item = Pair.Value)
            {
                if (const FItemData* ItemData = GM->ItemDataTable->FindRow<FItemData>(Item->TemplateID, TEXT("Sell")))
                {
                    SellableItems.Add(TPair<FName, int32>(Pair.Key, ItemData->Value * Item->CurrentStack));
                }
            }
        }

        for (const TPair<FName, int32>& SellableItem : SellableItems)
        {
            if (Inv->RemoveItem(SellableItem.Key))
            {
                TotalValue += SellableItem.Value;
            }
        }
    };

    SellGrid(GM->InventoryComponent);
    SellGrid(GM->LootContainerComponent);
    SellGrid(GM->SafeBoxComponent);
    SellGrid(GM->RigComponent);
    SellGrid(GM->PocketComponent);

    if (TotalValue > 0)
    {
        GM->AddScore(TotalValue);
    }
}

void UMainGameUI::OnBangButtonClicked()
{
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM || GM->RaidState != ERaidState::InRaid) return;

    if (!GM->CombatComponent || !GM->CombatComponent->bHasActiveEnemy)
    {
        QueueEventNotification(TEXT("공격할 적이 없습니다."));
        return;
    }

    if (!GM->EquipmentComponent)
    {
        QueueEventNotification(TEXT("무기를 준비할 수 없습니다."));
        return;
    }

    UItemInstance* ActiveWeapon = GM->EquipmentComponent->GetEquippedItem(ActiveWeaponSlot);
    if (!ActiveWeapon)
    {
        QueueEventNotification(TEXT("선택한 무기가 없습니다."));
        return;
    }

    if (!ActiveWeapon->EquippedMagazine)
    {
        QueueEventNotification(TEXT("탄창이 장착되지 않았습니다."));
        return;
    }

    if (ActiveWeapon->EquippedMagazine->CurrentAmmo <= 0)
    {
        QueueEventNotification(TEXT("탄약이 없습니다."));
        return;
    }

    if (ActiveWeapon->Damage <= 0)
    {
        QueueEventNotification(TEXT("이 무기로 공격할 수 없습니다."));
        return;
    }

    ActiveWeapon->EquippedMagazine->CurrentAmmo--;
    ActiveWeapon->EquippedMagazine->OnItemModified.Broadcast();
    ActiveWeapon->OnItemModified.Broadcast();

    if (!GM->CombatComponent->AttackEnemy(ActiveWeapon->Damage))
    {
        ++ActiveWeapon->EquippedMagazine->CurrentAmmo;
        ActiveWeapon->EquippedMagazine->OnItemModified.Broadcast();
        ActiveWeapon->OnItemModified.Broadcast();
        return;
    }

    GM->CombatComponent->EnemyAttackPlayer();

    // 소음기 장착 여부 확인 (Muzzle 부착물이 있으면 소음기로 간주)
    bool bIsSilenced = (ActiveWeapon->EquippedMuzzle != nullptr);

    // 사용자가 에디터로 임포트한 에셋 경로 (임포트 전이면 로드 실패함)
    FString SoundPath = bIsSilenced
            ? TEXT("/Script/Engine.SoundWave'/Game/Assets/Sounds/Sound_WpnShoot_Silenced.Sound_WpnShoot_Silenced'")
            : TEXT("/Script/Engine.SoundWave'/Game/Assets/Sounds/Sound_WpnShoot_Normal.Sound_WpnShoot_Normal'");

    USoundBase* ShootSound = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, *SoundPath));
    if (!ShootSound)
    {
        // 에셋 임포트가 안 되어 있을 때의 대체 사운드
        ShootSound = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, TEXT("/Engine/VREditor/Sounds/UI/Laser_Hover_01.Laser_Hover_01")));
    }

    if (ShootSound)
    {
        UGameplayStatics::PlaySound2D(this, ShootSound);
    }

    // UI 갱신
    if (ActiveWeaponSlot == TEXT("Primary1") && WeaponSlot1) WeaponSlot1->RefreshSlotUI();
    else if (ActiveWeaponSlot == TEXT("Primary2") && WeaponSlot2) WeaponSlot2->RefreshSlotUI();
}
