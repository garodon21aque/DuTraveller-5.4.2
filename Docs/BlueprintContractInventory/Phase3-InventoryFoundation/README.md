# Blueprint Contract Inventory

Generated from `Saved/Logs/Compile-InventoryFoundation.log` by `Tools/Analyze-BlueprintContracts.ps1`. This is evidence extraction only; it does not infer missing C++ signatures or runtime behavior.

## Baseline

- Failed Blueprints: 10
- Fatal Blueprint issues: 170
- Blueprint warnings on failed assets: 19
- Parsed compiler diagnostic records: 440
- Distinct missing function names: 10
- Distinct missing property names: 6
- Distinct delegate/event names with lost signatures: 3
- Distinct pins recovered from unknown structs: 0
- Distinct serialized project-native types: 16

## Failure clusters

| Subsystem | Blueprints | Fatal issues | Warnings |
|---|---:|---:|---:|
| Inventory.Equipment.Vendors | 7 | 124 | 10 |
| Crafting | 3 | 46 | 9 |

## Highest-frequency missing functions

| Function | Occurrences | Subsystems |
|---|---:|---|
| `UseItem` | 12 | Crafting; Inventory.Equipment.Vendors |
| `FrontCaptureOn` | 7 | Inventory.Equipment.Vendors |
| `FrontCaptureOff` | 5 | Inventory.Equipment.Vendors |
| `GetQuantity` | 4 | Crafting |
| `DropItem` | 3 | Inventory.Equipment.Vendors |
| `FaceCaptureOn` | 3 | Inventory.Equipment.Vendors |
| `GetEquippedItems` | 3 | Inventory.Equipment.Vendors |
| `GetRecipes` | 3 | Crafting |
| `FaceCaptureOff` | 2 | Inventory.Equipment.Vendors |
| `ShouldShowInInventory` | 2 | Crafting |

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
