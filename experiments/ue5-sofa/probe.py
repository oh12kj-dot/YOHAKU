import unreal
import json
from pathlib import Path

root = Path(unreal.Paths.project_dir()).resolve()
data = {"engine_version": unreal.SystemLibrary.get_engine_version()}
for cls, names in {
    "AutomationLibrary": ["take_high_res_screenshot"],
    "InterchangeManager": ["import_scene", "import_scene_async", "import_asset"],
    "ImportAssetParameters": [],
    "EditorLevelLibrary": ["set_level_viewport_camera_info", "pilot_level_actor"],
    "EditorActorSubsystem": ["spawn_actor_from_class"],
    "LevelEditorSubsystem": ["new_level", "save_current_level"],
    "MoviePipelinePIEExecutor": []
}.items():
    obj = getattr(unreal, cls, None)
    data[cls] = {"exists": obj is not None}
    if obj:
        data[cls]["doc"] = str(obj.__doc__)[:5000]
        for name in names:
            method = getattr(obj, name, None)
            data[cls][name] = str(method.__doc__) if method else None
for name in ["register_slate_post_tick_callback", "unregister_slate_post_tick_callback"]:
    data[name] = str(getattr(unreal, name).__doc__)
(root / "probe.json").write_text(json.dumps(data, indent=2), encoding="utf-8")
unreal.log("YOHAKU_PROBE_DONE")
