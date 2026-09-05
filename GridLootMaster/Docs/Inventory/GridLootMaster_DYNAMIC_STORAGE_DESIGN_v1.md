# GridLootMaster — Dynamic Equipment Storage Design v1

Status: **ACTIVE DESIGN**
Target: Unreal Engine 5.7 / C++ only
Scope: Backpack + Chest Rig dynamic multi-section storage
Out of scope: final art polish, world WASD redesign, sound system, boss/patrol AI

---

## 1. Why this phase exists

Current runtime storage is fixed in `AGridGameMode`:

- Backpack inventory: 5x6
- Chest Rig inventory: 4x3
- Pocket: 5x1
- SafeBox: 2x2
- Loot: 6x6
- Stash: 10x10

The new design changes **Backpack and Chest Rig only** so that the equipped gear item defines the storage layout it provides.

Examples:

- Standard Backpack: `5x6`
- Tactical Backpack: `3x4*1;2x2*2;1x3*1`
- Standard Rig: `4x3`
- Modular Rig: `1x2*4;1x1*2`
- Rifle Rig: `1x3*2;1x2*2`

Each storage section is an **independent grid**.

Two adjacent `1x2` sections do **not** become one `2x2` placement area.
An item can never span section boundaries.

---

## 2. Non-negotiable invariants

1. `InstanceID` uniqueness must remain intact.
2. No item duplication or loss during drag/drop, equipment swap, reload, resize, or rollback.
3. Storage reconfiguration must be transactional.
4. If the current contents cannot fit the incoming gear layout, the equipment change is rejected and the original state remains unchanged.
5. No automatic world drop or automatic Stash transfer on failed equipment change.
6. Current item rotation state should be preserved during automatic repack in v1.
7. Pocket, SafeBox, Loot Container, and Stash remain single-grid containers in this phase.
8. Existing Backpack 5x6 and Rig 4x3 gameplay capacity should remain the default starting layout so this structural phase does not also become a balance change.
9. Existing Reload Magazine Swap must still search all Chest Rig sections plus Pocket.
10. Existing Stash Save format remains compatible. This phase does not turn equipped Backpack/Rig contents into nested persistent Stash contents.

---

## 3. Data model

### 3.1 CSV-friendly equipment layout specification

`FItemData` currently comes from a DataTable-friendly structure. Avoid a difficult nested struct-array CSV format for v1.

Add a field:

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Storage")
FString StorageLayoutSpec;
```

Recommended syntax:

```text
5x6
4x3
1x2*4;1x1*2
1x3*2;1x2*2
3x4;2x2*2;1x3
```

Grammar:

```text
Entry      := Width "x" Height [ "*" Count ]
Layout     := Entry [ ";" Entry ... ]
```

Rules:

- Width >= 1
- Height >= 1
- Count defaults to 1
- Count >= 1
- whitespace ignored
- invalid layout => reject/log; never silently construct malformed grids

A small runtime representation may be used:

```cpp
USTRUCT(BlueprintType)
struct FStorageSectionDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FIntPoint Size = FIntPoint(1, 1);

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Count = 1;
};
```

`StorageLayoutSpec` is the DataTable-facing source; runtime parser expands it to concrete section sizes.

### 3.2 UItemInstance

Copy `StorageLayoutSpec` from `FItemData` in `InitFromData()`.

The two current hand-created defaults in `GridGameMode` must explicitly receive:

```text
DefaultBackpack -> "5x6"
DefaultRig      -> "4x3"
```

Do not accidentally leave hand-created QA/default gear with an empty storage layout.

---

## 4. Runtime inventory architecture

Do **not** replace every inventory type with a brand-new component hierarchy.

Extend the existing `UGridInventoryComponent` with optional multiple independent sections while preserving legacy single-grid compatibility.

### 4.1 Compatibility strategy

Section 0 remains represented by the current legacy fields:

```cpp
GridWidth
GridHeight
GridCells
```

This protects existing Stash/Loot/SafeBox/Pocket logic and limits regression scope.

Additional sections are stored separately, e.g.:

```cpp
USTRUCT()
struct FGridInventorySection
{
    GENERATED_BODY()

