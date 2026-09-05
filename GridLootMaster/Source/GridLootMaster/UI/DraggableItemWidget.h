#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../ItemData.h"
#include "DraggableItemWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemRightClicked, class UItemInstance*, ItemObj);

UCLASS()
class GRIDLOOTMASTER_API UDraggableItemWidget : public UUserWidget
{
    GENERATED_BODY()
    
public:
    virtual void NativeConstruct() override;

public:
    // 참조하는 아이템 인스턴스 (데이터 소스)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data", meta = (ExposeOnSpawn = "true"))
    class UItemInstance* ItemObj;

    UPROPERTY(BlueprintReadWrite, Category = "Inventory")
    class UGridInventoryComponent* SourceInventory;

    UPROPERTY(BlueprintReadWrite, Category = "Inventory")
    int32 SourceSectionIndex = 0;

    UPROPERTY(BlueprintAssignable, Category = "Item")
    FOnItemRightClicked OnRightClicked;

    UFUNCTION(BlueprintCallable, Category = "Item")
    void InitWidgetUI(bool bEquipped = false);

    void SetDragPreviewRotation(bool bInPreviewRotation);

    bool bIsEquippedVisual = false;

    // 프로그레스 바 포인터 (선택적)
    UPROPERTY()
    class UProgressBar* ExamineProgressBar;

    // 현재 진행 중인 드래그 오퍼레이션을 추적하여 회전 시 값 갱신
    UPROPERTY()
    class UItemDragDropOperation* CurrentDragOp;

    // 현재 드래그 중인 비주얼을 추적하여 회전 시 비주얼 갱신
    UPROPERTY()
    UDraggableItemWidget* CurrentDragVisual;

    bool bHasDragPreviewRotation = false;
    bool bDragPreviewRotation = false;


protected:
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    
    // 키보드 입력을 받을 수 있도록 포커스 가능 상태로 설정
    virtual bool NativeSupportsKeyboardFocus() const override { return true; }

    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

#if WITH_DEV_AUTOMATION_TESTS
public:
    FReply NativeOnKeyDownForTest(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
    {
        return NativeOnKeyDown(InGeometry, InKeyEvent);
    }
#endif

    UFUNCTION()
    void OnAutoSplitConfirmed(int32 SplitAmount);

    UFUNCTION()
    void HandleInspectItem(class UItemInstance* TargetItem);

    UFUNCTION()
    void HandleDiscardItem(class UItemInstance* TargetItem);

    UFUNCTION()
    void HandleUnloadItem(class UItemInstance* TargetItem);

    UFUNCTION()
    void HandleItemModified();

    // 회전 상태가 변경되었을 때 블루프린트에서 비주얼을 업데이트
    UFUNCTION(BlueprintImplementableEvent, Category = "Item UI")
    void OnItemRotated();
};
