"""Reparents regenerated Blueprint assets to corrected native parents and saves
them. Used when a first-pass placeholder parent is replaced by the reconstructed
contract class.
"""

import unreal


REPARENTS = (
    ("/Game/Blueprints/Quest/BP_QuestGiver", "/Script/DuTraveller.RPGQuestGiver"),
)


def main():
    for object_path, parent_path in REPARENTS:
        blueprint = unreal.load_asset(object_path)
        if not blueprint:
            raise RuntimeError("Unable to load {0}".format(object_path))
        parent_class = unreal.load_class(None, parent_path)
        if not parent_class:
            raise RuntimeError("Unable to load parent {0}".format(parent_path))
        unreal.BlueprintEditorLibrary.reparent_blueprint(blueprint, parent_class)
        unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)
        unreal.log("Reparented {0} -> {1}".format(object_path, parent_path))


try:
    main()
except Exception as exc:  # noqa: BLE001
    unreal.log_error("Reparent-BlueprintAssets failed: {0}".format(exc))
    raise
finally:
    unreal.SystemLibrary.quit_editor()
