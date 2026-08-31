#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ItemData.generated.h"

class UTexture2D;

/**
 * 게임 내 아이템의 기본 정보를 정의하는 데이터 구조체입니다.
 * UDataTable의 행(Row)으로 사용될 수 있습니다.
 */
USTRUCT(BlueprintType)
struct GRIDLOOTMASTER_API FItemData : public FTableRowBase
{
    GENERATED_BODY()

public:
    // 고유 아이템 ID
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    FName ItemID;

    // 표시될 아이템 이름
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    FString ItemName;

    // 아이템의 그리드 크기 (X=Width, Y=Height)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    FIntPoint Size;

    // 아이템을 팔았을 때 얻는 가치(점수)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    int32 Value;

    // 아이템 아이콘 (메모리 최적화를 위해 Soft Reference 사용)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    TSoftObjectPtr<UTexture2D> ItemIcon;

    FItemData()
        : ItemID(NAME_None)
        , ItemName(TEXT("Unknown"))
        , Size(FIntPoint(1, 1))
        , Value(0)
    {}
};
