"""Create native catalog, runtime finish materials and one clean editor map."""
import unreal
import json
from pathlib import Path
import sys

root = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(root / "Tools"))
from material_usage import ensure_nanite_usage
from runtime_materials import prepare_runtime_materials
prepare_runtime_materials()
source = json.loads((root / "Tools/catalog-source.json").read_text(encoding="utf-8"))
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.MaterialEditingLibrary

for entry in source["catalog"]:
    name = "PSF" + entry["suffix"]
    asset_path = "/Game/Catalog/" + name
    asset = unreal.load_asset(asset_path) if unreal.EditorAssetLibrary.does_asset_exist(asset_path) else None
    if not asset:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.FurnitureDefinition)
        asset = tools.create_asset(name, "/Game/Catalog", unreal.FurnitureDefinition, factory)
    asset.set_editor_property("asset_version_id", entry["assetVersionId"])
    asset.set_editor_property("dimensions_mm", unreal.Vector(1500,870,820))
    asset.set_editor_property("study_candidate", True)
    asset.set_editor_property("provenance", source["source"])
    parts = []
    for record in entry["parts"]:
        mesh = unreal.load_asset(record["mesh"])
        material = unreal.load_asset(record["material"])
        assert mesh and material
        ensure_nanite_usage(material)
        # Retire the imported placeholder at its actual source of truth.
        mesh.set_material(0,material)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        part = unreal.FurnitureMeshPart()
        part.set_editor_property("mesh",mesh)
        part.set_editor_property("material",material)
        transform = unreal.Transform(location=record["location"], rotation=record["rotation"], scale=record["scale"])
        part.set_editor_property("transform",transform)
        parts.append(part)
    asset.set_editor_property("parts",parts)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)

def scalar(mat,value,prop,y):
    node=lib.create_material_expression(mat,unreal.MaterialExpressionConstant,-250,y)
    node.set_editor_property("r",value)
    assert lib.connect_material_property(node,"",prop)

colors={"Oak":(.35,.22,.105),"Walnut":(.12,.065,.035),"WhiteOak":(.53,.46,.34),"Stone":(.26,.29,.28),"Ivory":(.68,.66,.57),"Sage":(.30,.40,.31),"Clay":(.43,.25,.19),"Chalk":(.77,.78,.72),"Slate":(.12,.17,.19)}
for name,rgb in colors.items():
    path="/Game/Finishes/M_"+name
    mat=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset("M_"+name,"/Game/Finishes",unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(mat)
    color=lib.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,-500,0)
    color.set_editor_property("constant",unreal.LinearColor(*rgb,1))
    assert lib.connect_material_property(color,"",unreal.MaterialProperty.MP_BASE_COLOR)
    if name in ["Oak","Walnut","WhiteOak"]:
        # World-space 18cm x 120cm staggered planks; independent of room size.
        world=lib.create_material_expression(mat,unreal.MaterialExpressionWorldPosition,-800,-200)
        wood=lib.create_material_expression(mat,unreal.MaterialExpressionCustom,-300,-100)
        inp=unreal.CustomInput()
        inp.set_editor_property("input_name","Position")
        wood.set_editor_property("inputs",[inp])
        wood.set_editor_property("output_type",unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        wood.set_editor_property("code",f"""
float2 q=Position.xy;
float row=floor(q.x/18.0);
float offset=frac(sin(row*127.1)*43758.5453)*120.0;
float column=floor((q.y+offset)/120.0);
float shade=frac(sin(row*41.13+column*17.27)*18231.17);
float2 uv=frac(float2(q.x/18.0,(q.y+offset)/120.0));
float edge=min(min(uv.x,1.0-uv.x)*18.0,min(uv.y,1.0-uv.y)*120.0);
float seam=1.0-smoothstep(0.035,0.10,edge);
float grain=sin(q.x*14.0+sin(q.y*0.05)*2.0+sin(q.x*1.7+q.y*.02));
return float3({rgb[0]},{rgb[1]},{rgb[2]})*(.86+.20*shade+.035*grain)*lerp(1.0,.52,seam);
""")
        assert lib.connect_material_expressions(world,"",wood,"Position")
        assert lib.connect_material_property(wood,"",unreal.MaterialProperty.MP_BASE_COLOR)
    scalar(mat,.62 if name in ["Oak","Walnut","WhiteOak"] else .9,unreal.MaterialProperty.MP_ROUGHNESS,180)
    lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)

# No studio geometry, startup script, old maps or scene actors are carried into runtime.
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if unreal.EditorAssetLibrary.does_asset_exist("/Game/RoomEditor"):
    assert level.load_level("/Game/RoomEditor")
    for actor in actors.get_all_level_actors():
        actors.destroy_actor(actor)
else:
    assert level.new_level("/Game/RoomEditor")
# Preview lighting is created per room by SceneProjection.
volume=actors.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector())
volume.set_editor_property("unbound",True)
settings=volume.get_editor_property("settings")
for key,value in [("override_auto_exposure_method",True),("auto_exposure_method",unreal.AutoExposureMethod.AEM_MANUAL),("override_auto_exposure_bias",True),("auto_exposure_bias",-6.0),("override_auto_exposure_apply_physical_camera_exposure",True),("auto_exposure_apply_physical_camera_exposure",False)]:settings.set_editor_property(key,value)
volume.set_editor_property("settings",settings)
assert level.save_current_level()
unreal.EditorAssetLibrary.save_directory("/Game",only_if_is_dirty=False,recursive=True)

# Delete now-unreferenced import placeholder materials from this new app only.
removed=[]
registry=unreal.AssetRegistryHelpers.get_asset_registry()
for asset_path in unreal.EditorAssetLibrary.list_assets("/Game/SofaV3",recursive=True,include_folder=False):
    if "/Materials/" in asset_path:
        package=str(asset_path).split(".")[0]
        refs=registry.get_referencers(package,unreal.AssetRegistryDependencyOptions(include_hard_package_references=True,include_soft_package_references=True))
        confirmed=unreal.EditorAssetLibrary.find_package_referencers_for_asset(package,load_assets_to_confirm=True)
        if not confirmed and unreal.EditorAssetLibrary.delete_asset(str(asset_path)):
            removed.append(str(asset_path))
(root/"Tools/native-asset-preparation.json").write_text(json.dumps({"catalogEntries":2,"partsPerEntry":36,"map":"/Game/RoomEditor","removedUnusedPlaceholders":removed},indent=2),encoding="utf-8")
unreal.log("YOHAKU_NATIVE_ASSETS_READY")
