#include "ItemInstance.h"
#include "ItemData.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Engine/Texture2D.h"

UItemInstance::UItemInstance()
{
    InstanceID = NAME_None;
    TemplateID = NAME_None;
    Rarity = EItemRarity::Common;
    Category = EItemCategory::Valuable;
    BaseSize = FIntPoint(1, 1);
    bIsRotated = false;
    bIsExamined = true;
    CurrentStack = 1;
    MaxStack = 1;
    CurrentAmmo = 0;
    MaxAmmo = 0;
    EquippedSight = nullptr;
    EquippedMuzzle = nullptr;
    EquippedMagazine = nullptr;
}

UTexture2D* UItemInstance::GetDynamicIcon() const
{
    FString IconFileName = TemplateID.ToString() + TEXT(".png");
    FString FilePath = FPaths::ProjectDir() + TEXT("RawAssets/Icons/") + IconFileName;

    if (FPaths::FileExists(FilePath))
    {
        TArray<uint8> RawFileData;
        if (FFileHelper::LoadFileToArray(RawFileData, *FilePath))
        {
            UTexture2D* LoadedTexture = FImageUtils::ImportBufferAsTexture2D(RawFileData);
            if (LoadedTexture)
            {
                // UI에서 사용하기 위해 설정
                LoadedTexture->CompressionSettings = TC_EditorIcon;
                LoadedTexture->SRGB = true;
                LoadedTexture->UpdateResource();
                return LoadedTexture;
            }
        }
    }
    return nullptr;
}

void UItemInstance::InitFromData(const FItemData& InData)
{
    TemplateID = InData.ItemID;
    ItemName = InData.ItemName;
    Category = InData.Category;
    Rarity = InData.Rarity;
    BaseSize = InData.Size;
    MaxStack = InData.MaxStack;
    AttachmentType = InData.AttachmentType;
    CompatibleAmmo = InData.CompatibleAmmo;
    MaxAmmo = InData.MaxAmmo;
    
    // 기본적으로 풀 스택/풀 장탄수 (테스트용)
    CurrentStack = 1;
    if (MaxStack > 1 && Category == EItemCategory::Consumable)
    {
        CurrentStack = FMath::RandRange(FMath::Max(1, MaxStack / 2), MaxStack);
    }
    
    if (MaxAmmo > 0)
    {
        CurrentAmmo = MaxAmmo;
    }
}
