#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ItemData.h" 
#include "ItemInstance.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnItemModified);

UCLASS(BlueprintType)
class GRIDLOOTMASTER_API UItemInstance : public UObject
{
    GENERATED_BODY()

public:
    UItemInstance();

    UPROPERTY(BlueprintReadWrite, Category = "Item Data")
    FName InstanceID;

    UPROPERTY(BlueprintReadWrite, Category = "Item Instance")
    FName TemplateID;

    // 아이템의 실제 표시 이름
    UPROPERTY(BlueprintReadWrite, Category = "Item Instance")
    FString ItemName;

    UPROPERTY(BlueprintReadWrite, Category = "Item Data")
    EItemRarity Rarity;

    UPROPERTY(BlueprintReadWrite, Category = "Item Data")
    EItemCategory Category;

    UPROPERTY(BlueprintReadWrite, Category = "Item Data")
    FIntPoint BaseSize;

    UPROPERTY(BlueprintReadWrite, Category = "Item Data")
    bool bIsRotated;

    UPROPERTY(BlueprintReadWrite, Category = "Item Data")
    bool bIsExamined;

    UPROPERTY(BlueprintReadWrite, Category = "Item Data")
    int32 CurrentStack;

    UPROPERTY(BlueprintReadWrite, Category = "Item Data")
    int32 MaxStack;

    // 카테고리가 Attachment일 경우의 타입
    UPROPERTY(BlueprintReadWrite, Category = "Item Data")
    EAttachmentType AttachmentType;

    // 카테고리가 Weapon일 경우 장착된 조준경
    UPROPERTY(BlueprintReadWrite, Category = "Modding")
    UItemInstance* EquippedSight;

    // 카테고리가 Weapon일 경우 장착된 소음기/총구
    UPROPERTY(BlueprintReadWrite, Category = "Item Instance")
    UItemInstance* EquippedMuzzle;

    // 장착된 탄창 (무기일 경우)
    UPROPERTY(BlueprintReadWrite, Category = "Item Instance")
    UItemInstance* EquippedMagazine;

    UPROPERTY(BlueprintReadWrite, Category = "Item Instance")
    FString CompatibleAmmo;

    UPROPERTY(BlueprintReadWrite, Category = "Item Instance")
    int32 CurrentAmmo;

    UPROPERTY(BlueprintReadWrite, Category = "Item Instance")
    int32 MaxAmmo;

    // 아이템 생성 시 데이터 테이블 행을 바탕으로 초기화하는 함수
    UFUNCTION(BlueprintCallable, Category = "Item Instance")
    void InitFromData(const struct FItemData& InData);

    UFUNCTION(BlueprintCallable, Category = "Icon")
    class UTexture2D* GetDynamicIcon() const;

    int32 GetWidth(bool bIgnoreRotation = false) const
    {
        return bIgnoreRotation ? BaseSize.X : (bIsRotated ? BaseSize.Y : BaseSize.X);
    }

    int32 GetHeight(bool bIgnoreRotation = false) const
    {
        return bIgnoreRotation ? BaseSize.Y : (bIsRotated ? BaseSize.X : BaseSize.Y);
    }

    FIntPoint GetCurrentSize() const
    {
        if (bIsRotated)
        {
            return FIntPoint(BaseSize.Y, BaseSize.X);
        }
        return BaseSize;
    }

    UPROPERTY(BlueprintAssignable, Category = "Item")
    FOnItemModified OnItemModified;

    UFUNCTION(BlueprintCallable, Category = "Item Data")
    bool IsStackable() const
    {
        return MaxStack > 1;
    }
};
