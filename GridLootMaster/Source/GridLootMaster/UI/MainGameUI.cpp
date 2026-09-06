#include "MainGameUI.h"
#include "ItemDragDropOperation.h"
#include "ContextMenuWidget.h"
#include "InspectWidget.h"
#include "SplitStackWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
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
#include "Components/ScrollBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "MinimapWidget.h"
#include "GridBoardWidget.h"
#include "SectionedStorageWidget.h"
#include "DraggableItemWidget.h"
#include "EquipmentSlotWidget.h"
#include "../GridGameMode.h"
#include "../GridInventoryComponent.h"
#include "../EquipmentComponent.h"
#include "../CombatComponent.h"
#include "../EnemyManagerComponent.h"
#include "../Map/MapManagerComponent.h"
#include "../ItemInstance.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

namespace
{
    const FLinearColor TacticalTextPrimary(0.91f, 0.94f, 0.97f, 1.0f);
    const FLinearColor TacticalTextSecondary(0.56f, 0.62f, 0.68f, 1.0f);

    FSlateBrush MakeTacticalBrush(const FLinearColor& Color)
    {
        FSlateBrush Brush;
        Brush.DrawAs = ESlateBrushDrawType::Box;
        Brush.TintColor = FSlateColor(Color);
        Brush.Margin = FMargin(0.08f);
        return Brush;
    }

    void StyleTacticalButton(UButton* Button, bool bPrimary = false, bool bWarning = false)
    {
        if (!Button) return;

        const FLinearColor Normal = bPrimary ? FLinearColor(0.08f, 0.28f, 0.34f, 1.0f)
            : bWarning ? FLinearColor(0.28f, 0.22f, 0.12f, 1.0f)
            : FLinearColor(0.09f, 0.13f, 0.17f, 1.0f);
        FButtonStyle Style;
        Style.Normal = MakeTacticalBrush(Normal);
        Style.Hovered = MakeTacticalBrush(Normal + FLinearColor(0.06f, 0.07f, 0.08f, 0.0f));
        Style.Pressed = MakeTacticalBrush(FLinearColor(0.04f, 0.06f, 0.08f, 1.0f));
        Style.Disabled = MakeTacticalBrush(FLinearColor(0.07f, 0.08f, 0.09f, 0.75f));
        Button->SetStyle(Style);
    }

