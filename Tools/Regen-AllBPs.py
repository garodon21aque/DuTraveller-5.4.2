import unreal
eal = unreal.EditorAssetLibrary
repair = unreal.DTStructRepairLibrary

count = 0
for package in eal.list_assets("/Game", recursive=True, include_folder=False):
    try:
        ad = eal.find_asset_data(package)
        if 'Blueprint' not in str(ad.asset_class_path.asset_name):
            continue
        bp = ad.get_asset()
        if not bp or not isinstance(bp, unreal.Blueprint):
            continue
        obj_path = "{0}.{1}".format(str(ad.package_name), str(ad.asset_name))
        if repair.regenerate_blueprint_class(obj_path):
            eal.save_loaded_asset(bp)
            count += 1
    except Exception as exc:
        unreal.log_warning("{0}: {1}".format(package, exc))
unreal.log("Regen pass complete, saved {0} assets".format(count))
unreal.SystemLibrary.quit_editor()
