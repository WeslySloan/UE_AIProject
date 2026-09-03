#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ContextMenuWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInspectClicked, class UItemInstance*, ItemObj);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDiscardClicked, class UItemInstance*, ItemObj);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMenuClosed);

UCLASS()
class GRIDLOOTMASTER_API UContextMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual bool Initialize() override;

    UFUNCTION(BlueprintCallable, Category = "Context Menu")
    void Setup(class UItemInstance* InItemObj, FVector2D ScreenPos, bool bCanUnequip = false);

    UPROPERTY(BlueprintAssignable, Category = "Context Menu")
    FOnInspectClicked OnInspectClicked;

    UPROPERTY(BlueprintAssignable, Category = "Context Menu")
    FOnDiscardClicked OnDiscardClicked;
    
    // 추가: 장착 해제용
    UPROPERTY(BlueprintAssignable, Category = "Context Menu")
    FOnDiscardClicked OnUnequipClicked;

    // 추가: 총알 빼기용
    UPROPERTY(BlueprintAssignable, Category = "Context Menu")
    FOnDiscardClicked OnUnloadClicked;

    UPROPERTY(BlueprintAssignable, Category = "Context Menu")
    FOnMenuClosed OnMenuClosed;

protected:
    UPROPERTY()
    class UVerticalBox* MenuContainer;

    UPROPERTY()
    class UButton* InspectButton;

    UPROPERTY()
    class UButton* DiscardButton;
    
    UPROPERTY()
    class UButton* UnequipButton;

    UPROPERTY()
    class UButton* UnloadButton;

    UPROPERTY()
    class UButton* BackgroundButton; // To detect clicks outside

    UPROPERTY()
    class UItemInstance* TargetItem;

    UFUNCTION()
    void HandleInspectClicked();

    UFUNCTION()
    void HandleDiscardClicked();
    
    UFUNCTION()
    void HandleUnequipClicked();

    UFUNCTION()
    void HandleUnloadClicked();

    UFUNCTION()
    void HandleBackgroundClicked();
};
