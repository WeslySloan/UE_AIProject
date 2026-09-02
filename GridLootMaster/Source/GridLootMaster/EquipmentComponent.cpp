#include "EquipmentComponent.h"
#include "ItemInstance.h"

UEquipmentComponent::UEquipmentComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UEquipmentComponent::EquipItem(FName SlotID, UItemInstance* Item)
{
    if (!Item) return false;

    if (EquippedItems.Contains(SlotID))
    {
        return false; // 이미 장착됨
    }

    EquippedItems.Add(SlotID, Item);
    OnEquipmentChanged.Broadcast();
    return true;
}

UItemInstance* UEquipmentComponent::GetEquippedItem(FName SlotID) const
{
    if (UItemInstance* const* FoundItem = EquippedItems.Find(SlotID))
    {
        return *FoundItem;
    }
    return nullptr;
}

void UEquipmentComponent::RemoveItemByInstanceID(FName InstanceID)
{
    bool bRemoved = false;
    for (auto It = EquippedItems.CreateIterator(); It; ++It)
    {
        if (It.Value() && It.Value()->InstanceID == InstanceID)
        {
            It.RemoveCurrent();
            bRemoved = true;
            break;
        }
    }

    if (bRemoved)
    {
        OnEquipmentChanged.Broadcast();
    }
}

void UEquipmentComponent::RemoveItemBySlotID(FName SlotID)
{
    if (EquippedItems.Remove(SlotID) > 0)
    {
        OnEquipmentChanged.Broadcast();
    }
}

void UEquipmentComponent::ClearEquipment()
{
    EquippedItems.Empty();
    OnEquipmentChanged.Broadcast();
}
