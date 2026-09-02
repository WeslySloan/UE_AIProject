#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SplitStackWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSplitConfirmed, int32, SplitAmount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSplitCancelled);

UCLASS()
class GRIDLOOTMASTER_API USplitStackWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual bool Initialize() override;

    UFUNCTION(BlueprintCallable, Category = "Split")
    void Setup(int32 MaxAmount);

    UPROPERTY(BlueprintAssignable, Category = "Split")
    FOnSplitConfirmed OnSplitConfirmed;

    UPROPERTY(BlueprintAssignable, Category = "Split")
    FOnSplitCancelled OnSplitCancelled;

protected:
    UPROPERTY()
    class USlider* AmountSlider;

    UPROPERTY()
    class UEditableTextBox* AmountTextBox;

    UPROPERTY()
    class UButton* OkButton;

    UPROPERTY()
    class UButton* CancelButton;

    UPROPERTY()
    class UTextBlock* TitleText;

    int32 MaxStackToSplit;
    int32 CurrentSplitAmount;

    UFUNCTION()
    void OnSliderValueChanged(float Value);

    UFUNCTION()
    void OnTextBoxTextChanged(const FText& Text);

    UFUNCTION()
    void OnOkClicked();

    UFUNCTION()
    void OnCancelClicked();
};
