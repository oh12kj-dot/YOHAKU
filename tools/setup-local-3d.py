"""Download and verify the pinned, public local shape checkpoint (no cloud inference)."""
import hashlib
import json
from pathlib import Path
import urllib.request
import os
import sys
import subprocess

root = Path(__file__).resolve().parents[1]
source = json.loads((root / 'tools/local-3d-model.json').read_text(encoding='utf-8-sig'))
record = source['file'][0]
folder = root / 'local-models/hunyuan3d'
folder.mkdir(parents=True, exist_ok=True)
target = folder / 'hunyuan3d-dit-v2_fp16.safetensors'
url = f"https://huggingface.co/Comfy-Org/Hunyuan-3D-2.0/resolve/{source['revision']}/{record['rfilename']}"
if not target.exists():
    temp = target.with_suffix('.part')
    with urllib.request.urlopen(url, timeout=60) as response, temp.open('wb') as out:
        while data := response.read(8 * 1024 * 1024):
            out.write(data)
    assert temp.stat().st_size == record['size']
    with temp.open('rb') as stream:
        digest = hashlib.sha256()
        for block in iter(lambda: stream.read(8 * 1024 * 1024), b''):
            digest.update(block)
    assert digest.hexdigest() == record['lfs']['sha256']
    temp.replace(target)
assert target.stat().st_size == record['size']
with target.open('rb') as stream:
    digest = hashlib.sha256()
    for block in iter(lambda: stream.read(8 * 1024 * 1024), b''):
        digest.update(block)
assert digest.hexdigest() == record['lfs']['sha256']
for name in ['LICENSE', 'NOTICE', 'README.md']:
    urllib.request.urlretrieve(f"https://huggingface.co/Comfy-Org/Hunyuan-3D-2.0/resolve/{source['revision']}/{name}", folder / name)
(folder / 'source.json').write_text(json.dumps(source, indent=2), encoding='utf-8')
packages = root / 'local-runtime/python_packages'
probe = subprocess.run([sys.executable, '-c', f"import sys;sys.path.insert(0,{str(packages)!r});import fast_simplification"], capture_output=True)
if probe.returncode:
    subprocess.run([sys.executable, '-m', 'pip', 'install', '--no-deps', '--target', str(packages), 'fast-simplification==0.2.0'], check=True)
comfy = Path(os.environ.get('YOHAKU_COMFY_ROOT', 'C:/AI/ComfyUI_windows_portable/ComfyUI')).resolve()
assert (comfy/'comfy').is_dir(), 'Set YOHAKU_COMFY_ROOT to the existing ComfyUI directory'
profile = Path(os.environ['LOCALAPPDATA'])/'Yohaku/LocalGeneration'
profile.mkdir(parents=True, exist_ok=True)
(profile/'runtime.json').write_text(json.dumps({'python':sys.executable, 'comfyRoot':str(comfy), 'checkpoint':str(target), 'checkpointSha256':record['lfs']['sha256'], 'packagesRoot':str(packages)}, indent=2), encoding='utf-8')
print('LOCAL_SHAPE_MODEL_VERIFIED', target, flush=True)
