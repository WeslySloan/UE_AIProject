#include "EquipmentSlotWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "ItemDragDropOperation.h"
#include "DraggableItemWidget.h"
#include "../ItemInstance.h"
#include "../GridGameMode.h"
#include "../GridInventoryComponent.h"
#include "../EquipmentComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "ContextMenuWidget.h"
#include "InspectWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

bool UEquipmentSlotWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (WidgetTree && !WidgetTree->RootWidget)
    {
        UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
        WidgetTree->RootWidget = RootOverlay;

        BackgroundBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BackgroundBorder"));
        BackgroundBorder->SetBrushColor(FLinearColor(0.07f, 0.10f, 0.13f, 1.0f));
        UOverlaySlot* BGOvSlot = RootOverlay->AddChildToOverlay(BackgroundBorder);
        BGOvSlot->SetHorizontalAlignment(HAlign_Fill);
        BGOvSlot->SetVerticalAlignment(VAlign_Fill);

        SlotNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SlotNameText"));
        SlotNameText->SetText(FText::FromString(SlotName));
        SlotNameText->SetColorAndOpacity(FLinearColor(0.56f, 0.62f, 0.68f, 1.0f));
        SlotNameText->SetJustification(ETextJustify::Center);
        UOverlaySlot* TextOvSlot = RootOverlay->AddChildToOverlay(SlotNameText);
        TextOvSlot->SetHorizontalAlignment(HAlign_Center);
        TextOvSlot->SetVerticalAlignment(VAlign_Center);

        ItemCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ItemCanvas"));
        UOverlaySlot* CanvasOvSlot = RootOverlay->AddChildToOverlay(ItemCanvas);
        CanvasOvSlot->SetHorizontalAlignment(HAlign_Fill);
        CanvasOvSlot->SetVerticalAlignment(VAlign_Fill);
    }
    
    // 강제로 이름 설정 (블루프린트 초기화 시점 고려)
    if (SlotNameText)
    {
        SlotNameText->SetText(FText::FromString(SlotName));
    }

    return true;
}

void UEquipmentSlotWidget::InitSlot(FName InSlotID, EItemCategory InCategory, const FString& InSlotName)
{
    SlotID = InSlotID;
    AllowedCategory = InCategory;
    SlotName = InSlotName;
    if (SlotNameText)
    {
        SlotNameText->SetText(FText::FromString(SlotName));
    }
}

void UEquipmentSlotWidget::SetEquippedItem(UItemInstance* NewItem)
{
    if (EquippedItem)
    {
        EquippedItem->OnItemModified.RemoveDynamic(this, &UEquipmentSlotWidget::HandleEquippedItemModified);
    }

    EquippedItem = NewItem;

    if (EquippedItem)
    {
        EquippedItem->OnItemModified.AddUniqueDynamic(this, &UEquipmentSlotWidget::HandleEquippedItemModified);
    }

    RefreshSlotUI();
}

void UEquipmentSlotWidget::HandleEquippedItemModified()
{
    RefreshSlotUI();
}

void UEquipmentSlotWidget::RefreshSlotUI()
{
    if (!ItemCanvas) return;

    ItemCanvas->ClearChildren();

    if (EquippedItem)
    {
        if (SlotNameText) SlotNameText->SetVisibility(ESlateVisibility::Hidden);

        UDraggableItemWidget* ItemVisual = WidgetTree->ConstructWidget<UDraggableItemWidget>(UDraggableItemWidget::StaticClass());
        ItemVisual->ItemObj = EquippedItem;
        ItemVisual->InitWidgetUI(true); // 장착 상태에서는 슬롯 크기에 꽉 차도록
        ItemVisual->OnRightClicked.AddDynamic(this, &UEquipmentSlotWidget::HandleRightClicked);
        
        UCanvasPanelSlot* CanvasSlot = ItemCanvas->AddChildToCanvas(ItemVisual);
        CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f)); // 꽉 채우기
        CanvasSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 0.0f));
    }
    else
    {
        if (SlotNameText)
        {
            SlotNameText->SetText(FText::FromString(SlotName));
            SlotNameText->SetVisibility(ESlateVisibility::Visible);
        }
    }
}

