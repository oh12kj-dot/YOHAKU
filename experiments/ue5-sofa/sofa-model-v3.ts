import * as THREE from 'three';
import { RoundedBoxGeometry } from 'three/addons/geometries/RoundedBoxGeometry.js';
import { mergeVertices } from 'three/addons/utils/BufferGeometryUtils.js';

export type StudyColor = 'brown' | 'blue-gray';
export const studyDimensions = { widthMm: 1500, depthMm: 870, heightMm: 820 } as const;
export const studyColors = { brown: { name: 'ブラウン', color: '#93623e' }, 'blue-gray': { name: 'ブルーグレー', color: '#414b59' } } as const;
export const upholsteryMaterialName = 'UE_SLOT_Upholstery';
const clamp = THREE.MathUtils.clamp;
const gaussian = (value: number, width: number) => Math.exp(-((value / width) ** 2));

// Unlike RoundedBoxGeometry's corner segments, this retains a grid over the broad faces.
function roundedSurfaceBox(width: number, height: number, depth: number, radius: number, divisions: [number, number, number]) {
  const geometry = new THREE.BoxGeometry(width, height, depth, ...divisions), p = geometry.getAttribute('position');
  const inner = new THREE.Vector3(width / 2 - radius, height / 2 - radius, depth / 2 - radius);
  for (let i = 0; i < p.count; i++) {
    const point = new THREE.Vector3(p.getX(i), p.getY(i), p.getZ(i));
    const core = new THREE.Vector3(clamp(point.x, -inner.x, inner.x), clamp(point.y, -inner.y, inner.y), clamp(point.z, -inner.z, inner.z));
    const result = point.sub(core).normalize().multiplyScalar(radius).add(core);
    p.setXYZ(i, result.x, result.y, result.z);
  }
  return geometry;
}
function smoothClothNormals(geometry: THREE.BufferGeometry): THREE.BufferGeometry {
  const result = geometry.index ? geometry.toNonIndexed() : geometry.clone();
  const positionsOnly = new THREE.BufferGeometry(); positionsOnly.setAttribute('position', result.getAttribute('position').clone());
  const welded = mergeVertices(positionsOnly, 0.000001); welded.computeVertexNormals();
  const normals = welded.getAttribute('normal'), indices = welded.getIndex()!;
  const output = new Float32Array(result.getAttribute('position').count * 3);
  for (let i = 0; i < indices.count; i++) { const at = indices.getX(i); output[i * 3] = normals.getX(at); output[i * 3 + 1] = normals.getY(at); output[i * 3 + 2] = normals.getZ(at); }
  result.setAttribute('normal', new THREE.BufferAttribute(output, 3));
  welded.dispose(); positionsOnly.dispose(); geometry.dispose();
  return result;
}

// Stable metre-based UV density, independent of the individual box dimensions.
function setMaterialUV(geometry: THREE.BufferGeometry) {
  const p = geometry.getAttribute('position'), n = geometry.getAttribute('normal');
  const uv = new Float32Array(p.count * 2), tileMetres = 0.34;
  for (let i = 0; i < p.count; i++) {
    const ax = Math.abs(n.getX(i)), ay = Math.abs(n.getY(i)), az = Math.abs(n.getZ(i));
    let u: number, v: number;
    if (ax > ay && ax > az) { u = p.getZ(i); v = p.getY(i); }
    else if (ay > az) { u = p.getX(i); v = p.getZ(i); }
    else { u = p.getX(i); v = p.getY(i); }
    uv[i * 2] = u / tileMetres; uv[i * 2 + 1] = v / tileMetres;
  }
  geometry.setAttribute('uv', new THREE.BufferAttribute(uv, 2));
}
function roundedFace(a: number, b: number, c: number, radius: number, x: number, y: number) {
  const dx = Math.max(0, Math.abs(x) - (a - radius)), dy = Math.max(0, Math.abs(y) - (b - radius));
  return c - radius + Math.sqrt(Math.max(0, radius * radius - dx * dx - dy * dy));
}
function roundedLoop(hw: number, hh: number, radius: number): THREE.Vector2[] {
  const points: THREE.Vector2[] = [];
  for (let corner = 0; corner < 4; corner++) {
    const a = corner * Math.PI / 2;
    const cx = (corner === 0 || corner === 3 ? 1 : -1) * (hw - radius);
    const cy = (corner < 2 ? 1 : -1) * (hh - radius);
    for (let j = 0; j <= 8; j++) { const angle = a + j / 8 * Math.PI / 2; points.push(new THREE.Vector2(cx + Math.cos(angle) * radius, cy + Math.sin(angle) * radius)); }
  }
  return points;
}

