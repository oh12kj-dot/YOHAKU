const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const root=__dirname,read=p=>JSON.parse(fs.readFileSync(path.join(root,p))),sha=b=>crypto.createHash('sha256').update(b).digest('hex');
const p=read('material-source-v3/provenance.json');
const glb=fs.readFileSync(path.join(root,'material-source-v3/sofa-geometry-only.glb'));
assert.equal(sha(glb),p.geometryOnlySha256);
const j=JSON.parse(glb.subarray(20,20+glb.readUInt32LE(12)));
assert.equal(j.images,undefined);assert.equal(j.textures,undefined);assert.equal(j.meshes.length,36);
for(const color of ['brown','blue-gray']){
  const e=read(`scene-evidence-v3-${color}.json`),r=read(`render-evidence-v3-${color}.json`);
  assert.equal(e.source_sha256,p.geometryOnlySha256);assert.equal(e.model_revision,3);
  assert.equal(e.mesh_actors.length,36);assert.equal(e.material.assignments.length,36);
  assert(e.material.assignments.every(a=>a.material.startsWith('/Game/UEFabricV3/')));
  assert.equal(e.material.web_textures_used,false);
  for(let i=0;i<3;i++)assert(Math.abs(e.bounds_cm.size[i]-[150,87,82][i])<.002);
  assert(Math.abs(e.bounds_cm.min[2])<.002);
  assert(Math.abs(e.component_proportions_cm.base-13)<.002);
  assert(e.component_proportions_cm.seat/e.component_proportions_cm.base>1.2);
  const seat=e.mesh_actors.find(a=>a.label==='single-seat-cushion');
  assert(Math.abs(seat.center_cm[2]+seat.extent_cm[2]-41)<.002);
  for(const t of e.material.texture_records){
    const raw=fs.readFileSync(t.source);assert.equal(sha(raw),t.sha256);assert.equal(t.srgb,t.kind==='basecolor');
    assert(t.power_of_two_mode.includes('STRETCH_TO_POWER_OF_TWO'));
    if(t.kind!=='basecolor'){assert.equal(raw.readUInt32BE(16),2048);assert.equal(raw.readUInt32BE(20),2048);}
    else for(const old of [path.join(root,`material-source-v2/${color}-basecolor.png`),path.join(root,`../../artifacts/materials/web-study-v2/${color}-albedo.png`)])assert.notEqual(sha(fs.readFileSync(old)),t.sha256);
  }
  assert.equal(r.model_revision,3);assert.equal(r.lumen_gi,1);assert.equal(r.lumen_reflections,1);
  assert.equal(r.shadow_ray_count,24);assert.equal(r.shadow_samples_per_ray,12);
  assert.deepEqual(r.captures.map(c=>c.view),color==='brown'?['hero','front','detail']:['hero','detail']);
  const sceneTime=fs.statSync(path.join(root,`scene-evidence-v3-${color}.json`)).mtimeMs;
  for(const capture of r.captures){
    const raw=fs.readFileSync(capture.output);assert.equal(sha(raw),capture.sha256);
    assert.equal(raw.readUInt32BE(16),1600);assert.equal(raw.readUInt32BE(20),1200);
    assert(fs.statSync(capture.output).mtimeMs>sceneTime,'Capture predates material/geometry build');
  }
  console.log(`${color}: dimensions, 130mm base / ${e.component_proportions_cm.seat.toFixed(3)}cm seat, 410mm seat height, 36 new material assignments, independent textures/mips, ${r.captures.length} fresh UE captures PASS`);
}
