const fs = require('node:fs');
const assert = require('node:assert/strict');
const path = require('node:path');
const crypto = require('node:crypto');
const read = p => JSON.parse(fs.readFileSync(path.join(__dirname,p)));
const provenance = read('material-source-v2/provenance.json');
assert.equal(provenance.sourceWebTexturesRemoved,true);
const b = fs.readFileSync(path.join(__dirname,'material-source-v2/sofa-geometry-only.glb'));
const mesh = JSON.parse(b.subarray(20,20+b.readUInt32LE(12)));
assert.equal(mesh.images,undefined);assert.equal(mesh.textures,undefined);
assert(mesh.materials.every(m=>!m.pbrMetallicRoughness.baseColorTexture));
for(const color of ['brown','blue-gray']){
  const e=read(`scene-evidence-v2-${color}.json`);
  assert.equal(e.source_sha256,provenance.geometryOnlySha256);
  assert.equal(e.mesh_actors.length,36);
  assert.equal(e.model_revision,2);
  assert.equal(e.material.web_textures_used,false);
  assert.equal(e.material.basecolor_gain,color==='brown'?.72:.5);
  assert.equal(e.material.assignments.length,36);
  assert(e.material.assignments.every(a=>a.material.startsWith('/Game/UEFabricV2/')));
  for(let i=0;i<3;i++)assert(Math.abs(e.bounds_cm.size[i]-[150,87,82][i])<.002);
  assert(Math.abs(e.bounds_cm.min[2])<.002);
  assert.equal(e.material.texture_records.length,3);
  for(const t of e.material.texture_records){
    const raw=fs.readFileSync(t.source);
    assert.equal(crypto.createHash('sha256').update(raw).digest('hex'),t.sha256);
    assert.equal(t.srgb,t.kind==='basecolor');
  }
  const web=fs.readFileSync(path.join(__dirname,`../../artifacts/materials/web-study-v2/${color}-albedo.png`));
  assert.notEqual(crypto.createHash('sha256').update(web).digest('hex'),provenance.colors[color].baseColorSha256);
  const render=read(`render-evidence-v2-${color}.json`);
  const sceneTime=fs.statSync(path.join(__dirname,`scene-evidence-v2-${color}.json`)).mtimeMs;
  assert(fs.statSync(render.output).mtimeMs>sceneTime,'Capture predates the latest material build');
  assert(fs.statSync(path.join(__dirname,`render-evidence-v2-${color}.json`)).mtimeMs>sceneTime,'Render evidence predates the latest material build');
  assert.equal(render.lumen_gi,1);assert.equal(render.lumen_reflections,1);
  const image=fs.readFileSync(render.output);
  assert.equal(image.readUInt32BE(16),1600);assert.equal(image.readUInt32BE(20),1200);
  assert(image.length>10000);
  console.log(`${color}: 36 native-material assignments; no Web textures; dimensions/floor; new texture hashes; fresh 1600x1200 UE capture verified`);
}
