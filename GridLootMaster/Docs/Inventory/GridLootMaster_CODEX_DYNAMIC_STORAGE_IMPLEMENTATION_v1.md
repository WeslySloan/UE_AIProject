# GridLootMaster — Dynamic Backpack / Chest Rig Storage Implementation

Use the current repository Source as the source of truth.

Read `Docs/Inventory/GridLootMaster_DYNAMIC_STORAGE_DESIGN_v1.md` first if it exists.
Implement the design, but do not merely acknowledge or summarize it.

This is a major structural phase. Start actual source work immediately.

## Goal

Replace the hard system assumption:

- Backpack = fixed 5x6 forever
- Chest Rig = fixed 4x3 forever

with:

- equipped Backpack item defines Backpack storage sections
- equipped Rig item defines Rig storage sections
- each section is an independent grid
- default items still provide one 5x6 Backpack section and one 4x3 Rig section so current balance does not change yet

Examples that the architecture must support:

```text
5x6
1x2*4;1x1*2
1x3*2;1x2*2
```

Two adjacent independent sections may never act as one larger grid.

---

## Current architecture constraints

Preserve the current `UGridInventoryComponent` architecture instead of replacing the entire inventory stack.

Use a backward-compatible multi-section extension:

- Section 0 continues using current `GridWidth / GridHeight / GridCells`
- additional sections are stored separately
- `ItemInstances` remains one logical map for the whole inventory component
- existing non-section-aware methods remain Section-0 compatibility wrappers

Pocket / SafeBox / Loot / Stash stay single-grid in this phase.

Do not redesign Stash persistence.

---

## Data

Add CSV-friendly equipment storage data:

```cpp
FString StorageLayoutSpec;
```

to item template/instance data as appropriate.

Support:

```text
5x6
4x3
1x2*4;1x1*2
1x3*2;1x2*2
```

Parse deterministically and reject malformed/zero/negative layout entries.

Current manually created default gear must explicitly use:

```text
DefaultBackpack = 5x6
DefaultRig = 4x3
```

Do not rely on an empty layout falling back silently.

---

## UGridInventoryComponent

Add runtime support for independent sections.

Required abilities:

- GetSectionCount
- GetSectionSize
- section cell access
- CheckItemFitInSection
- FindEmptySpaceInSection
- FindEmptySpaceAcrossSections
- AddItemToSection
- FindItemPlacement with SectionIndex/X/Y
- RemoveItem across all sections
- ClearInventory across all sections while preserving layout
- InitializeSections
- transactional ReconfigureSections / equivalent plan-then-commit

Existing:

```cpp
CheckItemFit
FindEmptySpace
AddItem
```

must retain Section-0 meaning for legacy single-grid callers.

Do not allow one item to span multiple sections.

Keep InstanceID uniqueness across the entire logical component.

---

## Reconfiguration transaction

Backpack slot controls `InventoryComponent`.
Rig slot controls `RigComponent`.

When replacing an equipped Backpack/Rig:

1. obtain the incoming gear layout
2. determine the final logical set of contained items
3. if incoming gear currently exists inside the storage being replaced, exclude that incoming gear from the content fit plan
4. preserve all other item objects / IDs / rotations
5. build a complete deterministic placement plan for the new sections
6. account for the outgoing equipped gear using the existing equipment drag/drop transaction
7. commit equipment + layout + placements only if everything succeeds
8. otherwise restore exact old equipment, layout, item placements, and source state

No partial commit.
No auto-drop.
No auto-Stash.
No duplicate.
No lost item.

If unequipping storage gear with no replacement, reject while its corresponding storage still contains items.

Do not implement this solely as visual-widget state. Put the gameplay validation/transaction in testable runtime code.

---

## Deterministic repack

During storage layout reconfiguration:

- preserve current item rotation in v1
- do not silently auto-rotate
- sort items:
  1. area descending
  2. max dimension descending
  3. InstanceID lexical
- place:
  1. SectionIndex ascending
  2. Y ascending
  3. X ascending

Failure to place any item => reject without live mutation.

---

## Drag/drop

Make `UGridBoardWidget` section-aware:

```cpp
int32 SectionIndex
```

Each board renders exactly one section.

Add:

```text
SourceSectionIndex
```

to `UDraggableItemWidget` / `UItemDragDropOperation`.

Support safely:

- same-section reposition
- Section A -> Section B in same logical inventory
- Backpack section -> Rig section
- merge between sections
- rotation preview
- split/unload space search
- exact rollback to original section/coordinate

Destination preview must use destination section dimensions.

