#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemData.h"
#include "GridInventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class GRIDLOOTMASTER_API UGridInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGridInventoryComponent();

    // 넓이(가로 칸 수)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 GridWidth;

    // 높이(세로 칸 수)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 GridHeight;

    // 아이템 배치 상태 (1차원 배열로 2D 그리드 관리)
    // 빈 공간은 NAME_None으로 처리
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    TArray<FName> GridCells;

    // 인벤토리가 변경되었을 때 호출되는 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Inventory|Events")
    FOnInventoryChanged OnInventoryChanged;

protected:
    virtual void BeginPlay() override;

public:
    // 그리드 크기를 초기화합니다.
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void InitializeGrid(int32 Width, int32 Height);

    // 특정 1D 인덱스가 유효한지 확인
    bool IsValidIndex(int32 Index) const;

    // 2D 좌표를 1D 인덱스로 변환
    int32 GetIndex(int32 X, int32 Y) const;

    // 배치된 아이템 인스턴스들을 관리하는 맵
    UPROPERTY()
    TMap<FName, class UItemInstance*> ItemInstances;

    // 해당 위치에 아이템을 배치할 수 있는지 확인합니다.
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool CheckItemFit(FName ItemID, int32 StartX, int32 StartY, int32 ItemWidth, int32 ItemHeight) const;

    // 인벤토리 내에서 아이템을 배치할 빈 공간을 찾습니다.
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool FindEmptySpace(int32 ItemWidth, int32 ItemHeight, int32& OutX, int32& OutY) const;

    // 아이템 인스턴스를 인벤토리에 추가합니다.
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool AddItem(class UItemInstance* ItemObj, int32 StartX, int32 StartY);

    // 스택 가능한 아이템을 겹치려고 시도합니다. (초과 시 false 혹은 남은 수량 처리 구현용)
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool TryMergeItem(class UItemInstance* SourceItem, FName TargetItemID);

    // 인스턴스를 반환합니다.
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    class UItemInstance* GetItemInstance(FName ItemID) const;

    // 아이템을 인벤토리에서 제거합니다.
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void RemoveItem(FName ItemID);

    // 인벤토리를 비웁니다.
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void ClearInventory();
};
