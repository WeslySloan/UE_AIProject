#include "SectionedStorageWidget.h"
#include "GridBoardWidget.h"
#include "../GridInventoryComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void USectionedStorageWidget::BindInventory(UGridInventoryComponent* InInventory)
{
    if (InventoryComponent) InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &USectionedStorageWidget::RebuildSections);
    InventoryComponent = InInventory;
    if (InventoryComponent) InventoryComponent->OnInventoryChanged.AddUniqueDynamic(this, &USectionedStorageWidget::RebuildSections);
    RebuildSections();
}

int32 USectionedStorageWidget::GetRenderedSectionCount() const
{
    return SectionBoards.Num();
}

void USectionedStorageWidget::RebuildSections()
{
    if (!WidgetTree || !InventoryComponent) return;
    TArray<FIntPoint> CurrentSizes;
    for (int32 Index = 0; Index < InventoryComponent->GetSectionCount(); ++Index) CurrentSizes.Add(InventoryComponent->GetSectionSize(Index));
    if (SectionBox && CurrentSizes == RenderedSectionSizes)
    {
        for (UGridBoardWidget* Board : SectionBoards) Board->RefreshGridUI();
        return;
    }
    if (!SectionBox)
    {
        SectionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SectionBox"));
        WidgetTree->RootWidget = SectionBox;
    }
    SectionBox->ClearChildren();
    SectionBoards.Empty();
    RenderedSectionSizes = CurrentSizes;
    for (int32 Index = 0; Index < InventoryComponent->GetSectionCount(); ++Index)
    {
        UGridBoardWidget* Board = WidgetTree->ConstructWidget<UGridBoardWidget>(UGridBoardWidget::StaticClass());
        Board->InventoryComponent = InventoryComponent;
        Board->SectionIndex = Index;
        SectionBoards.Add(Board);
        UVerticalBoxSlot* SectionSlot = SectionBox->AddChildToVerticalBox(Board);
        SectionSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
    }
    for (UGridBoardWidget* Board : SectionBoards) Board->RefreshGridUI();
}
