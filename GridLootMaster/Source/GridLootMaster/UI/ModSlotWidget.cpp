#include "ModSlotWidget.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "ItemDragDropOperation.h"
#include "DraggableItemWidget.h"
#include "../ItemInstance.h"
#include "../GridGameMode.h"
#include "../GridInventoryComponent.h"
#include "../EquipmentComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "ContextMenuWidget.h"

bool UModSlotWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (WidgetTree && !WidgetTree->RootWidget)
    {
        BackgroundBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        BackgroundBorder->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f));
        WidgetTree->RootWidget = BackgroundBorder;

        UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
        BackgroundBorder->AddChild(RootCanvas);

        SlotNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        SlotNameText->SetColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f));
        SlotNameText->SetJustification(ETextJustify::Center);
        UCanvasPanelSlot* TextSlot = RootCanvas->AddChildToCanvas(SlotNameText);
        TextSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
        TextSlot->SetAlignment(FVector2D(0.5f, 0.5f));

        ItemCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
        UCanvasPanelSlot* ItemCanvasSlot = RootCanvas->AddChildToCanvas(ItemCanvas);
        ItemCanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
        ItemCanvasSlot->SetOffsets(FMargin(0, 0, 0, 0));
    }
    return true;
}

void UModSlotWidget::Setup(UItemInstance* InWeaponObj, EAttachmentType InAllowedType)
{
    WeaponObj = InWeaponObj;
    AllowedType = InAllowedType;

    if (SlotNameText)
    {
        if (AllowedType == EAttachmentType::Sight) SlotNameText->SetText(FText::FromString("Sight"));
        else if (AllowedType == EAttachmentType::Muzzle) SlotNameText->SetText(FText::FromString("Muzzle"));
        else if (AllowedType == EAttachmentType::Magazine) SlotNameText->SetText(FText::FromString("Magazine"));
    }

    RefreshSlotUI();
}

void UModSlotWidget::RefreshSlotUI()
{
    if (ItemCanvas) ItemCanvas->ClearChildren();
    
    EquippedMod = nullptr;
    if (WeaponObj)
    {
        if (AllowedType == EAttachmentType::Sight) EquippedMod = WeaponObj->EquippedSight;
        else if (AllowedType == EAttachmentType::Muzzle) EquippedMod = WeaponObj->EquippedMuzzle;
        else if (AllowedType == EAttachmentType::Magazine) EquippedMod = WeaponObj->EquippedMagazine;
    }

    if (EquippedMod)
    {
        if (SlotNameText) SlotNameText->SetVisibility(ESlateVisibility::Hidden);

        UDraggableItemWidget* ItemVisual = WidgetTree->ConstructWidget<UDraggableItemWidget>(UDraggableItemWidget::StaticClass());
        ItemVisual->ItemObj = EquippedMod;
        ItemVisual->InitWidgetUI(true);
        ItemVisual->OnRightClicked.AddDynamic(this, &UModSlotWidget::HandleModRightClicked);

        UCanvasPanelSlot* ItemSlot = ItemCanvas->AddChildToCanvas(ItemVisual);
        ItemSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
        ItemSlot->SetOffsets(FMargin(0, 0, 0, 0));
    }
    else
    {
        if (SlotNameText) SlotNameText->SetVisibility(ESlateVisibility::Visible);
    }
}

void UModSlotWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);
    
    UItemDragDropOperation* ItemDropOp = Cast<UItemDragDropOperation>(InOperation);
    if (ItemDropOp && ItemDropOp->ItemObj)
    {
        if (ItemDropOp->ItemObj->Category == EItemCategory::Attachment && ItemDropOp->ItemObj->AttachmentType == AllowedType)
        {
            if (BackgroundBorder) BackgroundBorder->SetBrushColor(FLinearColor(0.2f, 0.8f, 0.2f, 0.8f));
        }
        else
        {
            if (BackgroundBorder) BackgroundBorder->SetBrushColor(FLinearColor(0.8f, 0.2f, 0.2f, 0.8f));
        }
    }
}

void UModSlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    Super::NativeOnDragLeave(InDragDropEvent, InOperation);
    if (BackgroundBorder) BackgroundBorder->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f));
}

