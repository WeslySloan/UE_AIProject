#include "GridInventoryComponent.h"

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
    GridWidth = Width;
    GridHeight = Height;
    
    int32 TotalCells = GridWidth * GridHeight;
    GridCells.Init(NAME_None, TotalCells);
    
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

bool UGridInventoryComponent::AddItem(FName ItemID, int32 StartX, int32 StartY, int32 ItemWidth, int32 ItemHeight, EItemRarity Rarity)
{
    if (!CheckItemFit(ItemID, StartX, StartY, ItemWidth, ItemHeight))
    {
        return false;
    }

    for (int32 X = StartX; X < StartX + ItemWidth; ++X)
    {
        for (int32 Y = StartY; Y < StartY + ItemHeight; ++Y)
        {
            int32 Index = GetIndex(X, Y);
            if (IsValidIndex(Index))
            {
                GridCells[Index] = ItemID;
            }
        }
    }

    // 아이템 희귀도 기억
    ItemRarityMap.Add(ItemID, Rarity);

    OnInventoryChanged.Broadcast();
    return true;
}

void UGridInventoryComponent::RemoveItem(FName ItemID)
{
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
        ItemRarityMap.Remove(ItemID);
        OnInventoryChanged.Broadcast();
    }
}

void UGridInventoryComponent::ClearInventory()
{
    for (int32 i = 0; i < GridCells.Num(); ++i)
    {
        GridCells[i] = NAME_None;
    }
    ItemRarityMap.Empty();
    OnInventoryChanged.Broadcast();
}
