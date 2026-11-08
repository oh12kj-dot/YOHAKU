import unreal
import json
import hashlib
import math
from pathlib import Path

root = Path(unreal.Paths.project_dir()).resolve()
workspace = root.parent.parent
source = root / "material-source-v3/sofa-geometry-only.glb"
color = "blue-gray" if "-YohakuBlueGray" in unreal.SystemLibrary.get_command_line() else "brown"
suffix = "BlueGray" if color == "blue-gray" else "Brown"
map_path = "/Game/SofaStudyV3" + suffix
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if unreal.EditorAssetLibrary.does_asset_exist(map_path):
    assert level.load_level(map_path), "Cannot load isolated study map"
    for actor in actors.get_all_level_actors():
        actors.destroy_actor(actor)
else:
    assert level.new_level(map_path), "Cannot create isolated study map"
manager = unreal.InterchangeManager.get_interchange_manager_scripted()
params = unreal.ImportAssetParameters()
params.set_editor_property("is_automated", True)
params.set_editor_property("replace_existing", True)
ok = manager.import_scene("/Game/SofaV3/" + suffix, unreal.InterchangeManager.create_source_data(str(source)), params)
assert ok, "GLB scene import failed"
furniture = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.StaticMeshActor)]
assert len(furniture) >= 10, "Expected detailed sofa meshes"
import sys
sys.path.insert(0, str(root))
from ue_material_v3 import apply_materials
material_evidence = apply_materials(root, color, furniture)
records = []
for a in furniture:
    center, extent = a.get_actor_bounds(False)
    records.append({"label": a.get_actor_label(), "center_cm": [center.x, center.y, center.z], "extent_cm": [extent.x, extent.y, extent.z]})
minimum = [min(r["center_cm"][i] - r["extent_cm"][i] for r in records) for i in range(3)]
maximum = [max(r["center_cm"][i] + r["extent_cm"][i] for r in records) for i in range(3)]
size = [maximum[i] - minimum[i] for i in range(3)]
assert abs(size[2] - 82) < 0.2 and abs(minimum[2]) < 0.2, f"Unexpected imported height/floor: {minimum} {maximum}"
assert sorted(round(v, 1) for v in size[:2]) == [87.0, 150.0], f"Unexpected footprint: {size}"
socket = next(r for r in records if "power" in r["label"].lower())
length = math.hypot(socket["center_cm"][0], socket["center_cm"][1])
front = unreal.Vector(socket["center_cm"][0] / length, socket["center_cm"][1] / length, 0)
right = unreal.Vector(front.y, -front.x, 0)
up = unreal.Vector(0, 0, 1)

def material(name, color, roughness):
    existing = unreal.load_asset("/Game/Studio/" + name)
    if existing:
        return existing
    factory = unreal.MaterialFactoryNew()
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, "/Game/Studio", unreal.Material, factory)
    assert mat, "Cannot create studio material"
    c = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -250, 0)
    c.set_editor_property("constant", unreal.LinearColor(*color, 1))
    unreal.MaterialEditingLibrary.connect_material_property(c, "", unreal.MaterialProperty.MP_BASE_COLOR)
    r = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant, -250, 200)
    r.set_editor_property("r", roughness)
    unreal.MaterialEditingLibrary.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    return mat

floor_mat = material("StudioFloor", [0.38, 0.34, 0.28], 0.88)
wall_mat = material("StudioWall", [0.68, 0.65, 0.59], 0.95)
cube = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
yaw = math.degrees(math.atan2(right.y, right.x))
def box(name, position, scale, mat):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, position, unreal.Rotator(0, yaw, 0))
    actor.set_actor_label(name)
    actor.static_mesh_component.set_static_mesh(cube)
    actor.static_mesh_component.set_material(0, mat)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor
