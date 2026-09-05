#include "GridBoardWidget.h"
#include "ItemDragDropOperation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "SplitStackWidget.h"
#include "DraggableItemWidget.h"
#include "../GridGameMode.h"
#include "../GridInventoryComponent.h"
#include "../EquipmentComponent.h"
#include "Kismet/GameplayStatics.h"
#include "../ItemInstance.h"

bool UGridBoardWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (WidgetTree && !WidgetTree->RootWidget)
    {
        RootSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RootSizeBox"));
        WidgetTree->RootWidget = RootSizeBox;
        
        GridCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GridCanvas"));
        RootSizeBox->AddChild(GridCanvas);

        // 1. 전체 영역에 대한 히트 판정을 받기 위한 투명 배경 (마우스가 그리드 밖으로 조금 나가도 놓치지 않음)
        UBorder* HitTestBG = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        HitTestBG->SetBrushColor(FLinearColor::Transparent);
        UCanvasPanelSlot* HitSlot = GridCanvas->AddChildToCanvas(HitTestBG);
        HitSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
        HitSlot->SetOffsets(FMargin(0, 0, 0, 0));

        // 2. 실제 시각적인 그리드 배경
        BackgroundBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        BackgroundBorder->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f));
        UCanvasPanelSlot* BGSlot = GridCanvas->AddChildToCanvas(BackgroundBorder);
        BGSlot->SetPosition(FVector2D(0.0f, 0.0f));
        // 임시 크기 부여, RefreshGridUI에서 다시 맞춤
        BGSlot->SetSize(FVector2D(64.0f, 64.0f));
        
        // 미리보기용 외곽선 (평소엔 숨김)
        PreviewBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PreviewBorder"));
        PreviewBorder->SetVisibility(ESlateVisibility::Hidden);
        UCanvasPanelSlot* PreviewSlot = GridCanvas->AddChildToCanvas(PreviewBorder);
        PreviewSlot->SetZOrder(100); // 아이템들보다 항상 위에 표시
    }
    return true;
}