    UPROPERTY()
    int32 Width = 1;

    UPROPERTY()
    int32 Height = 1;

    UPROPERTY()
    TArray<FName> GridCells;
};

UPROPERTY()
TArray<FGridInventorySection> AdditionalSections;
```

Logical section indices:

- Section 0 -> current `GridWidth/GridHeight/GridCells`
- Section 1+ -> `AdditionalSections[SectionIndex - 1]`

`ItemInstances` remains one map for the whole logical inventory component.

This is important: Backpack sections are separate placement grids but still one logical Backpack inventory.

---

## 5. Required section-aware API

Keep existing methods as legacy Section-0 wrappers.

Add section-aware APIs equivalent to:

```cpp
int32 GetSectionCount() const;
FIntPoint GetSectionSize(int32 SectionIndex) const;

bool IsValidSection(int32 SectionIndex) const;
bool IsValidSectionIndex(int32 SectionIndex, int32 CellIndex) const;

FName GetCellItemID(int32 SectionIndex, int32 X, int32 Y) const;

bool CheckItemFitInSection(
    FName ItemID,
    int32 SectionIndex,
    int32 StartX,
    int32 StartY,
    int32 ItemWidth,
    int32 ItemHeight) const;

bool FindEmptySpaceInSection(
    int32 SectionIndex,
    int32 ItemWidth,
    int32 ItemHeight,
    int32& OutX,
    int32& OutY) const;

bool FindEmptySpaceAcrossSections(
    int32 ItemWidth,
    int32 ItemHeight,
    int32& OutSectionIndex,
    int32& OutX,
    int32& OutY) const;

bool AddItemToSection(
    UItemInstance* ItemObj,
    int32 SectionIndex,
    int32 StartX,
    int32 StartY);

bool FindItemPlacement(
    FName ItemID,
    int32& OutSectionIndex,
    int32& OutX,
    int32& OutY) const;