    UTexture2D* LoadRawBackgroundTexture(const FString& FilePath, const TCHAR* BackgroundName)
    {
        if (!FPaths::FileExists(FilePath))
        {
            UE_LOG(LogTemp, Warning, TEXT("MainGameUI background not found: %s (%s)"), *FilePath, BackgroundName);
            return nullptr;
        }

        TArray<uint8> RawFileData;
        if (!FFileHelper::LoadFileToArray(RawFileData, *FilePath))
        {
            UE_LOG(LogTemp, Warning, TEXT("MainGameUI background failed to read: %s (%s)"), *FilePath, BackgroundName);
            return nullptr;
        }

        UTexture2D* LoadedTexture = FImageUtils::ImportBufferAsTexture2D(RawFileData);
        if (!LoadedTexture)
        {
            UE_LOG(LogTemp, Warning, TEXT("MainGameUI background failed to decode: %s (%s)"), *FilePath, BackgroundName);
            return nullptr;
        }

        LoadedTexture->CompressionSettings = TC_EditorIcon;
        LoadedTexture->SRGB = true;
        LoadedTexture->UpdateResource();
        return LoadedTexture;
    }
}

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

        // 배경은 기존 UI와 입력을 가리지 않는 최하단 visual layer다.
        UScaleBox* BackgroundScaleBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("BackgroundScaleBox"));
        BackgroundScaleBox->SetStretch(EStretch::ScaleToFill);
        BackgroundScaleBox->SetVisibility(ESlateVisibility::HitTestInvisible);
        BackgroundImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BackgroundImage"));
        BackgroundImage->SetVisibility(ESlateVisibility::HitTestInvisible);
        BackgroundScaleBox->AddChild(BackgroundImage);
        UCanvasPanelSlot* BackgroundSlot = RootCanvas->AddChildToCanvas(BackgroundScaleBox);
        BackgroundSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
        BackgroundSlot->SetOffsets(FMargin(0.0f));
        BackgroundSlot->SetZOrder(-100);

        const FString BackgroundDirectory = FPaths::ProjectDir() + TEXT("RawAssets/Backgrounds/");
        BGStashTexture = LoadRawBackgroundTexture(BackgroundDirectory + TEXT("BG_Stash.png"), TEXT("BG_Stash"));
        BGRaidTexture = LoadRawBackgroundTexture(BackgroundDirectory + TEXT("BG_Raid.png"), TEXT("BG_Raid"));
        BGEndingTexture = LoadRawBackgroundTexture(BackgroundDirectory + TEXT("BG_Ending.png"), TEXT("BG_Ending"));

        // 2. 전체 레이아웃 (가로 3분할)
        UHorizontalBox* HBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("MainLayout"));
        UCanvasPanelSlot* HBoxSlot = RootCanvas->AddChildToCanvas(HBox);
        HBoxSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
        HBoxSlot->SetOffsets(FMargin(30.0f, 30.0f, 30.0f, 30.0f));

        AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));

        auto CreateEquipSlotEx = [&](UEquipmentSlotWidget*& OutSlot, FName SlotID, EItemCategory Category, const FString& Name, float Width, float Height) -> USizeBox*
        {
            OutSlot = WidgetTree->ConstructWidget<UEquipmentSlotWidget>(UEquipmentSlotWidget::StaticClass());
            OutSlot->InitSlot(SlotID, Category, Name);
            
            USizeBox* SBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
            SBox->SetWidthOverride(Width);
            SBox->SetHeightOverride(Height);
            SBox->AddChild(OutSlot);

            if (SlotID == TEXT("Rig")) RigSlotSizeBox = SBox;
            else if (SlotID == TEXT("Backpack")) BackpackSlotSizeBox = SBox;

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

        auto AddToHorizontal = [](UHorizontalBox* Parent, UWidget* Child, float PaddingRight = 10.0f,
            EVerticalAlignment VerticalAlignment = VAlign_Fill) {
            UHorizontalBoxSlot* HSlot = Parent->AddChildToHorizontalBox(Child);
            HSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
            HSlot->SetPadding(FMargin(0, 0, PaddingRight, 0));
            HSlot->SetVerticalAlignment(VerticalAlignment);
        };

        // === 1. 왼쪽 패널 (캐릭터 장비 슬롯) ===
        LeftPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LeftPanel"));
        UHorizontalBoxSlot* LeftPanelSlot = HBox->AddChildToHorizontalBox(LeftPanel);
        LeftPanelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        LeftPanelSlot->SetPadding(FMargin(0, 0, 25, 0));

        // --- 왼쪽 패널: 헬멧, 방어구, 무기 ---
        AddToVertical(LeftPanel, CreateEquipSlotEx(HelmetSlot, TEXT("Helmet"), EItemCategory::Helmet, TEXT("Helmet"), 173.0f, 173.0f));
        AddToVertical(LeftPanel, CreateEquipSlotEx(ArmorSlot, TEXT("Armor"), EItemCategory::Armor, TEXT("Armor"), 173.0f, 288.0f));
        AddToVertical(LeftPanel, CreateEquipSlotEx(WeaponSlot1, TEXT("Primary1"), EItemCategory::Weapon, TEXT("Primary Weapon 1"), 288.0f, 115.0f));
        AddToVertical(LeftPanel, CreateEquipSlotEx(WeaponSlot2, TEXT("Primary2"), EItemCategory::Weapon, TEXT("Primary Weapon 2"), 288.0f, 115.0f));

        // === 2. 중앙 패널 (Rig, Pocket, Backpack, SafeBox) ===
        UVerticalBox* MiddlePanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MiddlePanel"));
        UHorizontalBoxSlot* MiddlePanelSlot = HBox->AddChildToHorizontalBox(MiddlePanel);
        MiddlePanelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

        // 1. Rig Row
        UHorizontalBox* RigRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        AddToVertical(MiddlePanel, RigRow, 20.0f);
        AddToHorizontal(RigRow, CreateEquipSlotEx(RigSlot, TEXT("Rig"), EItemCategory::Rig, TEXT("Chest Rig"), 128.0f, 128.0f), 10.0f, VAlign_Top); // 2x2 장비 슬롯
        RigBoard = WidgetTree->ConstructWidget<USectionedStorageWidget>(USectionedStorageWidget::StaticClass(), TEXT("RigBoard"));
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
        AddToHorizontal(BackpackRow, CreateEquipSlotEx(BackpackSlot, TEXT("Backpack"), EItemCategory::Backpack, TEXT("Backpack"), 128.0f, 192.0f), 10.0f, VAlign_Top);
        GridBoard = WidgetTree->ConstructWidget<USectionedStorageWidget>(USectionedStorageWidget::StaticClass(), TEXT("GridBoard"));
        AddToHorizontal(BackpackRow, GridBoard);

        // 4. SafeBox Row
        UHorizontalBox* SafeBoxRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        AddToVertical(MiddlePanel, SafeBoxRow, 20.0f);
        AddToHorizontal(SafeBoxRow, CreateEquipSlotEx(SafeBoxSlot, TEXT("SafeBox"), EItemCategory::SafeBox, TEXT("SafeBox"), 128.0f, 128.0f));
        SafeBoxBoard = WidgetTree->ConstructWidget<UGridBoardWidget>(UGridBoardWidget::StaticClass(), TEXT("SafeBoxBoard"));
        AddToHorizontal(SafeBoxRow, SafeBoxBoard);

        if (GM && GM->EquipmentComponent)
        {
            GM->EquipmentComponent->OnEquipmentChanged.AddUniqueDynamic(this, &UMainGameUI::UpdateEquipmentSlotSizes);
            UpdateEquipmentSlotSizes();
        }

        // Toggle 버튼 (상단 중앙 쯤 표시 배치)
        ToggleModeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ToggleModeButton"));
        UTextBlock* ToggleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        ToggleText->SetText(FText::FromString(TEXT("Toggle Map/Inv")));
        
        // 폰트 크기 축소 및 검은색 텍스트로 가독성 확보
        FSlateFontInfo ToggleFont = ToggleText->GetFont();
        ToggleFont.Size = 16; 
        ToggleText->SetFont(ToggleFont);
        ToggleText->SetColorAndOpacity(TacticalTextPrimary);
        StyleTacticalButton(ToggleModeButton);
        
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
        StashTitle->SetColorAndOpacity(TacticalTextPrimary);
        FSlateFontInfo StashTitleFont = StashTitle->GetFont();
        StashTitleFont.Size = 20;
        StashTitle->SetFont(StashTitleFont);
        StashPanel->AddChildToVerticalBox(StashTitle);

        RetirementAccountPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RetirementAccountPanel"));
        UBorder* RetirementFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RetirementFrame"));
        RetirementFrame->SetBrushColor(FLinearColor(0.07f, 0.10f, 0.13f, 1.0f));
        RetirementFrame->SetPadding(FMargin(10.0f, 8.0f));
        RetirementFrame->AddChild(RetirementAccountPanel);
        StashPanel->AddChildToVerticalBox(RetirementFrame);

        UTextBlock* RetirementTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        RetirementTitle->SetText(FText::FromString(TEXT("RETIREMENT ACCOUNT")));
        RetirementTitle->SetColorAndOpacity(TacticalTextSecondary);
        RetirementAccountPanel->AddChildToVerticalBox(RetirementTitle);

        RetirementBalanceText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RetirementBalanceText"));
        RetirementBalanceText->SetColorAndOpacity(TacticalTextPrimary);
        FSlateFontInfo RetirementBalanceFont = RetirementBalanceText->GetFont();
        RetirementBalanceFont.Size = 18;
        RetirementBalanceText->SetFont(RetirementBalanceFont);
        RetirementAccountPanel->AddChildToVerticalBox(RetirementBalanceText);

        RetirementStatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RetirementStatusText"));
        RetirementStatusText->SetText(FText::FromString(TEXT("SALE PROCEEDS AUTO-DEPOSIT")));
        RetirementStatusText->SetColorAndOpacity(TacticalTextSecondary);
        RetirementAccountPanel->AddChildToVerticalBox(RetirementStatusText);

        RetirementProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("RetirementProgressBar"));
        RetirementAccountPanel->AddChildToVerticalBox(RetirementProgressBar);

        RetirementButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RetirementButton"));
        UTextBlock* RetirementButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        RetirementButtonText->SetText(FText::FromString(TEXT("RETIRE")));
        RetirementButtonText->SetColorAndOpacity(TacticalTextPrimary);
        RetirementButton->AddChild(RetirementButtonText);
        RetirementButton->OnClicked.AddDynamic(this, &UMainGameUI::OnRetirementClicked);
        StyleTacticalButton(RetirementButton, true);
        RetirementAccountPanel->AddChildToVerticalBox(RetirementButton);

        StashBoard = WidgetTree->ConstructWidget<UGridBoardWidget>(UGridBoardWidget::StaticClass(), TEXT("StashBoard"));
        UVerticalBoxSlot* StashBoardSlot = StashPanel->AddChildToVerticalBox(StashBoard);
        StashBoardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        StashBoardSlot->SetHorizontalAlignment(HAlign_Left);

        UHorizontalBox* StashSellActions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("StashSellActions"));
        StashPanel->AddChildToVerticalBox(StashSellActions);

        SellBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SellButton"));
        UTextBlock* SellBtnText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        SellBtnText->SetText(FText::FromString(TEXT("SELL BAG")));
        SellBtnText->SetColorAndOpacity(TacticalTextPrimary);
        SellBtn->AddChild(SellBtnText);
        SellBtn->OnClicked.AddDynamic(this, &UMainGameUI::OnSellButtonClicked);
        StyleTacticalButton(SellBtn);
        StashSellActions->AddChildToHorizontalBox(SellBtn);

        SellAllBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SellAllButton"));
        UTextBlock* SellAllBtnText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        SellAllBtnText->SetText(FText::FromString(TEXT("SELL ALL")));
        SellAllBtnText->SetColorAndOpacity(TacticalTextPrimary);
        SellAllBtn->AddChild(SellAllBtnText);
        SellAllBtn->OnClicked.AddDynamic(this, &UMainGameUI::OnSellAllButtonClicked);
        StyleTacticalButton(SellAllBtn, false, true);
        StashSellActions->AddChildToHorizontalBox(SellAllBtn);

        StartRaidBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("StartRaidButton"));
        UTextBlock* StartRaidText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        StartRaidText->SetText(FText::FromString(TEXT("START RAID")));
        StartRaidText->SetColorAndOpacity(TacticalTextPrimary);
        StartRaidBtn->AddChild(StartRaidText);
        StartRaidBtn->OnClicked.AddDynamic(this, &UMainGameUI::OnStartRaidClicked);
        StyleTacticalButton(StartRaidBtn, true);
        StashPanel->AddChildToVerticalBox(StartRaidBtn);

        TimerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TimerText"));
        TimerText->SetText(FText::FromString(TEXT("Time: 60s")));
        TimerText->SetVisibility(ESlateVisibility::Collapsed);
        RightPanel->AddChildToVerticalBox(TimerText);

        ScoreText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ScoreText"));
        ScoreText->SetText(FText::FromString(TEXT("Score: 0 / 1000")));
        ScoreText->SetVisibility(ESlateVisibility::Collapsed);
        RightPanel->AddChildToVerticalBox(ScoreText);

        HealthText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HealthText"));
        HealthText->SetText(FText::FromString(TEXT("HP: 100 / 100")));
        HealthText->SetVisibility(ESlateVisibility::Collapsed);
        RightPanel->AddChildToVerticalBox(HealthText);

        CombatText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CombatText"));
        CombatText->SetText(FText::FromString(TEXT("Enemy: None")));
        CombatText->SetVisibility(ESlateVisibility::Collapsed);
        RightPanel->AddChildToVerticalBox(CombatText);

        CombatActionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CombatActionText"));
        CombatActionText->SetVisibility(ESlateVisibility::Collapsed);
        RightPanel->AddChildToVerticalBox(CombatActionText);

        EventLogBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("EventLogBorder"));
        EventLogBorder->SetBrushColor(FLinearColor(0.01f, 0.02f, 0.03f, 0.78f));
        EventLogBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        EventLogScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("EventLogScrollBox"));
        EventLogScrollBox->SetVisibility(ESlateVisibility::Visible);
        EventLogBorder->AddChild(EventLogScrollBox);
        USizeBox* EventLogSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("EventLogSize"));
        EventLogSize->SetHeightOverride(150.0f);
        EventLogSize->AddChild(EventLogBorder);
        UVerticalBoxSlot* EventLogSlot = RightPanel->AddChildToVerticalBox(EventLogSize);
        EventLogSlot->SetPadding(FMargin(0, 8, 0, 4));

        UTextBlock* EventLogTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        EventLogTitle->SetText(FText::FromString(TEXT("EVENT LOG")));
        EventLogTitle->SetColorAndOpacity(FLinearColor(0.7f, 0.9f, 1.0f, 1.0f));
        EventLogScrollBox->AddChild(EventLogTitle);

        StatusPanelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusPanelText"));
        StatusPanelText->SetColorAndOpacity(FLinearColor(0.85f, 0.95f, 1.0f, 1.0f));
        FSlateFontInfo StatusFont = StatusPanelText->GetFont();
        StatusFont.Size = 14;
        StatusPanelText->SetFont(StatusFont);
        StatusPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StatusPanel"));
        StatusPanel->SetBrushColor(FLinearColor(0.01f, 0.02f, 0.03f, 0.8f));
        StatusPanel->SetPadding(FMargin(10.0f));
        StatusPanel->AddChild(StatusPanelText);
        UCanvasPanelSlot* StatusSlot = RootCanvas->AddChildToCanvas(StatusPanel);
        StatusSlot->SetAnchors(FAnchors(0.0f, 1.0f));
        StatusSlot->SetAlignment(FVector2D(0.0f, 1.0f));
        StatusSlot->SetPosition(FVector2D(30.0f, -30.0f));
        StatusSlot->SetSize(FVector2D(300.0f, 150.0f));
        StatusSlot->SetZOrder(12);

        UBorder* Spacer1 = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Spacer1->SetBrushColor(FLinearColor::Transparent);
        UVerticalBoxSlot* SpacerSlot1 = RightPanel->AddChildToVerticalBox(Spacer1);
        SpacerSlot1->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        SpacerSlot1->SetPadding(FMargin(0, 20));

        LootContainerTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LootContainerTitle"));
        LootContainerTitle->SetText(FText::FromString(TEXT("Loot Container")));
        RightPanel->AddChildToVerticalBox(LootContainerTitle);

        ContainerBoard = WidgetTree->ConstructWidget<UGridBoardWidget>(UGridBoardWidget::StaticClass(), TEXT("ContainerBoard"));
        UVerticalBoxSlot* PoolSlot = RightPanel->AddChildToVerticalBox(ContainerBoard);
        PoolSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        PoolSlot->SetHorizontalAlignment(HAlign_Left);

        UUniformGridPanel* ActionGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("ActionGrid"));
        ActionGrid->SetSlotPadding(FMargin(2.0f));
        UVerticalBoxSlot* ActionGridSlot = RightPanel->AddChildToVerticalBox(ActionGrid);
        ActionGridSlot->SetPadding(FMargin(0, 6, 0, 0));
        int32 ActionIndex = 0;
        auto AddCompactAction = [&](UButton* Button)
        {
            if (Button && ActionGrid)
            {
                StyleTacticalButton(Button);
                ActionGrid->AddChildToUniformGrid(Button, ActionIndex / 2, ActionIndex % 2);
                ++ActionIndex;
            }
        };

        SearchBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SearchBtn"));
        SearchBtnText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        SearchBtnText->SetText(FText::FromString(TEXT("SEARCH CONTAINER")));
        SearchBtnText->SetColorAndOpacity(TacticalTextPrimary);
        SearchBtn->AddChild(SearchBtnText);
        AddCompactAction(SearchBtn);

        StashBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("StashButton"));
        UTextBlock* StashBtnText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        StashBtnText->SetText(FText::FromString(TEXT("OPEN STASH")));
        StashBtnText->SetColorAndOpacity(TacticalTextPrimary);
        StashBtn->AddChild(StashBtnText);
        StashBtn->OnClicked.AddDynamic(this, &UMainGameUI::OnStashButtonClicked);
        AddCompactAction(StashBtn);

        ExtractBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ExtractButton"));
        UTextBlock* ExtractBtnText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        ExtractBtnText->SetText(FText::FromString(TEXT("EXTRACT RAID")));
        ExtractBtnText->SetColorAndOpacity(TacticalTextPrimary);
        ExtractBtn->AddChild(ExtractBtnText);
        ExtractBtn->OnClicked.AddDynamic(this, &UMainGameUI::OnExtractButtonClicked);
        AddCompactAction(ExtractBtn);

        // Combat action buttons
        BangBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("BangButton"));
        BangButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        BangButtonText->SetText(FText::FromString(TEXT("FIRE")));
        BangButtonText->SetColorAndOpacity(TacticalTextPrimary);
        BangBtn->AddChild(BangButtonText);
        BangBtn->OnClicked.AddDynamic(this, &UMainGameUI::OnBangButtonClicked);
        AddCompactAction(BangBtn);

        ReloadBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ReloadButton"));
        UTextBlock* ReloadText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        ReloadText->SetText(FText::FromString(TEXT("RELOAD")));
        ReloadText->SetColorAndOpacity(TacticalTextPrimary);
        ReloadBtn->AddChild(ReloadText);
        ReloadBtn->OnClicked.AddDynamic(this, &UMainGameUI::OnReloadButtonClicked);
        AddCompactAction(ReloadBtn);

        // Combat Movement v1 is retained for non-player reuse, but is not exposed in the player UI.
        ApproachBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ApproachButton"));
        RetreatBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RetreatButton"));
        ApproachBtn->SetVisibility(ESlateVisibility::Collapsed);
        RetreatBtn->SetVisibility(ESlateVisibility::Collapsed);

        CombatMoveBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CombatMoveButton"));
        UTextBlock* CombatMoveText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        CombatMoveText->SetText(FText::FromString(TEXT("MOVE")));
        CombatMoveText->SetColorAndOpacity(TacticalTextPrimary);
        CombatMoveBtn->AddChild(CombatMoveText);
        CombatMoveBtn->OnClicked.AddDynamic(this, &UMainGameUI::OnCombatMoveButtonClicked);
        AddCompactAction(CombatMoveBtn);

        auto CreateCombatDirectionButton = [&](UButton*& OutButton, const TCHAR* Name, const TCHAR* Label)
        {
            OutButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
            UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
            Text->SetText(FText::FromString(Label));
            Text->SetColorAndOpacity(TacticalTextPrimary);
            OutButton->AddChild(Text);
            OutButton->SetVisibility(ESlateVisibility::Collapsed);
            AddCompactAction(OutButton);
        };
        CreateCombatDirectionButton(CombatDirectionNorthBtn, TEXT("CombatDirectionNorthButton"), TEXT("NORTH"));
        CombatDirectionNorthBtn->OnClicked.AddDynamic(this, &UMainGameUI::OnCombatDirectionNorthButtonClicked);
        CreateCombatDirectionButton(CombatDirectionWestBtn, TEXT("CombatDirectionWestButton"), TEXT("WEST"));
        CombatDirectionWestBtn->OnClicked.AddDynamic(this, &UMainGameUI::OnCombatDirectionWestButtonClicked);
        CreateCombatDirectionButton(CombatDirectionEastBtn, TEXT("CombatDirectionEastButton"), TEXT("EAST"));
        CombatDirectionEastBtn->OnClicked.AddDynamic(this, &UMainGameUI::OnCombatDirectionEastButtonClicked);
        CreateCombatDirectionButton(CombatDirectionSouthBtn, TEXT("CombatDirectionSouthButton"), TEXT("SOUTH"));
        CombatDirectionSouthBtn->OnClicked.AddDynamic(this, &UMainGameUI::OnCombatDirectionSouthButtonClicked);
        CreateCombatDirectionButton(CombatDirectionCancelBtn, TEXT("CombatDirectionCancelButton"), TEXT("CANCEL"));
        CombatDirectionCancelBtn->OnClicked.AddDynamic(this, &UMainGameUI::OnCombatDirectionCancelButtonClicked);

        CombatFleeBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CombatFleeButton"));
        UTextBlock* CombatFleeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        CombatFleeText->SetText(FText::FromString(TEXT("FLEE")));
        CombatFleeText->SetColorAndOpacity(TacticalTextPrimary);
        CombatFleeBtn->AddChild(CombatFleeText);
        CombatFleeBtn->OnClicked.AddDynamic(this, &UMainGameUI::OnCombatFleeButtonClicked);
        AddCompactAction(CombatFleeBtn);

        PlayerAmbushBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PlayerAmbushButton"));
        UTextBlock* PlayerAmbushText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        PlayerAmbushText->SetText(FText::FromString(TEXT("AMBUSH")));
        PlayerAmbushText->SetColorAndOpacity(TacticalTextPrimary);
        PlayerAmbushBtn->AddChild(PlayerAmbushText);
        PlayerAmbushBtn->OnClicked.AddDynamic(this, &UMainGameUI::OnPlayerAmbushButtonClicked);
        AddCompactAction(PlayerAmbushBtn);

        auto AddAmbushButton = [&](UButton*& OutButton, const TCHAR* Name, const TCHAR* Label, const TCHAR* FunctionName)
        {
            OutButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
            UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
            Text->SetText(FText::FromString(Label));
            Text->SetColorAndOpacity(TacticalTextPrimary);
            OutButton->AddChild(Text);
            FScriptDelegate Delegate;
            Delegate.BindUFunction(this, FunctionName);
            OutButton->OnClicked.Add(Delegate);
            AddCompactAction(OutButton);
        };
        AddAmbushButton(AmbushWaitBtn, TEXT("AmbushWaitButton"), TEXT("WAIT"), TEXT("OnAmbushWaitButtonClicked"));
        AddAmbushButton(AmbushCancelBtn, TEXT("AmbushCancelButton"), TEXT("CANCEL"), TEXT("OnAmbushCancelButtonClicked"));
        AddAmbushButton(AmbushLetPassBtn, TEXT("AmbushLetPassButton"), TEXT("LET PASS"), TEXT("OnAmbushLetPassButtonClicked"));
        AddAmbushButton(AmbushAssaultBtn, TEXT("AmbushAssaultButton"), TEXT("ASSAULT"), TEXT("OnAmbushAssaultButtonClicked"));
        AddAmbushButton(EnemyAmbushSearchBtn, TEXT("EnemyAmbushSearchButton"), TEXT("SEARCH"), TEXT("OnEnemyAmbushSearchButtonClicked"));
        AddAmbushButton(EnemyAmbushCoverBtn, TEXT("EnemyAmbushCoverButton"), TEXT("COVER"), TEXT("OnEnemyAmbushCoverButtonClicked"));
        AddAmbushButton(EnemyAmbushFleeBtn, TEXT("EnemyAmbushFleeButton"), TEXT("FLEE"), TEXT("OnEnemyAmbushFleeButtonClicked"));

