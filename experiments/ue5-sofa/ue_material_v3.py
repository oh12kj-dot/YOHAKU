"""Independent native UE materials. No Web material/texture references."""
import unreal
import hashlib

lib = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

def scalar(mat, value, prop, y):
    e = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -260, y)
    e.set_editor_property("r", value)
    assert lib.connect_material_property(e, "", prop)

def make(name, rgb=None, roughness=.8, metallic=0):
    mat = unreal.load_asset("/Game/UEFabricV3/" + name)
    if not mat:
        mat = tools.create_asset(name, "/Game/UEFabricV3", unreal.Material, unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(mat)
    if rgb:
        e = lib.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -260, 0)
        e.set_editor_property("constant", unreal.LinearColor(*rgb, 1))
        assert lib.connect_material_property(e, "", unreal.MaterialProperty.MP_BASE_COLOR)
    scalar(mat, roughness, unreal.MaterialProperty.MP_ROUGHNESS, 180)
    scalar(mat, metallic, unreal.MaterialProperty.MP_METALLIC, 300)
    scalar(mat, .42, unreal.MaterialProperty.MP_SPECULAR, 420)
    return mat

def apply_materials(root, color, actors):
    suffix = "BlueGray" if color == "blue-gray" else "Brown"
    textures = {}
    records = []
    for kind in ["basecolor", "normal", "roughness"]:
        source = root / ("material-source-v3/" + color + "-" + kind + ".png")
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(source))
        task.set_editor_property("destination_path", "/Game/UEFabricV3/Textures/" + suffix)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        tools.import_asset_tasks([task])
        tex = unreal.load_asset(task.get_editor_property("imported_object_paths")[0])
        tex.set_editor_property("srgb", kind == "basecolor")
        if kind == "normal":
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property("flip_green_channel", False)
        elif kind == "roughness":
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
        tex.set_editor_property("address_y", unreal.TextureAddress.TA_WRAP)
        tex.set_editor_property("power_of_two_mode", unreal.TexturePowerOfTwoSetting.STRETCH_TO_POWER_OF_TWO)
        unreal.EditorAssetLibrary.save_loaded_asset(tex)
        textures[kind] = tex
        records.append({"kind":kind, "source":str(source), "sha256":hashlib.sha256(source.read_bytes()).hexdigest(), "asset":tex.get_path_name(), "srgb":tex.get_editor_property("srgb"), "compression":str(tex.get_editor_property("compression_settings")), "power_of_two_mode":str(tex.get_editor_property("power_of_two_mode"))})
    mat = make("M_PSF_UE_" + suffix)
    for y, kind, prop, pin, sampler in [
        (0, "basecolor", unreal.MaterialProperty.MP_BASE_COLOR, "RGB", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR),
        (220, "normal", unreal.MaterialProperty.MP_NORMAL, "RGB", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL),
        (440, "roughness", unreal.MaterialProperty.MP_ROUGHNESS, "R", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS),
    ]:
        e = lib.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -600, y)
        e.set_editor_property("texture", textures[kind])
        e.set_editor_property("sampler_type", sampler)
        if kind == "basecolor":
            # Visual tuning against the product photo under this studio's lighting.
            # Independent UE-only factor, not a measured material albedo calibration.
            multiply = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -260, -100)
            multiply.set_editor_property("const_b", .72 if color == "brown" else .5)
            assert lib.connect_material_expressions(e, "RGB", multiply, "A")
            assert lib.connect_material_property(multiply, "", prop)
        else:
            assert lib.connect_material_property(e, pin, prop)
    lib.recompile_material(mat)
    mats = {
        "Upholstery":mat,
        "Stitch":make("M_Stitch_"+suffix, [.14,.077,.037] if color=="brown" else [.055,.065,.075], .96),
        "Leg":make("M_Leg_BlackPowdercoat", [.009,.011,.012], .46, .25),
        "PowerPlate":make("M_PowerPlate_MattePolymer", [.014,.016,.017], .76),
        "Socket":make("M_Socket_DarkPolymer", [.035,.038,.039], .65),
        "USB":make("M_USB_BrushedMetal", [.35,.37,.38], .32, .85),
        "Rubber":make("M_Foot_Rubber", [.006,.007,.007], .97),
        "Cavity":make("M_Port_Interior", [.003,.004,.004], .98),
    }
    for m in mats.values():
        lib.recompile_material(m)
        unreal.EditorAssetLibrary.save_loaded_asset(m)
    assigned = []
    for actor in actors:
        comp = actor.static_mesh_component
        for index, slot in enumerate(comp.static_mesh.get_editor_property("static_materials")):
            imported = slot.get_editor_property("material_interface")
            name = imported.get_name()
            key = next((k for k in mats if "UE_SLOT_"+k in name), None)
            assert key, "Unexpected material slot: " + name
            label = actor.get_actor_label()
            if "power-panel" in label:
                key = "PowerPlate"
            elif "rubber-foot" in label:
                key = "Rubber"
            elif "socket-slot" in label or "usb-port" in label:
                key = "Cavity"
            comp.set_material(index, mats[key])
            assigned.append({"actor":actor.get_actor_label(), "slot":index, "material":comp.get_material(index).get_path_name()})
    assert sum("M_PSF_UE_" in a["material"] for a in assigned) >= 6
    return {"new_materials":True, "web_textures_used":False, "basecolor_gain":.72 if color == "brown" else .5, "texture_records":records, "assignments":assigned, "validation":"new reference-guided generated albedo and derived maps; studio-specific visual tuning, not measured PBR"}

