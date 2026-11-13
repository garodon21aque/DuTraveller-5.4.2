# Blueprint Contract Inventory

Generated from `./Saved/Logs/CompileAllBlueprints-5.8-phase3-cooldown-task.log` by `Tools/Analyze-BlueprintContracts.ps1`. This is evidence extraction only; it does not infer missing C++ signatures or runtime behavior.

## Baseline

- Failed Blueprints: 37
- Fatal Blueprint issues: 763
- Blueprint warnings on failed assets: 156
- Parsed compiler diagnostic records: 2065
- Distinct missing function names: 25
- Distinct missing property names: 35
- Distinct delegate/event names with lost signatures: 11
- Distinct pins recovered from unknown structs: 14
- Distinct serialized project-native types: 16

## Failure clusters

| Subsystem | Blueprints | Fatal issues | Warnings |
|---|---:|---:|---:|
| Inventory.Equipment.Vendors | 11 | 390 | 77 |
| Quests | 5 | 116 | 19 |
| WorldWidgets.Effects | 5 | 69 | 12 |
| Crafting | 3 | 56 | 16 |
| LegacyAnimation | 4 | 56 | 20 |
| HUD | 5 | 55 | 12 |
| GAS.Spells | 3 | 15 | 0 |
| Menus | 1 | 6 | 0 |

## Highest-frequency missing functions

| Function | Occurrences | Subsystems |
|---|---:|---|
| `GetQuantity` | 40 | Crafting; Inventory.Equipment.Vendors |
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
| `DropItem` | 3 | Inventory.Equipment.Vendors |
| `FaceCaptureOff` | 3 | Inventory.Equipment.Vendors |
| `AddNewQuest` | 2 | Quests |
| `FindItem` | 2 | Inventory.Equipment.Vendors |
| `GetOwningComponent` | 2 | WorldWidgets.Effects |
| `GetQuestActor` | 2 | Quests |
| `GetRecipes` | 2 | Crafting |
| `GetTimeRemaining` | 2 | WorldWidgets.Effects |
| `IsAllObjectiveComplete` | 2 | Quests |
| `TogglePauseMenu` | 2 | Menus |
| `TurnInQuest` | 2 | Quests |

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
