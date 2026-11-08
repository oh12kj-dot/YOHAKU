import unreal
import json
import time
import hashlib
from pathlib import Path

root = Path(unreal.Paths.project_dir()).resolve()
workspace = root.parent.parent
color = "blue-gray" if "-YohakuBlueGray" in unreal.SystemLibrary.get_command_line() else "brown"
suffix = "BlueGray" if color == "blue-gray" else "Brown"
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert level.load_level("/Game/SofaStudyV3" + suffix), "V3 map missing"
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
cameras = {a.get_actor_label(): a for a in actors.get_all_level_actors() if isinstance(a, unreal.CameraActor)}
views = [("hero", "StudyCamera"), ("front", "FrontCamera"), ("detail", "DetailCamera")]
if color == "blue-gray":
    views = [("hero", "StudyCamera"), ("detail", "DetailCamera")]
commands = ["r.ScreenPercentage 100", "sg.ViewDistanceQuality 3", "sg.AntiAliasingQuality 3", "sg.ShadowQuality 3", "sg.GlobalIlluminationQuality 3", "sg.ReflectionQuality 3", "sg.PostProcessQuality 3", "sg.TextureQuality 3", "r.MotionBlurQuality 0", "r.DepthOfFieldQuality 0", "r.Shadow.Virtual.SMRT.RayCountLocal 24", "r.Shadow.Virtual.SMRT.SamplesPerRayLocal 12", "r.HighResScreenshotDelay 64", "t.MaxFPS 30"]
for command in commands:
    unreal.SystemLibrary.execute_console_command(world, command)
state = {"started":time.monotonic(), "index":0, "frames":0, "requested":False, "records":[]}

def prepare_view():
    name, label = views[state["index"]]
    state["camera"] = cameras[label]
    state["view_started"] = time.monotonic()
    state["view_wall"] = time.time()
    state["view_frames"] = 0
    state["requested"] = False
    state["output"] = workspace / ("artifacts/ue5-sofa-" + color + "-v3" + ("" if name == "hero" else "-" + name) + ".png")
    level.pilot_level_actor(state["camera"])
    unreal.log("YOHAKU_V3_VIEW_WARMUP " + name)

def tick(delta):
    state["frames"] += 1
    state["view_frames"] += 1
    elapsed = time.monotonic() - state["started"]
    if elapsed > 300:
        (root / ("render-error-v3-" + color + ".txt")).write_text("Capture timed out", encoding="utf-8")
        unreal.unregister_slate_post_tick_callback(state["handle"])
        unreal.SystemLibrary.quit_editor()
        return
    delay = 45 if state["index"] == 0 else 16
    if not state["requested"] and time.monotonic() - state["view_started"] > delay and state["view_frames"] > 180:
        state["requested"] = True
        state["task"] = unreal.AutomationLibrary.take_high_res_screenshot(1600, 1200, str(state["output"]), camera=state["camera"], delay=2.0, force_game_view=True)
    output = state["output"]
    if state["requested"] and output.exists() and output.stat().st_size > 10000 and output.stat().st_mtime >= state["view_wall"]:
        state["records"].append({"view":views[state["index"]][0], "output":str(output), "sha256":hashlib.sha256(output.read_bytes()).hexdigest(), "capture_unix":output.stat().st_mtime, "camera":state["camera"].get_actor_label(), "frames":state["view_frames"]})
        state["index"] += 1
        if state["index"] < len(views):
            prepare_view()
            return
        data = {"engine":unreal.SystemLibrary.get_engine_version(), "model_revision":3, "color":color, "captures":state["records"], "elapsed_s":elapsed, "lumen_gi":unreal.SystemLibrary.get_console_variable_int_value("r.DynamicGlobalIlluminationMethod"), "lumen_reflections":unreal.SystemLibrary.get_console_variable_int_value("r.ReflectionMethod"), "hardware_raytracing":unreal.SystemLibrary.get_console_variable_int_value("r.RayTracing"), "shadow_ray_count":unreal.SystemLibrary.get_console_variable_int_value("r.Shadow.Virtual.SMRT.RayCountLocal"), "shadow_samples_per_ray":unreal.SystemLibrary.get_console_variable_int_value("r.Shadow.Virtual.SMRT.SamplesPerRayLocal"), "material":"UE-only newly generated v3 color/local-detail normal/variable roughness", "validation":"reference-guided approximation; no measured PBR or performance acceptance"}
        (root / ("render-evidence-v3-" + color + ".json")).write_text(json.dumps(data, indent=2), encoding="utf-8")
        unreal.log("YOHAKU_V3_RENDER_DONE")
        unreal.unregister_slate_post_tick_callback(state["handle"])
        unreal.SystemLibrary.quit_editor()

prepare_view()
state["handle"] = unreal.register_slate_post_tick_callback(tick)
