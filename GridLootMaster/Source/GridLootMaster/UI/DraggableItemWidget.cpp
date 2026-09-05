#include "DraggableItemWidget.h"
#include "ItemDragDropOperation.h"
#include "SplitStackWidget.h"
#include "ContextMenuWidget.h"
#include "InspectWidget.h"
#include "../GridInventoryComponent.h"
#include "../ItemInstance.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Input/Reply.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Components/ProgressBar.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/Image.h"
#include "../GridGameMode.h"
#include "Kismet/GameplayStatics.h"

static TWeakObjectPtr<UItemDragDropOperation> GActiveItemDragOperation;

void UItemDragDropOperation::TogglePreviewRotation()
{
    bPreviewRotation = !bPreviewRotation;
    if (DragVisual)
    {
        DragVisual->SetDragPreviewRotation(bPreviewRotation);
    }
}

void UItemDragDropOperation::SetActiveOperation(UItemDragDropOperation* InOperation)
{
    GActiveItemDragOperation = InOperation;
}

void UItemDragDropOperation::ClearActiveOperation(const UItemDragDropOperation* InOperation)
{
    if (GActiveItemDragOperation.Get() == InOperation)
    {
        GActiveItemDragOperation.Reset();
    }
}

UItemDragDropOperation* UItemDragDropOperation::GetActiveOperation()
{
    return GActiveItemDragOperation.Get();
}

void UDraggableItemWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(true); 
}

void UDraggableItemWidget::HandleItemModified()
{
    InitWidgetUI(bIsEquippedVisual);
}