void UEquipmentSlotWidget::OnEquipmentChanged()
{
    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (GM && GM->EquipmentComponent)
    {
        SetEquippedItem(GM->EquipmentComponent->GetEquippedItem(SlotID));
    }
    else
    {
        SetEquippedItem(nullptr);
    }
}

void UEquipmentSlotWidget::SetHighlight(bool bActive)
{
    if (BackgroundBorder)
    {
        if (bActive)
        {
            BackgroundBorder->SetBrushColor(FLinearColor(0.2f, 0.4f, 0.8f, 0.8f)); // Active (Blue)
        }
        else
        {
            BackgroundBorder->SetBrushColor(FLinearColor(0.07f, 0.10f, 0.13f, 1.0f)); // Normal (Dark Gray)
        }
    }
}

void UEquipmentSlotWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);
    
    UItemDragDropOperation* ItemDropOp = Cast<UItemDragDropOperation>(InOperation);
    if (ItemDropOp && ItemDropOp->ItemObj)
    {
        if (ItemDropOp->ItemObj->Category == AllowedCategory)
        {
            if (BackgroundBorder) BackgroundBorder->SetBrushColor(FLinearColor(0.2f, 0.8f, 0.2f, 0.8f)); // 녹색
        }
        else if (ItemDropOp->ItemObj->Category == EItemCategory::Attachment && EquippedItem && EquippedItem->Category == EItemCategory::Weapon)
        {
            EAttachmentType ModType = ItemDropOp->ItemObj->AttachmentType;
            if ((ModType == EAttachmentType::Sight && !EquippedItem->EquippedSight) ||
                (ModType == EAttachmentType::Muzzle && !EquippedItem->EquippedMuzzle) ||
                (ModType == EAttachmentType::Magazine && !EquippedItem->EquippedMagazine))
            {
                if (BackgroundBorder) BackgroundBorder->SetBrushColor(FLinearColor(0.2f, 0.8f, 0.2f, 0.8f)); // 녹색
            }
            else
            {
                if (BackgroundBorder) BackgroundBorder->SetBrushColor(FLinearColor(0.8f, 0.2f, 0.2f, 0.8f)); // 적색
            }
        }
        else
        {
            if (BackgroundBorder) BackgroundBorder->SetBrushColor(FLinearColor(0.8f, 0.2f, 0.2f, 0.8f)); // 적색
        }
    }
}

void UEquipmentSlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    Super::NativeOnDragLeave(InDragDropEvent, InOperation);
    
    if (BackgroundBorder)
    {
        BackgroundBorder->SetBrushColor(FLinearColor(0.07f, 0.10f, 0.13f, 1.0f));
    }
}

