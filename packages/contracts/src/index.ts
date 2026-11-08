import { z } from 'zod';

// Development limits, not a published/calibrated ResourceProfile.
export const developmentLimits = { maxRoomMm: 30000, maxFurnitureMm: 30000, maxCoordinateMm: 100000, maxInstances: 200 } as const;
const id = z.uuid();
const dimension = z.number().finite().positive().max(developmentLimits.maxRoomMm);
const coordinate = z.number().finite().min(-developmentLimits.maxCoordinateMm).max(developmentLimits.maxCoordinateMm);
export const positionSchema = z.strictObject({ x: coordinate, y: z.literal(0), z: coordinate });
export const dimensionsSchema = z.strictObject({ widthMm: dimension, depthMm: dimension, heightMm: dimension });
export const columnSchema = dimensionsSchema.extend({ columnId: id, xMm: coordinate, zMm: coordinate });
export const roomSchema = dimensionsSchema.extend({ columns: z.array(columnSchema).max(100) });
const yaw = z.number().finite();
export const furnitureSchema = z.strictObject({ instanceId: id, assetVersionId: id, positionMm: positionSchema, yawRad: yaw });
export const wallSchema = z.enum(['NORTH', 'SOUTH', 'EAST', 'WEST']);
export const surfaceSchema = z.strictObject({ target: z.enum(['FLOOR', 'NORTH', 'SOUTH', 'EAST', 'WEST']), materialId: z.enum(['oak-natural', 'walnut', 'white-oak', 'stone', 'ivory', 'sage', 'clay', 'chalk', 'slate']) });
export const sceneCommandSchema = z.union([
  z.strictObject({ type: z.literal('SetRoom'), room: roomSchema }),
  z.strictObject({ type: z.literal('SetSurface'), surface: surfaceSchema }),
  z.strictObject({ type: z.literal('AddFurniture'), furniture: furnitureSchema }),
  z.strictObject({ type: z.literal('MoveFurniture'), instanceId: id, positionMm: positionSchema, yawRad: yaw }),
  z.strictObject({ type: z.literal('RemoveFurniture'), instanceId: id }),
  z.strictObject({ type: z.literal('DuplicateFurniture'), sourceInstanceId: id, instanceId: id, positionMm: positionSchema }),
  z.strictObject({ type: z.literal('ReplaceFurniture'), instanceId: id, expectedAssetVersionId: id, targetAssetVersionId: id, anchor: z.literal('center') }),
  z.strictObject({ type: z.literal('ReplaceFurniture'), instanceId: id, expectedAssetVersionId: id, targetAssetVersionId: id, anchor: z.literal('wall'), wallId: wallSchema }),
]);
export const envelopeSchema = z.strictObject({ schemaVersion: z.literal(1), operationId: id, expectedRevision: z.string().regex(/^[1-9]\d*$/), command: sceneCommandSchema });
export type Room = z.infer<typeof roomSchema>;
export type Dimensions = z.infer<typeof dimensionsSchema>;
export type PositionMm = z.infer<typeof positionSchema>;
export type Furniture = z.infer<typeof furnitureSchema>;
export type SceneCommand = z.infer<typeof sceneCommandSchema>;
export type Envelope = z.infer<typeof envelopeSchema>;
export type Wall = z.infer<typeof wallSchema>;
export type Eligibility = 'eligible' | 'caution' | 'blocked';
export type Surface = z.infer<typeof surfaceSchema>;
export type Asset = { assetVersionId: string; specVersionId: string; dimensions: Dimensions; specPolicy: Eligibility; assetPolicy: Eligibility; granted: boolean };
export type GeometryWarning = { kind: 'outside_room' | 'collision_candidate' | 'column_collision_candidate'; targetIds: string[]; shape: 'coarse'; geometryRuleVersion: 'coarse-obb-v1' };
// Initial domain subset. Openings, environment and saved views are not implemented yet.
export type SceneDraft = { schemaVersion: 1; layoutId: string; room: Room; surfaces: Surface[]; furniture: Furniture[]; geometryWarnings: GeometryWarning[] };
