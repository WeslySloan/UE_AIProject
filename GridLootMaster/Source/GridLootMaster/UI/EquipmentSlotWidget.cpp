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
        BackgroundBorder->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f));
        UOverlaySlot* BGOvSlot = RootOverlay->AddChildToOverlay(BackgroundBorder);
        BGOvSlot->SetHorizontalAlignment(HAlign_Fill);
        BGOvSlot->SetVerticalAlignment(VAlign_Fill);

        SlotNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SlotNameText"));
        SlotNameText->SetText(FText::FromString(SlotName));
        SlotNameText->SetColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f));
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
    EquippedItem = NewItem;
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
            BackgroundBorder->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f)); // Normal (Dark Gray)
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
        BackgroundBorder->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f));
    }
}

bool UEquipmentSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);

    if (BackgroundBorder) BackgroundBorder->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f));

    UItemDragDropOperation* ItemDropOp = Cast<UItemDragDropOperation>(InOperation);
    if (!ItemDropOp || !ItemDropOp->ItemObj) return false;

    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM) return false;

    // 퀵 모딩 (부착물 -> 장착된 무기)
    if (ItemDropOp->ItemObj->Category == EItemCategory::Attachment && EquippedItem && EquippedItem->Category == EItemCategory::Weapon)
    {
        EAttachmentType ModType = ItemDropOp->ItemObj->AttachmentType;
        bool bAttached = false;
        if (ModType == EAttachmentType::Sight && !EquippedItem->EquippedSight)
        {
            EquippedItem->EquippedSight = ItemDropOp->ItemObj;
            bAttached = true;
        }
        else if (ModType == EAttachmentType::Muzzle && !EquippedItem->EquippedMuzzle)
        {
            EquippedItem->EquippedMuzzle = ItemDropOp->ItemObj;
            bAttached = true;
        }
        else if (ModType == EAttachmentType::Magazine && !EquippedItem->EquippedMagazine)
        {
            EquippedItem->EquippedMagazine = ItemDropOp->ItemObj;
            bAttached = true;
        }

        if (bAttached)
        {
            if (ItemDropOp->SourceInventory) ItemDropOp->SourceInventory->RemoveItem(ItemDropOp->ItemObj->InstanceID);
            if (ItemDropOp->OriginalWidget) ItemDropOp->OriginalWidget->RemoveFromParent();
            
            RefreshSlotUI();
            if (ItemDropOp->SourceInventory) ItemDropOp->SourceInventory->OnInventoryChanged.Broadcast();
            return true;
        }
    }

    // 삽탄 (탄약 -> 장착된 무기)
    if (ItemDropOp->ItemObj->TemplateID.ToString().Contains("Ammo") && EquippedItem && EquippedItem->Category == EItemCategory::Weapon && EquippedItem->EquippedMagazine)
    {
        UItemInstance* TargetMag = EquippedItem->EquippedMagazine;
        if (TargetMag->CurrentAmmo < TargetMag->MaxAmmo)
        {
            int32 AvailableSpace = TargetMag->MaxAmmo - TargetMag->CurrentAmmo;
            int32 AmountToLoad = FMath::Min(ItemDropOp->ItemObj->CurrentStack, AvailableSpace);
            
            TargetMag->CurrentAmmo += AmountToLoad;
            ItemDropOp->ItemObj->CurrentStack -= AmountToLoad;
            
            bool bAmmoDepleted = (ItemDropOp->ItemObj->CurrentStack <= 0);
            if (bAmmoDepleted)
            {
                if (ItemDropOp->SourceInventory) ItemDropOp->SourceInventory->RemoveItem(ItemDropOp->ItemObj->InstanceID);
                if (ItemDropOp->OriginalWidget) ItemDropOp->OriginalWidget->RemoveFromParent();
            }
            
            RefreshSlotUI();
            if (ItemDropOp->SourceInventory) ItemDropOp->SourceInventory->OnInventoryChanged.Broadcast();
            return true;
        }
    }

    // 카테고리 체크
    if (ItemDropOp->ItemObj->Category != AllowedCategory) return false;

    // 이미 장착된 아이템이 있다면 인벤토리(가방)로 되돌림
    if (EquippedItem)
    {
        // 빈 공간 찾아서 넣기
        FIntPoint Size = EquippedItem->GetCurrentSize();
        int32 FreeX, FreeY;
        if (GM->InventoryComponent->FindEmptySpace(Size.X, Size.Y, FreeX, FreeY))
        {
            if (GM->EquipmentComponent) GM->EquipmentComponent->RemoveItemByInstanceID(EquippedItem->InstanceID);
            GM->InventoryComponent->AddItem(EquippedItem, FreeX, FreeY);
        }
        else
        {
            // 인벤토리 꽉 참 - 드롭 취소
            return false;
        }
    }

    // 새 아이템 장착 (원본 인벤토리에서 제거)
    if (GM->InventoryComponent) GM->InventoryComponent->RemoveItem(ItemDropOp->ItemObj->InstanceID);
    if (GM->LootContainerComponent) GM->LootContainerComponent->RemoveItem(ItemDropOp->ItemObj->InstanceID);
    if (GM->SafeBoxComponent) GM->SafeBoxComponent->RemoveItem(ItemDropOp->ItemObj->InstanceID);
    if (GM->RigComponent) GM->RigComponent->RemoveItem(ItemDropOp->ItemObj->InstanceID);
    if (GM->PocketComponent) GM->PocketComponent->RemoveItem(ItemDropOp->ItemObj->InstanceID);
    if (GM->EquipmentComponent)
    {
        GM->EquipmentComponent->RemoveItemByInstanceID(ItemDropOp->ItemObj->InstanceID);
        GM->EquipmentComponent->EquipItem(SlotID, ItemDropOp->ItemObj);
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
    DragDropOp->OriginalWidget = Cast<UDraggableItemWidget>(ItemCanvas->GetChildAt(0)); // 원본은 그대로 둠
    
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
            ContextMenu->Setup(ItemObj, ScreenPos);
            ContextMenu->OnInspectClicked.AddDynamic(this, &UEquipmentSlotWidget::HandleInspectItem);
            ContextMenu->OnUnequipClicked.AddDynamic(this, &UEquipmentSlotWidget::HandleUnequipClicked);
            ContextMenu->OnUnloadClicked.AddDynamic(this, &UEquipmentSlotWidget::HandleUnloadClicked);
            ContextMenu->AddToViewport(400); 
        }
    }
}

