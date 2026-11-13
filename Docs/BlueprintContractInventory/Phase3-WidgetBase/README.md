# Blueprint Contract Inventory

Generated from `./Saved/Logs/CompileAllBlueprints-5.8-phase3-widget-base.log` by `Tools/Analyze-BlueprintContracts.ps1`. This is evidence extraction only; it does not infer missing C++ signatures or runtime behavior.

## Baseline

- Failed Blueprints: 52
- Fatal Blueprint issues: 1109
- Blueprint warnings on failed assets: 160
- Parsed compiler diagnostic records: 1399
- Distinct missing function names: 36
- Distinct missing property names: 37
- Distinct delegate/event names with lost signatures: 22
- Distinct pins recovered from unknown structs: 24
- Distinct serialized project-native types: 13

## Failure clusters

| Subsystem | Blueprints | Fatal issues | Warnings |
|---|---:|---:|---:|
| Inventory.Equipment.Vendors | 11 | 390 | 77 |
| GAS.Spells | 9 | 203 | 2 |
| HUD | 10 | 144 | 12 |
| Quests | 5 | 116 | 19 |
| GAS.Attributes | 4 | 69 | 2 |
| WorldWidgets.Effects | 5 | 69 | 12 |
| Crafting | 3 | 56 | 16 |
| LegacyAnimation | 4 | 56 | 20 |
| Menus | 1 | 6 | 0 |

## Highest-frequency missing functions

| Function | Occurrences | Subsystems |
|---|---:|---|
| `GetQuantity` | 18 | Crafting; Inventory.Equipment.Vendors |
| `FindItemByClass` | 11 | Inventory.Equipment.Vendors |
| `GetAttributeWidgetController` | 6 | GAS.Attributes; HUD |
| `UseItem` | 6 | Crafting; Inventory.Equipment.Vendors |
| `FrontCaptureOn` | 4 | Inventory.Equipment.Vendors |
| `FindItemsByClass` | 3 | Inventory.Equipment.Vendors |
| `FrontCaptureOff` | 3 | Inventory.Equipment.Vendors |
| `GetCapacity` | 3 | Inventory.Equipment.Vendors |
| `GetItem` | 3 | Inventory.Equipment.Vendors |
| `RemoveItem` | 3 | Inventory.Equipment.Vendors |
| `ShouldShowInInventory` | 3 | Crafting; Inventory.Equipment.Vendors |
| `TryAddItemFromClass` | 3 | Inventory.Equipment.Vendors |
| `BroadcastInitialValues` | 2 | GAS.Attributes; GAS.Spells |
| `FaceCaptureOn` | 2 | Inventory.Equipment.Vendors |
| `GetEquippedItems` | 2 | Inventory.Equipment.Vendors |
| `SetQuantity` | 2 | Inventory.Equipment.Vendors |
| `UpgradeAttribute` | 2 | GAS.Attributes |
| `WaitForCooldownChange` | 2 | GAS.Spells |
| `AddNewQuest` | 1 | Quests |
| `DropItem` | 1 | Inventory.Equipment.Vendors |
| `EndTask` | 1 | GAS.Spells |
| `EquipButtonPressed` | 1 | GAS.Spells |
| `FaceCaptureOff` | 1 | Inventory.Equipment.Vendors |
| `FindItem` | 1 | Inventory.Equipment.Vendors |
| `GetOwningComponent` | 1 | WorldWidgets.Effects |
| `GetQuestActor` | 1 | Quests |
| `GetRecipes` | 1 | Crafting |
| `GetSpellMenuWidgetController` | 1 | GAS.Spells |
| `GetTimeRemaining` | 1 | WorldWidgets.Effects |
| `GlobeDeselect` | 1 | GAS.Spells |

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
