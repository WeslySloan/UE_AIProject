#include "GridInventoryComponent.h"
#include "ItemInstance.h"
#include "Misc/DefaultValueHelper.h"

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
    TArray<FIntPoint> SectionSizes;
    SectionSizes.Add(FIntPoint(FMath::Max(1, Width), FMath::Max(1, Height)));
    InitializeSections(SectionSizes);
}

bool UGridInventoryComponent::InitializeSections(const TArray<FIntPoint>& SectionSizes)
{
    if (SectionSizes.Num() == 0) return false;
    for (const FIntPoint& Size : SectionSizes)
    {
        if (Size.X < 1 || Size.Y < 1) return false;
    }

    GridWidth = SectionSizes[0].X;
    GridHeight = SectionSizes[0].Y;
    bStorageEnabled = true;
    GridCells.Init(NAME_None, GridWidth * GridHeight);
    AdditionalSections.Empty();
    for (int32 Index = 1; Index < SectionSizes.Num(); ++Index)
    {
        FGridInventorySection& Section = AdditionalSections.AddDefaulted_GetRef();
        Section.Width = SectionSizes[Index].X;
        Section.Height = SectionSizes[Index].Y;
        Section.GridCells.Init(NAME_None, Section.Width * Section.Height);
    }
    ItemInstances.Empty();
    OnInventoryChanged.Broadcast();
    return true;
}

int32 UGridInventoryComponent::GetSectionCount() const { return bStorageEnabled ? 1 + AdditionalSections.Num() : 0; }

FIntPoint UGridInventoryComponent::GetSectionSize(int32 SectionIndex) const
{
    if (!bStorageEnabled) return FIntPoint::ZeroValue;
    if (SectionIndex == 0) return FIntPoint(GridWidth, GridHeight);
    if (!AdditionalSections.IsValidIndex(SectionIndex - 1)) return FIntPoint::ZeroValue;
    const FGridInventorySection& Section = AdditionalSections[SectionIndex - 1];
    return FIntPoint(Section.Width, Section.Height);
}

bool UGridInventoryComponent::IsValidSection(int32 SectionIndex) const
{
    return SectionIndex >= 0 && SectionIndex < GetSectionCount();
}

bool UGridInventoryComponent::IsValidSectionIndex(int32 SectionIndex, int32 CellIndex) const
{
    if (SectionIndex == 0) return GridCells.IsValidIndex(CellIndex);
    return AdditionalSections.IsValidIndex(SectionIndex - 1) && AdditionalSections[SectionIndex - 1].GridCells.IsValidIndex(CellIndex);
}

FName UGridInventoryComponent::GetCellItemID(int32 SectionIndex, int32 X, int32 Y) const
{
    if (!IsValidSection(SectionIndex)) return NAME_None;
    const FIntPoint Size = GetSectionSize(SectionIndex);
    if (X < 0 || Y < 0 || X >= Size.X || Y >= Size.Y) return NAME_None;
    const int32 Index = X + Y * Size.X;
    if (SectionIndex == 0) return GridCells.IsValidIndex(Index) ? GridCells[Index] : NAME_None;
    const TArray<FName>& Cells = AdditionalSections[SectionIndex - 1].GridCells;
    return Cells.IsValidIndex(Index) ? Cells[Index] : NAME_None;
}

bool UGridInventoryComponent::CheckItemFitInSection(FName ItemID, int32 SectionIndex, int32 StartX, int32 StartY, int32 ItemWidth, int32 ItemHeight) const
{
    if (!IsValidSection(SectionIndex) || ItemWidth <= 0 || ItemHeight <= 0) return false;
    const FIntPoint Size = GetSectionSize(SectionIndex);
    if (StartX < 0 || StartY < 0 || StartX + ItemWidth > Size.X || StartY + ItemHeight > Size.Y) return false;
    for (int32 Y = StartY; Y < StartY + ItemHeight; ++Y)
    {
        for (int32 X = StartX; X < StartX + ItemWidth; ++X)
        {
            if (GetCellItemID(SectionIndex, X, Y) != NAME_None && GetCellItemID(SectionIndex, X, Y) != ItemID) return false;
        }
    }
    return true;
}

