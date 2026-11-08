"""Run against the study project; read-only export of referenced asset closure."""
import unreal
import json
import hashlib
import shutil
from pathlib import Path

study = Path(unreal.Paths.project_dir()).resolve()
workspace = study.parent.parent
target = workspace / "apps/ue"
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
registry = unreal.AssetRegistryHelpers.get_asset_registry()
packages = set()
catalog = []

def collect(package):
    package = str(package)
    if not package.startswith("/Game/") or package in packages:
        return
    packages.add(package)
    for dep in registry.get_dependencies(package, unreal.AssetRegistryDependencyOptions(include_hard_package_references=True, include_soft_package_references=True)):
        collect(dep)

for suffix, variant in [("Brown", "study-psf-v3-brown"), ("BlueGray", "study-psf-v3-blue-gray")]:
    assert level.load_level("/Game/SofaStudyV3" + suffix)
    parts = []
    for actor in actors.get_all_level_actors():
        if not isinstance(actor, unreal.StaticMeshActor) or actor.get_actor_label().startswith("Studio "):
            continue
        comp = actor.static_mesh_component
        mesh = comp.static_mesh
        material = comp.get_material(0)
        transform = actor.get_actor_transform()
        loc = transform.translation
        rot = transform.rotation.rotator()
        scale = transform.scale3d
        parts.append({"label":actor.get_actor_label(), "mesh":mesh.get_path_name(), "material":material.get_path_name(), "location":[loc.x,loc.y,loc.z], "rotation":[rot.pitch,rot.yaw,rot.roll], "scale":[scale.x,scale.y,scale.z]})
        collect(mesh.get_path_name().split(".")[0])
        collect(material.get_path_name().split(".")[0])
    assert len(parts) == 36
    catalog.append({"assetVersionId":variant, "suffix":suffix, "dimensionsMm":[1500,870,820], "studyCandidate":True, "parts":parts})

records = []
for package in sorted(packages):
    relative = package.removeprefix("/Game/")
    for extension in [".uasset", ".uexp", ".ubulk"]:
        source = study / "Content" / (relative + extension)
        if not source.exists():
            continue
        dest = target / "Content" / (relative + extension)
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, dest)
        records.append({"package":package, "file":str(dest.relative_to(target)), "sha256":hashlib.sha256(dest.read_bytes()).hexdigest()})
output = {"source":"photo-referenced code model and independently generated UE v3 materials; not calibrated", "catalog":catalog, "copiedAssets":records}
(target / "Tools/catalog-source.json").write_text(json.dumps(output,indent=2),encoding="utf-8")
unreal.log("YOHAKU_ASSETS_EXPORTED " + str(len(records)))
