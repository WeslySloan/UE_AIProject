#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemData.h"
#include "EquipmentComponent.generated.h"

class UItemInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEquipmentChanged);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GRIDLOOTMASTER_API UEquipmentComponent : public UActorComponent
{
    GENERATED_BODY()

public:    
    UEquipmentComponent();

    UPROPERTY(BlueprintAssignable, Category = "Equipment")
    FOnEquipmentChanged OnEquipmentChanged;

    UPROPERTY()
    TMap<FName, UItemInstance*> EquippedItems;

    UFUNCTION(BlueprintCallable, Category = "Equipment")
    bool EquipItem(FName SlotID, UItemInstance* Item);

    UFUNCTION(BlueprintCallable, Category = "Equipment")
    UItemInstance* GetEquippedItem(FName SlotID) const;

    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void RemoveItemByInstanceID(FName InstanceID);

    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void RemoveItemBySlotID(FName SlotID);

    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void ClearEquipment();
};
