import unreal
import json
import time
from pathlib import Path

root = Path(unreal.Paths.project_dir()).resolve()
workspace = root.parent.parent
output = workspace / "artifacts/ue5-sofa-brown.png"
state = {"started": time.monotonic(), "started_wall": time.time(), "requested": False, "task": None, "frames": 0}
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
camera = next((a for a in actors.get_all_level_actors() if a.get_actor_label() == "StudyCamera"), None)
assert camera, "StudyCamera is missing. Build the study scene first."
component = camera.get_component_by_class(unreal.CameraComponent)
settings = component.get_editor_property("post_process_settings")
settings.set_editor_property("override_auto_exposure_method", True)
settings.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
settings.set_editor_property("override_auto_exposure_bias", True)
settings.set_editor_property("auto_exposure_bias", -6.0)
settings.set_editor_property("override_auto_exposure_apply_physical_camera_exposure", True)
settings.set_editor_property("auto_exposure_apply_physical_camera_exposure", False)
component.set_editor_property("post_process_settings", settings)
level.save_current_level()
level.pilot_level_actor(camera)
for command in ["r.ScreenPercentage 100", "sg.ViewDistanceQuality 3", "sg.AntiAliasingQuality 3", "sg.ShadowQuality 3", "sg.GlobalIlluminationQuality 3", "sg.ReflectionQuality 3", "sg.PostProcessQuality 3", "sg.TextureQuality 3", "r.MotionBlurQuality 0", "r.DepthOfFieldQuality 0", "r.HighResScreenshotDelay 64", "t.MaxFPS 30"]:
    unreal.SystemLibrary.execute_console_command(world, command)
unreal.log("YOHAKU_RENDER_WARMUP")

def tick(delta):
    state["frames"] += 1
    elapsed = time.monotonic() - state["started"]
    if elapsed > 180:
        (root / "render-error.txt").write_text("Timed out waiting for screenshot", encoding="utf-8")
        unreal.unregister_slate_post_tick_callback(state["handle"])
        unreal.SystemLibrary.quit_editor()
        return
    if not state["requested"] and elapsed > 45 and state["frames"] > 300:
        state["requested"] = True
        state["task"] = unreal.AutomationLibrary.take_high_res_screenshot(1600, 1200, str(output), camera=camera, delay=2.0, force_game_view=True)
        unreal.log("YOHAKU_SCREENSHOT_REQUESTED")
    if state["requested"] and output.exists() and output.stat().st_size > 10000 and output.stat().st_mtime >= state["started_wall"]:
        data = {"engine": unreal.SystemLibrary.get_engine_version(), "output": str(output), "frames": state["frames"], "elapsed_after_level_ready_s": elapsed, "exposure": "manual; physical camera disabled; compensation -6 stops", "lumen_gi": unreal.SystemLibrary.get_console_variable_int_value("r.DynamicGlobalIlluminationMethod"), "lumen_reflections": unreal.SystemLibrary.get_console_variable_int_value("r.ReflectionMethod"), "hardware_raytracing": unreal.SystemLibrary.get_console_variable_int_value("r.RayTracing"), "source_bump_extension": "EXT_materials_bump unsupported by UE import; not treated as faithfully reproduced", "validation": "render experiment only; not performance or photorealism acceptance"}
        (root / "render-evidence.json").write_text(json.dumps(data, indent=2), encoding="utf-8")
        unreal.log("YOHAKU_RENDER_DONE")
        unreal.unregister_slate_post_tick_callback(state["handle"])
        unreal.SystemLibrary.quit_editor()

state["handle"] = unreal.register_slate_post_tick_callback(tick)