bool UGridInventoryComponent::FindEmptySpaceInSection(int32 SectionIndex, int32 ItemWidth, int32 ItemHeight, int32& OutX, int32& OutY) const
{
    const FIntPoint Size = GetSectionSize(SectionIndex);
    for (int32 Y = 0; Y <= Size.Y - ItemHeight; ++Y)
        for (int32 X = 0; X <= Size.X - ItemWidth; ++X)
            if (CheckItemFitInSection(NAME_None, SectionIndex, X, Y, ItemWidth, ItemHeight)) { OutX = X; OutY = Y; return true; }
    return false;
}

bool UGridInventoryComponent::FindEmptySpaceAcrossSections(int32 ItemWidth, int32 ItemHeight, int32& OutSectionIndex, int32& OutX, int32& OutY) const
{
    for (int32 SectionIndex = 0; SectionIndex < GetSectionCount(); ++SectionIndex)
        if (FindEmptySpaceInSection(SectionIndex, ItemWidth, ItemHeight, OutX, OutY)) { OutSectionIndex = SectionIndex; return true; }
    return false;
}

bool UGridInventoryComponent::FindEmptySpaceAcrossSectionsExcluding(int32 ItemWidth, int32 ItemHeight, FName ExcludedItemID, int32& OutSectionIndex, int32& OutX, int32& OutY) const
{
    for (int32 SectionIndex = 0; SectionIndex < GetSectionCount(); ++SectionIndex)
    {
        const FIntPoint Size = GetSectionSize(SectionIndex);
        for (int32 Y = 0; Y <= Size.Y - ItemHeight; ++Y)
            for (int32 X = 0; X <= Size.X - ItemWidth; ++X)
                if (CheckItemFitInSection(ExcludedItemID, SectionIndex, X, Y, ItemWidth, ItemHeight))
                {
                    OutSectionIndex = SectionIndex;
                    OutX = X;
                    OutY = Y;
                    return true;
                }
    }
    return false;
}

bool UGridInventoryComponent::FindEmptySpaceAcrossSectionsExcludingPlacement(int32 ItemWidth, int32 ItemHeight, FName ExcludedItemID,
    int32 ReservedSectionIndex, int32 ReservedX, int32 ReservedY, int32 ReservedWidth, int32 ReservedHeight,
    int32& OutSectionIndex, int32& OutX, int32& OutY) const
{
    for (int32 SectionIndex = 0; SectionIndex < GetSectionCount(); ++SectionIndex)
    {
        const FIntPoint Size = GetSectionSize(SectionIndex);
        for (int32 Y = 0; Y <= Size.Y - ItemHeight; ++Y)
            for (int32 X = 0; X <= Size.X - ItemWidth; ++X)
            {
                const bool bOverlapsReserved = SectionIndex == ReservedSectionIndex &&
                    X < ReservedX + ReservedWidth && X + ItemWidth > ReservedX &&
                    Y < ReservedY + ReservedHeight && Y + ItemHeight > ReservedY;
                if (!bOverlapsReserved && CheckItemFitInSection(ExcludedItemID, SectionIndex, X, Y, ItemWidth, ItemHeight))
                {
                    OutSectionIndex = SectionIndex;
                    OutX = X;
                    OutY = Y;
                    return true;
                }
            }
    }
    return false;
}

bool UGridInventoryComponent::FindItemPlacement(FName ItemID, int32& OutSectionIndex, int32& OutX, int32& OutY) const
{
    if (ItemID == NAME_None) return false;
    for (int32 SectionIndex = 0; SectionIndex < GetSectionCount(); ++SectionIndex)
    {
        const FIntPoint Size = GetSectionSize(SectionIndex);
        for (int32 Y = 0; Y < Size.Y; ++Y)
            for (int32 X = 0; X < Size.X; ++X)
                if (GetCellItemID(SectionIndex, X, Y) == ItemID) { OutSectionIndex = SectionIndex; OutX = X; OutY = Y; return true; }
    }
    return false;
}