// Photo-referenced, code-authored approximation. This remains an unvalidated study.
// +Y up, +Z front, bottom footprint centre at origin. All geometry units are metres.
export function createSofaStudy(color: StudyColor): THREE.Group {
  const model = new THREE.Group(); model.name = 'PSF SOFA 2P - photo-referenced UE study v3';
  model.userData = { provenance: 'photo-referenced-code-model', modelRevision: 3, validationStatus: 'unvalidated', sourceUrl: 'https://flymee.jp/product/106294/?pi_id=326211', referenceDimensionsMm: studyDimensions, color, materialProvenance: 'reference-guided ImageGen albedo; derived normal/roughness; not a measured scan', inferred: ['underside construction', 'individual wrinkles', 'material response'], placementEligible: false };
  const cloth = new THREE.MeshPhysicalMaterial({ color: '#c8c8c8', roughness: 0.94, metalness: 0, sheen: 0.13, sheenColor: new THREE.Color('#86817a'), sheenRoughness: 0.83, specularIntensity: 0.42 });
  cloth.name = upholsteryMaterialName;
  const stitch = new THREE.MeshStandardMaterial({ color: color === 'brown' ? '#6f4b32' : '#303743', roughness: 0.97 });
  const iron = new THREE.MeshStandardMaterial({ color: '#191c1b', roughness: 0.48, metalness: 0.55 });
  const rubber = new THREE.MeshStandardMaterial({ color: '#0d100e', roughness: 0.97 });
  const socketFace = new THREE.MeshStandardMaterial({ color: '#292b28', roughness: 0.8 });
  const metal = new THREE.MeshStandardMaterial({ color: '#858981', roughness: 0.38, metalness: 0.8 });
  function mesh(geometry: THREE.BufferGeometry, material: THREE.Material, name: string, x = 0, y = 0, z = 0) {
    if (material === cloth) { geometry = smoothClothNormals(geometry); setMaterialUV(geometry); }
    const indexed = mergeVertices(geometry, 0.000001); geometry.dispose();
    const result = new THREE.Mesh(indexed, material); result.name = name; result.position.set(x, y, z); result.castShadow = result.receiveShadow = true; model.add(result); return result;
  }
  function box(name: string, size: number[], position: number[], radius: number, material: THREE.Material = cloth) {
    return mesh(new RoundedBoxGeometry(size[0], size[1], size[2], 5, radius), material, name, position[0], position[1], position[2]);
  }
  function piping(name: string, points: THREE.Vector3[], radius = 0.00065, closed = false) {
    return mesh(new THREE.TubeGeometry(new THREE.CatmullRomCurve3(points, closed, 'centripetal'), Math.max(48, points.length * 2), radius, 5, closed), stitch, name);
  }
  // Front photo proportions: lower rail ~130mm, upper cushion ~160mm.
  // These component dimensions are photo estimates, not manufacturer measurements.
  box('upholstered-base', [1.49, 0.13, 0.855], [0, 0.185, 0], 0.013);
  box('rear-support-panel-photo-referenced', [1.23, 0.57, 0.12], [0, 0.405, -0.375], 0.018);
  // Arched arm cross-section: rounded shoulder instead of a flat-topped box.
  for (const sign of [-1, 1]) {
    const hw = 0.0615, bottom = 0.126, top = 0.544, bottomRadius = 0.009;
    const shape = new THREE.Shape();
    shape.moveTo(-hw, bottom + bottomRadius); shape.lineTo(-hw, top - .041);
    shape.bezierCurveTo(-hw, top - .012, -.038, top, 0, top);
    shape.bezierCurveTo(.038, top, hw, top - .012, hw, top - .041);
    shape.lineTo(hw, bottom + bottomRadius); shape.quadraticCurveTo(hw, bottom, hw - bottomRadius, bottom);
    shape.lineTo(-hw + bottomRadius, bottom); shape.quadraticCurveTo(-hw, bottom, -hw, bottom + bottomRadius);
    const geometry = new THREE.ExtrudeGeometry(shape, { depth: 0.858, steps: 24, bevelEnabled: true, bevelThickness: 0.006, bevelSize: 0.006, bevelSegments: 5, curveSegments: 16 });
    const p = geometry.getAttribute('position');
    for (let i = 0; i < p.count; i++) {
      const x = p.getX(i), y = p.getY(i), z = p.getZ(i);
      const upper = clamp((y - 0.46) / 0.09, 0, 1);
      p.setXYZ(i, x, y - 0.004 * upper * Math.sin(clamp(z / 0.858, 0, 1) * Math.PI), z);
    }
    geometry.computeVertexNormals(); mesh(geometry, cloth, sign < 0 ? 'left-arm' : 'right-arm', sign * 0.6825, 0, -0.429);
    piping(`arm-shoulder-stitch-${sign}`, [-0.395, -0.2, 0, 0.2, 0.395].map(z => new THREE.Vector3(sign * 0.744, 0.508, z)), 0.0005);
  }

  const seatHalf = { x: 0.62, y: 0.0775, z: 0.345 }, seatRadius = 0.023;
  const quiltX = [-0.31, 0, 0.31];
  const deformSeat = (x: number, y: number, z: number) => {
    const top = clamp(y / seatHalf.y, 0, 1);
    const billow = 0.009 * Math.max(0, 1 - (x / seatHalf.x) ** 4) * Math.max(0, 1 - (z / seatHalf.z) ** 4);
    const channels = quiltX.reduce((sum, at) => sum + gaussian(x - at, 0.009), 0) + gaussian(z - 0.035, 0.009);
    const settling = 0.003 * (gaussian(x - 0.3, 0.21) + gaussian(x + 0.3, 0.21)) * gaussian(z + 0.1, 0.19);
    const edgeGather = .0018 * Math.sin(x * 113 + .6) * gaussian(z - .31, .035) * Math.max(0, 1 - (x / seatHalf.x) ** 8);
    return new THREE.Vector3(x, y + top * (billow - channels * 0.0045 - settling + edgeGather), z);
  };
  const seat = roundedSurfaceBox(seatHalf.x * 2, seatHalf.y * 2, seatHalf.z * 2, seatRadius, [100, 12, 56]), sp = seat.getAttribute('position');
  for (let i = 0; i < sp.count; i++) { const q = deformSeat(sp.getX(i), sp.getY(i), sp.getZ(i)); sp.setXYZ(i, q.x, q.y, q.z); }
  seat.computeVertexNormals(); seat.computeBoundingBox();
  const seatY = 0.41 - seat.boundingBox!.max.y;
  mesh(seat, cloth, 'single-seat-cushion', 0, seatY, 0.085);
  const seatTop = (x: number, z: number) => {
    const y = roundedFace(seatHalf.x, seatHalf.z, seatHalf.y, seatRadius, x, z), p = deformSeat(x, y, z);
    return new THREE.Vector3(p.x, p.y + seatY + 0.0005, p.z + 0.085);
  };
  for (const x of quiltX) for (const offset of [-0.0007, 0.0007]) piping(`seat-quilt-${x}-${offset}`, Array.from({ length: 64 }, (_, i) => seatTop(x + offset, -0.245 + 0.56 * i / 63)), 0.00022);
  piping('seat-cross-quilt', Array.from({ length: 80 }, (_, i) => seatTop(-0.595 + 1.19 * i / 79, 0.035)), 0.00024);
  piping('seat-front-piping', Array.from({ length: 60 }, (_, i) => {
    const x = -0.59 + 1.18 * i / 59, y = 0.047;
    const z = roundedFace(seatHalf.x, seatHalf.y, seatHalf.z, seatRadius, x, y);
    const p = deformSeat(x, y, z); return new THREE.Vector3(p.x, p.y + seatY, p.z + 0.085 + 0.0004);
  }), 0.0004);

  const pillow = { w: 0.619, h: 0.415, d: 0.19, radius: 0.043 };
  for (const sign of [-1, 1]) {
    const deform = (x: number, y: number, z: number) => {
      const nx = x / (pillow.w / 2), ny = y / (pillow.h / 2), front = clamp(z / (pillow.d / 2), 0, 1);
      const fullness = 0.052 * Math.max(0, 1 - nx * nx) ** 0.65 * Math.max(0, 1 - ny * ny) ** 0.7;
      let folds = 0;
      // Irregular localized gathers, distinct at each edge, instead of repeated bars.
      const anchors = nx < 0 ? (sign < 0 ? [-.78, -.46, .39, .63, .79] : [-.53, -.2, .07, .31, .58, .76]) : (sign < 0 ? [-.49, -.15, .13, .38, .64, .8] : [-.68, -.32, .27, .7]);
      for (const [index, at] of anchors.entries()) {
        const reach = .17 + .11 * (.5 + .5 * Math.sin(index * 3.7 + sign * 2.1));
        const slant = .12 + .42 * (.5 + .5 * Math.cos(index * 2.3 + sign));
        const line = ny - at + (1 - Math.abs(nx)) * slant;
        const strength = .006 + .005 * (.5 + .5 * Math.sin(index * 4.1 + sign));
        folds += (-strength * gaussian(line, .031) + .0035 * gaussian(line - .043, .039)) * gaussian(1 - Math.abs(nx), reach);
      }
      const pulledEdge = 0.0025 * Math.sin(ny * 23 + sign) * Math.abs(nx) ** 9 * Math.max(0, 1 - ny * ny);
      const px = x + Math.sign(x) * 0.004 * Math.max(0, 1 - ny * ny) * Math.abs(nx) ** 3;
      const py = y - 0.007 * (0.5 + 0.5 * Math.sin(nx * 7 + sign * 1.7)) * Math.max(0, ny) ** 8;
      const settling = -.003 * gaussian(ny + .83, .13) * Math.cos(nx * 13 + sign);
      return new THREE.Vector3(px, py, z - 0.25 * (y + pillow.h / 2) + front * (fullness + folds + pulledEdge + settling));
    };
    const geometry = roundedSurfaceBox(pillow.w, pillow.h, pillow.d, pillow.radius, [112, 88, 20]), p = geometry.getAttribute('position');
    for (let i = 0; i < p.count; i++) { const q = deform(p.getX(i), p.getY(i), p.getZ(i)); p.setXYZ(i, q.x, q.y, q.z); }
    geometry.computeVertexNormals(); geometry.computeBoundingBox();
    const cx = sign * 0.311, cy = 0.82 - geometry.boundingBox!.max.y, cz = -0.425 - geometry.boundingBox!.min.z;
    mesh(geometry, cloth, sign < 0 ? 'left-back-cushion' : 'right-back-cushion', cx, cy, cz);
    const points = roundedLoop(pillow.w / 2 - 0.011, pillow.h / 2 - 0.011, 0.027).map(p => {
      const z = roundedFace(pillow.w / 2, pillow.h / 2, pillow.d / 2, pillow.radius, p.x, p.y), q = deform(p.x, p.y, z);
      return new THREE.Vector3(q.x + cx, q.y + cy, q.z + cz + 0.0004);
    });
    piping(`back-cushion-piping-${sign}`, points, 0.00035, true);
  }
  // Side/front photos put the legs further inboard than the original study.
  for (const x of [-0.515, 0.515]) for (const z of [-0.35, 0.35]) {
    mesh(new THREE.CylinderGeometry(0.013, 0.012, 0.115, 20), iron, 'iron-leg', x, 0.0625, z);
    mesh(new THREE.CylinderGeometry(0.0125, 0.0125, 0.01, 20), rubber, 'rubber-foot', x, 0.005, z);
  }
  const plate = new THREE.Shape(), hw = 0.078, r = 0.0135;
  plate.moveTo(-hw + r, -r); plate.lineTo(hw - r, -r); plate.absarc(hw - r, 0, r, -Math.PI / 2, Math.PI / 2, false);
  plate.lineTo(-hw + r, r); plate.absarc(-hw + r, 0, r, Math.PI / 2, Math.PI * 1.5, false);
  mesh(new THREE.ExtrudeGeometry(plate, { depth: 0.003, steps: 1, bevelEnabled: true, bevelSize: 0.0007, bevelThickness: 0.001, bevelSegments: 2, curveSegments: 12 }), iron, 'power-panel-approximation', 0, 0.18, 0.429);
  for (const cx of [-0.043, 0.002]) {
    box('socket-well', [0.031, 0.017, 0.001], [cx, 0.18, 0.4337], 0.00015, socketFace);
    for (const dx of [-0.009, 0.009]) box('socket-slot', [0.0028, 0.01, 0.0005], [cx + dx, 0.18, 0.4343], 0.0001, rubber);
  }
  box('usb-metal-surround', [0.019, 0.008, 0.0005], [0.04, 0.18, 0.434], 0.0001, metal);
  box('usb-port', [0.015, 0.0045, 0.0005], [0.04, 0.18, 0.4343], 0.0001, rubber);
  model.updateMatrixWorld(true);
  return model;
}