bool UEquipmentSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);

    if (BackgroundBorder) BackgroundBorder->SetBrushColor(FLinearColor(0.07f, 0.10f, 0.13f, 1.0f));

    UItemDragDropOperation* ItemDropOp = Cast<UItemDragDropOperation>(InOperation);
    if (!ItemDropOp || !ItemDropOp->ItemObj) return false;

    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM) return false;

    // 퀵 모딩 (부착물 -> 장착된 무기)
    if (ItemDropOp->ItemObj->Category == EItemCategory::Attachment && EquippedItem && EquippedItem->Category == EItemCategory::Weapon)
    {
        EAttachmentType ModType = ItemDropOp->ItemObj->AttachmentType;
        if (ModType == EAttachmentType::Magazine &&
            !EquippedItem->IsCompatibleMagazine(ItemDropOp->ItemObj))
        {
            return false;
        }
        const bool bSlotAvailable =
            (ModType == EAttachmentType::Sight && !EquippedItem->EquippedSight) ||
            (ModType == EAttachmentType::Muzzle && !EquippedItem->EquippedMuzzle) ||
            (ModType == EAttachmentType::Magazine && !EquippedItem->EquippedMagazine);

        if (bSlotAvailable)
        {
            bool bSourceRemoved = false;
            if (ItemDropOp->SourceInventory)
            {
                if (ItemDropOp->SourceInventory->GetItemInstance(ItemDropOp->ItemID) == ItemDropOp->ItemObj)
                {
                    bSourceRemoved = ItemDropOp->SourceInventory->RemoveItem(ItemDropOp->ItemObj->InstanceID);
                }
            }
            else if (ItemDropOp->SourceModSlot)
            {
                bSourceRemoved = true;
            }
            else
            {
                bSourceRemoved = GM->EquipmentComponent && GM->EquipmentComponent->RemoveAttachedItem(ItemDropOp->ItemObj);
            }

            if (!bSourceRemoved) return false;

            if (ModType == EAttachmentType::Sight) EquippedItem->EquippedSight = ItemDropOp->ItemObj;
            else if (ModType == EAttachmentType::Muzzle) EquippedItem->EquippedMuzzle = ItemDropOp->ItemObj;
            else if (ModType == EAttachmentType::Magazine) EquippedItem->EquippedMagazine = ItemDropOp->ItemObj;

            if (ItemDropOp->OriginalWidget) ItemDropOp->OriginalWidget->RemoveFromParent();
            
            EquippedItem->OnItemModified.Broadcast();
            RefreshSlotUI();
            if (ItemDropOp->SourceInventory) ItemDropOp->SourceInventory->OnInventoryChanged.Broadcast();
            UItemDragDropOperation::ClearActiveOperation(ItemDropOp);
            return true;
        }
    }

    // 삽탄 (탄약 -> 장착된 무기)
    if (ItemDropOp->ItemObj->Category == EItemCategory::Consumable && EquippedItem && EquippedItem->Category == EItemCategory::Weapon && EquippedItem->EquippedMagazine)
    {
        if (!ItemDropOp->SourceInventory ||
            ItemDropOp->SourceInventory->GetItemInstance(ItemDropOp->ItemID) != ItemDropOp->ItemObj)
        {
            return false;
        }

        UItemInstance* TargetMag = EquippedItem->EquippedMagazine;
        if (TargetMag->IsCompatibleAmmo(ItemDropOp->ItemObj) && TargetMag->CurrentAmmo < TargetMag->MaxAmmo)
        {
            int32 AvailableSpace = TargetMag->MaxAmmo - TargetMag->CurrentAmmo;
            int32 AmountToLoad = FMath::Min(ItemDropOp->ItemObj->CurrentStack, AvailableSpace);
            
            TargetMag->CurrentAmmo += AmountToLoad;
            ItemDropOp->ItemObj->CurrentStack -= AmountToLoad;
            
            bool bAmmoDepleted = (ItemDropOp->ItemObj->CurrentStack <= 0);
            if (bAmmoDepleted)
            {
                if (!ItemDropOp->SourceInventory ||
                    ItemDropOp->SourceInventory->GetItemInstance(ItemDropOp->ItemID) != ItemDropOp->ItemObj ||
                    !ItemDropOp->SourceInventory->RemoveItem(ItemDropOp->ItemObj->InstanceID))
                {
                    TargetMag->CurrentAmmo -= AmountToLoad;
                    ItemDropOp->ItemObj->CurrentStack += AmountToLoad;
                    return false;
                }
                if (ItemDropOp->OriginalWidget) ItemDropOp->OriginalWidget->RemoveFromParent();
            }
            
            EquippedItem->OnItemModified.Broadcast();
            RefreshSlotUI();
            if (ItemDropOp->SourceInventory) ItemDropOp->SourceInventory->OnInventoryChanged.Broadcast();
            UItemDragDropOperation::ClearActiveOperation(ItemDropOp);
            return true;
        }
    }

    // 카테고리 체크
    if (ItemDropOp->ItemObj->Category != AllowedCategory) return false;

    if (!GM->InventoryComponent || !GM->EquipmentComponent) return false;

    if (ItemDropOp->SourceInventory &&
        ItemDropOp->SourceInventory->GetItemInstance(ItemDropOp->ItemID) != ItemDropOp->ItemObj)
    {
        return false;
    }

    if (ItemDropOp->SourceEquipmentSlot != NAME_None &&
        GM->EquipmentComponent->GetEquippedItem(ItemDropOp->SourceEquipmentSlot) != ItemDropOp->ItemObj)
    {
        return false;
    }

    UItemInstance* PreviousItem = GM->EquipmentComponent->GetEquippedItem(SlotID);
    if (PreviousItem == ItemDropOp->ItemObj) return false;

    int32 FreeX = 0;
    int32 FreeY = 0;
    int32 FreeSection = 0;
    int32 IncomingOriginalSection = INDEX_NONE;
    int32 IncomingOriginalX = INDEX_NONE;
    int32 IncomingOriginalY = INDEX_NONE;
    const bool bIncomingHasOriginalPlacement = ItemDropOp->SourceInventory &&
        ItemDropOp->SourceInventory->FindItemPlacement(ItemDropOp->ItemObj->InstanceID,
            IncomingOriginalSection, IncomingOriginalX, IncomingOriginalY);
    if (bIncomingHasOriginalPlacement && ItemDropOp->SourceSectionIndex != IncomingOriginalSection)
    {
        UE_LOG(LogTemp, Warning, TEXT("Drag source section %d disagrees with actual section %d for item %s; using actual placement."),
            ItemDropOp->SourceSectionIndex, IncomingOriginalSection, *ItemDropOp->ItemObj->InstanceID.ToString());
    }

    auto RestoreIncomingItem = [&]() -> bool
    {
        if (ItemDropOp->SourceEquipmentSlot != NAME_None)
        {
            return GM->EquipmentComponent->EquipItem(ItemDropOp->SourceEquipmentSlot, ItemDropOp->ItemObj);
        }

        FIntPoint IncomingSize = ItemDropOp->ItemObj->GetCurrentSize();
        int32 RestoreX = 0;
        int32 RestoreY = 0;
        UGridInventoryComponent* RestoreInventory = ItemDropOp->SourceInventory
            ? ItemDropOp->SourceInventory
            : GM->InventoryComponent;
        if (bIncomingHasOriginalPlacement && RestoreInventory == ItemDropOp->SourceInventory)
        {
            if (RestoreInventory->AddItemToSection(ItemDropOp->ItemObj, IncomingOriginalSection, IncomingOriginalX, IncomingOriginalY))
            {
                return true;
            }
            UE_LOG(LogTemp, Error, TEXT("Failed to restore incoming item to its original section/coordinate: %s"),
                *ItemDropOp->ItemObj->InstanceID.ToString());
            return false;
        }
        if (ItemDropOp->SourceInventory && RestoreInventory == ItemDropOp->SourceInventory)
        {
            UE_LOG(LogTemp, Error, TEXT("Cannot safely restore incoming item because its original placement is unavailable: %s"),
                *ItemDropOp->ItemObj->InstanceID.ToString());
            return false;
        }
        return RestoreInventory &&
            RestoreInventory->FindEmptySpaceAcrossSections(IncomingSize.X, IncomingSize.Y, FreeSection, RestoreX, RestoreY) &&
            RestoreInventory->AddItemToSection(ItemDropOp->ItemObj, FreeSection, RestoreX, RestoreY);
    };

    bool bIncomingRemoved = false;

    // 이미 장착된 아이템이 있다면 인벤토리(가방)로 되돌림
    if (PreviousItem)
    {
        FIntPoint Size = PreviousItem->GetCurrentSize();
        bool bCanStorePreviousItem = false;
        if (ItemDropOp->SourceInventory == GM->InventoryComponent)
        {
            bCanStorePreviousItem = bIncomingHasOriginalPlacement
                ? GM->InventoryComponent->FindEmptySpaceAcrossSectionsExcludingPlacement(
                    Size.X, Size.Y, ItemDropOp->ItemObj->InstanceID,
                    IncomingOriginalSection, IncomingOriginalX, IncomingOriginalY,
                    ItemDropOp->ItemObj->GetCurrentSize().X, ItemDropOp->ItemObj->GetCurrentSize().Y,
                    FreeSection, FreeX, FreeY)
                : GM->InventoryComponent->FindEmptySpaceAcrossSectionsExcluding(
                    Size.X, Size.Y, ItemDropOp->ItemObj->InstanceID, FreeSection, FreeX, FreeY);
        }
        else
        {
            bCanStorePreviousItem = GM->InventoryComponent->FindEmptySpaceAcrossSections(Size.X, Size.Y, FreeSection, FreeX, FreeY);
        }

        if (!bCanStorePreviousItem)
        {
            // 인벤토리 꽉 참 - 드롭 취소
            return false;
        }

        // 새 아이템이 플레이어 인벤토리에 있으면 먼저 제거하여 위에서 확인한 칸을 확보합니다.
        if (ItemDropOp->SourceInventory == GM->InventoryComponent)
        {
            bIncomingRemoved = GM->InventoryComponent->RemoveItem(ItemDropOp->ItemObj->InstanceID);
            if (!bIncomingRemoved)
            {
                return false;
            }
        }

        if (!GM->EquipmentComponent->RemoveItemBySlotID(SlotID))
        {
            if (bIncomingRemoved)
            {
                if (!RestoreIncomingItem())
                {
                    UE_LOG(LogTemp, Error, TEXT("Failed to restore incoming item after equipment slot removal failure."));
                }
            }
            return false;
        }

        if (!GM->InventoryComponent->AddItemToSection(PreviousItem, FreeSection, FreeX, FreeY))
        {
            // 예기치 않은 실패가 발생하면 기존 장비를 즉시 장착 상태로 복구합니다.
            const bool bPreviousRestored = GM->EquipmentComponent->EquipItem(SlotID, PreviousItem);
            bool bIncomingRestored = true;
            if (bIncomingRemoved)
            {
                bIncomingRestored = RestoreIncomingItem();
            }
            if (!bPreviousRestored || !bIncomingRestored)
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to restore equipment swap state after previous item storage failure."));
            }
            return false;
        }
    }

    // 새 아이템 장착 (원본 인벤토리에서 제거)
    if (!bIncomingRemoved && ItemDropOp->SourceInventory)
    {
        bIncomingRemoved = ItemDropOp->SourceInventory->RemoveItem(ItemDropOp->ItemObj->InstanceID);
    }
    else if (!bIncomingRemoved && ItemDropOp->SourceEquipmentSlot != NAME_None)
    {
        bIncomingRemoved = GM->EquipmentComponent->RemoveItemBySlotID(ItemDropOp->SourceEquipmentSlot);
    }
    else if (!bIncomingRemoved)
    {
        bIncomingRemoved = GM->EquipmentComponent->RemoveItemByInstanceID(ItemDropOp->ItemObj->InstanceID);
    }

    if (!bIncomingRemoved)
    {
        if (PreviousItem)
        {
            const bool bPreviousRemoved = GM->InventoryComponent->RemoveItem(PreviousItem->InstanceID);
            const bool bPreviousRestored = bPreviousRemoved &&
                GM->EquipmentComponent->EquipItem(SlotID, PreviousItem);
            if (!bPreviousRestored)
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to restore previous equipment after incoming item removal failure."));
            }
        }
        return false;
    }

    if (!GM->EquipmentComponent->EquipItem(SlotID, ItemDropOp->ItemObj))
    {
        bool bPreviousRestored = true;
        if (PreviousItem)
        {
            bPreviousRestored = GM->InventoryComponent->RemoveItem(PreviousItem->InstanceID) &&
                GM->EquipmentComponent->EquipItem(SlotID, PreviousItem);
        }
        const bool bIncomingRestored = RestoreIncomingItem();
        if (!bPreviousRestored || !bIncomingRestored)
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to restore equipment swap state after incoming equip failure."));
        }
        return false;
    }

    if (SlotID == TEXT("Backpack") || SlotID == TEXT("Rig"))
    {
        if (!GM->ReconfigureStorageForEquipmentSlot(SlotID, ItemDropOp->ItemObj))
        {
            GM->EquipmentComponent->RemoveItemBySlotID(SlotID);
            if (!RestoreIncomingItem())
            {
                UE_LOG(LogTemp, Error, TEXT("Storage equipment swap rollback failed for incoming item: %s"),
                    *ItemDropOp->ItemObj->InstanceID.ToString());
            }
            if (PreviousItem)
            {
                if (!GM->InventoryComponent->RemoveItem(PreviousItem->InstanceID) ||
                    !GM->EquipmentComponent->EquipItem(SlotID, PreviousItem))
                {
                    UE_LOG(LogTemp, Error, TEXT("Storage equipment swap rollback failed for previous item: %s"),
                        *PreviousItem->InstanceID.ToString());
                }
            }
            return false;
        }
    }

    if (ItemDropOp->OriginalWidget)
    {
        // 원래 장비창에 있던 거면 부모에서 제거. UCanvasPanelSlot에서 제거됨.
        ItemDropOp->OriginalWidget->RemoveFromParent();
    }

    SetEquippedItem(ItemDropOp->ItemObj);
    
    return true;
}

