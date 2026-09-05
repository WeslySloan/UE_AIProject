#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../ItemData.h"
#include "ModSlotWidget.generated.h"

UCLASS()
class GRIDLOOTMASTER_API UModSlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual bool Initialize() override;

    UFUNCTION(BlueprintCallable, Category = "Mod Slot")
    void Setup(class UItemInstance* InWeaponObj, EAttachmentType InAllowedType);

    UFUNCTION(BlueprintCallable, Category = "Mod Slot")
    void RefreshSlotUI();

protected:
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

    UPROPERTY()
    class UBorder* BackgroundBorder;

    UPROPERTY()
    class UTextBlock* SlotNameText;

    UPROPERTY()
    class UCanvasPanel* ItemCanvas;

    UPROPERTY()
    class UItemInstance* WeaponObj;

    UPROPERTY()
    EAttachmentType AllowedType;

    UPROPERTY()
    class UItemInstance* EquippedMod;

    UFUNCTION()
    void HandleModRightClicked(class UItemInstance* ModObj);

    UFUNCTION()
    void HandleUnequipClicked(class UItemInstance* ModObj);

    UFUNCTION()
    void HandleUnloadClicked(class UItemInstance* ModObj);
};