#if !UE_BUILD_SHIPPING
        DebugSpawnEnemyBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("DebugSpawnEnemyButton"));
        UTextBlock* DebugSpawnText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        DebugSpawnText->SetText(FText::FromString(TEXT("DEBUG SPAWN ENEMY")));
        DebugSpawnText->SetColorAndOpacity(TacticalTextSecondary);
        DebugSpawnEnemyBtn->AddChild(DebugSpawnText);
        DebugSpawnEnemyBtn->OnClicked.AddDynamic(this, &UMainGameUI::OnDebugSpawnEnemyClicked);
        ActionGrid->AddChildToUniformGrid(DebugSpawnEnemyBtn, ActionIndex / 2, ActionIndex % 2);
        ++ActionIndex;
#endif

        RetirementEndingOverlay = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RetirementEndingOverlay"));
        RetirementEndingOverlay->SetVisibility(ESlateVisibility::Collapsed);
        UCanvasPanelSlot* EndingOverlaySlot = RootCanvas->AddChildToCanvas(RetirementEndingOverlay);
        EndingOverlaySlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
        EndingOverlaySlot->SetOffsets(FMargin(0.0f));
        EndingOverlaySlot->SetZOrder(1000);

        UImage* EndingImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RetirementEndingImage"));
        EndingImage->SetVisibility(ESlateVisibility::HitTestInvisible);
        if (BGEndingTexture)
        {
            EndingImage->SetBrushFromTexture(BGEndingTexture, true);
        }
        UCanvasPanelSlot* EndingImageSlot = RetirementEndingOverlay->AddChildToCanvas(EndingImage);
        EndingImageSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
        EndingImageSlot->SetOffsets(FMargin(0.0f));

        UBorder* EndingDimmer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RetirementEndingDimmer"));
        EndingDimmer->SetBrushColor(FLinearColor(0.015f, 0.02f, 0.025f, BGEndingTexture ? 0.55f : 0.95f));
        UCanvasPanelSlot* EndingDimmerSlot = RetirementEndingOverlay->AddChildToCanvas(EndingDimmer);
        EndingDimmerSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
        EndingDimmerSlot->SetOffsets(FMargin(0.0f));

        UVerticalBox* EndingContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RetirementEndingContent"));
        UCanvasPanelSlot* EndingContentSlot = RetirementEndingOverlay->AddChildToCanvas(EndingContent);
        EndingContentSlot->SetAnchors(FAnchors(0.5f, 0.5f));
        EndingContentSlot->SetAlignment(FVector2D(0.5f, 0.5f));
        EndingContentSlot->SetSize(FVector2D(440.0f, 170.0f));

        UTextBlock* EndingTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        EndingTitle->SetText(FText::FromString(TEXT("RETIREMENT COMPLETE")));
        EndingTitle->SetColorAndOpacity(TacticalTextPrimary);
        FSlateFontInfo EndingTitleFont = EndingTitle->GetFont();
        EndingTitleFont.Size = 26;
        EndingTitle->SetFont(EndingTitleFont);
        EndingTitle->SetJustification(ETextJustify::Center);
        EndingContent->AddChildToVerticalBox(EndingTitle);

        UTextBlock* EndingSubtitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        EndingSubtitle->SetText(FText::FromString(TEXT("YOU MADE IT OUT")));
        EndingSubtitle->SetColorAndOpacity(TacticalTextSecondary);
        EndingSubtitle->SetJustification(ETextJustify::Center);
        EndingContent->AddChildToVerticalBox(EndingSubtitle);

        RetirementReturnButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RetirementReturnButton"));
        UTextBlock* RetirementReturnText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        RetirementReturnText->SetText(FText::FromString(TEXT("RETURN TO STASH")));
        RetirementReturnText->SetColorAndOpacity(TacticalTextPrimary);
        RetirementReturnButton->AddChild(RetirementReturnText);
        RetirementReturnButton->OnClicked.AddDynamic(this, &UMainGameUI::OnRetirementReturnClicked);
        StyleTacticalButton(RetirementReturnButton, true);
        UVerticalBoxSlot* ReturnButtonSlot = EndingContent->AddChildToVerticalBox(RetirementReturnButton);
        ReturnButtonSlot->SetPadding(FMargin(0.0f, 20.0f, 0.0f, 0.0f));

        // --- 인벤토리/장비 컴포넌트 연결 ---
        if (GM)
        {
            if (GM->InventoryComponent)
            {
                GridBoard->BindInventory(GM->InventoryComponent);
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
                RigBoard->BindInventory(GM->RigComponent);
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
            GM->OnGameStateChanged.AddUniqueDynamic(this, &UMainGameUI::UpdateBackgroundForPhase);
            UpdateActionAvailability();
            UpdateBackgroundForPhase();

            SearchBtn->OnClicked.AddDynamic(this, &UMainGameUI::OnSearchButtonClicked);
        }
    }
    return true;
}

