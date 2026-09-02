#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../ItemData.h"
#include "EquipmentSlotWidget.generated.h"

class UItemInstance;
class UBorder;
class UTextBlock;
class UCanvasPanel;

UCLASS()
class GRIDLOOTMASTER_API UEquipmentSlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual bool Initialize() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
    FName SlotID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
    EItemCategory AllowedCategory;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
    FString SlotName;

    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void InitSlot(FName InSlotID, EItemCategory Category, const FString& InSlotName);

    UPROPERTY(BlueprintReadOnly, Category = "Equipment")
    UItemInstance* EquippedItem;

    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void SetEquippedItem(UItemInstance* NewItem);

    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void RefreshSlotUI();

    UFUNCTION()
    void OnEquipmentChanged();

    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void SetHighlight(bool bActive);

    UFUNCTION()
    void HandleRightClicked(class UItemInstance* ItemObj);

    UFUNCTION()
    void HandleUnequipClicked(class UItemInstance* ItemObj);
    
    UFUNCTION()
    void HandleUnloadClicked(class UItemInstance* ItemObj);
    
    UFUNCTION()
    void HandleInspectItem(class UItemInstance* TargetItem);

protected:
    UPROPERTY()
    UBorder* BackgroundBorder;

    UPROPERTY()
    UTextBlock* SlotNameText;

    UPROPERTY()
    UCanvasPanel* ItemCanvas;

    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
};
