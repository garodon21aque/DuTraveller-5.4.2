# Blueprint Contract Inventory

Generated from `./Saved/Logs/CompileAllBlueprints-5.8-phase3-controller-base.log` by `Tools/Analyze-BlueprintContracts.ps1`. This is evidence extraction only; it does not infer missing C++ signatures or runtime behavior.

## Baseline

- Failed Blueprints: 50
- Fatal Blueprint issues: 1052
- Blueprint warnings on failed assets: 157
- Parsed compiler diagnostic records: 2675
- Distinct missing function names: 33
- Distinct missing property names: 36
- Distinct delegate/event names with lost signatures: 20
- Distinct pins recovered from unknown structs: 20
- Distinct serialized project-native types: 13

## Failure clusters

| Subsystem | Blueprints | Fatal issues | Warnings |
|---|---:|---:|---:|
| Inventory.Equipment.Vendors | 11 | 390 | 77 |
| GAS.Spells | 9 | 201 | 1 |
| HUD | 10 | 141 | 12 |
| Quests | 5 | 116 | 19 |
| WorldWidgets.Effects | 5 | 69 | 12 |
| Crafting | 3 | 56 | 16 |
| LegacyAnimation | 4 | 56 | 20 |
| GAS.Attributes | 2 | 17 | 0 |
| Menus | 1 | 6 | 0 |

## Highest-frequency missing functions

| Function | Occurrences | Subsystems |
|---|---:|---|
| `GetQuantity` | 39 | Crafting; Inventory.Equipment.Vendors |
| `FindItemByClass` | 24 | Inventory.Equipment.Vendors |
| `UseItem` | 14 | Crafting; Inventory.Equipment.Vendors |
| `FrontCaptureOn` | 10 | Inventory.Equipment.Vendors |
| `FindItemsByClass` | 9 | Inventory.Equipment.Vendors |
| `FrontCaptureOff` | 7 | Inventory.Equipment.Vendors |
| `GetCapacity` | 7 | Inventory.Equipment.Vendors |
| `GetItem` | 7 | Inventory.Equipment.Vendors |
| `RemoveItem` | 6 | Inventory.Equipment.Vendors |
| `ShouldShowInInventory` | 6 | Crafting; Inventory.Equipment.Vendors |
| `TryAddItemFromClass` | 6 | Inventory.Equipment.Vendors |
| `FaceCaptureOn` | 4 | Inventory.Equipment.Vendors |
| `GetEquippedItems` | 4 | Inventory.Equipment.Vendors |
| `SetQuantity` | 4 | Inventory.Equipment.Vendors |
| `WaitForCooldownChange` | 4 | GAS.Spells |
| `FaceCaptureOff` | 3 | Inventory.Equipment.Vendors |
| `AddNewQuest` | 2 | Quests |
| `DropItem` | 2 | Inventory.Equipment.Vendors |
| `EndTask` | 2 | GAS.Spells |
| `EquipButtonPressed` | 2 | GAS.Spells |
| `FindItem` | 2 | Inventory.Equipment.Vendors |
| `GetOwningComponent` | 2 | WorldWidgets.Effects |
| `GetQuestActor` | 2 | Quests |
| `GetRecipes` | 2 | Crafting |
| `GetSpellMenuWidgetController` | 2 | GAS.Spells |
| `GetTimeRemaining` | 2 | WorldWidgets.Effects |
| `GlobeDeselect` | 2 | GAS.Spells |
| `IsAllObjectiveComplete` | 2 | Quests |
| `SpellGlobeSelected` | 2 | GAS.Spells |
| `SpellRowGlobePressed` | 2 | GAS.Spells |

## Inventory files

- `failing-blueprints.csv`: authoritative per-Blueprint fatal/warning counts emitted by the commandlet.
- `missing-functions.csv`: missing reflected function names and referencing assets.
- `missing-properties.csv`: missing reflected properties and referencing assets.
- `missing-delegates.csv`: delegate/event nodes whose signatures can no longer be resolved.
- `unknown-struct-pins.csv`: serialized field names retained by broken struct nodes.
- `missing-node-pins.csv`: serialized function/node pin names for later signature reconstruction.
- `raw-blueprint-diagnostics.csv`: normalized raw Blueprint compiler evidence.
- `serialized-native-types.csv`: exact `/Script/DT` and `/Script/DuTraveller` type identities and referencing assets.

Names in these files are contract candidates, not proof of owning class, parameter type, replication policy, or behavior. Those details must be corroborated before implementation.