bool UModSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);

    if (BackgroundBorder) BackgroundBorder->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f));

    UItemDragDropOperation* ItemDropOp = Cast<UItemDragDropOperation>(InOperation);
    if (!ItemDropOp || !ItemDropOp->ItemObj || !WeaponObj) return false;

    // 삽탄 (탄약 -> 매거진 슬롯)
    if (AllowedType == EAttachmentType::Magazine && EquippedMod && ItemDropOp->ItemObj->Category == EItemCategory::Consumable)
    {
        if (!ItemDropOp->SourceInventory ||
            ItemDropOp->SourceInventory->GetItemInstance(ItemDropOp->ItemID) != ItemDropOp->ItemObj)
        {
            return false;
        }

        if (EquippedMod->IsCompatibleAmmo(ItemDropOp->ItemObj) && EquippedMod->CurrentAmmo < EquippedMod->MaxAmmo)
        {
            int32 AvailableSpace = EquippedMod->MaxAmmo - EquippedMod->CurrentAmmo;
            int32 AmountToLoad = FMath::Min(ItemDropOp->ItemObj->CurrentStack, AvailableSpace);
            
            EquippedMod->CurrentAmmo += AmountToLoad;
            ItemDropOp->ItemObj->CurrentStack -= AmountToLoad;
            
            bool bAmmoDepleted = (ItemDropOp->ItemObj->CurrentStack <= 0);
            if (bAmmoDepleted)
            {
                if (!ItemDropOp->SourceInventory ||
                    ItemDropOp->SourceInventory->GetItemInstance(ItemDropOp->ItemID) != ItemDropOp->ItemObj ||
                    !ItemDropOp->SourceInventory->RemoveItem(ItemDropOp->ItemObj->InstanceID))
                {
                    EquippedMod->CurrentAmmo -= AmountToLoad;
                    ItemDropOp->ItemObj->CurrentStack += AmountToLoad;
                    return false;
                }
                if (ItemDropOp->OriginalWidget) ItemDropOp->OriginalWidget->RemoveFromParent();
            }
            
            RefreshSlotUI();
            if (ItemDropOp->SourceInventory) ItemDropOp->SourceInventory->OnInventoryChanged.Broadcast();
            
            EquippedMod->OnItemModified.Broadcast();
            // InspectWidget 등 갱신을 위해 무기 상태 변경 브로드캐스트 필요하다면 여기서 처리
            WeaponObj->OnItemModified.Broadcast();
            return true;
        }
    }

    if (ItemDropOp->ItemObj->Category != EItemCategory::Attachment || ItemDropOp->ItemObj->AttachmentType != AllowedType) return false;

    if (AllowedType == EAttachmentType::Magazine &&
        !WeaponObj->IsCompatibleMagazine(ItemDropOp->ItemObj))
    {
        return false;
    }

    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM || !GM->InventoryComponent || !GM->EquipmentComponent) return false;
    if (EquippedMod == ItemDropOp->ItemObj) return false;

    // 장착된 모드가 있으면 인벤토리로 보냄
    if (EquippedMod)
    {
        FIntPoint Size = EquippedMod->GetCurrentSize();
        int32 FreeX, FreeY;
        const bool bCanStoreEquippedMod = ItemDropOp->SourceInventory == GM->InventoryComponent
            ? GM->InventoryComponent->FindEmptySpaceExcluding(Size.X, Size.Y, ItemDropOp->ItemObj->InstanceID, FreeX, FreeY)
            : GM->InventoryComponent->FindEmptySpace(Size.X, Size.Y, FreeX, FreeY);
        if (bCanStoreEquippedMod &&
            GM->InventoryComponent->AddItem(EquippedMod, FreeX, FreeY))
        {
            // 새 출처 제거가 실패하면 방금 보관한 기존 모드를 되돌립니다.
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
                bSourceRemoved = GM->EquipmentComponent->RemoveAttachedItem(ItemDropOp->ItemObj);
            }

            if (!bSourceRemoved)
            {
                GM->InventoryComponent->RemoveItem(EquippedMod->InstanceID);
                return false;
            }

            if (AllowedType == EAttachmentType::Sight) WeaponObj->EquippedSight = nullptr;
            else if (AllowedType == EAttachmentType::Muzzle) WeaponObj->EquippedMuzzle = nullptr;
            else if (AllowedType == EAttachmentType::Magazine) WeaponObj->EquippedMagazine = nullptr;
        }
        else return false;
    }
    else
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
            bSourceRemoved = GM->EquipmentComponent->RemoveAttachedItem(ItemDropOp->ItemObj);
        }

        if (!bSourceRemoved) return false;
    }

    // 새 모드 장착
    if (AllowedType == EAttachmentType::Sight) WeaponObj->EquippedSight = ItemDropOp->ItemObj;
    else if (AllowedType == EAttachmentType::Muzzle) WeaponObj->EquippedMuzzle = ItemDropOp->ItemObj;
    else if (AllowedType == EAttachmentType::Magazine) WeaponObj->EquippedMagazine = ItemDropOp->ItemObj;

    if (ItemDropOp->OriginalWidget) ItemDropOp->OriginalWidget->RemoveFromParent();

    RefreshSlotUI();
    if (ItemDropOp->SourceInventory) ItemDropOp->SourceInventory->OnInventoryChanged.Broadcast();
    WeaponObj->OnItemModified.Broadcast();

    return true;
}

