"""Read-only catalog/material audit, run in the native editor commandlet."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
source = json.loads((root / "Tools/catalog-source.json").read_text(encoding="utf-8"))
materials = {}
meshes = {}
for entry in source["catalog"]:
    for part in entry["parts"]:
        mesh = unreal.load_asset(part["mesh"])
        material = unreal.load_asset(part["material"])
        assert mesh and material
        materials[part["material"]] = {
            "usedWithNanite": material.get_editor_property("used_with_nanite"),
            "automaticUsage": material.get_editor_property("automatically_set_usage_in_editor"),
        }
        meshes[part["mesh"]] = {
            "naniteEnabled": mesh.get_editor_property("nanite_settings").get_editor_property("enabled"),
            "materialMatches": mesh.get_material(0) == material,
        }
output = {"materials": materials, "meshes": meshes,
          "allNaniteMaterialsReady": all(m["usedWithNanite"] for m in materials.values()),
          "allAssignmentsMatch": all(m["materialMatches"] for m in meshes.values())}
(root / "Saved/material-audit.json").write_text(json.dumps(output, indent=2), encoding="utf-8")
unreal.log("YOHAKU_MATERIAL_AUDIT_COMPLETE")
assert output["allNaniteMaterialsReady"], "Furniture material lacks persisted Nanite usage"
assert output["allAssignmentsMatch"], "Furniture material assignment differs from catalog"
