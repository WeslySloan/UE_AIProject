#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drag Drop", meta = (ExposeOnSpawn = "true"))
    FIntPoint ItemSize;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drag Drop")
    bool bIsRotated = false;
    
    UPROPERTY()
    class UWidget* OriginalWidget;

    // 드래그 시작 시, 아이템의 왼쪽 상단(Top-Left)을 기준으로 한 마우스의 위치 오프셋
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drag Drop")
    FVector2D MouseOffset;
};
