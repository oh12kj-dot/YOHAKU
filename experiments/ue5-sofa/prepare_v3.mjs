import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import crypto from 'node:crypto';
import assert from 'node:assert/strict';
import * as THREE from 'three';
import {GLTFExporter} from 'three/addons/exporters/GLTFExporter.js';
import {chromium} from '@playwright/test';
import {createSofaStudy} from './sofa-model-v3.ts';

const root=path.dirname(fileURLToPath(import.meta.url));
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
globalThis.FileReader=class {
  readAsArrayBuffer(blob){blob.arrayBuffer().then(result=>{this.result=result;this.onloadend?.();});}
  readAsDataURL(blob){blob.arrayBuffer().then(result=>{this.result='data:application/octet-stream;base64,'+Buffer.from(result).toString('base64');this.onloadend?.();});}
};
const model=createSofaStudy('brown');
model.name='PSF UE-only proportion and upholstery study v3';
model.userData={...model.userData,modelRevision:3,materialProvenance:'independent UE v3 native material',componentDimensions:'photo-estimated, not measured',placementEligible:false};
const names=['Upholstery','Stitch','Leg','PowerPlate','Socket','USB'];
const seen=new Map();
model.traverse(o=>{if(o.isMesh){if(!seen.has(o.material))seen.set(o.material,seen.size);o.material.name='UE_SLOT_'+names[seen.get(o.material)];}});
const bounds=new THREE.Box3().setFromObject(model);
const size=bounds.getSize(new THREE.Vector3());
assert(Math.abs(size.x-1.5)<1e-5&&Math.abs(size.y-.82)<1e-5&&Math.abs(size.z-.87)<1e-5);
assert(Math.abs(bounds.min.y)<1e-6);
const records=[];
model.traverse(o=>{if(o.isMesh){const b=new THREE.Box3().setFromObject(o);records.push({name:o.name,min:b.min.toArray(),max:b.max.toArray(),size:b.getSize(new THREE.Vector3()).toArray()});}});
const base=records.find(r=>r.name==='upholstered-base');
const seat=records.find(r=>r.name==='single-seat-cushion');
assert(Math.abs(base.size[1]-.13)<1e-5);
assert(seat.size[1]>.158&&seat.size[1]<.166,'Cushion thickness must exceed 158mm');
assert(seat.size[1]/base.size[1]>1.2,'Cushion must be thicker than the base');
assert(Math.abs(seat.max[1]-.41)<1e-5);
assert(Math.abs(base.max[1]-seat.min[1])<.01,'No visible vertical separation of base and cushion');
const glb=Buffer.from(await new GLTFExporter().parseAsync(model,{binary:true}));
const j=JSON.parse(glb.subarray(20,20+glb.readUInt32LE(12)));
assert.equal(j.images,undefined);assert.equal(j.meshes.length,36);
fs.writeFileSync(path.join(root,'material-source-v3/sofa-geometry-only.glb'),glb);
const evidence={modelRevision:3,geometryOnlySha256:hash(glb),boundsMetres:{min:bounds.min.toArray(),max:bounds.max.toArray()},components:records,photoEstimates:{baseThicknessMm:130,seatCushionThicknessMm:seat.size[1]*1000,seatHeightMm:410},materialOrigin:'new UE-specific built-in ImageGen albedos, independently derived physical detail maps',colors:{}};
const browser=await chromium.launch({headless:true});
const page=await browser.newPage();
for(const color of ['brown','blue-gray']){
  const raw=fs.readFileSync(path.join(root,`material-source-v3/${color}-basecolor.png`));
  const maps=await page.evaluate(async ({url,color})=>{
    const img=new Image();img.src=url;await img.decode();
    // Power-of-two technical maps support full UE mips; original albedo stays intact.
    const n=2048,c=document.createElement('canvas');c.width=c.height=n;const ctx=c.getContext('2d');ctx.drawImage(img,0,0,n,n);
    const rgba=ctx.getImageData(0,0,n,n).data,h=new Float32Array(n*n);
    for(let i=0;i<h.length;i++)h[i]=(rgba[i*4]*.2126+rgba[i*4+1]*.7152+rgba[i*4+2]*.0722)/255;
    const at=(x,y)=>h[((y+n)%n)*n+(x+n)%n];
    const height=new Float32Array(h.length);
    // Remove broad color variation: only local fine cracks influence relief.
    for(let y=0;y<n;y++)for(let x=0;x<n;x++){
      let mean=0;for(const [dx,dy]of [[-7,0],[7,0],[0,-7],[0,7],[-5,-5],[5,5],[-5,5],[5,-5]])mean+=at(x+dx,y+dy)/8;
      height[y*n+x]=Math.max(-.14,Math.min(.14,-(at(x,y)-mean)*3.5));
    }
    const hp=(x,y)=>height[((y+n)%n)*n+(x+n)%n];
    const normal=ctx.createImageData(n,n),rough=ctx.createImageData(n,n);
    for(let y=0;y<n;y++)for(let x=0;x<n;x++){
      const i=y*n+x,k=i*4,gx=(hp(x+1,y)-hp(x-1,y))*2.5,gy=(hp(x,y+1)-hp(x,y-1))*2.5,len=Math.hypot(gx,gy,1);
      normal.data[k]=Math.round((.5-.5*gx/len)*255);normal.data[k+1]=Math.round((.5+.5*gy/len)*255);normal.data[k+2]=Math.round((.5+.5/len)*255);normal.data[k+3]=255;
      const r=Math.max(.5,Math.min(.81,(color==='brown'?.61:.69)-height[i]*1.3));
      rough.data[k]=rough.data[k+1]=rough.data[k+2]=Math.round(r*255);rough.data[k+3]=255;
    }
    ctx.putImageData(normal,0,0);const normalURL=c.toDataURL('image/png');ctx.putImageData(rough,0,0);
    return {normal:normalURL,roughness:c.toDataURL('image/png')};
  },{url:'data:image/png;base64,'+raw.toString('base64'),color});
  const entry={baseColorSha256:hash(raw),textureTileMm:340,normalFormat:'DirectX; local contrast relief; not measured',roughnessRange:color==='brown'?[.5,.79]:[.51,.81]};
  for(const kind of ['normal','roughness']){const b=Buffer.from(maps[kind].split(',')[1],'base64');fs.writeFileSync(path.join(root,`material-source-v3/${color}-${kind}.png`),b);entry[`${kind}Sha256`]=hash(b);}
  evidence.colors[color]=entry;
}
await browser.close();
fs.writeFileSync(path.join(root,'material-source-v3/provenance.json'),JSON.stringify(evidence,null,2));
console.log(JSON.stringify({bounds:evidence.boundsMetres,photoEstimates:evidence.photoEstimates,meshes:j.meshes.length,geometryBytes:glb.length,geometrySha256:evidence.geometryOnlySha256},null,2));
