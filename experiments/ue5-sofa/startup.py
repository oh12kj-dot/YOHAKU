import unreal
from pathlib import Path

if "-YohakuCapture" in unreal.SystemLibrary.get_command_line():
    command_line = unreal.SystemLibrary.get_command_line()
    filename = "render_study_v3.py" if "-YohakuV3" in command_line else ("render_study_v2.py" if "-YohakuV2" in command_line else "render_study.py")
    script = Path(unreal.Paths.project_dir()) / filename
    exec(compile(script.read_text(encoding="utf-8"), str(script), "exec"))
