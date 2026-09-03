#include "EquipmentComponent.h"
#include "ItemInstance.h"

UEquipmentComponent::UEquipmentComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UEquipmentComponent::EquipItem(FName SlotID, UItemInstance* Item)
{
    if (!Item || SlotID == NAME_None || Item->InstanceID == NAME_None) return false;

    if (EquippedItems.Contains(SlotID))
    {
        return false; // 이미 장착됨
    }

    for (const TPair<FName, UItemInstance*>& Pair : EquippedItems)
    {
        if (Pair.Value && Pair.Value->InstanceID == Item->InstanceID)
        {
            return false; // 동일 인스턴스는 여러 슬롯에 장착할 수 없음
        }
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

bool UEquipmentComponent::RemoveItemByInstanceID(FName InstanceID)
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
    return bRemoved;
}

bool UEquipmentComponent::RemoveAttachedItem(UItemInstance* Attachment)
{
    if (!Attachment) return false;

    bool bRemoved = false;
    for (const TPair<FName, UItemInstance*>& Pair : EquippedItems)
    {
        UItemInstance* EquippedItem = Pair.Value;
        if (!EquippedItem || EquippedItem->Category != EItemCategory::Weapon) continue;

        bool bWeaponModified = false;
        if (EquippedItem->EquippedSight == Attachment)
        {
            EquippedItem->EquippedSight = nullptr;
            bWeaponModified = true;
        }
        if (EquippedItem->EquippedMuzzle == Attachment)
        {
            EquippedItem->EquippedMuzzle = nullptr;
            bWeaponModified = true;
        }
        if (EquippedItem->EquippedMagazine == Attachment)
        {
            EquippedItem->EquippedMagazine = nullptr;
            bWeaponModified = true;
        }

        if (bWeaponModified)
        {
            EquippedItem->OnItemModified.Broadcast();
            bRemoved = true;
        }
    }

    if (bRemoved)
    {
        OnEquipmentChanged.Broadcast();
    }
    return bRemoved;
}

bool UEquipmentComponent::RemoveItemBySlotID(FName SlotID)
{
    const bool bRemoved = EquippedItems.Remove(SlotID) > 0;
    if (bRemoved)
    {
        OnEquipmentChanged.Broadcast();
    }
    return bRemoved;
}

void UEquipmentComponent::ClearEquipment()
{
    EquippedItems.Empty();
    OnEquipmentChanged.Broadcast();
}
