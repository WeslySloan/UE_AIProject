#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SectionedStorageWidget.generated.h"

class UGridInventoryComponent;
class UVerticalBox;

UCLASS()
class GRIDLOOTMASTER_API USectionedStorageWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite, Category = "Inventory")
    UGridInventoryComponent* InventoryComponent = nullptr;

    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void BindInventory(UGridInventoryComponent* InInventory);

    UFUNCTION(BlueprintPure, Category = "Inventory")
    int32 GetRenderedSectionCount() const;

    UFUNCTION()
    void RebuildSections();

private:
    UPROPERTY()
    UVerticalBox* SectionBox = nullptr;

    UPROPERTY()
    TArray<class UGridBoardWidget*> SectionBoards;

    UPROPERTY()
    TArray<FIntPoint> RenderedSectionSizes;
};