void UMainGameUI::UpdateBackgroundForPhase()
{
    if (!BackgroundImage) return;

    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    const bool bInRaid = GM && GM->RaidState == ERaidState::InRaid;
    UTexture2D* DesiredTexture = bInRaid ? BGRaidTexture : BGStashTexture;
    if (DesiredTexture)
    {
        BackgroundImage->SetBrushFromTexture(DesiredTexture, true);
        BackgroundImage->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    else
    {
        BackgroundImage->SetVisibility(ESlateVisibility::Hidden);
    }
}

void UMainGameUI::UpdateEquipmentSlotSizes()
{
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM || !GM->EquipmentComponent) return;

    if (RigSlotSizeBox)
    {
        RigSlotSizeBox->SetWidthOverride(128.0f);
        RigSlotSizeBox->SetHeightOverride(128.0f);
    }
    if (BackpackSlotSizeBox)
    {
        const bool bBackpackEquipped = GM->EquipmentComponent->GetEquippedItem(TEXT("Backpack")) != nullptr;
        BackpackSlotSizeBox->SetWidthOverride(128.0f);
        BackpackSlotSizeBox->SetHeightOverride(bBackpackEquipped ? 192.0f : 192.0f);
    }
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
    AddEventLogEntry(FString::Printf(TEXT("Player Moved: %d-%d"), NewCoordinate.X, NewCoordinate.Y));

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
    const bool bPlayerAmbushing = bInRaid && GM->PlayerPosture == EPlayerRaidPosture::Ambushing;
    const bool bEnemyAmbush = bInRaid && GM->EnemyManagerComponent && GM->EnemyManagerComponent->HasActiveAmbushReaction();
    if (StatusPanel) StatusPanel->SetVisibility(bInRaid ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
    FName AmbushTargetID = NAME_None;
    const bool bHasAmbushTarget = bPlayerAmbushing && GM->EnemyManagerComponent &&
        GM->EnemyManagerComponent->FindPlayerAmbushTarget(AmbushTargetID);
    const bool bAtExtractionPoint = bInRaid && GM->IsAtExtractionPoint();
    if (!bInCombat)
    {
        CombatDirectionMode = ECombatDirectionMode::None;
    }

    const bool bHasDeadBody = bInRaid && GM && GM->HasDeadBodyAtCurrentPlayerCoord();
    if (SearchBtn) SearchBtn->SetIsEnabled(bInRaid && !bInCombat && !bPlayerAmbushing && !bEnemyAmbush);
    if (SearchBtnText) SearchBtnText->SetText(FText::FromString(bHasDeadBody ? TEXT("SEARCH DEAD BODY") : TEXT("SEARCH CONTAINER")));
    if (StashBtn)
    {
        StashBtn->SetVisibility(bInRaid ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
        StashBtn->SetIsEnabled(!bInRaid);
    }
    if (ExtractBtn) ExtractBtn->SetIsEnabled(bAtExtractionPoint && !bInCombat);
    const bool bCanSell = GM && !bInRaid;
    if (SellBtn)
    {
        SellBtn->SetVisibility(bCanSell ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        SellBtn->SetIsEnabled(bCanSell);
    }
    if (SellAllBtn)
    {
        SellAllBtn->SetVisibility(bCanSell ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        SellAllBtn->SetIsEnabled(bCanSell);
    }
    if (RetirementAccountPanel)
    {
        RetirementAccountPanel->SetVisibility(bCanSell ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
    if (RetirementButton)
    {
        RetirementButton->SetVisibility(bCanSell ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        RetirementButton->SetIsEnabled(bCanSell && GM->RetirementBalance >= AGridGameMode::RetirementGoal);
    }
    if (bCanSell)
    {
        const float RetirementProgress = FMath::Clamp(
            static_cast<float>(GM->RetirementBalance) / static_cast<float>(AGridGameMode::RetirementGoal), 0.0f, 1.0f);
        if (RetirementBalanceText)
        {
            RetirementBalanceText->SetText(FText::Format(
                FText::FromString(TEXT("{0} / {1}")),
                FText::AsNumber(GM->RetirementBalance),
                FText::AsNumber(AGridGameMode::RetirementGoal)));
        }
        if (RetirementStatusText)
        {
            RetirementStatusText->SetText(FText::FromString(
                GM->RetirementBalance >= AGridGameMode::RetirementGoal
                    ? TEXT("RETIREMENT READY")
                    : TEXT("SALE PROCEEDS AUTO-DEPOSIT")));
        }
        if (RetirementProgressBar) RetirementProgressBar->SetPercent(RetirementProgress);
    }
    if (BangBtn) BangBtn->SetIsEnabled(bInCombat);
    if (ReloadBtn) ReloadBtn->SetIsEnabled(bInRaid && !bEnemyAmbush);
    if (ApproachBtn) ApproachBtn->SetVisibility(ESlateVisibility::Collapsed);
    if (RetreatBtn) RetreatBtn->SetVisibility(ESlateVisibility::Collapsed);
    if (CombatMoveBtn) CombatMoveBtn->SetVisibility(ESlateVisibility::Collapsed);
    if (CombatFleeBtn) CombatFleeBtn->SetIsEnabled(bInCombat);
    const bool bDirectionMode = false;
    if (CombatDirectionNorthBtn) CombatDirectionNorthBtn->SetVisibility(bDirectionMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (CombatDirectionWestBtn) CombatDirectionWestBtn->SetVisibility(bDirectionMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (CombatDirectionEastBtn) CombatDirectionEastBtn->SetVisibility(bDirectionMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (CombatDirectionSouthBtn) CombatDirectionSouthBtn->SetVisibility(bDirectionMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (CombatDirectionCancelBtn) CombatDirectionCancelBtn->SetVisibility(bDirectionMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (PlayerAmbushBtn)
    {
        PlayerAmbushBtn->SetVisibility(bInRaid && !bInCombat && !bPlayerAmbushing && !bEnemyAmbush
            ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        PlayerAmbushBtn->SetIsEnabled(bInRaid && !bInCombat && !bEnemyAmbush);
    }
    if (AmbushWaitBtn) AmbushWaitBtn->SetVisibility(bPlayerAmbushing ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (AmbushCancelBtn) AmbushCancelBtn->SetVisibility(bPlayerAmbushing ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (AmbushLetPassBtn) AmbushLetPassBtn->SetVisibility(bHasAmbushTarget ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (AmbushAssaultBtn) AmbushAssaultBtn->SetVisibility(bHasAmbushTarget ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (EnemyAmbushSearchBtn) EnemyAmbushSearchBtn->SetVisibility(bEnemyAmbush ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (EnemyAmbushCoverBtn) EnemyAmbushCoverBtn->SetVisibility(bEnemyAmbush ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (EnemyAmbushFleeBtn) EnemyAmbushFleeBtn->SetVisibility(bEnemyAmbush ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
#if !UE_BUILD_SHIPPING
    if (DebugSpawnEnemyBtn)
    {
        DebugSpawnEnemyBtn->SetVisibility(bInRaid ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        DebugSpawnEnemyBtn->SetIsEnabled(bInRaid && !bInCombat && !bEnemyAmbush);
    }
#endif
    if (MinimapUI && MinimapUI->AdvanceButton)
    {
        MinimapUI->AdvanceButton->SetIsEnabled(bInRaid && !bInCombat && MinimapUI->CurrentPath.Num() > 0);
    }
    if (CompactMinimapUI && CompactMinimapUI->AdvanceButton)
    {
        CompactMinimapUI->AdvanceButton->SetIsEnabled(bInRaid && !bInCombat && CompactMinimapUI->CurrentPath.Num() > 0);
    }
    const bool bFullMapVisible = RightPanelSwitcher && RightPanelSwitcher->GetActiveWidgetIndex() == 1;
    if (CompactMinimapUI)
    {
        CompactMinimapUI->SetVisibility(bInRaid && !bFullMapVisible
            ? ESlateVisibility::Visible
            : ESlateVisibility::Hidden);
    }
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
        UpdateActionAvailability();
    }
}

void UMainGameUI::OnStashButtonClicked()
{
    if (!RightPanelSwitcher) return;

    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM || GM->RaidState == ERaidState::InRaid) return;
    if (!GM->SaveStash()) return;

    GM->PlaySoundEffect(TEXT("UI_Click"));

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

            // START RAID button disappears with the lobby/stash panel, so keyboard focus can be lost.
            // Restore focus to the persistent root UI after the panel switch has completed.
            ScheduleRaidInputFocusRestore();
        }
        else if (GM->RaidState == ERaidState::Lobby)
        {
            GM->PlaySoundEffect(TEXT("UI_Error"));
        }
    }
}

void UMainGameUI::RestoreRaidInputFocus()
{
    bRaidFocusRestorePending = false;
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM || GM->RaidState != ERaidState::InRaid || HasRaidUIFocus() || HasActiveRaidTransientPopup()) return;

    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;

    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(TakeWidget());
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PC->SetInputMode(InputMode);
    SetKeyboardFocus();
    SetUserFocus(PC);
}

bool UMainGameUI::HasRaidUIFocus() const
{
    APlayerController* PC = GetOwningPlayer();
    return PC && (HasUserFocus(PC) || HasUserFocusedDescendants(PC));
}

bool UMainGameUI::HasActiveRaidTransientPopup() const
{
    UWorld* World = GetWorld();
    if (!World) return false;

    TArray<UUserWidget*> Popups;
    const TArray<TSubclassOf<UUserWidget>> PopupClasses = {
        UContextMenuWidget::StaticClass(),
        UInspectWidget::StaticClass(),
        USplitStackWidget::StaticClass()
    };

    for (const TSubclassOf<UUserWidget>& PopupClass : PopupClasses)
    {
        Popups.Reset();
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Popups, PopupClass, true);
        for (UUserWidget* Popup : Popups)
        {
            if (Popup && Popup->IsInViewport() && Popup->GetVisibility() != ESlateVisibility::Collapsed &&
                Popup->GetVisibility() != ESlateVisibility::Hidden)
            {
                return true;
            }
        }
    }

    return false;
}

void UMainGameUI::ScheduleRaidInputFocusRestore()
{
    if (bRaidFocusRestorePending || !GetWorld()) return;
    bRaidFocusRestorePending = true;

    FTimerDelegate FocusDelegate;
    FocusDelegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UMainGameUI, RestoreRaidInputFocus));
    GetWorld()->GetTimerManager().SetTimerForNextTick(FocusDelegate);
}

void UMainGameUI::OnExtractButtonClicked()
{
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (GM && !GM->ExtractRaid()) GM->PlaySoundEffect(TEXT("UI_Error"));
}

void UMainGameUI::OnSearchButtonClicked()
{
    if (AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        if (GM->HasDeadBodyAtCurrentPlayerCoord())
        {
            if (GM->RequestSearchDeadBody()) GM->PlaySoundEffect(TEXT("UI_Click"));
            else GM->PlaySoundEffect(TEXT("UI_Error"));
        }
        else
        {
            GM->StartContainerSearch();
        }
    }
}

void UMainGameUI::SetLootInventory(UGridInventoryComponent* Inventory)
{
    if (!ContainerBoard || !Inventory) return;
    if (LootContainerTitle)
    {
        const AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
        LootContainerTitle->SetText(FText::FromString(
            GM && GM->HasDeadBodyAtCurrentPlayerCoord() ? TEXT("DEAD BODY") : TEXT("Loot Container")));
    }
    ContainerBoard->InventoryComponent = Inventory;
    Inventory->OnInventoryChanged.AddUniqueDynamic(ContainerBoard, &UGridBoardWidget::RefreshGridUI);
    ContainerBoard->RefreshGridUI();
}

void UMainGameUI::ClearCorpseLootView()
{
    if (AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        SetLootInventory(GM->LootContainerComponent);
    }
}

void UMainGameUI::RefreshEnemyMarkers()
{
    if (MinimapUI) MinimapUI->RefreshEnemyDebugMarkers();
    if (CompactMinimapUI) CompactMinimapUI->RefreshEnemyDebugMarkers();
}

void UMainGameUI::OnDebugSpawnEnemyClicked()
{
#if !UE_BUILD_SHIPPING
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM || GM->RaidState != ERaidState::InRaid || !GM->EnemyManagerComponent)
    {
        QueueEventNotification(TEXT("DEBUG SPAWN은 레이드 중에만 사용할 수 있습니다."));
        return;
    }
    if (GM->EnemyManagerComponent->DebugSpawnNearestScav())
    {
        const TArray<FEnemyWorldInstance>& Enemies = GM->EnemyManagerComponent->GetEnemyInstances();
        if (Enemies.Num() > 0)
        {
            const FEnemyWorldInstance& Enemy = Enemies.Last();
            QueueEventNotification(FString::Printf(TEXT("DEBUG Enemy Spawned: SCAV @ %d-%d"),
                Enemy.Coordinate.X, Enemy.Coordinate.Y));
        }
        return;
    }
    QueueEventNotification(TEXT("유효한 DEBUG Spawn Tile을 찾지 못했습니다."));
#endif
}

FReply UMainGameUI::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    FKey Key = InKeyEvent.GetKey();
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (Key == EKeys::Escape && CombatDirectionMode == ECombatDirectionMode::Flee)
    {
        SetCombatDirectionMode(ECombatDirectionMode::None);
        return FReply::Handled();
    }

    ECombatMovementDirection Direction = ECombatMovementDirection::North;
    bool bHasDirection = true;
    if (Key == EKeys::W) Direction = ECombatMovementDirection::North;
    else if (Key == EKeys::A) Direction = ECombatMovementDirection::West;
    else if (Key == EKeys::S) Direction = ECombatMovementDirection::South;
    else if (Key == EKeys::D) Direction = ECombatMovementDirection::East;
    else bHasDirection = false;
    if (bHasDirection && GM && GM->RaidState == ERaidState::InRaid)
    {
        if (CombatDirectionMode == ECombatDirectionMode::Flee)
        {
            RequestCombatDirection(Direction);
        }
        else if (GM->CombatComponent && GM->CombatComponent->bHasActiveEnemy)
        {
            if (!GM->CombatComponent->RequestCombatMovementDirection(
                ECombatMovementAction::Approach, Direction) &&
                !GM->CombatComponent->LastCombatMessage.IsEmpty())
            {
                QueueEventNotification(GM->CombatComponent->LastCombatMessage);
            }
        }
        else
        {
            const FIntPoint Delta = Direction == ECombatMovementDirection::North ? FIntPoint(0, -1) :
                Direction == ECombatMovementDirection::West ? FIntPoint(-1, 0) :
                Direction == ECombatMovementDirection::East ? FIntPoint(1, 0) : FIntPoint(0, 1);
            GM->RequestPlayerCardinalMove(Delta);
        }
        return FReply::Handled();
    }
    if (Key == EKeys::One)
    {
        if (GM && GM->CombatComponent)
        {
            if (!GM->CombatComponent->RequestWeaponSwap(TEXT("Primary1")) &&
                !GM->CombatComponent->LastCombatMessage.IsEmpty())
            {
                QueueEventNotification(GM->CombatComponent->LastCombatMessage);
            }
        }
        return FReply::Handled();
    }
    else if (Key == EKeys::Two)
    {
        if (GM && GM->CombatComponent)
        {
            if (!GM->CombatComponent->RequestWeaponSwap(TEXT("Primary2")) &&
                !GM->CombatComponent->LastCombatMessage.IsEmpty())
            {
                QueueEventNotification(GM->CombatComponent->LastCombatMessage);
            }
        }
        return FReply::Handled();
    }
    else if (Key == EKeys::R)
    {
        if (UItemDragDropOperation* ActiveDrag = UItemDragDropOperation::GetActiveOperation())
        {
            ActiveDrag->TogglePreviewRotation();
            return FReply::Handled();
        }

        if (GM && GM->CombatComponent)
        {
            if (!GM->CombatComponent->RequestReload() &&
                !GM->CombatComponent->LastCombatMessage.IsEmpty())
            {
                QueueEventNotification(GM->CombatComponent->LastCombatMessage);
            }
        }
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UMainGameUI::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        if (GM->RaidState == ERaidState::InRaid && !HasRaidUIFocus() && !HasActiveRaidTransientPopup())
        {
            ScheduleRaidInputFocusRestore();
        }
    }
    if (MinimapUI) MinimapUI->RefreshEnemyDebugMarkers();
    if (CompactMinimapUI) CompactMinimapUI->RefreshEnemyDebugMarkers();
    UpdateEnemyEventLog();
    UpdateCombatUI();
    UpdateStatusPanel();
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
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (GM && GM->CombatComponent)
    {
        ActiveWeaponSlot = GM->CombatComponent->ActiveWeaponSlot;
        if (WeaponSlot1) WeaponSlot1->SetHighlight(ActiveWeaponSlot == TEXT("Primary1"));
        if (WeaponSlot2) WeaponSlot2->SetHighlight(ActiveWeaponSlot == TEXT("Primary2"));
    }

    if (CombatActionText && GM && GM->CombatComponent)
    {
        FString ActionText = TEXT("READY");
        if (GM->CombatComponent->PlayerActionState == ECombatPlayerActionState::Swapping)
        {
            ActionText = FString::Printf(TEXT("SWAPPING %.1fs"), GM->CombatComponent->PlayerActionTimeRemaining);
        }
        else if (GM->CombatComponent->PlayerActionState == ECombatPlayerActionState::Reloading)
        {
            ActionText = FString::Printf(TEXT("RELOADING %.1fs"), GM->CombatComponent->PlayerActionTimeRemaining);
        }
        else if (GM->CombatComponent->PlayerAttackCooldownRemaining > KINDA_SMALL_NUMBER)
        {
            ActionText = FString::Printf(TEXT("COOLDOWN %.1fs"), GM->CombatComponent->PlayerAttackCooldownRemaining);
        }
        ActionText += FString::Printf(TEXT("\nRECOIL %.0f"), GM->CombatComponent->CurrentRecoil);
        CombatActionText->SetText(FText::FromString(ActionText));
    }

    if (!CombatText) return;

    if (!GM || !GM->CombatComponent || GM->RaidState != ERaidState::InRaid ||
        GM->CombatComponent->CurrentEnemy.Definition.EnemyID.IsNone())
    {
        const FString Message = GM && GM->CombatComponent && GM->RaidState == ERaidState::InRaid
            ? GM->CombatComponent->LastCombatMessage
            : TEXT("");
        FString StatusText = TEXT("Enemy: None");
        if (GM && GM->RaidState == ERaidState::InRaid)
        {
            if (GM->EnemyManagerComponent && GM->EnemyManagerComponent->HasActiveAmbushReaction())
            {
                StatusText = TEXT("AMBUSHED\nSEARCH / COVER / FLEE");
            }
            else if (GM->PlayerPosture == EPlayerRaidPosture::Ambushing)
            {
                FName AmbushTargetID = NAME_None;
                StatusText = GM->EnemyManagerComponent &&
                    GM->EnemyManagerComponent->FindPlayerAmbushTarget(AmbushTargetID)
                    ? TEXT("AMBUSHING\nLET PASS / ASSAULT") : TEXT("AMBUSHING\nWAIT");
            }
        }
        CombatText->SetText(FText::FromString(Message.IsEmpty() ? StatusText : FString::Printf(TEXT("%s\n%s"), *StatusText, *Message)));
        if (!Message.IsEmpty() && Message != LastDisplayedCombatMessage)
        {
            LastDisplayedCombatMessage = Message;
            QueueEventNotification(Message);
        }
        return;
    }

    const FEnemyInstanceData& Enemy = GM->CombatComponent->CurrentEnemy;
    int32 Distance = -1;
    FIntPoint EnemyCoordinate;
    if (GM->EnemyManagerComponent && GM->MapManagerComponent &&
        GM->EnemyManagerComponent->GetActiveEnemyCoordinate(EnemyCoordinate))
    {
        Distance = GM->MapManagerComponent->GetTileDistance(GM->CurrentPlayerCoord, EnemyCoordinate);
    }
    const FString StateText = GM->CombatComponent->bHasActiveEnemy ?
        FString::Printf(TEXT("HP: %d / %d | Distance: %d tiles"), Enemy.CurrentHealth, Enemy.Definition.MaxHealth, Distance) :
        TEXT("DEFEATED");
    const FString Message = GM->CombatComponent->LastCombatMessage;
    UItemInstance* ActiveWeapon = GM->EquipmentComponent
        ? GM->EquipmentComponent->GetEquippedItem(GM->CombatComponent->ActiveWeaponSlot) : nullptr;
    const FString WeaponText = ActiveWeapon
        ? ActiveWeapon->WeaponAttackType == EWeaponAttackType::Firearm && ActiveWeapon->EquippedMagazine
            ? FString::Printf(TEXT("%s | %d / %d"), *ActiveWeapon->ItemName,
                ActiveWeapon->EquippedMagazine->CurrentAmmo, ActiveWeapon->EquippedMagazine->MaxAmmo)
            : ActiveWeapon->ItemName
        : TEXT("No weapon");
    if (BangButtonText)
    {
        BangButtonText->SetText(FText::FromString(ActiveWeapon &&
            ActiveWeapon->WeaponAttackType == EWeaponAttackType::Melee ? TEXT("ATTACK") : TEXT("FIRE")));
    }
    const FString CombatStatus = FString::Printf(TEXT("Enemy: %s (%s)\nWeapon: %s"), *Enemy.Definition.DisplayName, *StateText, *WeaponText);
    CombatText->SetText(FText::FromString(Message.IsEmpty() ? CombatStatus : FString::Printf(TEXT("%s\n%s"), *CombatStatus, *Message)));
    if (!Message.IsEmpty() && Message != LastDisplayedCombatMessage)
    {
        LastDisplayedCombatMessage = Message;
        QueueEventNotification(Message);
    }

}

void UMainGameUI::UpdateStatusPanel()
{
    if (!StatusPanelText) return;
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM || !GM->CombatComponent) return;

    UItemInstance* StatusWeapon = GM->EquipmentComponent
        ? GM->EquipmentComponent->GetEquippedItem(GM->CombatComponent->ActiveWeaponSlot) : nullptr;
    FString AmmoText = TEXT("-");
    if (StatusWeapon && StatusWeapon->EquippedMagazine)
    {
        AmmoText = FString::Printf(TEXT("%d / %d"), StatusWeapon->EquippedMagazine->CurrentAmmo,
            StatusWeapon->EquippedMagazine->MaxAmmo);
    }
    FString ActionState = TEXT("READY");
    if (GM->CombatComponent->PlayerActionState == ECombatPlayerActionState::Swapping)
    {
        ActionState = TEXT("SWAPPING");
    }
    else if (GM->CombatComponent->PlayerActionState == ECombatPlayerActionState::Reloading)
    {
        ActionState = TEXT("RELOADING");
    }
    const FString EnemyText = GM->CombatComponent->bHasActiveEnemy
        ? GM->CombatComponent->CurrentEnemy.Definition.DisplayName : TEXT("NONE");
    const int32 ElapsedSeconds = FMath::Max(0, FMath::FloorToInt(GM->TotalTimeLimit - GM->RemainingTime));
    StatusPanelText->SetText(FText::FromString(FString::Printf(
        TEXT("HP %d / %d\nTIME %02d:%02d  SCORE %d / %d\nWEAPON %s\nAMMO %s  STATE %s\nRECOIL %.0f  ENEMY %s"),
        GM->CurrentHealth, GM->MaxHealth, ElapsedSeconds / 60, ElapsedSeconds % 60,
        GM->CurrentScore, GM->QuotaScore, StatusWeapon ? *StatusWeapon->ItemName : TEXT("NONE"),
        *AmmoText, *ActionState, GM->CombatComponent->CurrentRecoil, *EnemyText)));
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

void UMainGameUI::UpdateEnemyEventLog()
{
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM || GM->RaidState != ERaidState::InRaid || !GM->EnemyManagerComponent)
    {
        if (bEnemyEventLogRaidActive)
        {
            LastEnemyCoordinates.Empty();
            LastEnemyKnowledgeStates.Empty();
            bEnemyEventLogRaidActive = false;
            bLastCombatActive = false;
        }
        return;
    }

    bEnemyEventLogRaidActive = true;
    TSet<FName> AliveEnemyIDs;
    for (const FEnemyWorldInstance& Enemy : GM->EnemyManagerComponent->GetEnemyInstances())
    {
        if (!Enemy.bAlive) continue;
        AliveEnemyIDs.Add(Enemy.InstanceID);
        const FString EnemyName = Enemy.Definition.DisplayName.IsEmpty()
            ? Enemy.InstanceID.ToString() : Enemy.Definition.DisplayName;
        if (!LastEnemyCoordinates.Contains(Enemy.InstanceID))
        {
            AddEventLogEntry(FString::Printf(TEXT("Enemy Spawned: %s @ %d-%d"), *EnemyName,
                Enemy.Coordinate.X, Enemy.Coordinate.Y));
        }
        else if (LastEnemyCoordinates[Enemy.InstanceID] != Enemy.Coordinate)
        {
            AddEventLogEntry(FString::Printf(TEXT("%s moved %d-%d -> %d-%d"), *EnemyName,
                LastEnemyCoordinates[Enemy.InstanceID].X, LastEnemyCoordinates[Enemy.InstanceID].Y,
                Enemy.Coordinate.X, Enemy.Coordinate.Y));
        }
        if (EEnemyKnowledgeState* PreviousState = LastEnemyKnowledgeStates.Find(Enemy.InstanceID))
        {
            if (*PreviousState != Enemy.KnowledgeState)
            {
                const TCHAR* StateName = Enemy.KnowledgeState == EEnemyKnowledgeState::Suspected
                    ? TEXT("Suspected") : Enemy.KnowledgeState == EEnemyKnowledgeState::Revealed
                        ? TEXT("Revealed") : TEXT("Hidden");
                AddEventLogEntry(FString::Printf(TEXT("Enemy %s: %s"), StateName, *EnemyName));
            }
        }
        LastEnemyCoordinates.Add(Enemy.InstanceID, Enemy.Coordinate);
        LastEnemyKnowledgeStates.Add(Enemy.InstanceID, Enemy.KnowledgeState);
    }

    TArray<FName> KnownEnemyIDs;
    LastEnemyCoordinates.GetKeys(KnownEnemyIDs);
    for (const FName EnemyID : KnownEnemyIDs)
    {
        if (!AliveEnemyIDs.Contains(EnemyID))
        {
            AddEventLogEntry(FString::Printf(TEXT("Enemy Killed: %s"), *EnemyID.ToString()));
            LastEnemyCoordinates.Remove(EnemyID);
            LastEnemyKnowledgeStates.Remove(EnemyID);
        }
    }

    const bool bCombatActive = GM->CombatComponent && GM->CombatComponent->bHasActiveEnemy;
    if (bCombatActive && !bLastCombatActive)
    {
        AddEventLogEntry(TEXT("Combat Started"));
    }
    bLastCombatActive = bCombatActive;
}

void UMainGameUI::QueueEventNotification(FString Message)
{
    if (Message.IsEmpty() || !EventNotificationText) return;

    AddEventLogEntry(Message);
}

void UMainGameUI::AddEventLogEntry(const FString& Message)
{
    if (Message.IsEmpty()) return;

    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    const int32 ElapsedSeconds = GM ? FMath::Max(0, FMath::FloorToInt(GM->TotalTimeLimit - GM->RemainingTime)) : 0;
    EventLogEntries.Add(FString::Printf(TEXT("[%02d:%02d] %s"), ElapsedSeconds / 60, ElapsedSeconds % 60, *Message));
    while (EventLogEntries.Num() > 10)
    {
        EventLogEntries.RemoveAt(0);
    }

    if (!EventLogScrollBox) return;

    EventLogScrollBox->ClearChildren();
    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Title->SetText(FText::FromString(TEXT("EVENT LOG")));
    Title->SetColorAndOpacity(TacticalTextSecondary);
    EventLogScrollBox->AddChild(Title);
    for (const FString& Entry : EventLogEntries)
    {
        UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Line->SetText(FText::FromString(Entry));
        Line->SetColorAndOpacity(TacticalTextSecondary);
        EventLogScrollBox->AddChild(Line);
    }
    EventLogScrollBox->ScrollToEnd();
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
    if (GM && (GM->PlayerPosture == EPlayerRaidPosture::Ambushing ||
        (GM->EnemyManagerComponent && GM->EnemyManagerComponent->HasActiveAmbushReaction())))
    {
        QueueEventNotification(TEXT("매복 중에는 판매할 수 없습니다."));
        return;
    }
    if (GM && GM->RaidState != ERaidState::InRaid && GM->InventoryComponent && GM->ItemDataTable)
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
            GM->RetirementBalance += TotalValue;
            GM->SaveStash();
            UpdateActionAvailability();
            GM->PlaySoundEffect(TEXT("UI_Click"));
        }
    }
}

void UMainGameUI::OnSellAllButtonClicked()
{
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM || GM->RaidState == ERaidState::InRaid || !GM->ItemDataTable) return;

    if (GM->PlayerPosture == EPlayerRaidPosture::Ambushing ||
        (GM->EnemyManagerComponent && GM->EnemyManagerComponent->HasActiveAmbushReaction()))
    {
        QueueEventNotification(TEXT("매복 중에는 판매할 수 없습니다."));
        return;
    }

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
        GM->RetirementBalance += TotalValue;
        GM->SaveStash();
        UpdateActionAvailability();
        GM->PlaySoundEffect(TEXT("UI_Click"));
    }
}

void UMainGameUI::OnRetirementClicked()
{
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM || GM->RaidState == ERaidState::InRaid || GM->RetirementBalance < AGridGameMode::RetirementGoal)
    {
        return;
    }

    if (RetirementEndingOverlay)
    {
        RetirementEndingOverlay->SetVisibility(ESlateVisibility::Visible);
        GM->PlaySoundEffect(TEXT("Retirement_Ending"));
    }
}

void UMainGameUI::OnRetirementReturnClicked()
{
    if (AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        GM->PlaySoundEffect(TEXT("UI_Click"));
    }
    if (RetirementEndingOverlay)
    {
        RetirementEndingOverlay->SetVisibility(ESlateVisibility::Collapsed);
    }
    UpdateActionAvailability();
}

void UMainGameUI::OnBangButtonClicked()
{
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM || GM->RaidState != ERaidState::InRaid) return;

    if (!GM->CombatComponent || !GM->CombatComponent->bHasActiveEnemy)
    {
        GM->PlaySoundEffect(TEXT("UI_Error"));
        QueueEventNotification(TEXT("공격할 적이 없습니다."));
        return;
    }

    if (!GM->EquipmentComponent)
    {
        GM->PlaySoundEffect(TEXT("UI_Error"));
        QueueEventNotification(TEXT("무기를 준비할 수 없습니다."));
        return;
    }

    UItemInstance* ActiveWeapon = GM->EquipmentComponent->GetEquippedItem(ActiveWeaponSlot);
    if (!ActiveWeapon)
    {
        GM->PlaySoundEffect(TEXT("UI_Error"));
        QueueEventNotification(TEXT("선택한 무기가 없습니다."));
        return;
    }

    if (ActiveWeapon->Damage <= 0)
    {
        GM->PlaySoundEffect(TEXT("UI_Error"));
        QueueEventNotification(TEXT("이 무기로 공격할 수 없습니다."));
        return;
    }

    const bool bNeedsAmmo = ActiveWeapon->WeaponAttackType == EWeaponAttackType::Firearm;
    if (bNeedsAmmo && !ActiveWeapon->EquippedMagazine)
    {
        GM->PlaySoundEffect(TEXT("UI_Error"));
        QueueEventNotification(TEXT("탄창이 장착되지 않았습니다."));
        return;
    }

    if (bNeedsAmmo && ActiveWeapon->EquippedMagazine->CurrentAmmo <= 0)
    {
        GM->PlaySoundEffect(TEXT("UI_Error"));
        QueueEventNotification(TEXT("탄약이 없습니다."));
        return;
    }

    if (!GM->CombatComponent->RequestPlayerAttack(ActiveWeapon->Damage,
        ActiveWeapon->BaseAccuracyPercent, ActiveWeapon->AttackIntervalSeconds, ActiveWeapon->MaxRangeTiles,
        ActiveWeapon->RecoilPerShot, ActiveWeapon->RecoilRecoveryPerSecond, ActiveWeapon->OptimalRangeTiles))
    {
        GM->PlaySoundEffect(TEXT("UI_Error"));
        if (!GM->CombatComponent->LastCombatMessage.IsEmpty())
        {
            QueueEventNotification(GM->CombatComponent->LastCombatMessage);
        }
        return;
    }

    if (bNeedsAmmo)
    {
        ActiveWeapon->EquippedMagazine->CurrentAmmo--;
        ActiveWeapon->EquippedMagazine->OnItemModified.Broadcast();
        ActiveWeapon->OnItemModified.Broadcast();
    }

    // UI 갱신
    if (ActiveWeaponSlot == TEXT("Primary1") && WeaponSlot1) WeaponSlot1->RefreshSlotUI();
    else if (ActiveWeaponSlot == TEXT("Primary2") && WeaponSlot2) WeaponSlot2->RefreshSlotUI();
}

void UMainGameUI::OnReloadButtonClicked()
{
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (GM && GM->CombatComponent && !GM->CombatComponent->RequestReload() &&
        !GM->CombatComponent->LastCombatMessage.IsEmpty())
    {
        GM->PlaySoundEffect(TEXT("UI_Error"));
        QueueEventNotification(GM->CombatComponent->LastCombatMessage);
    }
}

void UMainGameUI::SetCombatDirectionMode(ECombatDirectionMode InMode)
{
    CombatDirectionMode = InMode;
    UpdateActionAvailability();
    if (CombatActionText && InMode != ECombatDirectionMode::None)
    {
        CombatActionText->SetText(FText::FromString(
            InMode == ECombatDirectionMode::Move
                ? TEXT("SELECT MOVE DIRECTION") : TEXT("FLEE: W/A/S/D 방향 선택, ESC 취소")));
    }
}

void UMainGameUI::RequestCombatDirection(ECombatMovementDirection Direction)
{
    if (CombatDirectionMode == ECombatDirectionMode::None) return;

    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    const ECombatMovementAction Action = CombatDirectionMode == ECombatDirectionMode::Flee
        ? ECombatMovementAction::Flee : ECombatMovementAction::Approach;
    if (!GM || !GM->CombatComponent || !GM->CombatComponent->RequestCombatMovementDirection(Action, Direction))
    {
        if (GM && GM->CombatComponent && !GM->CombatComponent->LastCombatMessage.IsEmpty())
        {
            QueueEventNotification(GM->CombatComponent->LastCombatMessage);
        }
        return;
    }
    CombatDirectionMode = ECombatDirectionMode::None;
    UpdateActionAvailability();
}

void UMainGameUI::OnCombatMoveButtonClicked()
{
    SetCombatDirectionMode(ECombatDirectionMode::Move);
}

void UMainGameUI::OnCombatDirectionNorthButtonClicked() { RequestCombatDirection(ECombatMovementDirection::North); }
void UMainGameUI::OnCombatDirectionWestButtonClicked() { RequestCombatDirection(ECombatMovementDirection::West); }
void UMainGameUI::OnCombatDirectionEastButtonClicked() { RequestCombatDirection(ECombatMovementDirection::East); }
void UMainGameUI::OnCombatDirectionSouthButtonClicked() { RequestCombatDirection(ECombatMovementDirection::South); }
void UMainGameUI::OnCombatDirectionCancelButtonClicked()
{
    SetCombatDirectionMode(ECombatDirectionMode::None);
}

void UMainGameUI::OnApproachButtonClicked()
{
    if (AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        if (GM->CombatComponent) GM->CombatComponent->RequestCombatMovement(ECombatMovementAction::Approach);
    }
}

void UMainGameUI::OnRetreatButtonClicked()
{
    if (AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        if (GM->CombatComponent) GM->CombatComponent->RequestCombatMovement(ECombatMovementAction::Retreat);
    }
}

void UMainGameUI::OnCombatFleeButtonClicked()
{
    SetCombatDirectionMode(ECombatDirectionMode::Flee);
}

void UMainGameUI::OnPlayerAmbushButtonClicked()
{
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (GM && !GM->RequestPlayerAmbush()) QueueEventNotification(TEXT("AMBUSH를 시작할 수 없습니다."));
}

void UMainGameUI::OnAmbushWaitButtonClicked()
{
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (GM && !GM->RequestAmbushWait()) QueueEventNotification(TEXT("AMBUSH 대기 요청이 거부되었습니다."));
}

void UMainGameUI::OnAmbushCancelButtonClicked()
{
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (GM && !GM->RequestAmbushCancel()) QueueEventNotification(TEXT("AMBUSH 취소 요청이 거부되었습니다."));
}

void UMainGameUI::OnAmbushLetPassButtonClicked()
{
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (GM && !GM->RequestAmbushLetPass()) QueueEventNotification(TEXT("LET PASS 요청이 거부되었습니다."));
}

void UMainGameUI::OnAmbushAssaultButtonClicked()
{
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (GM && !GM->RequestAmbushAssault()) QueueEventNotification(TEXT("ASSAULT 요청이 거부되었습니다."));
}

void UMainGameUI::OnEnemyAmbushSearchButtonClicked()
{
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (GM && !GM->RequestAmbushSearch()) QueueEventNotification(TEXT("SEARCH 요청이 거부되었습니다."));
}

void UMainGameUI::OnEnemyAmbushCoverButtonClicked()
{
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (GM && !GM->RequestAmbushCover()) QueueEventNotification(TEXT("COVER 요청이 거부되었습니다."));
}

void UMainGameUI::OnEnemyAmbushFleeButtonClicked()
{
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (GM && !GM->RequestAmbushFlee()) QueueEventNotification(TEXT("FLEE 요청이 거부되었습니다."));
}
