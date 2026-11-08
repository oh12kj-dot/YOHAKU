"""Materials for generated geometry drafts and architectural glass, not photo textures."""
import unreal


def prepare_runtime_materials():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    lib = unreal.MaterialEditingLibrary
    for name, color, roughness, opacity in [
        ('GeneratedStudy', (.52, .52, .52), .72, 1),
        ('WindowGlass', (.72, .88, .95), .15, .22),
    ]:
        path = '/Game/Finishes/M_' + name
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            continue
        mat = tools.create_asset('M_'+name, '/Game/Finishes', unreal.Material, unreal.MaterialFactoryNew())
        mat.set_editor_property('two_sided', True)
        rgb = lib.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -300, 0)
        rgb.set_editor_property('constant', unreal.LinearColor(*color, 1))
        assert lib.connect_material_property(rgb, '', unreal.MaterialProperty.MP_BASE_COLOR)
        for value, prop, y in [(roughness, unreal.MaterialProperty.MP_ROUGHNESS, 120), (opacity, unreal.MaterialProperty.MP_OPACITY, 240)]:
            node = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, y)
            node.set_editor_property('r', value)
            assert lib.connect_material_property(node, '', prop)
        if opacity < 1:
            mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
        lib.recompile_material(mat)
        assert unreal.EditorAssetLibrary.save_loaded_asset(mat)


if __name__ == '__main__':
    prepare_runtime_materials()