FReply UModSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && EquippedMod)
    {
        return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UModSlotWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && EquippedMod && WeaponObj)
    {
        AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
        if (GM && GM->InventoryComponent && GM->EquipmentComponent)
        {
            bool bWeaponIsEquipped = false;
            for (const TPair<FName, UItemInstance*>& Pair : GM->EquipmentComponent->EquippedItems)
            {
                if (Pair.Value == WeaponObj)
                {
                    bWeaponIsEquipped = true;
                    break;
                }
            }

            const bool bModMatchesSlot =
                (AllowedType == EAttachmentType::Sight && WeaponObj->EquippedSight == EquippedMod) ||
                (AllowedType == EAttachmentType::Muzzle && WeaponObj->EquippedMuzzle == EquippedMod) ||
                (AllowedType == EAttachmentType::Magazine && WeaponObj->EquippedMagazine == EquippedMod);
            if (!bWeaponIsEquipped || !bModMatchesSlot) return Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);

            FIntPoint Size = EquippedMod->GetCurrentSize();
            int32 FreeX, FreeY;
            if (GM->InventoryComponent->FindEmptySpace(Size.X, Size.Y, FreeX, FreeY) &&
                GM->InventoryComponent->AddItem(EquippedMod, FreeX, FreeY))
            {
                if (AllowedType == EAttachmentType::Sight) WeaponObj->EquippedSight = nullptr;
                else if (AllowedType == EAttachmentType::Muzzle) WeaponObj->EquippedMuzzle = nullptr;
                else if (AllowedType == EAttachmentType::Magazine) WeaponObj->EquippedMagazine = nullptr;
                
                WeaponObj->OnItemModified.Broadcast();
                RefreshSlotUI();
                GM->InventoryComponent->OnInventoryChanged.Broadcast();
                return FReply::Handled();
            }
        }
    }
    return Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
}

void UModSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    if (!EquippedMod || !WeaponObj) return;

    UItemInstance* DraggedMod = EquippedMod;
    UItemDragDropOperation* DragDropOp = NewObject<UItemDragDropOperation>();
    DragDropOp->ItemID = DraggedMod->InstanceID;
    DragDropOp->ItemObj = DraggedMod;
    DragDropOp->SourceModSlot = this;
    
    // 모드를 분리
    if (AllowedType == EAttachmentType::Sight) WeaponObj->EquippedSight = nullptr;
    else if (AllowedType == EAttachmentType::Muzzle) WeaponObj->EquippedMuzzle = nullptr;
    else if (AllowedType == EAttachmentType::Magazine) WeaponObj->EquippedMagazine = nullptr;
    
    WeaponObj->OnItemModified.Broadcast();
    RefreshSlotUI();

    UDraggableItemWidget* DragVisual = WidgetTree->ConstructWidget<UDraggableItemWidget>(UDraggableItemWidget::StaticClass());
    DragVisual->ItemObj = DraggedMod;
    DragVisual->InitWidgetUI(false); 
    
    DragDropOp->DefaultDragVisual = DragVisual; 
    DragDropOp->Pivot = EDragPivot::MouseDown;
    
    FVector2D LocalClickPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    FVector2D SlotSize = InGeometry.GetLocalSize();
    FVector2D VisualSize = FVector2D(DraggedMod->GetCurrentSize().X * 64.0f, DraggedMod->GetCurrentSize().Y * 64.0f);
    
    float RatioX = FMath::Clamp(LocalClickPos.X / SlotSize.X, 0.0f, 1.0f);
    float RatioY = FMath::Clamp(LocalClickPos.Y / SlotSize.Y, 0.0f, 1.0f);
    DragDropOp->MouseOffset = FVector2D(VisualSize.X * RatioX, VisualSize.Y * RatioY);
    
    OutOperation = DragDropOp;
}

void UModSlotWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    Super::NativeOnDragCancelled(InDragDropEvent, InOperation);

    UItemDragDropOperation* ItemDropOp = Cast<UItemDragDropOperation>(InOperation);
    if (!ItemDropOp || ItemDropOp->SourceModSlot != this || !ItemDropOp->ItemObj || !WeaponObj) return;

    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    bool bWeaponIsEquipped = false;
    if (GM && GM->EquipmentComponent)
    {
        for (const TPair<FName, UItemInstance*>& Pair : GM->EquipmentComponent->EquippedItems)
        {
            if (Pair.Value == WeaponObj)
            {
                bWeaponIsEquipped = true;
                break;
            }
        }
    }

    UItemInstance* CurrentSlotItem = nullptr;
    if (AllowedType == EAttachmentType::Sight) CurrentSlotItem = WeaponObj->EquippedSight;
    else if (AllowedType == EAttachmentType::Muzzle) CurrentSlotItem = WeaponObj->EquippedMuzzle;
    else if (AllowedType == EAttachmentType::Magazine) CurrentSlotItem = WeaponObj->EquippedMagazine;

    if (bWeaponIsEquipped && (!CurrentSlotItem || CurrentSlotItem == ItemDropOp->ItemObj))
    {
        if (AllowedType == EAttachmentType::Sight) WeaponObj->EquippedSight = ItemDropOp->ItemObj;
        else if (AllowedType == EAttachmentType::Muzzle) WeaponObj->EquippedMuzzle = ItemDropOp->ItemObj;
        else if (AllowedType == EAttachmentType::Magazine) WeaponObj->EquippedMagazine = ItemDropOp->ItemObj;

        WeaponObj->OnItemModified.Broadcast();
        RefreshSlotUI();
        return;
    }

    if (GM && GM->InventoryComponent)
    {
        const FIntPoint Size = ItemDropOp->ItemObj->GetCurrentSize();
        int32 FreeX = 0;
        int32 FreeY = 0;
        if (GM->InventoryComponent->FindEmptySpace(Size.X, Size.Y, FreeX, FreeY) &&
            GM->InventoryComponent->AddItem(ItemDropOp->ItemObj, FreeX, FreeY))
        {
            GM->InventoryComponent->OnInventoryChanged.Broadcast();
            return;
        }
    }

    UE_LOG(LogTemp, Error, TEXT("Failed to restore cancelled mod drag without overwriting an active attachment."));
}

void UModSlotWidget::HandleModRightClicked(UItemInstance* ModObj)
{
    if (ModObj)
    {
        // ContextMenuWidget 팝업
        UClass* ContextMenuClass = LoadClass<UUserWidget>(nullptr, TEXT("/Script/GridLootMaster.ContextMenuWidget")); // Note: In C++ it's static class
        UContextMenuWidget* ContextMenu = CreateWidget<UContextMenuWidget>(GetWorld(), UContextMenuWidget::StaticClass());
        if (ContextMenu)
        {
            FVector2D ScreenPos = UWidgetLayoutLibrary::GetMousePositionOnPlatform();
            ContextMenu->Setup(ModObj, ScreenPos, true);
            // Inspect는 여기서 굳이 처리하지 않음 (필요하면 추가)
            ContextMenu->OnUnequipClicked.AddDynamic(this, &UModSlotWidget::HandleUnequipClicked);
            ContextMenu->OnUnloadClicked.AddDynamic(this, &UModSlotWidget::HandleUnloadClicked);
            ContextMenu->AddToViewport(400); // 팝업으로 최상단에 띄움
        }
    }
}