bool UGridInventoryComponent::ParseStorageLayoutSpec(const FString& LayoutSpec, TArray<FIntPoint>& OutSectionSizes)
{
    OutSectionSizes.Empty();
    FString CompactLayout = LayoutSpec;
    CompactLayout.ReplaceInline(TEXT(" "), TEXT(""));
    CompactLayout.ReplaceInline(TEXT("\t"), TEXT(""));
    TArray<FString> Entries;
    CompactLayout.ParseIntoArray(Entries, TEXT(";"), true);
    if (Entries.Num() == 0) return false;
    for (FString Entry : Entries)
    {
        Entry.TrimStartAndEndInline();
        FString CountString;
        int32 Count = 1;
        FString EntryWithoutCount;
        if (Entry.Split(TEXT("*"), &EntryWithoutCount, &CountString))
        {
            Entry = EntryWithoutCount;
            CountString.TrimStartAndEndInline();
            if (!FDefaultValueHelper::ParseInt(CountString, Count) || Count < 1) return false;
        }
        FString WidthString, HeightString;
        if (!Entry.Split(TEXT("x"), &WidthString, &HeightString))
        {
            if (!Entry.Split(TEXT("X"), &WidthString, &HeightString)) return false;
        }
        WidthString.TrimStartAndEndInline();
        HeightString.TrimStartAndEndInline();
        int32 Width = 0, Height = 0;
        if (!FDefaultValueHelper::ParseInt(WidthString, Width) || !FDefaultValueHelper::ParseInt(HeightString, Height) || Width < 1 || Height < 1) return false;
        for (int32 Index = 0; Index < Count; ++Index) OutSectionSizes.Add(FIntPoint(Width, Height));
    }
    return OutSectionSizes.Num() > 0;
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
    return CheckItemFitInSection(ItemID, 0, StartX, StartY, ItemWidth, ItemHeight);
}

bool UGridInventoryComponent::FindEmptySpace(int32 ItemWidth, int32 ItemHeight, int32& OutX, int32& OutY) const
{
    return FindEmptySpaceExcluding(ItemWidth, ItemHeight, NAME_None, OutX, OutY);
}

bool UGridInventoryComponent::FindEmptySpaceExcluding(int32 ItemWidth, int32 ItemHeight, FName ExcludedItemID, int32& OutX, int32& OutY) const
{
    const FIntPoint Size = GetSectionSize(0);
    for (int32 Y = 0; Y <= Size.Y - ItemHeight; ++Y)
        for (int32 X = 0; X <= Size.X - ItemWidth; ++X)
            if (CheckItemFitInSection(ExcludedItemID, 0, X, Y, ItemWidth, ItemHeight)) { OutX = X; OutY = Y; return true; }
    return false;
}

bool UGridInventoryComponent::AddItem(UItemInstance* ItemObj, int32 StartX, int32 StartY)
{
    return AddItemToSection(ItemObj, 0, StartX, StartY);
}

bool UGridInventoryComponent::AddItemToSection(UItemInstance* ItemObj, int32 SectionIndex, int32 StartX, int32 StartY)
{
    if (!bStorageEnabled || !ItemObj || ItemObj->InstanceID == NAME_None) return false;

    if (UItemInstance* const* ExistingItem = ItemInstances.Find(ItemObj->InstanceID))
    {
        if (*ExistingItem && *ExistingItem != ItemObj)
        {
            return false;
        }
    }

    FIntPoint Size = ItemObj->GetCurrentSize();
    if (!CheckItemFitInSection(ItemObj->InstanceID, SectionIndex, StartX, StartY, Size.X, Size.Y))
    {
        return false;
    }

    // 동일한 인스턴스를 다시 배치할 때 기존 점유 셀이 남지 않도록 전체 섹션을 정리합니다.
    if (ItemInstances.Contains(ItemObj->InstanceID))
    {
        for (FName& CellItemID : GridCells) if (CellItemID == ItemObj->InstanceID) CellItemID = NAME_None;
        for (FGridInventorySection& Section : AdditionalSections)
            for (FName& CellItemID : Section.GridCells) if (CellItemID == ItemObj->InstanceID) CellItemID = NAME_None;
        ItemInstances.Remove(ItemObj->InstanceID);
    }

    TArray<FName>* Cells = SectionIndex == 0 ? &GridCells : &AdditionalSections[SectionIndex - 1].GridCells;
    const FIntPoint SectionSize = GetSectionSize(SectionIndex);
    for (int32 X = StartX; X < StartX + Size.X; ++X)
    {
        for (int32 Y = StartY; Y < StartY + Size.Y; ++Y)
        {
            const int32 Index = X + Y * SectionSize.X;
            if (Cells->IsValidIndex(Index)) (*Cells)[Index] = ItemObj->InstanceID;
        }
    }

    ItemInstances.Add(ItemObj->InstanceID, ItemObj);
    OnInventoryChanged.Broadcast();
    return true;
}