box("Studio floor", unreal.Vector(0, 0, -0.5), [8, 8, 0.01], floor_mat)
box("Studio rear wall", front * -220 + up * 150, [8, 0.1, 3], wall_mat)
box("Studio side wall", right * -300 + up * 150, [0.1, 8, 3], wall_mat)
target = unreal.Vector(0, 0, 40)
def rect(name, position, intensity, width, height, temperature):
    actor = actors.spawn_actor_from_class(unreal.RectLight, position)
    actor.set_actor_label(name)
    actor.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(position, target), False)
    light = actor.get_component_by_class(unreal.RectLightComponent)
    light.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    light.set_editor_property("intensity_units", unreal.LightUnits.LUMENS)
    light.set_intensity(intensity)
    light.set_editor_property("source_width", width)
    light.set_editor_property("source_height", height)
    light.set_editor_property("attenuation_radius", 1200)
    light.set_editor_property("use_temperature", True)
    light.set_editor_property("temperature", temperature)
    return actor
rect("Softbox key", front * 140 - right * 170 + up * 250, 4500, 160, 120, 5500)
rect("Softbox fill", front * 200 + right * 180 + up * 150, 1700, 180, 160, 6500)
rect("Softbox top rim", front * -90 + right * 80 + up * 250, 2200, 130, 80, 6200)
camera_location = front * 270 + right * 190 + up * 130
camera = actors.spawn_actor_from_class(unreal.CameraActor, camera_location, unreal.MathLibrary.find_look_at_rotation(camera_location, target))
camera.set_actor_label("StudyCamera")
component = camera.get_component_by_class(unreal.CameraComponent)
component.set_editor_property("field_of_view", 43.0)
component.set_editor_property("aspect_ratio", 4 / 3)
settings = component.get_editor_property("post_process_settings")
settings.set_editor_property("override_auto_exposure_method", True)
settings.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
settings.set_editor_property("override_auto_exposure_bias", True)
settings.set_editor_property("auto_exposure_bias", -6.0)
settings.set_editor_property("override_auto_exposure_apply_physical_camera_exposure", True)
settings.set_editor_property("auto_exposure_apply_physical_camera_exposure", False)
component.set_editor_property("post_process_settings", settings)
for name, position, focus, fov in [
    ("DetailCamera", front * 110 + right * 60 + up * 75, right * 22 + up * 43, 39),
    ("FrontCamera", front * 330 + up * 69, up * 41, 35),
]:
    extra = actors.spawn_actor_from_class(unreal.CameraActor, position, unreal.MathLibrary.find_look_at_rotation(position, focus))
    extra.set_actor_label(name)
    extra_component = extra.get_component_by_class(unreal.CameraComponent)
    extra_component.set_editor_property("field_of_view", fov)
    extra_component.set_editor_property("aspect_ratio", 4 / 3)
    extra_component.set_editor_property("post_process_settings", settings)
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(camera_location, camera.get_actor_rotation())
assert level.save_current_level()
unreal.EditorAssetLibrary.save_directory("/Game", only_if_is_dirty=False, recursive=True)
data = {"engine": unreal.SystemLibrary.get_engine_version(), "source": str(source), "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(), "source_provenance": "photo-referenced-code-model; unvalidated", "bounds_cm": {"min": minimum, "max": maximum, "size": size}, "mesh_actors": records, "camera_cm": [camera_location.x, camera_location.y, camera_location.z], "lighting": "three movable rectangular softboxes; Lumen software GI; manual exposure compensation -6 stops with physical camera disabled", "ready_for_production": False}
data["model_revision"] = 3
data["material"] = material_evidence
data["map"] = map_path
base_record = next(r for r in records if r["label"] == "upholstered-base")
seat_record = next(r for r in records if r["label"] == "single-seat-cushion")
assert abs(base_record["extent_cm"][2] * 2 - 13) < .01
assert 15.8 < seat_record["extent_cm"][2] * 2 < 16.7
assert seat_record["extent_cm"][2] > base_record["extent_cm"][2] * 1.2
data["component_proportions_cm"] = {"base":base_record["extent_cm"][2]*2, "seat":seat_record["extent_cm"][2]*2, "source":"photo estimates, not measured dimensions"}
(root / ("scene-evidence-v3-" + color + ".json")).write_text(json.dumps(data, indent=2), encoding="utf-8")
unreal.log("YOHAKU_SCENE_BUILT")

