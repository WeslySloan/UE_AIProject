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

    // 배치된 아이템들의 Rarity를 기억하기 위한 맵
    UPROPERTY()
    TMap<FName, EItemRarity> ItemRarityMap;

    // 해당 위치에 아이템을 배치할 수 있는지 확인합니다.
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool CheckItemFit(FName ItemID, int32 StartX, int32 StartY, int32 ItemWidth, int32 ItemHeight) const;

    // 아이템을 배치합니다. (성공 시 true 반환)
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool AddItem(FName ItemID, int32 StartX, int32 StartY, int32 ItemWidth, int32 ItemHeight, EItemRarity Rarity = EItemRarity::Common);

    // 아이템을 인벤토리에서 제거합니다.
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void RemoveItem(FName ItemID);

    // 인벤토리를 비웁니다.
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void ClearInventory();
};
