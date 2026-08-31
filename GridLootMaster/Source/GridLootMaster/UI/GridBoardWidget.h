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
    UCanvasPanel* GridCanvas;

    UPROPERTY()
    class UBorder* PreviewBorder;

protected:
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    
public:
    UFUNCTION()
    void RefreshGridUI();

    UFUNCTION()
    bool GetGridCellFromMousePosition(const FGeometry& Geometry, const FPointerEvent& PointerEvent, UItemDragDropOperation* Operation, int32& OutX, int32& OutY);
};
