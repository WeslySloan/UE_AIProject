#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../ItemData.h"
#include "DraggableItemWidget.generated.h"

UCLASS()
class GRIDLOOTMASTER_API UDraggableItemWidget : public UUserWidget
{
    GENERATED_BODY()
    
public:
    virtual void NativeConstruct() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data", meta = (ExposeOnSpawn = "true"))
    FName ItemID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data", meta = (ExposeOnSpawn = "true"))
    FIntPoint ItemSize;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data", meta = (ExposeOnSpawn = "true"))
    int32 Value;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data", meta = (ExposeOnSpawn = "true"))
    EItemRarity Rarity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data", meta = (ExposeOnSpawn = "true"))
    bool bIsExamined = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item State")
    bool bIsRotated = false;

    // 프로그레스 바 포인터 (선택적)
    UPROPERTY()
    class UProgressBar* ExamineProgressBar;

    // 현재 진행 중인 드래그 오퍼레이션을 추적하여 회전 시 값 갱신
    UPROPERTY()
    class UItemDragDropOperation* CurrentDragOp;

    // 현재 드래그 중인 비주얼을 추적하여 회전 시 비주얼 갱신
    UPROPERTY()
    UDraggableItemWidget* CurrentDragVisual;

    UFUNCTION()
    void InitWidgetUI();

protected:
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    
    // 키보드 입력을 받을 수 있도록 포커스 가능 상태로 설정
    virtual bool NativeSupportsKeyboardFocus() const override { return true; }

    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    // 회전 상태가 변경되었을 때 블루프린트에서 비주얼을 업데이트
    UFUNCTION(BlueprintImplementableEvent, Category = "Item UI")
    void OnItemRotated();
};