void UEquipmentSlotWidget::HandleInspectItem(UItemInstance* TargetItem)
{
    if (TargetItem)
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
        AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
        if (GM && GM->InventoryComponent && GM->EquipmentComponent)
        {
            FIntPoint Size = ItemObj->GetCurrentSize();
            int32 FreeX, FreeY;
            if (GM->InventoryComponent->FindEmptySpace(Size.X, Size.Y, FreeX, FreeY))
            {
                GM->EquipmentComponent->RemoveItemByInstanceID(ItemObj->InstanceID);
                GM->InventoryComponent->AddItem(ItemObj, FreeX, FreeY);
                RefreshSlotUI();
                GM->InventoryComponent->OnInventoryChanged.Broadcast();
            }
        }
    }
}

void UEquipmentSlotWidget::HandleUnloadClicked(UItemInstance* ItemObj)
{
    if (!ItemObj) return;

    AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM || !GM->ItemDataTable) return;

    // 1. 무기인 경우: 결합된 탄창을 분리하여 인벤토리에 넣음
    if (ItemObj->Category == EItemCategory::Weapon && ItemObj->EquippedMagazine)
    {
        UItemInstance* MagItem = ItemObj->EquippedMagazine;
        
        int32 FreeX, FreeY;
        if (GM->InventoryComponent && GM->InventoryComponent->FindEmptySpace(MagItem->GetCurrentSize().X, MagItem->GetCurrentSize().Y, FreeX, FreeY))
        {
            GM->InventoryComponent->AddItem(MagItem, FreeX, FreeY);
            ItemObj->EquippedMagazine = nullptr;
            
            RefreshSlotUI();
            GM->InventoryComponent->OnInventoryChanged.Broadcast();
        }
        return;
    }

    // 2. 탄창인 경우: 내부 총알 추출
    if (ItemObj->Category == EItemCategory::Attachment && ItemObj->AttachmentType == EAttachmentType::Magazine)
    {
        if (ItemObj->CurrentAmmo <= 0) return;

        FName AmmoID = TEXT("Ammo_556_M995");
        if (ItemObj->TemplateID == TEXT("Mag_M4"))
        {
            AmmoID = TEXT("Ammo_556_M995");
        }
        
        FItemData* AmmoData = GM->ItemDataTable->FindRow<FItemData>(AmmoID, TEXT("Unload"));
        if (!AmmoData) return;

        int32 AmmoAmount = ItemObj->CurrentAmmo;
        ItemObj->CurrentAmmo = 0;

        while (AmmoAmount > 0)
        {
            int32 StackToSpawn = FMath::Min(AmmoAmount, AmmoData->MaxStack);
            AmmoAmount -= StackToSpawn;

            UItemInstance* NewAmmo = NewObject<UItemInstance>(GM);
            NewAmmo->InstanceID = FName(*FString::Printf(TEXT("%s_Unloaded_%d"), *AmmoID.ToString(), FMath::RandRange(10000, 99999)));
            NewAmmo->InitFromData(*AmmoData);
            NewAmmo->CurrentStack = StackToSpawn;
            NewAmmo->bIsExamined = true;
            NewAmmo->bIsRotated = false;

            int32 FreeX, FreeY;
            if (GM->InventoryComponent && GM->InventoryComponent->FindEmptySpace(NewAmmo->GetCurrentSize().X, NewAmmo->GetCurrentSize().Y, FreeX, FreeY))
            {
                GM->InventoryComponent->AddItem(NewAmmo, FreeX, FreeY);
            }
        }

        RefreshSlotUI();
        if (GM->InventoryComponent) GM->InventoryComponent->OnInventoryChanged.Broadcast();
    }
}

void UEquipmentSlotWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    Super::NativeOnDragCancelled(InDragDropEvent, InOperation);
    // 드래그가 취소되면 슬롯 UI를 갱신하여 꽉 찬 뷰로 되돌림
    RefreshSlotUI();
}
