#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InspectWidget.generated.h"

UCLASS()
class GRIDLOOTMASTER_API UInspectWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual bool Initialize() override;

    UFUNCTION(BlueprintCallable, Category = "Inspect")
    void Setup(class UItemInstance* InItemObj);

protected:
    UPROPERTY()
    class UItemInstance* TargetItem;

    UPROPERTY()
    class UVerticalBox* MainBox;

    UPROPERTY()
    class UTextBlock* TitleText;

    UPROPERTY()
    class UTextBlock* DescText;

    UPROPERTY()
    class UHorizontalBox* ModBox;

    UPROPERTY()
    class UModSlotWidget* SightSlot;

    UPROPERTY()
    class UModSlotWidget* MuzzleSlot;

    UPROPERTY()
    class UModSlotWidget* MagazineSlot;

    UFUNCTION()
    void OnCloseClicked();
};