void UDraggableItemWidget::InitWidgetUI(bool bEquipped)
{
    if (!WidgetTree || !ItemObj) return;

    bIsEquippedVisual = bEquipped;

    ItemObj->OnItemModified.RemoveDynamic(this, &UDraggableItemWidget::HandleItemModified);
    ItemObj->OnItemModified.AddUniqueDynamic(this, &UDraggableItemWidget::HandleItemModified);

    if (!WidgetTree->RootWidget)
    {
        USizeBox* RootBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RootBox"));
        WidgetTree->RootWidget = RootBox;
    }

    USizeBox* RootBox = Cast<USizeBox>(WidgetTree->RootWidget);
    RootBox->ClearChildren();

    if (bEquipped)
    {
        RootBox->ClearWidthOverride();
        RootBox->ClearHeightOverride();
    }
    else
    {
        FIntPoint CurrentSize = ItemObj->GetCurrentSize();
        if (bHasDragPreviewRotation && bDragPreviewRotation != ItemObj->bIsRotated)
        {
            CurrentSize = FIntPoint(CurrentSize.Y, CurrentSize.X);
        }
        RootBox->SetWidthOverride(CurrentSize.X * 64.0f);
        RootBox->SetHeightOverride(CurrentSize.Y * 64.0f);
    }

    UBorder* BG = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    BG->SetClipping(EWidgetClipping::ClipToBounds); // 텍스트 삐져나감 방지
    
    // Rarity 등급 배경색
    FLinearColor RarityColor;
    if (ItemObj->Category == EItemCategory::Weapon)
    {
        // 무기의 경우 기본 희귀도 적용(그레이/블랙) 또는 나중에 추가
        RarityColor = FLinearColor(0.25f, 0.25f, 0.25f, 1.0f);
    }
    else
    {
        switch (ItemObj->Rarity)
        {
            case EItemRarity::Common:    RarityColor = FLinearColor(0.3f, 0.3f, 0.3f, 1.0f); break; 
            case EItemRarity::Uncommon:  RarityColor = FLinearColor(0.2f, 0.8f, 0.2f, 1.0f); break; 
            case EItemRarity::Rare:      RarityColor = FLinearColor(0.1f, 0.4f, 1.0f, 1.0f); break; 
            case EItemRarity::Epic:      RarityColor = FLinearColor(0.6f, 0.1f, 0.8f, 1.0f); break; 
            case EItemRarity::Legendary: RarityColor = FLinearColor(1.0f, 0.8f, 0.1f, 1.0f); break; 
            case EItemRarity::Mythic:    RarityColor = FLinearColor(1.0f, 0.1f, 0.1f, 1.0f); break; 
            default:                     RarityColor = FLinearColor(0.3f, 0.3f, 0.3f, 1.0f); break;
        }
    }

    // 미식별 상태일 경우 어둡게
    if (!ItemObj->bIsExamined)
    {
        RarityColor = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);
    }

    FLinearColor DarkTint = FLinearColor(RarityColor.R * 0.3f, RarityColor.G * 0.3f, RarityColor.B * 0.3f, 0.85f);
    BG->SetBrushColor(DarkTint);
    RootBox->AddChild(BG);
    
    // UBorder에 자식을 하나만 넣을 수 있으므로 UOverlay를 넣습니다.
    UOverlay* ContentOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    ContentOverlay->SetClipping(EWidgetClipping::ClipToBounds);
    BG->AddChild(ContentOverlay);
    
    // 아이콘 표시 (우선)
    if (UTexture2D* IconTex = ItemObj->GetDynamicIcon())
    {
        UImage* IconImg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        IconImg->SetBrushFromTexture(IconTex);
        
        if (UOverlaySlot* IconSlot = Cast<UOverlaySlot>(ContentOverlay->AddChild(IconImg)))
        {
            IconSlot->SetHorizontalAlignment(HAlign_Center);
            IconSlot->SetVerticalAlignment(VAlign_Center);
            IconSlot->SetPadding(FMargin(5.0f));
            // IconImg->SetBrushSize(FVector2D(IconTex->GetSizeX(), IconTex->GetSizeY())); 
        }
    }

    // VBox for Name and CompatibleAmmo
    UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    VBox->SetClipping(EWidgetClipping::ClipToBounds);
    if (UOverlaySlot* VBoxSlot = Cast<UOverlaySlot>(ContentOverlay->AddChild(VBox)))
    {
        VBoxSlot->SetHorizontalAlignment(HAlign_Fill);
        VBoxSlot->SetVerticalAlignment(VAlign_Top);
        VBoxSlot->SetPadding(FMargin(4.0f, 2.0f, 4.0f, 0.0f));
    }

    // --- 텍스트 블록 (이름) ---
    UTextBlock* NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    FString DisplayName = ItemObj->ItemName;
    
    if (!ItemObj->bIsExamined)
    {
        DisplayName = TEXT("???");
        NameText->SetColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f));
    }
    else
    {
        NameText->SetColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f, 1.0f));
    }

    NameText->SetText(FText::FromString(DisplayName));
    FSlateFontInfo NameFont = NameText->GetFont();
    NameFont.Size = 9; // 줄임
    NameText->SetFont(NameFont);
    NameText->SetShadowOffset(FVector2D(1.0f, 1.0f));
    NameText->SetShadowColorAndOpacity(FLinearColor::Black);
    NameText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
    NameText->SetClipping(EWidgetClipping::ClipToBounds);
    
    VBox->AddChild(NameText);

    // --- 텍스트 블록 (호환 탄약) ---
    if (ItemObj->Category == EItemCategory::Weapon && !ItemObj->CompatibleAmmo.IsEmpty())
    {
        UTextBlock* AmmoTypeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        AmmoTypeText->SetText(FText::FromString(ItemObj->CompatibleAmmo));
        AmmoTypeText->SetColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f));
        FSlateFontInfo AmmoFont = AmmoTypeText->GetFont();
        AmmoFont.Size = 8; // 줄임
        AmmoTypeText->SetFont(AmmoFont);
        AmmoTypeText->SetShadowOffset(FVector2D(1.0f, 1.0f));
        AmmoTypeText->SetShadowColorAndOpacity(FLinearColor::Black);
        AmmoTypeText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
        AmmoTypeText->SetClipping(EWidgetClipping::ClipToBounds);
        
        VBox->AddChild(AmmoTypeText);
    }

    // --- 텍스트 설정 (탄약 수 / 최대 탄약 수) ---
    FString AmmoStr = TEXT("");
    if (ItemObj->Category == EItemCategory::Weapon)
    {
        if (ItemObj->EquippedMagazine)
        {
            AmmoStr = FString::Printf(TEXT("%d/%d"), ItemObj->EquippedMagazine->CurrentAmmo, ItemObj->EquippedMagazine->MaxAmmo);
        }
        else
        {
            // 탄창이 장착되지 않은 경우
            AmmoStr = TEXT("0/0");
        }
    }
    else if (ItemObj->Category == EItemCategory::Attachment && ItemObj->AttachmentType == EAttachmentType::Magazine)
    {
        AmmoStr = FString::Printf(TEXT("%d/%d"), ItemObj->CurrentAmmo, ItemObj->MaxAmmo);
    }

    if (!AmmoStr.IsEmpty())
    {
        UTextBlock* AmmoCountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        AmmoCountText->SetText(FText::FromString(AmmoStr));
        AmmoCountText->SetColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f));
        FSlateFontInfo AmmoCountFont = AmmoCountText->GetFont();
        AmmoCountFont.Size = 10; // 줄임
        AmmoCountText->SetFont(AmmoCountFont);
        AmmoCountText->SetShadowOffset(FVector2D(1.0f, 1.0f));
        AmmoCountText->SetShadowColorAndOpacity(FLinearColor::Black);
        AmmoCountText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
        AmmoCountText->SetClipping(EWidgetClipping::ClipToBounds);

        if (UOverlaySlot* TextSlot = Cast<UOverlaySlot>(ContentOverlay->AddChild(AmmoCountText)))
        {
            TextSlot->SetHorizontalAlignment(HAlign_Right);
            TextSlot->SetVerticalAlignment(VAlign_Bottom);
            TextSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 2.0f));
        }
    }

    // --- 텍스트 설정 (수량/스택 표시) ---
    if (ItemObj->bIsExamined && ItemObj->IsStackable())
    {
        UTextBlock* StackText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        StackText->SetText(FText::FromString(FString::FromInt(ItemObj->CurrentStack)));
        StackText->SetColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f));
        FSlateFontInfo StackFont = StackText->GetFont();
        StackFont.Size = 9; // 줄임
        StackText->SetFont(StackFont);
        StackText->SetShadowOffset(FVector2D(1.0f, 1.0f));
        StackText->SetShadowColorAndOpacity(FLinearColor::Black);
        StackText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
        StackText->SetClipping(EWidgetClipping::ClipToBounds);

        if (UOverlaySlot* StackSlot = Cast<UOverlaySlot>(ContentOverlay->AddChild(StackText)))
        {
            StackSlot->SetHorizontalAlignment(HAlign_Right);
            StackSlot->SetVerticalAlignment(VAlign_Bottom);
            StackSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 2.0f));
        }
    }

    // --- 프로그레스 바 ---
    ExamineProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
    ExamineProgressBar->SetPercent(0.0f);
    FProgressBarStyle ProgStyle = ExamineProgressBar->GetWidgetStyle();
    ProgStyle.FillImage.TintColor = FLinearColor::White;
    ExamineProgressBar->SetWidgetStyle(ProgStyle);
    ExamineProgressBar->SetVisibility(ESlateVisibility::Hidden);
    
    if (UOverlaySlot* ProgressSlot = Cast<UOverlaySlot>(ContentOverlay->AddChild(ExamineProgressBar)))
    {
        ProgressSlot->SetHorizontalAlignment(HAlign_Fill);
        ProgressSlot->SetVerticalAlignment(VAlign_Bottom);
        ProgressSlot->SetPadding(FMargin(4.0f, 0.0f, 4.0f, 4.0f));
    }
}