FReply UEquipmentSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && EquippedItem)
    {
        return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UEquipmentSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    if (!EquippedItem) return;

    UItemDragDropOperation* DragDropOp = NewObject<UItemDragDropOperation>();
    DragDropOp->ItemID = EquippedItem->InstanceID;
    DragDropOp->ItemObj = EquippedItem;
    DragDropOp->SourceEquipmentSlot = SlotID;
    DragDropOp->OriginalWidget = ItemCanvas
        ? Cast<UDraggableItemWidget>(ItemCanvas->GetChildAt(0))
        : nullptr; // 원본은 그대로 둠
    
    // 드래그 비주얼 전용 위젯 생성 (본래 크기로)
    UDraggableItemWidget* DragVisual = WidgetTree->ConstructWidget<UDraggableItemWidget>(UDraggableItemWidget::StaticClass());
    DragVisual->ItemObj = EquippedItem;
    DragVisual->InitWidgetUI(false); 
    
    DragDropOp->DefaultDragVisual = DragVisual; 
    DragDropOp->Pivot = EDragPivot::MouseDown;

    // 슬롯(확대됨)에서 클릭한 비율을 구해, 드래그 비주얼(본래 크기)의 MouseOffset으로 변환
    FVector2D LocalClickPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    FVector2D SlotSize = InGeometry.GetLocalSize();
    FVector2D VisualSize = FVector2D(EquippedItem->GetCurrentSize().X * 64.0f, EquippedItem->GetCurrentSize().Y * 64.0f);
    
    // X, Y 비율 계산 후 비주얼 사이즈에 곱함
    float RatioX = FMath::Clamp(LocalClickPos.X / SlotSize.X, 0.0f, 1.0f);
    float RatioY = FMath::Clamp(LocalClickPos.Y / SlotSize.Y, 0.0f, 1.0f);
    DragDropOp->MouseOffset = FVector2D(VisualSize.X * RatioX, VisualSize.Y * RatioY);
    
    OutOperation = DragDropOp;
}