void UModSlotWidget::HandleUnequipClicked(UItemInstance* ModObj)
{
    if (!ModObj || ModObj != EquippedMod || !WeaponObj) return;

    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM || !GM->InventoryComponent || !GM->EquipmentComponent) return;

    bool bWeaponIsEquipped = false;
    for (const TPair<FName, UItemInstance*>& Pair : GM->EquipmentComponent->EquippedItems)
    {
        if (Pair.Value == WeaponObj)
        {
            bWeaponIsEquipped = true;
            break;
        }
    }
    if (!bWeaponIsEquipped) return;

    const bool bModMatchesSlot =
        (AllowedType == EAttachmentType::Sight && WeaponObj->EquippedSight == ModObj) ||
        (AllowedType == EAttachmentType::Muzzle && WeaponObj->EquippedMuzzle == ModObj) ||
        (AllowedType == EAttachmentType::Magazine && WeaponObj->EquippedMagazine == ModObj);
    if (!bModMatchesSlot) return;

    FIntPoint Size = ModObj->GetCurrentSize();
    int32 FreeX, FreeY;
    if (GM->InventoryComponent->FindEmptySpace(Size.X, Size.Y, FreeX, FreeY) &&
        GM->InventoryComponent->AddItem(ModObj, FreeX, FreeY))
    {
        if (AllowedType == EAttachmentType::Sight) WeaponObj->EquippedSight = nullptr;
        else if (AllowedType == EAttachmentType::Muzzle) WeaponObj->EquippedMuzzle = nullptr;
        else if (AllowedType == EAttachmentType::Magazine) WeaponObj->EquippedMagazine = nullptr;

        WeaponObj->OnItemModified.Broadcast();
        RefreshSlotUI();
        GM->InventoryComponent->OnInventoryChanged.Broadcast();
    }
}

void UModSlotWidget::HandleUnloadClicked(UItemInstance* ModObj)
{
    if (!ModObj || ModObj != EquippedMod || !WeaponObj) return;

    if (ModObj->Category == EItemCategory::Attachment && ModObj->AttachmentType == EAttachmentType::Magazine && ModObj->CurrentAmmo > 0)
    {
        AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
        if (!GM || !GM->ItemDataTable || !GM->EquipmentComponent) return;

        bool bWeaponIsEquipped = false;
        for (const TPair<FName, UItemInstance*>& Pair : GM->EquipmentComponent->EquippedItems)
        {
            if (Pair.Value == WeaponObj)
            {
                bWeaponIsEquipped = true;
                break;
            }
        }
        if (!bWeaponIsEquipped || WeaponObj->EquippedMagazine != ModObj) return;

        FName AmmoID = GM->FindCompatibleAmmoID(ModObj);
        if (AmmoID == NAME_None) return;
        
        FItemData* AmmoData = GM->ItemDataTable->FindRow<FItemData>(AmmoID, TEXT("Unload"));
        if (!AmmoData) return;
        if (AmmoData->MaxStack <= 0) return;

        int32 AmmoAmount = ModObj->CurrentAmmo;

        while (AmmoAmount > 0)
        {
            int32 StackToSpawn = FMath::Min(AmmoAmount, AmmoData->MaxStack);

            UItemInstance* NewAmmo = NewObject<UItemInstance>(GM);
            NewAmmo->InstanceID = FName(*FString::Printf(TEXT("%s_Unloaded_%s"), *AmmoID.ToString(), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
            NewAmmo->InitFromData(*AmmoData);
            NewAmmo->CurrentStack = StackToSpawn;
            NewAmmo->bIsExamined = true;
            NewAmmo->bIsRotated = false;

            int32 FreeX, FreeY;
            bool bAdded = false;
            if (GM->InventoryComponent && GM->InventoryComponent->FindEmptySpace(NewAmmo->GetCurrentSize().X, NewAmmo->GetCurrentSize().Y, FreeX, FreeY))
            {
                bAdded = GM->InventoryComponent->AddItem(NewAmmo, FreeX, FreeY);
            }

            if (!bAdded) break;
            AmmoAmount -= StackToSpawn;
        }

        ModObj->CurrentAmmo = AmmoAmount;

        RefreshSlotUI();
        if (GM->InventoryComponent) GM->InventoryComponent->OnInventoryChanged.Broadcast();
        WeaponObj->OnItemModified.Broadcast();
    }
}
