#include "GridInventoryComponent.h"
#include "ItemInstance.h"

UGridInventoryComponent::UGridInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    // Default grid size
    GridWidth = 6;
    GridHeight = 8;
}

void UGridInventoryComponent::BeginPlay()
{
    Super::BeginPlay();
    InitializeGrid(GridWidth, GridHeight);
}

void UGridInventoryComponent::InitializeGrid(int32 Width, int32 Height)
{
    GridWidth = FMath::Max(1, Width);
    GridHeight = FMath::Max(1, Height);
    
    int32 TotalCells = GridWidth * GridHeight;
    GridCells.Init(NAME_None, TotalCells);
    ItemInstances.Empty();
    
    OnInventoryChanged.Broadcast();
}

bool UGridInventoryComponent::IsValidIndex(int32 Index) const
{
    return GridCells.IsValidIndex(Index);
}

int32 UGridInventoryComponent::GetIndex(int32 X, int32 Y) const
{
    return X + (Y * GridWidth);
}

bool UGridInventoryComponent::CheckItemFit(FName ItemID, int32 StartX, int32 StartY, int32 ItemWidth, int32 ItemHeight) const
{
    if (ItemWidth <= 0 || ItemHeight <= 0)
    {
        return false;
    }

    // 그리드 경계를 벗어나는지 확인
    if (StartX < 0 || StartY < 0 || (StartX + ItemWidth) > GridWidth || (StartY + ItemHeight) > GridHeight)
    {
        return false;
    }

    // 다른 아이템과 겹치는지 확인
    for (int32 X = StartX; X < StartX + ItemWidth; ++X)
    {
        for (int32 Y = StartY; Y < StartY + ItemHeight; ++Y)
        {
            int32 Index = GetIndex(X, Y);
            if (!IsValidIndex(Index))
            {
                return false;
            }
            
            if (GridCells[Index] != NAME_None && GridCells[Index] != ItemID)
            {
                return false; // 이미 다른 아이템이 차지함
            }
        }
    }

    return true;
}

bool UGridInventoryComponent::FindEmptySpace(int32 ItemWidth, int32 ItemHeight, int32& OutX, int32& OutY) const
{
    return FindEmptySpaceExcluding(ItemWidth, ItemHeight, NAME_None, OutX, OutY);
}

bool UGridInventoryComponent::FindEmptySpaceExcluding(int32 ItemWidth, int32 ItemHeight, FName ExcludedItemID, int32& OutX, int32& OutY) const
{
    for (int32 Y = 0; Y <= GridHeight - ItemHeight; ++Y)
    {
        for (int32 X = 0; X <= GridWidth - ItemWidth; ++X)
        {
            if (CheckItemFit(ExcludedItemID, X, Y, ItemWidth, ItemHeight))
            {
                OutX = X;
                OutY = Y;
                return true;
            }
        }
    }
    return false;
}

bool UGridInventoryComponent::AddItem(UItemInstance* ItemObj, int32 StartX, int32 StartY)
{
    if (!ItemObj || ItemObj->InstanceID == NAME_None) return false;

    if (UItemInstance* const* ExistingItem = ItemInstances.Find(ItemObj->InstanceID))
    {
        if (*ExistingItem && *ExistingItem != ItemObj)
        {
            return false;
        }
    }

    FIntPoint Size = ItemObj->GetCurrentSize();
    if (!CheckItemFit(ItemObj->InstanceID, StartX, StartY, Size.X, Size.Y))
    {
        return false;
    }

    // 동일한 인스턴스를 다시 배치할 때 기존 점유 셀이 남지 않도록 정리합니다.
    if (ItemInstances.Contains(ItemObj->InstanceID))
    {
        for (FName& CellItemID : GridCells)
        {
            if (CellItemID == ItemObj->InstanceID)
            {
                CellItemID = NAME_None;
            }
        }
        ItemInstances.Remove(ItemObj->InstanceID);
    }

    for (int32 X = StartX; X < StartX + Size.X; ++X)
    {
        for (int32 Y = StartY; Y < StartY + Size.Y; ++Y)
        {
            int32 Index = GetIndex(X, Y);
            if (IsValidIndex(Index))
            {
                GridCells[Index] = ItemObj->InstanceID;
            }
        }
    }

    ItemInstances.Add(ItemObj->InstanceID, ItemObj);
    OnInventoryChanged.Broadcast();
    return true;
}

bool UGridInventoryComponent::RemoveItem(FName ItemID)
{
    if (ItemID == NAME_None) return false;

    bool bRemoved = false;
    for (int32 i = 0; i < GridCells.Num(); ++i)
    {
        if (GridCells[i] == ItemID)
        {
            GridCells[i] = NAME_None;
            bRemoved = true;
        }
    }

    if (bRemoved)
    {
        ItemInstances.Remove(ItemID);
        OnInventoryChanged.Broadcast();
    }

    return bRemoved;
}

void UGridInventoryComponent::ClearInventory()
{
    for (int32 i = 0; i < GridCells.Num(); ++i)
    {
        GridCells[i] = NAME_None;
    }
    ItemInstances.Empty();
    OnInventoryChanged.Broadcast();
}

UItemInstance* UGridInventoryComponent::GetItemInstance(FName ItemID) const
{
    if (UItemInstance* const* FoundItem = ItemInstances.Find(ItemID))
    {
        return *FoundItem;
    }
    return nullptr;
}

bool UGridInventoryComponent::TryMergeItem(UItemInstance* SourceItem, FName TargetItemID)
{
    if (!SourceItem) return false;

    UItemInstance* TargetItem = GetItemInstance(TargetItemID);
    if (!TargetItem || TargetItem == SourceItem || SourceItem->CurrentStack <= 0) return false;

    // 종류가 같고 스택 가능할 때만 병합
    if (SourceItem->TemplateID == TargetItem->TemplateID && TargetItem->IsStackable())
    {
        int32 AvailableSpace = TargetItem->MaxStack - TargetItem->CurrentStack;
        if (AvailableSpace > 0)
        {
            int32 AmountToMove = FMath::Min(AvailableSpace, SourceItem->CurrentStack);
            TargetItem->CurrentStack += AmountToMove;
            SourceItem->CurrentStack -= AmountToMove;

            OnInventoryChanged.Broadcast();
            
            // 일부라도 이동되었으면 병합 처리 완료로 반환합니다.
            return (AmountToMove > 0);
        }
    }
    return false;
}