bool UGridBoardWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);

    if (PreviewBorder)
    {
        PreviewBorder->SetVisibility(ESlateVisibility::Hidden);
    }

    UItemDragDropOperation* ItemDropOp = Cast<UItemDragDropOperation>(InOperation);
    if (ItemDropOp && ItemDropOp->ItemObj && InventoryComponent)
    {
        if (ItemDropOp->SourceEquipmentSlot == TEXT("Backpack") || ItemDropOp->SourceEquipmentSlot == TEXT("Rig"))
        {
            if (AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this)))
            {
                return GM->TryStandaloneStorageUnequip(ItemDropOp->SourceEquipmentSlot, ItemDropOp->ItemObj);
            }
            return false;
        }
        int32 GridX = 0;
        int32 GridY = 0;

        if (GetGridCellFromMousePosition(InGeometry, InDragDropEvent, ItemDropOp, GridX, GridY))
        {
            FIntPoint CurrentSize = ItemDropOp->ItemObj->GetCurrentSize();
            int32 Width = CurrentSize.X;
            int32 Height = CurrentSize.Y;

            // 스플릿 모드인 경우 팝업 띄우고 즉시 종료
            if (ItemDropOp->bIsSplitDrag && ItemDropOp->SourceInventory)
            {
                PendingSplitItem = ItemDropOp->ItemObj;
                PendingSplitSourceInv = ItemDropOp->SourceInventory;
                PendingSplitSectionIndex = SectionIndex;
                PendingSplitX = GridX;
                PendingSplitY = GridY;
                
                USplitStackWidget* SplitWidget = CreateWidget<USplitStackWidget>(GetWorld(), USplitStackWidget::StaticClass());
                if (SplitWidget)
                {
                    SplitWidget->Setup(ItemDropOp->ItemObj->CurrentStack - 1);
                    SplitWidget->OnSplitConfirmed.AddDynamic(this, &UGridBoardWidget::OnSplitDragConfirmed);
                    SplitWidget->AddToViewport(100);
                }
                
                if (ItemDropOp->OriginalWidget)
                {
                    ItemDropOp->OriginalWidget->SetVisibility(ESlateVisibility::Visible);
                }
                return true;
            }

            // 1. 해당 칸에 이미 동일한 종류의 아이템이 있는지 (병합 가능 여부 확인)
            if (InventoryComponent->IsValidSection(SectionIndex))
            {
                if (InventoryComponent->IsValidSectionIndex(SectionIndex, GridX + GridY * InventoryComponent->GetSectionSize(SectionIndex).X))
                {
                    FName ExistingItem = InventoryComponent->GetCellItemID(SectionIndex, GridX, GridY);
                    if (ExistingItem != NAME_None && ExistingItem != ItemDropOp->ItemID)
                    {
                        UItemInstance* ExistingItemObj = InventoryComponent->GetItemInstance(ExistingItem);
                        if (!ExistingItemObj)
                        {
                            return false;
                        }
                        
                        // 퀵 모딩 (부착물 -> 무기)
                        if (ItemDropOp->ItemObj->Category == EItemCategory::Attachment && ExistingItemObj && ExistingItemObj->Category == EItemCategory::Weapon)
                        {
                            AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
                            EAttachmentType ModType = ItemDropOp->ItemObj->AttachmentType;

                            if (ModType == EAttachmentType::Magazine &&
                                !ExistingItemObj->IsCompatibleMagazine(ItemDropOp->ItemObj))
                            {
                                return false;
                            }

                            const bool bSlotAvailable =
                                (ModType == EAttachmentType::Sight && !ExistingItemObj->EquippedSight) ||
                                (ModType == EAttachmentType::Muzzle && !ExistingItemObj->EquippedMuzzle) ||
                                (ModType == EAttachmentType::Magazine && !ExistingItemObj->EquippedMagazine);

                            if (GM && GM->EquipmentComponent && bSlotAvailable)
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

                                if (ModType == EAttachmentType::Sight) ExistingItemObj->EquippedSight = ItemDropOp->ItemObj;
                                else if (ModType == EAttachmentType::Muzzle) ExistingItemObj->EquippedMuzzle = ItemDropOp->ItemObj;
                                else if (ModType == EAttachmentType::Magazine) ExistingItemObj->EquippedMagazine = ItemDropOp->ItemObj;

                                if (ItemDropOp->OriginalWidget) ItemDropOp->OriginalWidget->RemoveFromParent();
                                
                                ExistingItemObj->OnItemModified.Broadcast();
                                RefreshGridUI();
                                if (ItemDropOp->SourceInventory) ItemDropOp->SourceInventory->OnInventoryChanged.Broadcast();
                                return true;
                            }
                        }
                        
                        // 탄약 삽탄 로직 추가 (드래그 대상이 탄약인 경우)
                        if (ItemDropOp->ItemObj->Category == EItemCategory::Consumable)
                        {
                            if (!ItemDropOp->SourceInventory ||
                                ItemDropOp->SourceInventory->GetItemInstance(ItemDropOp->ItemID) != ItemDropOp->ItemObj)
                            {
                                return false;
                            }

                            UItemInstance* TargetMag = nullptr;
                            if (ExistingItemObj->Category == EItemCategory::Attachment && ExistingItemObj->AttachmentType == EAttachmentType::Magazine)
                            {
                                TargetMag = ExistingItemObj;
                            }
                            else if (ExistingItemObj->Category == EItemCategory::Weapon && ExistingItemObj->EquippedMagazine)
                            {
                                TargetMag = ExistingItemObj->EquippedMagazine;
                            }

                            if (TargetMag && TargetMag->IsCompatibleAmmo(ItemDropOp->ItemObj) && TargetMag->CurrentAmmo < TargetMag->MaxAmmo)
                            {
                                int32 AvailableSpace = TargetMag->MaxAmmo - TargetMag->CurrentAmmo;
                                int32 AmountToLoad = FMath::Min(ItemDropOp->ItemObj->CurrentStack, AvailableSpace);
                                
                                TargetMag->CurrentAmmo += AmountToLoad;
                                ItemDropOp->ItemObj->CurrentStack -= AmountToLoad;
                                TargetMag->OnItemModified.Broadcast();
                                if (ExistingItemObj != TargetMag)
                                {
                                    ExistingItemObj->OnItemModified.Broadcast();
                                }
                                
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
                                
                                RefreshGridUI();
                                if (ItemDropOp->SourceInventory) ItemDropOp->SourceInventory->OnInventoryChanged.Broadcast();
                                return true;
                            }
                        }

                        if (ItemDropOp->SourceInventory &&
                            (ItemDropOp->ItemObj->InstanceID != ItemDropOp->ItemID ||
                             ItemDropOp->SourceInventory->GetItemInstance(ItemDropOp->ItemID) != ItemDropOp->ItemObj))
                        {
                            return false;
                        }

                        const int32 SourceStackBeforeMerge = ItemDropOp->ItemObj->CurrentStack;
                        const int32 TargetStackBeforeMerge = ExistingItemObj->CurrentStack;
                        if (InventoryComponent->TryMergeItem(ItemDropOp->ItemObj, ExistingItem))
                        {
                            // 완전 병합되어 남은 수량이 0이 된 경우에만 원본을 제거합니다.
                            if (ItemDropOp->ItemObj->CurrentStack <= 0)
                            {
                                AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
                                bool bSourceRemoved = false;
                                if (ItemDropOp->SourceInventory)
                                {
                                    if (ItemDropOp->SourceInventory->GetItemInstance(ItemDropOp->ItemID) == ItemDropOp->ItemObj)
                                    {
                                        bSourceRemoved = ItemDropOp->SourceInventory->RemoveItem(ItemDropOp->ItemObj->InstanceID);
                                    }
                                }
                                else if (GM && GM->EquipmentComponent)
                                {
                                    bSourceRemoved = GM->EquipmentComponent->RemoveItemByInstanceID(ItemDropOp->ItemID);
                                }
                                else
                                {
                                    bSourceRemoved = true;
                                }

                                if (!bSourceRemoved)
                                {
                                    ExistingItemObj->CurrentStack = TargetStackBeforeMerge;
                                    ItemDropOp->ItemObj->CurrentStack = SourceStackBeforeMerge;
                                    ExistingItemObj->OnItemModified.Broadcast();
                                    ItemDropOp->ItemObj->OnItemModified.Broadcast();
                                    if (ItemDropOp->SourceInventory && ItemDropOp->SourceInventory != InventoryComponent)
                                    {
                                        ItemDropOp->SourceInventory->OnInventoryChanged.Broadcast();
                                    }
                                    return false;
                                }
                                if (ItemDropOp->OriginalWidget)
                                {
                                    ItemDropOp->OriginalWidget->RemoveFromParent();
                                }
                            }

                            // 부분 병합이든 완전 병합이든 UI를 갱신하고 드롭 처리 완료
                            // (부분 병합 시에는 원본 아이템이 계속 원래 자리에 남아있고 UI의 스택 숫자만 갱신됨)
                            RefreshGridUI();
                            if (ItemDropOp->SourceInventory)
                            {
                                ItemDropOp->SourceInventory->OnInventoryChanged.Broadcast();
                            }

                            // 원본 인벤토리(상자 등) UI도 갱신해야 하므로 GameMode 등을 통해 전체 브로드캐스트 권장
                            AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
                            if (GM && GM->LootContainerComponent) GM->LootContainerComponent->OnInventoryChanged.Broadcast();
                            if (GM && GM->SafeBoxComponent) GM->SafeBoxComponent->OnInventoryChanged.Broadcast();
                            if (GM && GM->RigComponent) GM->RigComponent->OnInventoryChanged.Broadcast();
                            if (GM && GM->PocketComponent) GM->PocketComponent->OnInventoryChanged.Broadcast();
                            if (GM && GM->StashComponent) GM->StashComponent->OnInventoryChanged.Broadcast();

                            return true;
                        }
                    }
                }
            }

            // 2. 병합이 아니면 빈 공간 이동 처리
            if (InventoryComponent->CheckItemFitInSection(ItemDropOp->ItemID, SectionIndex, GridX, GridY, Width, Height))
            {
                AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
                bool bMoved = false;

                if (ItemDropOp->SourceInventory &&
                    ItemDropOp->SourceInventory->GetItemInstance(ItemDropOp->ItemID) != ItemDropOp->ItemObj)
                {
                    return false;
                }

                // 다른 인벤토리에서 이동할 때는 대상 추가를 먼저 성공시킨 뒤 원본을 제거합니다.
                if (ItemDropOp->SourceInventory && ItemDropOp->SourceInventory != InventoryComponent)
                {
                    bMoved = InventoryComponent->AddItemToSection(ItemDropOp->ItemObj, SectionIndex, GridX, GridY);
                    if (bMoved)
                    {
                        if (!ItemDropOp->SourceInventory->RemoveItem(ItemDropOp->ItemID))
                        {
                            InventoryComponent->RemoveItem(ItemDropOp->ItemID);
                            bMoved = false;
                        }
                    }
                }
                else if (ItemDropOp->SourceInventory == InventoryComponent)
                {
                    int32 OriginalSection = INDEX_NONE;
                    int32 OriginalX = INDEX_NONE;
                    int32 OriginalY = INDEX_NONE;
                    InventoryComponent->FindItemPlacement(ItemDropOp->ItemID, OriginalSection, OriginalX, OriginalY);

                    const bool bSourceRemoved = InventoryComponent->RemoveItem(ItemDropOp->ItemID);
                    bMoved = bSourceRemoved && InventoryComponent->AddItemToSection(ItemDropOp->ItemObj, SectionIndex, GridX, GridY);
                    if (!bMoved && OriginalX != INDEX_NONE)
                    {
                        // 새 위치 수납 실패 시 원래 위치로 복구합니다.
                        InventoryComponent->AddItemToSection(ItemDropOp->ItemObj, OriginalSection, OriginalX, OriginalY);
                    }
                }
                else
                {
                    UGridInventoryComponent* UnknownSourceInventory = nullptr;
                    FName SourceEquipmentSlot = NAME_None;

                    if (GM)
                    {
                        auto FindUnknownSource = [&](UGridInventoryComponent* Candidate)
                        {
                            if (!UnknownSourceInventory && Candidate && Candidate != InventoryComponent &&
                                Candidate->GetItemInstance(ItemDropOp->ItemID) == ItemDropOp->ItemObj)
                            {
                                UnknownSourceInventory = Candidate;
                            }
                        };

                        FindUnknownSource(GM->InventoryComponent);
                        FindUnknownSource(GM->LootContainerComponent);
                        FindUnknownSource(GM->SafeBoxComponent);
                        FindUnknownSource(GM->RigComponent);
                        FindUnknownSource(GM->PocketComponent);
                        FindUnknownSource(GM->StashComponent);
                        if (!UnknownSourceInventory)
                        {
                            UnknownSourceInventory = GM->FindCorpseLootInventory(ItemDropOp->ItemID, UnknownSourceInventory)
                                ? UnknownSourceInventory : nullptr;
                        }

                        if (!UnknownSourceInventory && GM->EquipmentComponent)
                        {
                            for (const TPair<FName, UItemInstance*>& Pair : GM->EquipmentComponent->EquippedItems)
                            {
                                if (Pair.Value == ItemDropOp->ItemObj)
                                {
                                    SourceEquipmentSlot = Pair.Key;
                                    break;
                                }
                            }
                        }
                    }

                    if (!ItemDropOp->SourceModSlot && SourceEquipmentSlot == NAME_None && !UnknownSourceInventory)
                    {
                        return false;
                    }
                    if (SourceEquipmentSlot == TEXT("Backpack") || SourceEquipmentSlot == TEXT("Rig"))
                    {
                        return GM && GM->TryStandaloneStorageUnequip(SourceEquipmentSlot, ItemDropOp->ItemObj);
                    }

                    // 출처가 없는 드래그도 대상 추가를 먼저 성공시켜 실패 시 원본을 보존합니다.
                    bMoved = InventoryComponent->AddItemToSection(ItemDropOp->ItemObj, SectionIndex, GridX, GridY);
                    if (bMoved)
                    {
                        if (UnknownSourceInventory)
                        {
                            if (!UnknownSourceInventory->RemoveItem(ItemDropOp->ItemID))
                            {
                                InventoryComponent->RemoveItem(ItemDropOp->ItemID);
                                bMoved = false;
                            }
                        }
                        else if (GM && SourceEquipmentSlot != NAME_None)
                        {
                            if (!GM->EquipmentComponent->RemoveItemBySlotID(SourceEquipmentSlot))
                            {
                                InventoryComponent->RemoveItem(ItemDropOp->ItemID);
                                bMoved = false;
                            }
                        }
                    }
                }
                
                // 새 위치에 아이템 추가
                if (bMoved)
                {
                    if (ItemDropOp->OriginalWidget)
                    {
                        ItemDropOp->OriginalWidget->RemoveFromParent();
                    }
                    RefreshGridUI();
                    if (GM && GM->LootContainerComponent) GM->LootContainerComponent->OnInventoryChanged.Broadcast();
                    if (GM && GM->SafeBoxComponent) GM->SafeBoxComponent->OnInventoryChanged.Broadcast();
                    if (GM && GM->RigComponent) GM->RigComponent->OnInventoryChanged.Broadcast();
                    if (GM && GM->PocketComponent) GM->PocketComponent->OnInventoryChanged.Broadcast();
                    if (GM && GM->StashComponent) GM->StashComponent->OnInventoryChanged.Broadcast();
                    return true;
                }
            }
        }
    }
    return false;
}

void UGridBoardWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);
    // 아무 처리도 하지 않지만, 이벤트를 받아들이기 위해 오버라이드
}

bool UGridBoardWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    Super::NativeOnDragOver(InGeometry, InDragDropEvent, InOperation);

    UItemDragDropOperation* ItemDropOp = Cast<UItemDragDropOperation>(InOperation);
    if (ItemDropOp && ItemDropOp->ItemObj && InventoryComponent && PreviewBorder)
    {
        int32 GridX = 0;
        int32 GridY = 0;

        if (GetGridCellFromMousePosition(InGeometry, InDragDropEvent, ItemDropOp, GridX, GridY))
        {
            FIntPoint ItemSize = ItemDropOp->ItemObj->GetCurrentSize();
            int32 Width = ItemSize.X;
            int32 Height = ItemSize.Y;

            bool bFits = InventoryComponent->CheckItemFitInSection(ItemDropOp->ItemID, SectionIndex, GridX, GridY, Width, Height);

            PreviewBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
            PreviewBorder->SetBrushColor(bFits ? FLinearColor(0.0f, 1.0f, 0.0f, 0.4f) : FLinearColor(1.0f, 0.0f, 0.0f, 0.4f));

            if (UCanvasPanelSlot* PreviewSlot = Cast<UCanvasPanelSlot>(PreviewBorder->Slot))
            {
                PreviewSlot->SetPosition(FVector2D(GridX * 64.0f, GridY * 64.0f));
                PreviewSlot->SetSize(FVector2D(Width * 64.0f, Height * 64.0f));
            }
            return true;
        }
    }
    return false;
}

void UGridBoardWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    Super::NativeOnDragLeave(InDragDropEvent, InOperation);
    
    if (PreviewBorder)
    {
        PreviewBorder->SetVisibility(ESlateVisibility::Hidden);
    }
}

bool UGridBoardWidget::GetGridCellFromMousePosition(const FGeometry& Geometry, const FPointerEvent& PointerEvent, UItemDragDropOperation* Operation, int32& OutX, int32& OutY)
{
    FVector2D MouseScreenPos = PointerEvent.GetScreenSpacePosition();
    FVector2D VisualTopLeftScreenPos = MouseScreenPos;
    
    if (Operation)
    {
        VisualTopLeftScreenPos -= Operation->MouseOffset;
    }

    FVector2D LocalPos = Geometry.AbsoluteToLocal(VisualTopLeftScreenPos);
    const float CellSize = 64.0f; 
    
    OutX = FMath::FloorToInt(LocalPos.X / CellSize);
    OutY = FMath::FloorToInt(LocalPos.Y / CellSize);
    return true;
}

void UGridBoardWidget::RefreshGridUI()
{
    if (!GridCanvas || !InventoryComponent) return;

    const FIntPoint SectionSize = InventoryComponent->GetSectionSize(SectionIndex);

    for (int32 i = GridCanvas->GetChildrenCount() - 1; i >= 0; --i)
    {
        UWidget* Child = GridCanvas->GetChildAt(i);
        if (Child->GetName() == TEXT("PreviewBorder") || i <= 1) continue;
        GridCanvas->RemoveChildAt(i);
    }

    if (BackgroundBorder)
    {
        if (UCanvasPanelSlot* BGSlot = Cast<UCanvasPanelSlot>(BackgroundBorder->Slot))
        {
            BGSlot->SetSize(FVector2D(SectionSize.X * 64.0f, SectionSize.Y * 64.0f));
        }
    }

    if (RootSizeBox)
    {
        RootSizeBox->SetWidthOverride(SectionSize.X * 64.0f);
        RootSizeBox->SetHeightOverride(SectionSize.Y * 64.0f);
    }

    if (GridCanvas && GridCanvas->GetChildrenCount() > 0)
    {
        if (UWidget* HitTestBGWidget = GridCanvas->GetChildAt(0))
        {
            if (UCanvasPanelSlot* HitSlot = Cast<UCanvasPanelSlot>(HitTestBGWidget->Slot))
            {
                HitSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f)); // 자동 확장을 막음
                HitSlot->SetSize(FVector2D(SectionSize.X * 64.0f, SectionSize.Y * 64.0f));
            }
        }
    }

    for (int Y = 0; Y < SectionSize.Y; ++Y)
    {
        for (int X = 0; X < SectionSize.X; ++X)
        {
            UBorder* CellVisual = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
            CellVisual->SetBrushColor(FLinearColor(0.5f, 0.5f, 0.5f, 0.4f));
            CellVisual->SetVisibility(ESlateVisibility::HitTestInvisible);
            
            UCanvasPanelSlot* CellSlot = GridCanvas->AddChildToCanvas(CellVisual);
            CellSlot->SetPosition(FVector2D(X * 64.0f + 1.0f, Y * 64.0f + 1.0f));
            CellSlot->SetSize(FVector2D(62.0f, 62.0f)); 
        }
    }

    TArray<FName> CheckedItems;

    for (int Y = 0; Y < SectionSize.Y; ++Y)
    {
        for (int X = 0; X < SectionSize.X; ++X)
        {
            FName ItemID = InventoryComponent->GetCellItemID(SectionIndex, X, Y);

            if (ItemID != NAME_None && !CheckedItems.Contains(ItemID))
            {
                CheckedItems.Add(ItemID);

                UDraggableItemWidget* ItemVisual = WidgetTree->ConstructWidget<UDraggableItemWidget>(UDraggableItemWidget::StaticClass());
                ItemVisual->ItemObj = InventoryComponent->GetItemInstance(ItemID);
                ItemVisual->SourceInventory = InventoryComponent;
                ItemVisual->SourceSectionIndex = SectionIndex;
                ItemVisual->InitWidgetUI();
                
                UCanvasPanelSlot* ItemSlot = GridCanvas->AddChildToCanvas(ItemVisual);
                ItemSlot->SetPosition(FVector2D(X * 64.0f, Y * 64.0f));
                ItemSlot->SetAutoSize(true);
                ItemSlot->SetZOrder(10); // 아이템이 항상 칸 배경 위로 오게 함
            }
        }
    }
}

