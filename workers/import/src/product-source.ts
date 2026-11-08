import { load } from 'cheerio';
import { createHash } from 'node:crypto';

export type ProductSource = {
  productId: string; name: string; brand: string; sourceUrl: string; retrievedAt: string; contentHash: string;
  dimensions: { widthMm: number; depthMm: number; heightMm: number };
  dimensionStatus: 'single-source-unverified';
  alternateConfiguration: string;
  variants: { id: string; name: string; imageUrl: string | null }[];
  modelStatus: 'not-generated';
};
export function validateProductUrl(input: string): URL {
  if (!/^https:\/\/flymee\.jp\/product\/[0-9]+\/(?:\?pi_id=[0-9]+)?$/.test(input) || /[\r\n]/.test(input) || input.length > 2048) throw new Error('現在はFLYMEeの商品URL（https://flymee.jp/product/番号/）に対応しています。');
  const url = new URL(input);
  if (url.protocol !== 'https:' || url.hostname !== 'flymee.jp' || url.port || url.username || url.password || !/^\/product\/\d+\/$/.test(url.pathname) || url.hash) throw new Error('現在はFLYMEeの商品URL（https://flymee.jp/product/番号/）に対応しています。');
  for (const [key, value] of url.searchParams) if (key !== 'pi_id' || !/^\d+$/.test(value)) throw new Error('商品URLのパラメーターを確認してください。');
  return url;
}
export function parseFlymee(html: string, input: string, retrievedAt = new Date().toISOString()): ProductSource {
  const url = validateProductUrl(input), $ = load(html);
  const dimensions: Record<string, number> = {};
  $('.spec_area_list._size').each((_i, el) => {
    const label = $(el).find('.title').text().trim(), text = $(el).find('.text').text().trim();
    const match = text.match(/^(\d+(?:\.\d+)?)\s*(cm|mm)$/i), axis = ({ 幅: 'widthMm', 奥行: 'depthMm', 高さ: 'heightMm' } as Record<string, string>)[label];
    if (match && axis) {
      const value = Number(match[1]) * (match[2].toLowerCase() === 'cm' ? 10 : 1);
      if (dimensions[axis] !== undefined && dimensions[axis] !== value) throw new Error('同じ軸に複数の寸法候補があります。仕様別の追加調査が必要です。');
      dimensions[axis] = value;
    }
  });
  if (!['widthMm', 'depthMm', 'heightMm'].every(key => Number.isFinite(dimensions[key]) && dimensions[key] > 0 && dimensions[key] <= 30000)) throw new Error('このページの3軸外寸を特定できません。追加の資料解析が必要です。');
  const photoFor = (name: string) => {
    const photo = $('img[data-caption]').toArray().find(el => $(el).attr('data-caption')?.trim() === `張地：${name}`);
    const value = photo ? $(photo).attr('src') : null;
    if (!value) return null;
    const image = new URL(value);
    return image.protocol === 'https:' && image.hostname === 'static2.flymee.jp' && image.pathname.startsWith('/product_images/') ? image.href : null;
  };
  const variants = $('.js-specHandler').toArray().map(el => ({ id: $(el).attr('data-spec-item-id') ?? '', name: $(el).find('.js-specItemName').text().trim() })).filter(v => v.id && v.name).map(v => ({ ...v, imageUrl: photoFor(v.name) }));
  if (!variants.length) throw new Error('この商品の仕様選択を特定できません。');
  const title = $('title').text().split(' - インテリア')[0].trim();
  if (!title) throw new Error('商品名がありません。');
  const alternateConfiguration = $('.spec_area_table').first().find('dd.separate').text().trim().replace(/\s+/g, ' ');
  return { productId: url.pathname.split('/')[2], name: title.split(' / ')[0], brand: title.split(' / ')[0].split(' PSF')[0], sourceUrl: url.href, retrievedAt, contentHash: createHash('sha256').update(html).digest('hex'), dimensions: dimensions as ProductSource['dimensions'], dimensionStatus: 'single-source-unverified', alternateConfiguration, variants, modelStatus: 'not-generated' };
}
export async function fetchProduct(input: string): Promise<ProductSource> {
  const url = validateProductUrl(input);
  const response = await fetch(url, { redirect: 'manual', signal: AbortSignal.timeout(12000), headers: { Accept: 'text/html' } });
  if (!response.ok || response.headers.get('content-type')?.split(';')[0].trim().toLowerCase() !== 'text/html') throw new Error('商品ページを取得できませんでした。アクセス制限は回避しません。');
  const reader = response.body?.getReader(); if (!reader) throw new Error('本文がありません。');
  const chunks: Uint8Array[] = []; let size = 0;
  try {
    for (;;) {
      const { done, value } = await reader.read(); if (done) break;
      size += value.byteLength; if (size > 2 * 1024 * 1024) throw new Error('商品ページが取得上限を超えました。');
      chunks.push(value);
    }
  } catch (error) { await reader.cancel(); throw error; }
  return parseFlymee(Buffer.concat(chunks).toString('utf8'), url.href);
}
