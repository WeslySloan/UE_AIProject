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
    ItemIcon = nullptr;
    CachedDynamicIcon = nullptr;
    BaseSize = FIntPoint(1, 1);
    bIsRotated = false;
    bIsExamined = true;
    CurrentStack = 1;
    MaxStack = 1;
    CurrentAmmo = 0;
    MaxAmmo = 0;
    Damage = 0;
    Armor = 0;
    WeaponAttackType = EWeaponAttackType::Firearm;
    BaseAccuracyPercent = 100;
    AttackIntervalSeconds = 1.0f;
    OptimalRangeTiles = 1;
    MaxRangeTiles = 3;
    RecoilPerShot = 0.0f;
    RecoilRecoveryPerSecond = 0.0f;
    SwapTimeSeconds = 0.0f;
    ReloadTimeSeconds = 0.0f;
    NoiseRadiusTiles = 0;
    EquippedSight = nullptr;
    EquippedMuzzle = nullptr;
    EquippedMagazine = nullptr;
}

UTexture2D* UItemInstance::GetDynamicIcon()
{
    if (!ItemIcon.IsNull())
    {
        if (UTexture2D* DataTableIcon = ItemIcon.LoadSynchronous())
        {
            return DataTableIcon;
        }
    }

    if (CachedDynamicIcon)
    {
        return CachedDynamicIcon;
    }

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
                CachedDynamicIcon = LoadedTexture;
                return CachedDynamicIcon;
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
    ItemIcon = InData.ItemIcon;
    CachedDynamicIcon = nullptr;
    Rarity = InData.Rarity;
    BaseSize = FIntPoint(FMath::Max(1, InData.Size.X), FMath::Max(1, InData.Size.Y));
    bIsRotated = false;
    bIsExamined = true;
    MaxStack = FMath::Max(1, InData.MaxStack);
    AttachmentType = InData.AttachmentType;
    CompatibleAmmo = InData.CompatibleAmmo;
    MaxAmmo = FMath::Max(0, InData.MaxAmmo);
    Damage = InData.Damage;
    Armor = InData.Armor;
    WeaponAttackType = InData.WeaponAttackType;
    BaseAccuracyPercent = FMath::Clamp(InData.BaseAccuracyPercent, 0, 100);
    if (BaseAccuracyPercent == 0) BaseAccuracyPercent = 100;
    AttackIntervalSeconds = InData.AttackIntervalSeconds > 0.0f ? InData.AttackIntervalSeconds : 1.0f;
    OptimalRangeTiles = FMath::Max(1, InData.OptimalRangeTiles);
    MaxRangeTiles = InData.MaxRangeTiles > 0
        ? FMath::Max(OptimalRangeTiles, InData.MaxRangeTiles)
        : FMath::Max(OptimalRangeTiles, 3);
    RecoilPerShot = FMath::Max(0.0f, InData.RecoilPerShot);
    RecoilRecoveryPerSecond = FMath::Max(0.0f, InData.RecoilRecoveryPerSecond);
    SwapTimeSeconds = FMath::Max(0.0f, InData.SwapTimeSeconds);
    ReloadTimeSeconds = FMath::Max(0.0f, InData.ReloadTimeSeconds);
    NoiseRadiusTiles = FMath::Max(0, InData.NoiseRadiusTiles);
    EquippedSight = nullptr;
    EquippedMuzzle = nullptr;
    EquippedMagazine = nullptr;
    
    // 기본적으로 풀 스택/풀 장탄수 (테스트용)
    CurrentStack = 1;
    if (MaxStack > 1 && Category == EItemCategory::Consumable)
    {
        CurrentStack = FMath::RandRange(FMath::Max(1, MaxStack / 2), MaxStack);
    }
    
    CurrentAmmo = 0;
    if (MaxAmmo > 0)
    {
        CurrentAmmo = MaxAmmo;
    }
}
