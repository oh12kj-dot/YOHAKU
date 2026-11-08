"""One local photo -> immutable untextured shape draft. No server or paid API."""
import hashlib
import json
import math
from pathlib import Path
import sys
import time
import urllib.request
import re
from local_3d_geometry import clip_collision_parts

job_path = Path(sys.argv[1]).resolve()
profile_path = Path(sys.argv[2]).resolve()
job = json.loads(job_path.read_text(encoding='utf-8-sig'))
profile = json.loads(profile_path.read_text(encoding='utf-8-sig'))
out_dir = job_path.parent


def atomic(name, value):
    target = out_dir / name
    temp = target.with_suffix('.tmp')
    temp.write_text(json.dumps(value, ensure_ascii=False, separators=(',', ':'), allow_nan=False), encoding='utf-8')
    temp.replace(target)


def status(message, state='running'):
    atomic('status.json', {'state': state, 'message': message})


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        return None


def run():
    source = job['source']
    variant = next(v for v in source['variants'] if v['id'] == job['variantId'])
    url = variant['imageUrl']
    assert re.fullmatch(r'https://static2\.flymee\.jp/product_images/[A-Za-z0-9_./-]+', url) and '..' not in url
    assert source['dimensionStatus'] == 'single-source-unverified'
    dims = [source['dimensions'][k] for k in ['widthMm', 'depthMm', 'heightMm']]
    assert all(isinstance(x, (float, int)) and math.isfinite(x) and 0 < x <= 30000 for x in dims)
    status('商品写真を取得しています')
    with urllib.request.build_opener(NoRedirect).open(url, timeout=30) as response:
        assert response.status == 200 and response.headers.get_content_type() in ['image/jpeg', 'image/png', 'image/webp']
        image_bytes = response.read(16 * 1024 * 1024 + 1)
    assert len(image_bytes) <= 16 * 1024 * 1024
    image_hash = hashlib.sha256(image_bytes).hexdigest()
    (out_dir / 'source-image.bin').write_bytes(image_bytes)
    checkpoint = Path(profile['checkpoint'])
    assert hashlib.file_digest(checkpoint.open('rb'), 'sha256').hexdigest() == profile['checkpointSha256']
    comfy_root = Path(profile['comfyRoot'])
    sys.path.insert(0, str(comfy_root))
    sys.path.insert(0, profile['packagesRoot'])
    sys.argv = ['local-shape', '--lowvram', '--reserve-vram', '2.5', '--disable-all-custom-nodes']
    import comfy.options
    comfy.options.enable_args_parsing()
    import torch
    import numpy as np
    import io
    from PIL import Image, ImageOps
    Image.MAX_IMAGE_PIXELS = 16 * 1024 * 1024
    with Image.open(io.BytesIO(image_bytes)) as photo:
        photo = ImageOps.exif_transpose(photo).convert('RGB')
        photo.thumbnail((512, 512))
        canvas = Image.new('RGB', (512, 512), (255, 255, 255))
        canvas.paste(photo, ((512-photo.width)//2, (512-photo.height)//2))
        pixels = torch.from_numpy(np.asarray(canvas).copy().astype(np.float32)/255.0).unsqueeze(0)
    import comfy.sd
    import comfy.model_management as mm
    import nodes
    from comfy_extras.nodes_hunyuan3d import Hunyuan3Dv2Conditioning, EmptyLatentHunyuan3Dv2, VAEDecodeHunyuan3D, VoxelToMesh
    from comfy_extras.nodes_model_advanced import ModelSamplingAuraFlow
    status('ローカル生成モデルを読み込んでいます（形状のみ）')
    with torch.inference_mode():
        loaded = comfy.sd.load_checkpoint_guess_config(str(checkpoint), output_vae=True, output_clip=False, output_clipvision=True)
        model, vision, vae = loaded[0], loaded[3], loaded[2]
        assert vision is not None and vae is not None
        encoded = nodes.CLIPVisionEncode().encode(vision, pixels, 'center')[0]
        positive, negative = Hunyuan3Dv2Conditioning.execute(encoded).result
        latent = EmptyLatentHunyuan3Dv2.execute(3072, 1)[0]
        model = ModelSamplingAuraFlow().patch_aura(model, 1.0)[0]
        status('写真から3D形状を生成しています（素材は未生成）')
        samples = nodes.KSampler().sample(model, int(job.get('seed', 42)), 20, 8.0, 'euler', 'normal', positive, negative, latent)[0]
        status('生成形状をメッシュへ変換しています')
        voxel = VAEDecodeHunyuan3D.execute(vae, samples, 8000, 256)[0]
        mesh = VoxelToMesh.execute(voxel, 'surface net', .6)[0]
        vertices = mesh.vertices[0].cpu().numpy().astype(np.float64)
        faces = mesh.faces[0].cpu().numpy().astype(np.int64)
        binary = torch.nn.functional.adaptive_max_pool3d((voxel.data[0] > .6).float()[None,None], (16,16,16))[0,0].cpu().numpy() > 0
        mm.unload_all_models()
    atomic('raw-mesh-statistics.json', {'vertices':int(vertices.shape[0]), 'faces':int(faces.shape[0]), 'finite':bool(np.isfinite(vertices).all())})
    np.savez(out_dir/'raw-shape.npz', vertices=vertices, faces=faces, binary=binary)
    if len(faces) > 50000:
        import fast_simplification
        vertices, faces = fast_simplification.simplify(vertices, faces, target_count=50000)
    assert vertices.shape[0] >= 100 and vertices.shape[0] <= 150000 and faces.shape[0] <= 300000
    assert np.isfinite(vertices).all() and faces.min() >= 0 and faces.max() < len(vertices)
    vertices = vertices[:, [0,2,1]] * np.array([1,-1,1])
    low, high = vertices.min(axis=0), vertices.max(axis=0)
    assert ((high-low) > .01).all()
    scale = np.asarray(dims)/(high-low)
    origin = np.array([(low[0]+high[0])/2, (low[1]+high[1])/2, low[2]])
    vertices = (vertices-origin)*scale
    a,b,c = vertices[faces[:,0]],vertices[faces[:,1]],vertices[faces[:,2]]
    faces = faces[np.linalg.norm(np.cross(b-a,c-a), axis=1) > 1e-6]
    assert len(faces) >= 100
    parts = []
    for k in range(16):
        for j in range(16):
            i = 0
            while i < 16:
                if not binary[k,j,i]:
                    i += 1
                    continue
                start = i
                while i < 16 and binary[k,j,i]:
                    i += 1
                center = np.array([-1+(start+i)/16, 1-(k+.5)*2/16, -1+(j+.5)*2/16])
                size = np.array([(i-start)*2/16, 2/16, 2/16])*scale
                parts.append({'center': ((center-origin)*scale).round(4).tolist(), 'size': size.round(4).tolist()})
    parts = clip_collision_parts(parts, dims)
    assert 0 < len(parts) <= 2048
    atomic('mesh.json', {'version':1, 'units':'mm-x-depth-height', 'vertices':vertices.round(4).tolist(), 'triangles':faces.reshape(-1).tolist()})
    mesh_hash = hashlib.sha256((out_dir/'mesh.json').read_bytes()).hexdigest()
    manifest = {'format':'yohaku-generated-study', 'version':1, 'id':job['assetId'], 'name':source['name']+' / '+variant['name'],
                'candidateId':job['candidateId'], 'source':source, 'variantId':variant['id'], 'imageUrl':url, 'imageHash':image_hash,
                'meshHash':mesh_hash, 'checkpointHash':profile['checkpointSha256'], 'generatedAt':time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
                'sizeMm':dims, 'dimensionStatus':'single-source-unverified', 'materialStatus':'not-generated',
                'qualityStatus':'unadopted-study', 'inferred':['back-and-hidden-surfaces','front-direction','single-image-shape'],
                'coordinates':'mm-x-depth-height-centered-floor', 'parts':parts,
                'settings':{'steps':20,'seed':int(job.get('seed',42)), 'octree':256, 'collisionGrid':16, 'meshTargetFaces':50000}}
    atomic('manifest.json', manifest)
    status('3D形状の試作を作成しました。掲載値に合わせた無地形状・寸法と材質は未照合です。', 'completed')


try:
    run()
except Exception as error:
    status('ローカル生成に失敗しました。既存の家具・部屋は保持しています。', 'failed')
    (out_dir/'error.txt').write_text(repr(error), encoding='utf-8')
    raise
