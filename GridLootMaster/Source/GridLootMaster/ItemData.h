#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ItemData.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EItemCategory : uint8
{
    Weapon      UMETA(DisplayName = "Weapon"),
    Armor       UMETA(DisplayName = "Armor"),
    Helmet      UMETA(DisplayName = "Helmet"),
    Rig         UMETA(DisplayName = "Rig"),
    Backpack    UMETA(DisplayName = "Backpack"),
    SafeBox     UMETA(DisplayName = "Safe Box"),
    Valuable    UMETA(DisplayName = "Valuable"),
    Consumable  UMETA(DisplayName = "Consumable"),
    Attachment  UMETA(DisplayName = "Attachment")
};

UENUM(BlueprintType)
enum class EAttachmentType : uint8
{
    None        UMETA(DisplayName = "None"),
    Sight       UMETA(DisplayName = "Sight"),
    Muzzle      UMETA(DisplayName = "Muzzle"),
    Magazine    UMETA(DisplayName = "Magazine")
};

UENUM(BlueprintType)
enum class EItemRarity : uint8
{
    Common      UMETA(DisplayName = "Common (Gray)"),
    Uncommon    UMETA(DisplayName = "Uncommon (Green)"),
    Rare        UMETA(DisplayName = "Rare (Blue)"),
    Epic        UMETA(DisplayName = "Epic (Purple)"),
    Legendary   UMETA(DisplayName = "Legendary (Gold)"),
    Mythic      UMETA(DisplayName = "Mythic (Red)")
};

/**
 * 게임 내 아이템의 기본 정보를 정의하는 데이터 구조체입니다.
 * UDataTable의 행(Row)으로 사용될 수 있습니다.
 */
USTRUCT(BlueprintType)
struct GRIDLOOTMASTER_API FItemData : public FTableRowBase
{
    GENERATED_BODY()

public:
    // 고유 아이템 ID (템플릿 ID)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    FName ItemID;

    // 표시될 아이템 이름
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    FString ItemName;

    // 아이템의 카테고리
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    EItemCategory Category;

    // 아이템의 희귀도
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    EItemRarity Rarity;

    // 아이템의 그리드 크기 (X=Width, Y=Height)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    FIntPoint Size;

    // 겹치기 최대 수량
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    int32 MaxStack;

    // 부착물일 경우 어떤 부위인지
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    EAttachmentType AttachmentType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    FString CompatibleAmmo;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    int32 MaxAmmo;

    // 상점 기준가
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    int32 Value;

    // 랜덤 스폰 확률용 가중치
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    int32 DropWeight;

    // 기본 무게
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    float Weight;

    // 아이템 아이콘 (메모리 최적화를 위해 Soft Reference 사용)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
    TSoftObjectPtr<UTexture2D> ItemIcon;

    FItemData()
        : ItemID(NAME_None)
        , ItemName(TEXT("Unknown"))
        , Category(EItemCategory::Valuable)
        , Rarity(EItemRarity::Common)
        , Size(FIntPoint(1, 1))
        , MaxStack(1)
        , AttachmentType(EAttachmentType::None)
        , MaxAmmo(0)
        , Value(0)
        , DropWeight(100)
        , Weight(1.0f)
    {}
};
