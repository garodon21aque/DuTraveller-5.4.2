# DuTraveller — Meshy AI Character Retargeting Pipeline

Workflow for importing rigged Meshy AI characters and wiring them into the
existing DuTraveller enemy framework (UE 5.8.3).

## Architecture

| Layer | Existing | Meshy pipeline produces |
|---|---|---|
| Skeleton | `SK_Mannequin_Skeleton` (UE4) — most enemies; `SK_UEFN_Mannequin` (FN) — player | `SK_Meshy_<Name>_Skeleton` per import |
| Enemy AnimBP | `AnimBP_RPGAICharacter` → `URPGAnimInstance` | `ABP_<Name>_Enemy` (duplicated, re-skeletoned, refs remapped) |
| Combat montages | UE4-skeleton montages via `UDTAnimationRouter.CombatAnimations` | `DT_*_Meshy_<Name>` retargeted montages (notifies/slots preserved) |
| Enemy BP | `BP_RPGAICharacter` → `ARPGAICharacter` (+ `BP_CombatTest_Controller`) | `BP_<Name>_Enemy` child, same router/AI/equipment contract |
| Sockets | `hand_r`, `hand_l`, `weapon_r`, `head` | added to the Meshy mesh on the matching humanoid bones |

Content layout:

```
Content/DuTraveller/Characters/Imported/Meshy/<Name>/   imported FBX assets
Content/DuTraveller/Animations/IKRigs/                  IK_Meshy_<Name> (+ IK_DT_* source rigs)
Content/DuTraveller/Animations/Retargeters/             RTG_<Src>_To_Meshy_<Name>
Content/DuTraveller/Animations/Retargeted/<Name>/RouteA Route A enemy library retargets
Content/DuTraveller/Animations/Retargeted/<Name>/RouteC FN-mannequin retargets
Content/DuTraveller/Editor/Retargeting/                 EUW_DuTraveller_CharacterRetargeter
```

## Retarget routes

- **Route A — Existing Enemy → Meshy** (primary). Source `IK_DT_LegacyUE4`
  (UE4 mannequin) → `IK_Meshy_<Name>`. Retargets the combat montages,
  hit reactions, death montages, `LocomotionBS` and every sequence the enemy
  AnimBP references.
- **Route B — UE5 Manny/Quinn → Meshy**. Auto-detected: currently the engine
  `MoverExamples` plugin (which ships `SKM_Manny`, `IK_Mannequin`, `RTG_Mannequin`
  and the `MM_*` library) is not enabled, so the route reports unavailable and
  still builds a UE5-named rig from the in-project Serpent skeleton when its
  bone names match the UE5 mannequin convention.
- **Route C — UEFN FN_Mannequin → Meshy**. Source `IK_DT_UEFN` /
  `SKM_UEFN_Mannequin` → `IK_Meshy_<Name>` (`RTG_DT_UEFN_To_Meshy_<Name>`).

Reverse direction (Meshy → mannequin) uses the same `ensure_retargeter`
helper with rigs swapped (`direction="meshy_to_source"`).

## Headless usage

```
Tools\Run-MeshyPipeline.cmd Meshy-Pipeline.py "" "" "<abs fbx path>" "A,C"
Tools\Run-MeshyPipeline.cmd Meshy-Import.py
Tools\Run-MeshyPipeline.cmd Meshy-Fixup.py
Tools\Run-MeshyPipeline.cmd Meshy-Validate.py
Tools\Run-MeshyPipeline.cmd Meshy-CreateEUW.py   (after building DuTravellerEditor)
```

Env vars: `MESHY_FBX`, `MESHY_MESH` (skip import), `MESHY_NAME`, `MESHY_ROUTES`.
Reports: `Saved/Automation/Meshy*.json`.

## Editor UI

`Tools → DuTraveller Character Retargeter (Meshy)` opens
`EUW_DuTraveller_CharacterRetargeter`, a native `UDuTravellerCharacterRetargeterWidget`
that builds its UMG panel in code and delegates actions to `Tools/MeshyRetargetCore.py`
through `IPythonScriptPlugin`. Requires the `DuTravellerEditor` module build.

## Non-humanoid / unrigged imports

`analyze_skeletal_mesh()` detects bone count, naming convention
(`mixamo` / `ue` / `meshy` / `generic`), T-pose vs A-pose, missing canonical
chains and extra bones (cloak/staff/tail → `Extra_*` single-bone chains).
An FBX with no skin weights produces a `requires_rigging` report instead of an
invalid retargeter (e.g. `Azure_Lich_King_Import0003.fbx` is static-only;
`Azure_Lich_King_Import0001.fbx` is the rigged variant).

## GAS/combat contract preserved

- `UDTAnimationRouter.CombatAnimations` keys stay identical
  (`AI.CombatTest.Slash`, `AI.CombatTest.FireBolt`, `AI.Death`) — only the
  montage assets swap to retargeted ones. `HitReaction` likewise.
- `UDTAnimNotifyEvent`/`ComboWindow`/`MeleeWindow` notifies ride through the
  retargeted montages (batch retarget preserves notify tracks/slots).
- Melee sweep sockets `weapon_r`/`hand_r`, equipment `AttachSocket="hand_r"`,
  projectile `ReleaseSocket="hand_r"` and health-bar `"head"` are created on
  the Meshy mesh by `DTMeshyEditorLibrary::AddSkeletalSocket`.
- AI: `BP_CombatTest_Controller` + `DA_CombatTest_*` abilities reused as-is.