bool UGridInventoryComponent::ReconfigureSections(const TArray<FIntPoint>& SectionSizes)
{
    if (SectionSizes.Num() == 0) return false;
    for (const FIntPoint& Size : SectionSizes) if (Size.X < 1 || Size.Y < 1) return false;

    struct FSortableItem { UItemInstance* Item; int32 Area; int32 MaxSide; };
    TArray<FSortableItem> Items;
    for (const TPair<FName, UItemInstance*>& Pair : ItemInstances)
    {
        if (!Pair.Value || Pair.Key != Pair.Value->InstanceID) return false;
        const FIntPoint Size = Pair.Value->GetCurrentSize();
        Items.Add({Pair.Value, Size.X * Size.Y, FMath::Max(Size.X, Size.Y)});
    }
    Items.Sort([](const FSortableItem& A, const FSortableItem& B)
    {
        if (A.Area != B.Area) return A.Area > B.Area;
        if (A.MaxSide != B.MaxSide) return A.MaxSide > B.MaxSide;
        return A.Item->InstanceID.ToString() < B.Item->InstanceID.ToString();
    });

    TArray<TArray<FName>> PlannedCells;
    for (const FIntPoint& Size : SectionSizes) { TArray<FName>& Cells = PlannedCells.AddDefaulted_GetRef(); Cells.Init(NAME_None, Size.X * Size.Y); }
    for (const FSortableItem& Entry : Items)
    {
        const FIntPoint ItemSize = Entry.Item->GetCurrentSize();
        bool bPlaced = false;
        for (int32 SectionIndex = 0; SectionIndex < SectionSizes.Num() && !bPlaced; ++SectionIndex)
        {
            const FIntPoint SectionSize = SectionSizes[SectionIndex];
            for (int32 Y = 0; Y <= SectionSize.Y - ItemSize.Y && !bPlaced; ++Y)
                for (int32 X = 0; X <= SectionSize.X - ItemSize.X && !bPlaced; ++X)
                {
                    bool bFits = true;
                    for (int32 ItemY = Y; ItemY < Y + ItemSize.Y && bFits; ++ItemY)
                        for (int32 ItemX = X; ItemX < X + ItemSize.X; ++ItemX)
                            if (PlannedCells[SectionIndex][ItemX + ItemY * SectionSize.X] != NAME_None) { bFits = false; break; }
                    if (bFits)
                    {
                        for (int32 ItemY = Y; ItemY < Y + ItemSize.Y; ++ItemY)
                            for (int32 ItemX = X; ItemX < X + ItemSize.X; ++ItemX)
                                PlannedCells[SectionIndex][ItemX + ItemY * SectionSize.X] = Entry.Item->InstanceID;
                        bPlaced = true;
                    }
                }
        }
        if (!bPlaced) return false;
    }

    GridWidth = SectionSizes[0].X;
    GridHeight = SectionSizes[0].Y;
    bStorageEnabled = true;
    GridCells = MoveTemp(PlannedCells[0]);
    AdditionalSections.Empty();
    for (int32 Index = 1; Index < SectionSizes.Num(); ++Index)
    {
        FGridInventorySection& Section = AdditionalSections.AddDefaulted_GetRef();
        Section.Width = SectionSizes[Index].X;
        Section.Height = SectionSizes[Index].Y;
        Section.GridCells = MoveTemp(PlannedCells[Index]);
    }
    OnInventoryChanged.Broadcast();
    return true;
}

bool UGridInventoryComponent::DisableStorage()
{
    if (ItemInstances.Num() > 0) return false;

    bStorageEnabled = false;
    GridCells.Empty();
    AdditionalSections.Empty();
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

    for (FGridInventorySection& Section : AdditionalSections)
        for (FName& CellItemID : Section.GridCells)
            if (CellItemID == ItemID) { CellItemID = NAME_None; bRemoved = true; }

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
    for (FGridInventorySection& Section : AdditionalSections)
        for (FName& CellItemID : Section.GridCells) CellItemID = NAME_None;
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
