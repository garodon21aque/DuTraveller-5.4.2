import unreal, traceback
out = []
def log(*a): out.append(" ".join(str(x) for x in a))
try:
    bp = unreal.load_asset("/Game/UI/SpellMenu/WBP_EquippedSpell_Row")
    log("bp:", type(bp), bp)
    for attr in ("widget_tree", "WidgetTree", "tree"):
        try:
            v = getattr(bp, attr)
            log("getattr", attr, "->", v)
        except Exception as e:
            log("getattr", attr, "ERR", e)
    for prop in ("widget_tree", "tree"):
        try:
            v = bp.get_editor_property(prop)
            log("gep", prop, "->", v, type(v))
            if v:
                for pa in ("root_widget", "all_widgets", "get_all_widgets"):
                    try:
                        val = getattr(v, pa)
                        log("  wt.", pa, "->", val if not callable(val) else val())
                    except Exception as e:
                        log("  wt.", pa, "ERR", e)
        except Exception as e:
            log("gep", prop, "ERR", e)
    # try the CDO's own members
    try:
        cls = bp.generated_class()
        cdo = unreal.get_default_object(cls)
        log("cdo:", cdo)
        for w in ("widget_tree",):
            try: log("cdo.", w, "->", getattr(cdo, w))
            except Exception as e: log("cdo.", w, "ERR", e)
    except Exception as e:
        log("cdo ERR", e)
except Exception:
    out.append(traceback.format_exc())
unreal.log("DIAG\n" + "\n".join(out))
unreal.SystemLibrary.quit_editor()