void UEquipmentSlotWidget::HandleRightClicked(UItemInstance* ItemObj)
{
    if (ItemObj)
    {
        UContextMenuWidget* ContextMenu = CreateWidget<UContextMenuWidget>(GetWorld(), UContextMenuWidget::StaticClass());
        if (ContextMenu)
        {
            FVector2D ScreenPos = UWidgetLayoutLibrary::GetMousePositionOnPlatform();
            ContextMenu->Setup(ItemObj, ScreenPos, true);
            ContextMenu->OnInspectClicked.AddDynamic(this, &UEquipmentSlotWidget::HandleInspectItem);
            ContextMenu->OnUnequipClicked.AddDynamic(this, &UEquipmentSlotWidget::HandleUnequipClicked);
            ContextMenu->OnUnloadClicked.AddDynamic(this, &UEquipmentSlotWidget::HandleUnloadClicked);
            ContextMenu->AddToViewport(400); 
        }
    }
}

void UEquipmentSlotWidget::HandleInspectItem(UItemInstance* TargetItem)
{
    if (TargetItem && TargetItem == EquippedItem)
    {
        UInspectWidget* InspectUI = CreateWidget<UInspectWidget>(GetWorld(), UInspectWidget::StaticClass());
        if (InspectUI)
        {
            InspectUI->Setup(TargetItem);
            InspectUI->AddToViewport(500); 
        }
    }
}