FReply UDraggableItemWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    FEventReply Reply = UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton);
    return Reply.NativeReply;
}

FReply UDraggableItemWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && InMouseEvent.IsShiftDown())
    {
        if (ItemObj && ItemObj->CurrentStack > 1 && SourceInventory)
        {
            USplitStackWidget* SplitWidget = CreateWidget<USplitStackWidget>(GetWorld(), USplitStackWidget::StaticClass());
            if (SplitWidget)
            {
                SplitWidget->Setup(ItemObj->CurrentStack - 1); // 최소 1개는 남겨야 함
                SplitWidget->OnSplitConfirmed.AddDynamic(this, &UDraggableItemWidget::OnAutoSplitConfirmed);
                SplitWidget->AddToViewport(100); // 팝업으로 띄움
            }
        }
        return FReply::Handled();
    }
    else if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        if (ItemObj)
        {
            if (!SourceInventory)
            {
                OnRightClicked.Broadcast(ItemObj);
                return FReply::Handled();
            }

            UContextMenuWidget* ContextMenu = CreateWidget<UContextMenuWidget>(GetWorld(), UContextMenuWidget::StaticClass());
            if (ContextMenu)
            {
                FVector2D ScreenPos = InMouseEvent.GetScreenSpacePosition();
                ContextMenu->Setup(ItemObj, ScreenPos);
                ContextMenu->OnInspectClicked.AddDynamic(this, &UDraggableItemWidget::HandleInspectItem);
                ContextMenu->OnDiscardClicked.AddDynamic(this, &UDraggableItemWidget::HandleDiscardItem);
                ContextMenu->OnUnloadClicked.AddDynamic(this, &UDraggableItemWidget::HandleUnloadItem);
                ContextMenu->AddToViewport(200); // 팝업으로 최상단에 띄움
            }
        }
        return FReply::Handled();
    }
    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UDraggableItemWidget::SetDragPreviewRotation(bool bInPreviewRotation)
{
    bHasDragPreviewRotation = true;
    bDragPreviewRotation = bInPreviewRotation;
    InitWidgetUI(bIsEquippedVisual);
}