```

Existing:

```cpp
CheckItemFit(...)
FindEmptySpace(...)
AddItem(...)
```

must continue to mean Section 0 so single-grid callers do not unexpectedly change semantics.

`RemoveItem()` and `ClearInventory()` must operate across **all sections**.

`ClearInventory()` clears contents but should not destroy the current configured section layout.

---

## 6. Configuring layouts

Add explicit layout initialization:

```cpp
bool InitializeSections(const TArray<FIntPoint>& SectionSizes);
```

For one section, it behaves like the old single-grid setup.

For Backpack/Rig during initial startup:

```text
Equipped Backpack StorageLayoutSpec -> InventoryComponent sections
Equipped Rig StorageLayoutSpec      -> RigComponent sections
```

Current defaults must therefore still create:

```text
Backpack: one 5x6 section
Rig:      one 4x3 section
```

Pocket/SafeBox/Loot/Stash use existing `InitializeGrid()` and remain single-section.

---

## 7. Transactional reconfiguration

Changing Backpack or Rig means the logical storage component must change layout without losing items.

Add a **plan-then-commit** reconfiguration path.

Never resize the live grid destructively first.

Conceptually:

```cpp
bool BuildReconfigurePlan(...);
bool CommitReconfigurePlan(...);
```

or an equivalent implementation with a complete snapshot/rollback.

### 7.1 Deterministic repack

When validating existing contents against a new layout:

- preserve item object and InstanceID
- preserve current rotation
- do not silently rotate items in v1
- deterministic order:
  1. larger occupied area first
  2. larger max side first
  3. InstanceID lexical tie-break
- placement:
  1. SectionIndex ascending
  2. Y ascending
  3. X ascending

If every item fits, produce a complete placement plan.
If any item fails, do not mutate live state.

### 7.2 Swap / unequip semantics

For storage gear:

- Backpack slot controls `InventoryComponent`
- Rig slot controls `RigComponent`

Incoming storage gear defines the candidate new layout.

If swapping old gear -> new gear:

1. determine final set of storage contents after the equipment transaction
2. exclude the incoming gear if it is currently stored inside the storage being replaced
3. preserve every other contained item
4. account for the outgoing gear's destination according to the existing drag/drop transaction
5. validate the new layout
6. only commit equipment + layout + item placements if all steps succeed

If unequipping Backpack/Rig with no replacement:

- new layout is effectively no provided storage
- therefore reject unless corresponding storage is empty (after accounting for the dragged-out equipment transaction)

No partial commit.

### 7.3 Centralize storage-gear transaction

Do not bury dynamic storage mutation solely inside visual widget code.

It is acceptable for `UEquipmentSlotWidget` to initiate the operation, but storage layout validation/commit should live in gameplay/runtime helpers (`AGridGameMode`, `UGridInventoryComponent`, or a focused helper), so automation can test it without UI.

---

## 8. Drag & drop across sections

`UGridBoardWidget` currently binds one `UGridInventoryComponent`.

Keep that model and add:

```cpp
int32 SectionIndex = 0;
```

Every board is one section view.

Add source section metadata to drag state:

```cpp
UDraggableItemWidget::SourceSectionIndex
UItemDragDropOperation::SourceSectionIndex
```

Destination board already knows its own `SectionIndex`.

Required behavior:

- same section reposition -> transactional
- section A -> section B in same Backpack -> transactional
- Backpack section -> Rig section -> transactional
- merge between sections -> works
- rotation preview -> section-aware
- split/unload operations -> find space across the logical source inventory sections where appropriate
- rollback restores the exact original section/coordinate

No item may occupy cells from two sections.

---

## 9. UI

Create a reusable C++ wrapper such as:

```text
USectionedStorageWidget
```

Responsibilities:

- receive one `UGridInventoryComponent`
- generate one `UGridBoardWidget` per runtime section
- each board gets:
  - InventoryComponent
  - SectionIndex
- display sections as separate pouches/grids
- use a simple `UWrapBox`, `UHorizontalBox`, or similar C++ layout
- small visible spacing between independent sections
- rebuild only when section layout changes
- refresh item visuals when inventory contents change

This phase is functional UI, not final art polish.

For one-section default Backpack/Rig, visual result should remain close to the current UI.

For a synthetic layout:

```text
1x2*4;1x1*2
```

six visibly independent grids must appear.

An item must not visually or logically span the gap between them.

---

## 10. Reload integration

Current reload searches:

```text
RigComponent
PocketComponent
```

Dynamic Rig means all Rig sections must be searched.

Reload candidate identity must include:

```text
SourceInventory
SourceSectionIndex
GridCoordinate
Magazine InstanceID
```

Candidate priority remains:

1. most ammo
2. Rig before Pocket
3. for Rig: SectionIndex ascending
4. Y
5. X
6. InstanceID lexical

Reload completion must revalidate that the selected magazine is still in the same section and coordinate.

Magazine swap puts the old equipped magazine into the selected magazine's **exact original section and cell position**.

Backpack remains excluded from automatic R reload.

---

## 11. High-level logical inventory behavior

Because `ItemInstances` is shared across all sections in one logical component:

- selling Backpack contents must see all sections
- item lookup by InstanceID must see all sections
- uniqueness checks must cover all sections
- discard/context operations must work regardless of section

Audit direct use of:

```cpp
GridWidth
GridHeight
GridCells
GetIndex()
```

Outside `UGridInventoryComponent`.

Known areas that require section-awareness include at least:

- `UGridBoardWidget`
- `UDraggableItemWidget` rotation/original-position lookup
- `UCombatComponent` reload source coordinate lookup/revalidation

Single-grid-only Stash/Loot code may remain legacy if it never operates on Backpack/Rig.

Do not mechanically rewrite single-grid Stash save logic into multi-section format in this phase.

---

## 12. Save/load policy for v1

Current persistent save is Stash-oriented.

This phase should **not** invent nested persistent Backpack/Rig contents inside Stash.

Rules:

- Stash remains 10x10 single-grid and existing save compatibility is preserved.
- Storage gear's `StorageLayoutSpec` comes from item template data after load.
- Equipped runtime Backpack/Rig contents remain governed by the current raid/lobby lifecycle.
- If a later phase adds persistent equipped loadout saving, store `(SectionIndex, GridX, GridY, Rotation)` for each contained item then.

Do not expand save scope unless current source already requires it for an existing workflow.

---

## 13. Default migration strategy

Structural migration first, balance later.

Initial visible defaults:

```text
Standard Backpack = 5x6*1
Standard Chest Rig = 4x3*1
```

Therefore current player loadout remains usable.

Automation must also create synthetic multi-section layouts, for example:

```text
Test Rig A = 1x2*4;1x1*2
Test Rig B = 1x3*2;1x2*2
```

This proves the architecture without forcing final content design now.

---

## 14. Required automation coverage

### Core section inventory

1. one-section legacy inventory behavior unchanged
2. parse `5x6`
3. parse `1x2*4;1x1*2`
4. invalid layout rejected
5. independent section boundaries enforced
6. item cannot span two adjacent sections
7. same-section move
8. cross-section move
9. cross-section merge
10. RemoveItem removes occupancy from correct section
11. ClearInventory clears all sections but preserves layout
12. InstanceID duplication rejected across sections

### Reconfiguration

13. old layout -> larger layout preserves all items
14. old layout -> different multi-section layout deterministic repack
15. insufficient layout -> reject with zero mutation
16. rotation state preserved
17. failed transaction restores exact old placements/layout
18. unequip non-empty storage without replacement rejected
19. empty storage unequip allowed where existing equipment flow allows it

### Equipment

20. default Backpack yields one 5x6 section
21. default Rig yields one 4x3 section
22. equipping synthetic multi-section Rig changes Rig layout
23. failed smaller-Rig swap keeps old equipment + old contents
24. incoming item from same storage is excluded correctly from fit plan
25. no duplicate/lost InstanceID after successful swap

### UI / drag-drop

26. section boards count equals runtime sections count
27. SectionIndex binding correct
28. drag Section0 -> Section1 succeeds when fit
29. failed cross-section drop rolls back
30. preview uses destination section dimensions
31. rotation uses correct source section

### Reload regression

32. compatible magazine found in Rig Section0
33. compatible magazine found in later Rig section
34. most-ammo priority across multiple Rig sections
35. old magazine returns to exact selected section/cell
36. Pocket fallback still works
37. Backpack remains excluded
38. reload transaction rollback still safe

### Regression

39. Stash save/load unchanged
40. Loot 6x6 unchanged
41. SafeBox unchanged
42. Pocket unchanged
43. weapon attachment drag/drop unchanged
44. split stack unchanged
45. sell/discard sees items in nonzero Backpack sections

---

## 15. Manual QA after implementation

Use a QA multi-section rig:

```text
1x2*4;1x1*2
```

Verify:

- six separate pouches visible
- 2x2 item cannot bridge two pouches
- magazine can move between pouches
- reload can find magazine in any pouch
- swap to different Rig layout works if all contents fit
- swap rejected if contents do not fit
- rejection loses/duplicates nothing
- default Backpack remains usable
- drag/drop to Stash/Loot still works

---

## 16. Deferred / future work

Do not include in this phase:

- final Backpack/Rig content balance
- final pouch art/frame design
- nested bags stored inside bags with their own persistent contents
- weight capacity per section
- category-restricted pouches
- secure-container special rules beyond current SafeBox
- hotkeys per Rig pouch
- sound/UI polish
- final icons
- persistent equipped-loadout SaveGame expansion

These can build on the section architecture later.
