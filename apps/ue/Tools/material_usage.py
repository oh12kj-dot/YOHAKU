"""Persist the shader usage required by the native Nanite furniture meshes."""
import json
from pathlib import Path
import unreal


def ensure_nanite_usage(material):
    if material.get_editor_property("used_with_nanite"):
        return False
    material.set_editor_property("used_with_nanite", True)
    unreal.MaterialEditingLibrary.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material)
    return True


if __name__ == "__main__":
    root = Path(unreal.Paths.project_dir()).resolve()
    source = json.loads((root / "Tools/catalog-source.json").read_text(encoding="utf-8"))
    paths = sorted({p["material"] for e in source["catalog"] for p in e["parts"]})
    changed = []
    for path in paths:
        material = unreal.load_asset(path)
        assert material
        if ensure_nanite_usage(material):
            changed.append(path)
    (root / "Saved/material-usage-preparation.json").write_text(
        json.dumps({"checked": len(paths), "changed": changed}, indent=2), encoding="utf-8")
    unreal.log("YOHAKU_MATERIAL_USAGE_READY")