void UDraggableItemWidget::OnAutoSplitConfirmed(int32 SplitAmount)
{
    if (!ItemObj || !SourceInventory) return;
    if (SourceInventory->GetItemInstance(ItemObj->InstanceID) != ItemObj) return;
    if (ItemObj->EquippedSight || ItemObj->EquippedMuzzle || ItemObj->EquippedMagazine) return;

    const int32 MaxSplitAmount = ItemObj->CurrentStack - 1;
    if (MaxSplitAmount < 1) return;
    SplitAmount = FMath::Clamp(SplitAmount, 1, MaxSplitAmount);

    FIntPoint Size = ItemObj->GetCurrentSize();
    int32 FreeSection = INDEX_NONE;
    int32 FreeX, FreeY;
    if (SourceInventory->FindEmptySpaceAcrossSections(Size.X, Size.Y, FreeSection, FreeX, FreeY))
    {
        // 1. 기존 아이템 수량 차감
        ItemObj->CurrentStack -= SplitAmount;

        // 2. 새 아이템 생성
        UItemInstance* NewItem = NewObject<UItemInstance>(SourceInventory);
        NewItem->InstanceID = FName(*FGuid::NewGuid().ToString());
        NewItem->TemplateID = ItemObj->TemplateID;
        NewItem->ItemName = ItemObj->ItemName;
        NewItem->Category = ItemObj->Category;
        NewItem->Rarity = ItemObj->Rarity;
        NewItem->ItemIcon = ItemObj->ItemIcon;
        NewItem->CachedDynamicIcon = ItemObj->CachedDynamicIcon;
        NewItem->BaseSize = ItemObj->BaseSize;
        NewItem->CurrentStack = SplitAmount;
        NewItem->MaxStack = ItemObj->MaxStack;
        NewItem->bIsRotated = ItemObj->bIsRotated;
        NewItem->bIsExamined = ItemObj->bIsExamined;
        NewItem->AttachmentType = ItemObj->AttachmentType;
        NewItem->CompatibleAmmo = ItemObj->CompatibleAmmo;
        NewItem->CurrentAmmo = ItemObj->CurrentAmmo;
        NewItem->MaxAmmo = ItemObj->MaxAmmo;
        NewItem->Damage = ItemObj->Damage;
        NewItem->Armor = ItemObj->Armor;
        NewItem->EquippedSight = ItemObj->EquippedSight;
        NewItem->EquippedMuzzle = ItemObj->EquippedMuzzle;
        NewItem->EquippedMagazine = ItemObj->EquippedMagazine;

        // 3. 인벤토리에 추가 (자동으로 브로드캐스트 안될수있으므로 수동호출)
        if (!SourceInventory->AddItemToSection(NewItem, FreeSection, FreeX, FreeY))
        {
            ItemObj->CurrentStack += SplitAmount;
            return;
        }
        SourceInventory->OnInventoryChanged.Broadcast();
    }
}

void UDraggableItemWidget::HandleInspectItem(UItemInstance* TargetItem)
{
    if (TargetItem && (!SourceInventory ||
        SourceInventory->GetItemInstance(TargetItem->InstanceID) == TargetItem))
    {
        UInspectWidget* InspectUI = CreateWidget<UInspectWidget>(GetWorld(), UInspectWidget::StaticClass());
        if (InspectUI)
        {
            InspectUI->Setup(TargetItem);
            InspectUI->AddToViewport(300); // 팝업창
        }
    }
}

void UDraggableItemWidget::HandleDiscardItem(UItemInstance* TargetItem)
{
    if (TargetItem && SourceInventory &&
        SourceInventory->GetItemInstance(TargetItem->InstanceID) == TargetItem)
    {
        SourceInventory->RemoveItem(TargetItem->InstanceID);
    }
}

void UDraggableItemWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);

    if (!ItemObj || !ItemObj->bIsExamined)
    {
        return;
    }

    UItemDragDropOperation* DragDropOp = NewObject<UItemDragDropOperation>();
    DragDropOp->ItemID = ItemObj->InstanceID;
    DragDropOp->ItemObj = ItemObj;
    DragDropOp->OriginalWidget = this; 
    DragDropOp->bIsSplitDrag = InMouseEvent.IsShiftDown() && (ItemObj->CurrentStack > 1);
    DragDropOp->bOriginalRotation = ItemObj->bIsRotated;
    DragDropOp->bPreviewRotation = ItemObj->bIsRotated;
    DragDropOp->SourceInventory = SourceInventory;
    DragDropOp->SourceSectionIndex = SourceSectionIndex;
    
    FVector2D TopLeftScreenPos = InGeometry.GetAbsolutePosition();
    DragDropOp->MouseOffset = InMouseEvent.GetScreenSpacePosition() - TopLeftScreenPos;
    
    UDraggableItemWidget* DragVisual = CreateWidget<UDraggableItemWidget>(GetWorld(), UDraggableItemWidget::StaticClass());
    DragVisual->ItemObj = ItemObj;
    DragVisual->CurrentDragOp = DragDropOp;
    DragDropOp->DragVisual = DragVisual;
    DragVisual->InitWidgetUI();
    
    DragDropOp->DefaultDragVisual = DragVisual;
    DragDropOp->Pivot = EDragPivot::MouseDown;

    CurrentDragOp = DragDropOp;
    CurrentDragVisual = DragVisual;
    UItemDragDropOperation::SetActiveOperation(DragDropOp);
    SetVisibility(ESlateVisibility::Hidden);

    OutOperation = DragDropOp;
}

void UDraggableItemWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    Super::NativeOnDragCancelled(InDragDropEvent, InOperation);

    UItemDragDropOperation* ItemDropOp = Cast<UItemDragDropOperation>(InOperation);
    if (!ItemDropOp || ItemDropOp->OriginalWidget != this) return;

    UItemDragDropOperation::ClearActiveOperation(ItemDropOp);

    if (ItemObj)
    {
        ItemObj->bIsRotated = ItemDropOp->bOriginalRotation;
    }
    SetVisibility(ESlateVisibility::Visible);
    CurrentDragOp = nullptr;
    CurrentDragVisual = nullptr;
}