void UEquipmentSlotWidget::HandleUnequipClicked(UItemInstance* ItemObj)
{
    if (ItemObj == EquippedItem)
    {
        if (SlotID == TEXT("Backpack") || SlotID == TEXT("Rig"))
        {
            if (AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this)))
            {
                if (GM->TryStandaloneStorageUnequip(SlotID, ItemObj)) RefreshSlotUI();
            }
            return;
        }
        AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
        if (GM && GM->InventoryComponent && GM->EquipmentComponent)
        {
            FIntPoint Size = ItemObj->GetCurrentSize();
            int32 FreeX, FreeY;
            int32 FreeSection = INDEX_NONE;
            if (GM->EquipmentComponent->GetEquippedItem(SlotID) != ItemObj)
            {
                return;
            }

            if (GM->InventoryComponent->FindEmptySpaceAcrossSections(Size.X, Size.Y, FreeSection, FreeX, FreeY))
            {
                if (!GM->EquipmentComponent->RemoveItemByInstanceID(ItemObj->InstanceID))
                {
                    return;
                }

                if (!GM->InventoryComponent->AddItemToSection(ItemObj, FreeSection, FreeX, FreeY))
                {
                    // 예기치 않은 수납 실패 시 장비를 원래 슬롯으로 복구합니다.
                    GM->EquipmentComponent->EquipItem(SlotID, ItemObj);
                    return;
                }
                RefreshSlotUI();
                GM->InventoryComponent->OnInventoryChanged.Broadcast();
            }
        }
    }
}

