"""Retire fixed first-room lighting; runtime now owns per-room preview lights."""
import unreal
import json
from pathlib import Path

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert level.load_level('/Game/RoomEditor')
removed = 0
for actor in actors.get_all_level_actors():
    if isinstance(actor, unreal.RectLight):
        assert actors.destroy_actor(actor)
        removed += 1
assert level.save_current_level()
preview = '/Game/Finishes/M_Preview'
deleted = False
if unreal.EditorAssetLibrary.does_asset_exist(preview):
    refs = unreal.EditorAssetLibrary.find_package_referencers_for_asset(preview, load_assets_to_confirm=True)
    assert not refs, f'Unexpected preview material references: {refs}'
    deleted = unreal.EditorAssetLibrary.delete_asset(preview)
    assert deleted
(Path(unreal.Paths.project_dir())/'Saved/room-environment-migration.json').write_text(
    json.dumps({'removedFixedLights':removed, 'retiredUnusedPreviewMaterial':deleted}), encoding='utf-8')