FReply UDraggableItemWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::R && ItemObj)
    {
        if (CurrentDragOp && CurrentDragOp->ItemObj == ItemObj)
        {
            CurrentDragOp->TogglePreviewRotation();
            OnItemRotated();
            return FReply::Handled();
        }

        int32 CurrentSection = INDEX_NONE;
        int32 CurrentX = INDEX_NONE;
        int32 CurrentY = INDEX_NONE;
        if (SourceInventory)
        {
            if (SourceInventory->GetItemInstance(ItemObj->InstanceID) != ItemObj)
            {
                return FReply::Handled();
            }

            SourceInventory->FindItemPlacement(ItemObj->InstanceID, CurrentSection, CurrentX, CurrentY);

            if (CurrentX == INDEX_NONE || CurrentY == INDEX_NONE)
            {
                return FReply::Handled();
            }

            const FIntPoint RotatedSize(ItemObj->GetCurrentSize().Y, ItemObj->GetCurrentSize().X);
            if (!SourceInventory->CheckItemFitInSection(ItemObj->InstanceID, CurrentSection, CurrentX, CurrentY, RotatedSize.X, RotatedSize.Y))
            {
                return FReply::Handled();
            }
        }

        ItemObj->bIsRotated = !ItemObj->bIsRotated;
        if (SourceInventory && !SourceInventory->AddItemToSection(ItemObj, CurrentSection, CurrentX, CurrentY))
        {
            ItemObj->bIsRotated = !ItemObj->bIsRotated;
            return FReply::Handled();
        }

        ItemObj->OnItemModified.Broadcast();
        if (SourceInventory)
        {
            SourceInventory->OnInventoryChanged.Broadcast();
        }

        OnItemRotated();
        return FReply::Handled();
    }
    
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UDraggableItemWidget::HandleUnloadItem(UItemInstance* TargetItem)
{
    if (!TargetItem) return;

    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM || !GM->ItemDataTable) return;

    if (SourceInventory && SourceInventory->GetItemInstance(TargetItem->InstanceID) != TargetItem)
    {
        return;
    }

    // 1. 무기인 경우: 결합된 탄창을 분리하여 인벤토리에 넣음
    if (TargetItem->Category == EItemCategory::Weapon && TargetItem->EquippedMagazine)
    {
        UItemInstance* MagItem = TargetItem->EquippedMagazine;
        
        int32 FreeSection = INDEX_NONE;
        int32 FreeX, FreeY;
        bool bAdded = false;
        if (SourceInventory && SourceInventory->FindEmptySpaceAcrossSections(MagItem->GetCurrentSize().X, MagItem->GetCurrentSize().Y, FreeSection, FreeX, FreeY))
        {
            bAdded = SourceInventory->AddItemToSection(MagItem, FreeSection, FreeX, FreeY);
        }
        else if (GM->InventoryComponent && GM->InventoryComponent->FindEmptySpaceAcrossSections(MagItem->GetCurrentSize().X, MagItem->GetCurrentSize().Y, FreeSection, FreeX, FreeY))
        {
            bAdded = GM->InventoryComponent->AddItemToSection(MagItem, FreeSection, FreeX, FreeY);
        }

        if (bAdded)
        {
            TargetItem->EquippedMagazine = nullptr;
            TargetItem->OnItemModified.Broadcast();
            InitWidgetUI(SourceInventory == nullptr);
            if (SourceInventory) SourceInventory->OnInventoryChanged.Broadcast();
            else if (GM->InventoryComponent) GM->InventoryComponent->OnInventoryChanged.Broadcast();
        }
        return;
    }

    // 2. 탄창인 경우: 내부의 총알을 추출하여 인벤토리에 넣음
    if (TargetItem->Category == EItemCategory::Attachment && TargetItem->AttachmentType == EAttachmentType::Magazine)
    {
        if (TargetItem->CurrentAmmo <= 0) return;

        FName AmmoID = GM->FindCompatibleAmmoID(TargetItem);
        if (AmmoID == NAME_None) return;
        
        FItemData* AmmoData = GM->ItemDataTable->FindRow<FItemData>(AmmoID, TEXT("Unload"));
        if (!AmmoData) return;
        if (AmmoData->MaxStack <= 0) return;

        int32 AmmoAmount = TargetItem->CurrentAmmo;

        while (AmmoAmount > 0)
        {
            int32 StackToSpawn = FMath::Min(AmmoAmount, AmmoData->MaxStack);

            UItemInstance* NewAmmo = NewObject<UItemInstance>(GM);
            NewAmmo->InstanceID = FName(*FString::Printf(TEXT("%s_Unloaded_%s"), *AmmoID.ToString(), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
            NewAmmo->InitFromData(*AmmoData);
            NewAmmo->CurrentStack = StackToSpawn;
            NewAmmo->bIsExamined = true;
            NewAmmo->bIsRotated = false;

            int32 FreeSection = INDEX_NONE;
            int32 FreeX, FreeY;
            bool bAdded = false;
            if (SourceInventory && SourceInventory->FindEmptySpaceAcrossSections(NewAmmo->GetCurrentSize().X, NewAmmo->GetCurrentSize().Y, FreeSection, FreeX, FreeY))
            {
                bAdded = SourceInventory->AddItemToSection(NewAmmo, FreeSection, FreeX, FreeY);
            }
            else if (GM->InventoryComponent && GM->InventoryComponent->FindEmptySpaceAcrossSections(NewAmmo->GetCurrentSize().X, NewAmmo->GetCurrentSize().Y, FreeSection, FreeX, FreeY))
            {
                bAdded = GM->InventoryComponent->AddItemToSection(NewAmmo, FreeSection, FreeX, FreeY);
            }

            if (!bAdded) break;
            AmmoAmount -= StackToSpawn;
        }

        TargetItem->CurrentAmmo = AmmoAmount;

        TargetItem->OnItemModified.Broadcast();
        InitWidgetUI(SourceInventory == nullptr);
        if (SourceInventory) SourceInventory->OnInventoryChanged.Broadcast();
        else if (GM->InventoryComponent) GM->InventoryComponent->OnInventoryChanged.Broadcast();
    }
}