void UEquipmentSlotWidget::HandleUnloadClicked(UItemInstance* ItemObj)
{
    if (!ItemObj || ItemObj != EquippedItem) return;

    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM || !GM->ItemDataTable || !GM->EquipmentComponent) return;
    if (GM->EquipmentComponent->GetEquippedItem(SlotID) != ItemObj) return;

    // 1. 무기인 경우: 결합된 탄창을 분리하여 인벤토리에 넣음
    if (ItemObj->Category == EItemCategory::Weapon && ItemObj->EquippedMagazine)
    {
        UItemInstance* MagItem = ItemObj->EquippedMagazine;
        
        int32 FreeSection = INDEX_NONE;
        int32 FreeX, FreeY;
        if (GM->InventoryComponent && GM->InventoryComponent->FindEmptySpaceAcrossSections(MagItem->GetCurrentSize().X, MagItem->GetCurrentSize().Y, FreeSection, FreeX, FreeY) &&
            GM->InventoryComponent->AddItemToSection(MagItem, FreeSection, FreeX, FreeY))
        {
            ItemObj->EquippedMagazine = nullptr;
            
            ItemObj->OnItemModified.Broadcast();
            RefreshSlotUI();
            GM->InventoryComponent->OnInventoryChanged.Broadcast();
        }
        return;
    }

    // 2. 탄창인 경우: 내부 총알 추출
    if (ItemObj->Category == EItemCategory::Attachment && ItemObj->AttachmentType == EAttachmentType::Magazine)
    {
        if (ItemObj->CurrentAmmo <= 0) return;

        FName AmmoID = GM->FindCompatibleAmmoID(ItemObj);
        if (AmmoID == NAME_None) return;
        
        FItemData* AmmoData = GM->ItemDataTable->FindRow<FItemData>(AmmoID, TEXT("Unload"));
        if (!AmmoData) return;
        if (AmmoData->MaxStack <= 0) return;

        int32 AmmoAmount = ItemObj->CurrentAmmo;

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
            if (GM->InventoryComponent && GM->InventoryComponent->FindEmptySpaceAcrossSections(NewAmmo->GetCurrentSize().X, NewAmmo->GetCurrentSize().Y, FreeSection, FreeX, FreeY))
            {
                bAdded = GM->InventoryComponent->AddItemToSection(NewAmmo, FreeSection, FreeX, FreeY);
            }

            if (!bAdded) break;
            AmmoAmount -= StackToSpawn;
        }

        ItemObj->CurrentAmmo = AmmoAmount;

        ItemObj->OnItemModified.Broadcast();
        RefreshSlotUI();
        if (GM->InventoryComponent) GM->InventoryComponent->OnInventoryChanged.Broadcast();
    }
}

void UEquipmentSlotWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    Super::NativeOnDragCancelled(InDragDropEvent, InOperation);
    if (UItemDragDropOperation* ItemDropOp = Cast<UItemDragDropOperation>(InOperation))
    {
        UItemDragDropOperation::ClearActiveOperation(ItemDropOp);
    }
    // 드래그가 취소되면 슬롯 UI를 갱신하여 꽉 찬 뷰로 되돌림
    RefreshSlotUI();
}
