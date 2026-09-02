#include "ContextMenuWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "../ItemInstance.h"

bool UContextMenuWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (WidgetTree && !WidgetTree->RootWidget)
    {
        UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
        WidgetTree->RootWidget = RootCanvas;

        BackgroundButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
        BackgroundButton->SetBackgroundColor(FLinearColor(0, 0, 0, 0));
        UCanvasPanelSlot* BGSlot = RootCanvas->AddChildToCanvas(BackgroundButton);
        BGSlot->SetAnchors(FAnchors(0, 0, 1, 1));
        BGSlot->SetOffsets(FMargin(0, 0, 0, 0));
        BackgroundButton->OnClicked.AddDynamic(this, &UContextMenuWidget::HandleBackgroundClicked);

        MenuContainer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        UCanvasPanelSlot* MenuSlot = RootCanvas->AddChildToCanvas(MenuContainer);
        MenuSlot->SetAutoSize(true);
        // Will set position in Setup()

        InspectButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
        UTextBlock* InspectText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        InspectText->SetText(FText::FromString("Inspect"));
        InspectText->SetColorAndOpacity(FLinearColor::Black);
        InspectButton->AddChild(InspectText);
        InspectButton->OnClicked.AddDynamic(this, &UContextMenuWidget::HandleInspectClicked);
        MenuContainer->AddChild(InspectButton);

        DiscardButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
        UTextBlock* DiscardText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        DiscardText->SetText(FText::FromString("Discard"));
        DiscardText->SetColorAndOpacity(FLinearColor::Black);
        DiscardButton->AddChild(DiscardText);
        DiscardButton->OnClicked.AddDynamic(this, &UContextMenuWidget::HandleDiscardClicked);
        MenuContainer->AddChild(DiscardButton);
        
        UnequipButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
        UTextBlock* UnequipText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        UnequipText->SetText(FText::FromString("Unequip"));
        UnequipText->SetColorAndOpacity(FLinearColor::Black);
        UnequipButton->AddChild(UnequipText);
        UnequipButton->OnClicked.AddDynamic(this, &UContextMenuWidget::HandleUnequipClicked);
        MenuContainer->AddChild(UnequipButton);

        UnloadButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
        UTextBlock* UnloadText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        UnloadText->SetText(FText::FromString("Unload"));
        UnloadText->SetColorAndOpacity(FLinearColor::Black);
        UnloadButton->AddChild(UnloadText);
        UnloadButton->OnClicked.AddDynamic(this, &UContextMenuWidget::HandleUnloadClicked);
        MenuContainer->AddChild(UnloadButton);
    }
    return true;
}

void UContextMenuWidget::Setup(UItemInstance* InItemObj, FVector2D ScreenPos)
{
    TargetItem = InItemObj;
    if (MenuContainer)
    {
        if (UCanvasPanelSlot* MenuSlot = Cast<UCanvasPanelSlot>(MenuContainer->Slot))
        {
            MenuSlot->SetPosition(ScreenPos);
        }
    }

    if (UnloadButton && TargetItem)
    {
        bool bCanUnload = false;
        if (TargetItem->Category == EItemCategory::Attachment && TargetItem->AttachmentType == EAttachmentType::Magazine && TargetItem->CurrentAmmo > 0)
        {
            // 탄창 자체의 내부 탄약을 빼내는 경우 (탄약이 1발 이상 있어야 함)
            bCanUnload = true;
        }
        else if (TargetItem->Category == EItemCategory::Weapon && TargetItem->EquippedMagazine)
        {
            // 무기에 탄창이 결합되어 있으면 탄약 개수와 무관하게 탄창 분리 가능
            bCanUnload = true;
        }
        UnloadButton->SetVisibility(bCanUnload ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

void UContextMenuWidget::HandleInspectClicked()
{
    if (TargetItem) OnInspectClicked.Broadcast(TargetItem);
    RemoveFromParent();
}

void UContextMenuWidget::HandleDiscardClicked()
{
    if (TargetItem) OnDiscardClicked.Broadcast(TargetItem);
    RemoveFromParent();
}

void UContextMenuWidget::HandleUnequipClicked()
{
    if (TargetItem) OnUnequipClicked.Broadcast(TargetItem);
    RemoveFromParent();
}

void UContextMenuWidget::HandleUnloadClicked()
{
    if (TargetItem) OnUnloadClicked.Broadcast(TargetItem);
    RemoveFromParent();
}

void UContextMenuWidget::HandleBackgroundClicked()
{
    OnMenuClosed.Broadcast();
    RemoveFromParent();
}