Audit direct access to `GridWidth/GridHeight/GridCells/GetIndex`.

Known multi-section-sensitive locations include:

- GridBoardWidget
- DraggableItemWidget
- CombatComponent reload logic

Do not rewrite Stash/Loot single-grid code unless necessary.

---

## UI

Add a reusable C++ section-container widget, preferably something equivalent to:

```text
USectionedStorageWidget
```

It should:

- bind one `UGridInventoryComponent`
- dynamically create one `UGridBoardWidget` per section
- assign InventoryComponent + SectionIndex
- display independent grids with small spacing
- rebuild when layout changes
- refresh items on inventory changes

No Blueprint/WBP.

Default single-section Backpack/Rig should remain visually close to current UI.

A synthetic `1x2*4;1x1*2` layout must visibly render six independent grids.

This is functional layout only, not final art polish.

---

## Reload integration

Existing R reload semantics remain Magazine Swap.

Search:

- all Rig sections
- Pocket

Do not search Backpack.

Reload candidate identity must include:

```text
SourceInventory
SourceSectionIndex
GridCoordinate
MagazineID
```

Priority:

1. CurrentAmmo descending
2. Rig before Pocket
3. Rig SectionIndex ascending
4. Y
5. X
6. InstanceID lexical

On completion revalidate exact section + coordinate.

The old equipped magazine must return to the chosen spare magazine's exact original section/cell.

Preserve all existing rollback safety.

---

## Default migration

Do NOT change current capacity balance yet.

After implementation current default player must still have:

```text
Backpack: 5x6 single section
Rig: 4x3 single section
Pocket: 5x1 unchanged
SafeBox: 2x2 unchanged
```

Use automation-only synthetic multi-section gear to prove the new architecture.

Do not introduce final tactical-rig balance in this phase.

---

## Save scope

Do not redesign Stash SaveGame.

Existing Stash remains a 10x10 single-grid persistent container.

`StorageLayoutSpec` comes from item template data.

Do not create nested persistent backpack contents in Stash.

Do not add equipped-loadout persistence unless the current code already requires it for existing behavior.

---

## Required tests

Add/adjust deterministic automation covering at minimum:

### Sections
- legacy one-section behavior unchanged
- parse 5x6
- parse 1x2*4;1x1*2
- malformed layout rejected
- independent boundaries
- item cannot span sections
- same-section move
- cross-section move
- cross-section merge
- RemoveItem/ClearInventory across sections
- InstanceID duplicate rejection

### Reconfigure/equipment
- larger layout preserves contents
- multi-section deterministic repack
- insufficient new layout => zero mutation
- rotation preserved
- failed swap exact rollback
- non-empty unequip rejected
- default Backpack 5x6
- default Rig 4x3
- synthetic multi-section Rig equips correctly
- smaller Rig swap rejection preserves all state
- incoming item contained in same storage handled correctly

### UI/drag
- section-board count correct
- board SectionIndex correct
- cross-section drop
- failed cross-section rollback
- preview dimensions section-aware
- rotation source section correct

### Reload
- magazine in Rig section 0
- magazine in later Rig section
- most ammo across sections
- old mag returns to same selected section/cell
- Pocket still works
- Backpack excluded

### Regression
- Stash save/load
- Loot
- SafeBox
- Pocket
- split stack
- attachments/modding
- sell/discard finds items in nonzero Backpack sections

---

## Validation strategy

This is a large phase. Work incrementally.

Checkpoint A:
- data parser
- multi-section inventory core
- section-aware APIs
- targeted Inventory tests
- incremental compile

Checkpoint B:
- GridBoard/drag/drop dynamic sections
- equipment layout reconfiguration transaction
- Reload multi-section integration
- targeted Inventory/Equipment/Reload/UI tests
- incremental compile

Only after both checkpoints pass:

- relevant Stash regression
- full GridLootMaster Automation exactly once
- `git diff --check`

World tests use `-game` when required.

Do not clean/rebuild unless necessary.

---

## Do not do

- no final UI art polish
- no sound system
- no icon generation
- no enemy AI work
- no map changes
- no combat balance changes
- no world WASD redesign
- no A* redesign
- no Stash save format redesign
- no nested persistent bags
- no auto-drop or auto-Stash on failed swap
- no unrelated refactor

---

## Completion report

Only report after actual implementation and validation.

Report:

- changed files
- final section runtime representation
- default Backpack/Rig layouts
- equipment swap/rollback behavior
- dynamic UI behavior
- Reload multi-section behavior
- targeted test counts
- full automation result
- `git diff --check`

Then stop. Do not start another phase.