void UGridBoardWidget::OnSplitDragConfirmed(int32 SplitAmount)
{
    if (!PendingSplitItem || !PendingSplitSourceInv || !InventoryComponent) return;
    if (PendingSplitSourceInv->GetItemInstance(PendingSplitItem->InstanceID) != PendingSplitItem)
    {
        PendingSplitItem = nullptr;
        PendingSplitSourceInv = nullptr;
        return;
    }
    if (PendingSplitItem->EquippedSight || PendingSplitItem->EquippedMuzzle || PendingSplitItem->EquippedMagazine)
    {
        return;
    }

    const int32 MaxSplitAmount = PendingSplitItem->CurrentStack - 1;
    if (MaxSplitAmount < 1) return;
    SplitAmount = FMath::Clamp(SplitAmount, 1, MaxSplitAmount);

    FIntPoint Size = PendingSplitItem->GetCurrentSize();
    int32 Width = Size.X;
    int32 Height = Size.Y;

    // 1. 병합 가능한 타겟 아이템 확인
    if (InventoryComponent->IsValidSection(PendingSplitSectionIndex))
    {
        if (InventoryComponent->IsValidSectionIndex(PendingSplitSectionIndex, PendingSplitX + PendingSplitY * InventoryComponent->GetSectionSize(PendingSplitSectionIndex).X))
        {
            FName ExistingItem = InventoryComponent->GetCellItemID(PendingSplitSectionIndex, PendingSplitX, PendingSplitY);
            if (ExistingItem != NAME_None && ExistingItem != PendingSplitItem->InstanceID)
            {
                UItemInstance* TargetObj = InventoryComponent->GetItemInstance(ExistingItem);
                if (TargetObj && TargetObj->TemplateID == PendingSplitItem->TemplateID && TargetObj->IsStackable())
                {
                    int32 AvailableSpace = TargetObj->MaxStack - TargetObj->CurrentStack;
                    if (AvailableSpace > 0)
                    {
                        int32 AmountToMove = FMath::Min(AvailableSpace, SplitAmount);
                        TargetObj->CurrentStack += AmountToMove;
                        PendingSplitItem->CurrentStack -= AmountToMove;

                        PendingSplitSourceInv->OnInventoryChanged.Broadcast();
                        InventoryComponent->OnInventoryChanged.Broadcast();
                        
                        AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
                        if (GM && GM->LootContainerComponent) GM->LootContainerComponent->OnInventoryChanged.Broadcast();
                        return; // 완료
                    }
                }
            }
        }
    }

    // 2. 빈 공간에 새로 아이템 생성하여 이동
    if (InventoryComponent->CheckItemFitInSection(NAME_None, PendingSplitSectionIndex, PendingSplitX, PendingSplitY, Width, Height))
    {
        PendingSplitItem->CurrentStack -= SplitAmount;

        UItemInstance* NewItem = NewObject<UItemInstance>(InventoryComponent);
        NewItem->InstanceID = FName(*FGuid::NewGuid().ToString());
        NewItem->TemplateID = PendingSplitItem->TemplateID;
        NewItem->ItemName = PendingSplitItem->ItemName;
        NewItem->Category = PendingSplitItem->Category;
        NewItem->Rarity = PendingSplitItem->Rarity;
        NewItem->ItemIcon = PendingSplitItem->ItemIcon;
        NewItem->CachedDynamicIcon = PendingSplitItem->CachedDynamicIcon;
        NewItem->BaseSize = PendingSplitItem->BaseSize;
        NewItem->CurrentStack = SplitAmount;
        NewItem->MaxStack = PendingSplitItem->MaxStack;
        NewItem->bIsRotated = PendingSplitItem->bIsRotated;
        NewItem->bIsExamined = PendingSplitItem->bIsExamined;
        NewItem->AttachmentType = PendingSplitItem->AttachmentType;
        NewItem->CompatibleAmmo = PendingSplitItem->CompatibleAmmo;
        NewItem->CurrentAmmo = PendingSplitItem->CurrentAmmo;
        NewItem->MaxAmmo = PendingSplitItem->MaxAmmo;
        NewItem->Damage = PendingSplitItem->Damage;
        NewItem->Armor = PendingSplitItem->Armor;
        NewItem->EquippedSight = PendingSplitItem->EquippedSight;
        NewItem->EquippedMuzzle = PendingSplitItem->EquippedMuzzle;
        NewItem->EquippedMagazine = PendingSplitItem->EquippedMagazine;

        if (!InventoryComponent->AddItemToSection(NewItem, PendingSplitSectionIndex, PendingSplitX, PendingSplitY))
        {
            PendingSplitItem->CurrentStack += SplitAmount;
            return;
        }

        PendingSplitSourceInv->OnInventoryChanged.Broadcast();
        InventoryComponent->OnInventoryChanged.Broadcast();
        
        AGridGameMode* GM = Cast<AGridGameMode>(UGameplayStatics::GetGameMode(this));
        if (GM && GM->LootContainerComponent) GM->LootContainerComponent->OnInventoryChanged.Broadcast();
    }
}
