#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "../ItemData.h"
#include "ItemDragDropOperation.generated.h"

UCLASS()
class GRIDLOOTMASTER_API UItemDragDropOperation : public UDragDropOperation
{
    GENERATED_BODY()
    
public:
    // 드래그 중인 아이템의 ID
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drag Drop", meta = (ExposeOnSpawn = "true"))
    FName ItemID;

    // 드래그 중인 아이템의 원래 크기
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drag and Drop")
    class UItemInstance* ItemObj;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drag and Drop")
    FVector2D MouseOffset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drag and Drop")
    class UDraggableItemWidget* OriginalWidget;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drag and Drop")
    bool bIsSplitDrag = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drag and Drop")
    class UGridInventoryComponent* SourceInventory = nullptr;
};
