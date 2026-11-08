// Geometry-only GLB and technical PBR maps for the independent UE material.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const { chromium } = require('@playwright/test');
const root = __dirname;
const hash = b => crypto.createHash('sha256').update(b).digest('hex');

async function main() {
  const input = fs.readFileSync(path.join(root, '../../artifacts/psf-sofa-study-brown-v2.glb'));
  const jsonLength = input.readUInt32LE(12);
  const j = JSON.parse(input.subarray(20, 20 + jsonLength));
  const binary = input.subarray(28 + jsonLength);
  // Repack only accessor buffer views. No source material images are retained.
  const used = [...new Set(j.accessors.map(a => a.bufferView).filter(v => v !== undefined))].sort((a,b)=>a-b);
  let offset = 0;
  const chunks = [];
  const views = used.map(i => {
    const v = j.bufferViews[i];
    const b = binary.subarray(v.byteOffset || 0, (v.byteOffset || 0) + v.byteLength);
    chunks.push(b, Buffer.alloc((4 - b.length % 4) % 4));
    const out = {...v, byteOffset:offset};
    offset += b.length + (4 - b.length % 4) % 4;
    return out;
  });
  for (const a of j.accessors) if (a.bufferView !== undefined) a.bufferView = used.indexOf(a.bufferView);
  j.bufferViews = views;
  const names = ['Upholstery', 'Stitch', 'Leg', 'PowerPlate', 'Socket', 'USB'];
  j.materials = names.map(n => ({name:`UE_SLOT_${n}`, pbrMetallicRoughness:{baseColorFactor:[0.5,0.5,0.5,1],metallicFactor:0,roughnessFactor:1}}));
  delete j.images; delete j.textures; delete j.samplers;
  delete j.extensionsUsed; delete j.extensionsRequired;
  j.buffers = [{byteLength:offset}];
  j.asset.extras = {purpose:'geometry only; independently authored UE material v2'};
  const jsonRaw = Buffer.from(JSON.stringify(j));
  const json = Buffer.concat([jsonRaw,Buffer.alloc((4-jsonRaw.length%4)%4,32)]);
  const bin = Buffer.concat(chunks);
  const header = Buffer.alloc(20); header.write('glTF');header.writeUInt32LE(2,4);header.writeUInt32LE(28+json.length+bin.length,8);header.writeUInt32LE(json.length,12);header.write('JSON',16);
  const bh = Buffer.alloc(8);bh.writeUInt32LE(bin.length);bh.write('BIN\0',4);
  const mesh = Buffer.concat([header,json,bh,bin]);
  fs.writeFileSync(path.join(root,'material-source-v2/sofa-geometry-only.glb'),mesh);
  const browser = await chromium.launch({headless:true});
  const page = await browser.newPage();
  const provenance = {sourceMeshSha256:hash(input),geometryOnlySha256:hash(mesh),sourceWebTexturesRemoved:true,materialOrigin:'new built-in ImageGen generations from official product references; independent UE native graphs',colors:{}};
  for (const color of ['brown','blue-gray']) {
    const p = path.join(root,`material-source-v2/${color}-basecolor.png`);
    const bytes = fs.readFileSync(p);
    const maps = await page.evaluate(async (url) => {
      const img = new Image();img.src=url;await img.decode();
      const n=img.width; const c=document.createElement('canvas');c.width=n;c.height=n;const ctx=c.getContext('2d');ctx.drawImage(img,0,0);
      const src=ctx.getImageData(0,0,n,n).data;
      const h=new Float32Array(n*n);for(let i=0;i<h.length;i++)h[i]=(src[i*4]*.2126+src[i*4+1]*.7152+src[i*4+2]*.0722)/255;
      const normal=ctx.createImageData(n,n),rough=ctx.createImageData(n,n);
      for(let y=0;y<n;y++)for(let x=0;x<n;x++){
        const i=y*n+x,k=i*4;
        const gx=(h[y*n+(x+1)%n]-h[y*n+(x+n-1)%n])*.85;
        const gy=(h[((y+1)%n)*n+x]-h[((y+n-1)%n)*n+x])*.85;
        const inv=1/Math.hypot(gx,gy,1);
        // DirectX tangent-space normal: green points downward in image space.
        normal.data[k]=Math.round((-.5*gx*inv+.5)*255);normal.data[k+1]=Math.round((.5*gy*inv+.5)*255);normal.data[k+2]=Math.round((.5*inv+.5)*255);normal.data[k+3]=255;
        const r=Math.round(Math.min(.94,Math.max(.69,.84-(h[i]-.3)*.22))*255);
        rough.data[k]=rough.data[k+1]=rough.data[k+2]=r;rough.data[k+3]=255;
      }
      ctx.putImageData(normal,0,0);const normalURL=c.toDataURL('image/png');ctx.putImageData(rough,0,0);
      return {normal:normalURL,roughness:c.toDataURL('image/png'),width:n,height:img.height};
    },'data:image/png;base64,'+bytes.toString('base64'));
    const record={baseColorSha256:hash(bytes),width:maps.width,height:maps.height,maps:'new albedo; derived DirectX normal and linear roughness; not measured/scanned PBR'};
    for(const kind of ['normal','roughness']){const b=Buffer.from(maps[kind].split(',')[1],'base64');fs.writeFileSync(path.join(root,`material-source-v2/${color}-${kind}.png`),b);record[`${kind}Sha256`]=hash(b);}
    provenance.colors[color]=record;
  }
  await browser.close();
  fs.writeFileSync(path.join(root,'material-source-v2/provenance.json'),JSON.stringify(provenance,null,2));
  console.log(JSON.stringify(provenance,null,2));
}
main().catch(e=>{console.error(e);process.exitCode=1;});
