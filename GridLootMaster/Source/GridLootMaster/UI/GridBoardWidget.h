#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GridBoardWidget.generated.h"

class UGridInventoryComponent;
class UCanvasPanel;

UCLASS()
class GRIDLOOTMASTER_API UGridBoardWidget : public UUserWidget
{
    GENERATED_BODY()
    
public:
    virtual bool Initialize() override;

    UPROPERTY(BlueprintReadWrite, Category = "Inventory")
    UGridInventoryComponent* InventoryComponent;

    UPROPERTY()
    class UItemInstance* PendingSplitItem;
    
    UPROPERTY()
    class UGridInventoryComponent* PendingSplitSourceInv;

    int32 PendingSplitX;
    int32 PendingSplitY;

    UFUNCTION()
    void OnSplitDragConfirmed(int32 SplitAmount);

    UPROPERTY()
    UCanvasPanel* GridCanvas;

    UPROPERTY()
    class UBorder* PreviewBorder;

    UPROPERTY()
    class UBorder* BackgroundBorder;

    UPROPERTY()
    class USizeBox* RootSizeBox;

protected:
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

#if WITH_DEV_AUTOMATION_TESTS
public:
    bool NativeOnDropForTest(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
    {
        return NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
    }
#endif
    
public:
    UFUNCTION()
    void RefreshGridUI();

    UFUNCTION()
    bool GetGridCellFromMousePosition(const FGeometry& Geometry, const FPointerEvent& PointerEvent, UItemDragDropOperation* Operation, int32& OutX, int32& OutY);
};
