#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "StashSaveGame.generated.h"

USTRUCT(BlueprintType)
struct GRIDLOOTMASTER_API FStashAttachedItemRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName InstanceID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName TemplateID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CurrentStack = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CurrentAmmo = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsRotated = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsExamined = true;
};

USTRUCT(BlueprintType)
struct GRIDLOOTMASTER_API FStashItemRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName InstanceID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName TemplateID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 GridX = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 GridY = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CurrentStack = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CurrentAmmo = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsRotated = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsExamined = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHasEquippedSight = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FStashAttachedItemRecord EquippedSight;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHasEquippedMuzzle = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FStashAttachedItemRecord EquippedMuzzle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHasEquippedMagazine = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FStashAttachedItemRecord EquippedMagazine;
};

UCLASS()
class GRIDLOOTMASTER_API UStashSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stash")
    int32 GridWidth = 10;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stash")
    int32 GridHeight = 10;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stash")
    TArray<FStashItemRecord> Items;
};
