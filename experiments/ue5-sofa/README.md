# UE5 sofa study — proportions and upholstery v3

Updated 2026-11-08 JST. Current version is v3: thinner 130mm base, thicker approximately 164mm seat cushion, fuller asymmetric back cushions, shallower arm shoulder, finer seams and newly generated UE-only upholstery. Overall 1500x870x820mm, seat height410mm and floor origin are preserved. Component thicknesses are photo estimates, not measured product specifications. [Current changes, prompts and source assets](../../docs/UE5モデル改修_v3.md).

## Current v3 reproduction

Use the same commands below with prepare_v3.mjs, build_scene_v3.py, -YohakuV3 and verify_v3.cjs. Native geometry source is sofa-model-v3.ts; Web source is unchanged. Normal startup opens /Game/SofaStudyV3Brown. Blue-gray is /Game/SofaStudyV3BlueGray. Materials are /Game/UEFabricV3. Source textures/geometry/hashes are in material-source-v3. Scene/render evidence uses -v3- filenames. Actual captures are artifacts/ue5-sofa-brown-v3.png, -v3-front.png, -v3-detail.png and blue-gray-v3.png/-v3-detail.png. The capture script takes multiple camera views sequentially and exits only after fresh files exist. Source generation and UE import both assert the cushion/base thickness ratio >1.2 and seat height410mm. verify_v3.cjs also checks each image hash/time/dimensions and independent textures. VSM local shadow tracing uses 24 rays with12 samples, read back in render evidence. Photorealism and real-time performance acceptance remain unverified.

## Retained v2 reproduction and evidence

## Current files

- `YohakuStudy.uproject`: native project. Retained v2 maps are `/Game/SofaStudyV2Brown` and `/Game/SofaStudyV2BlueGray`; the original map is also retained.
- `material-source-v2/`: newly generated brown/blue-gray base colors, derived DirectX normals and linear roughness; hashes in `provenance.json`.
- `prepare_v2.cjs`: extracts only v2 geometry/accessor data, strips all Web images/material settings, derives technical PBR maps from the new UE images. Geometry-only GLB contains no images/textures.
- `ue_material_v2.py`: creates `/Game/UEFabricV2` native materials and assigns all 36 sofa mesh actors. New stitches, powder-coated legs, polymer power panel, rubber feet, port interiors and USB metal. Base color is sRGB; normals TC_NORMALMAP/linear/DirectX; roughness TC_MASKS/linear. Color gains .72 brown/.5 blue-gray are visual studio tuning, not calibrated measurements.
- `build_scene_v2.py`: import, material assignment, bounds assertions, studio/camera setup, save.
- `scene-evidence-v2-*.json`: geometry hash, 150x87x82cm bounds/floor Z=0, texture hashes/settings, actual material assignments.
- `render_study_v2.py`: camera capture after warmup, fresh-file check, finite timeout and automatic editor exit.
- `render-evidence-v2-*.json`: actual render settings and capture timings.
- `../../artifacts/ue5-sofa-brown-v2.png`, `ue5-sofa-blue-gray-v2.png`: final native captures, 1600x1200.
- `verify_v2.cjs`: geometry-only/no-Web-images invariant, native assignments, hashes, dimensions/floor, texture settings and capture dimensions.
- [Full generation prompts and saved assets](../../docs/UE5専用素材_v2.md).

Original SofaStudy map, /Game/Sofa assets, v1 scripts/evidence and ue5-sofa-brown.png are retained. Browser app/catalog/scene adoption are unchanged.

## Reproduce

From the workspace root, with Node dependencies installed:

```powershell
node experiments/ue5-sofa/prepare_v2.cjs
$ueBin = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64'
$studyProject = 'C:/AI/App_Dev/YOHAKU/experiments/ue5-sofa/YohakuStudy.uproject'
$studyBuild = 'C:/AI/App_Dev/YOHAKU/experiments/ue5-sofa/build_scene_v2.py'
& "$ueBin/UnrealEditor-Cmd.exe" $studyProject -run=pythonscript "-script=$studyBuild" -NullRHI -unattended -nosplash -nosound -NoSourceControl -NoLiveCoding
& "$ueBin/UnrealEditor-Cmd.exe" $studyProject -run=pythonscript "-script=$studyBuild" -YohakuBlueGray -NullRHI -unattended -nosplash -nosound -NoSourceControl -NoLiveCoding
& "$ueBin/UnrealEditor.exe" $studyProject -YohakuCapture -YohakuV2 -RenderOffscreen -unattended -nosplash -nosound -NoSourceControl -NoLiveCoding -windowed -ResX=1600 -ResY=1200
& "$ueBin/UnrealEditor.exe" $studyProject -YohakuCapture -YohakuV2 -YohakuBlueGray -RenderOffscreen -unattended -nosplash -nosound -NoSourceControl -NoLiveCoding -windowed -ResX=1600 -ResY=1200
node experiments/ue5-sofa/verify_v2.cjs
```

Run project operations sequentially. Only explicit -YohakuCapture triggers capture/quit. V1 capture remains available without -YohakuV2. No paid external 3D API, marketplace content or engine installation changes.

## Verification limits

Both v2 imports/captures and verify_v2.cjs passed. Images were visually inspected. Verification also rejects captures older than the latest material build; this negative check was exercised before the final blue-gray recapture. No reported material compilation failures. Interchange warned about degenerate triangles in the two small socket-well meshes; all 36 actors and expected bounds were retained. This is not production-asset acceptance.

One blue-gray recapture exited with DXGI_ERROR_DEVICE_HUNG / GPU page fault while using 2892MB of a 7181MB local memory budget. Its log is retained as capture-v2-blue-gray-gpu-error.log; the cause is unproven. A retry with identical settings completed and the final fresh image passed verification. This is not a stability qualification.

DX12/SM6, software Lumen GI/reflections, Virtual Shadow Maps, three softboxes, manual exposure -6, physical-camera exposure disabled. Shadow/contact grain remains despite 64-frame capture delay. Lighting/exposure/background differ from Three.js; this is not a controlled engine comparison. Color and irregular wrinkles are approximate. No performance or photorealism acceptance is claimed.
