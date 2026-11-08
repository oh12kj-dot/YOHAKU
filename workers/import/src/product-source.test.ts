import { describe, expect, it } from 'vitest';
import { parseFlymee, validateProductUrl } from './product-source';
import { readFileSync } from 'node:fs';
describe('product source extraction, not D/M gate approval', () => {
  const url = 'https://flymee.jp/product/106294/?pi_id=326211';
  const html = `<title>JOURNAL STANDARD FURNITURE PSF SOFA 2P / 商品名 - インテリア</title>
  <dl class="spec_area_table"><dd class="separate">ロータイプ脚部使用時：高さ70cm</dd></dl>
  <div class="spec_area_list _size"><span class="title">幅</span><span class="text">150cm</span></div>
  <div class="spec_area_list _size"><span class="title">奥行</span><span class="text">87cm</span></div>
  <div class="spec_area_list _size"><span class="title">高さ</span><span class="text">82cm</span></div>
  <div class="spec_area_list"><span class="title">座面高</span><span class="text">41cm</span></div>
  <li class="js-specHandler" data-spec-item-id="1"><span class="js-specItemName">ブラウン</span></li>
  <li class="js-specHandler" data-spec-item-id="2"><span class="js-specItemName">ブルーグレー</span></li>
  <img data-caption="張地：ブラウン" src="https://static2.flymee.jp/product_images/brown.jpg">
  <img data-caption="張地：ブルーグレー" src="https://static2.flymee.jp/product_images/blue.jpg">`;
  it('maps overall cm axes and exact color photos without claiming validated or generated', () => {
    const product = parseFlymee(html, url, '2026-11-07T00:00:00.000Z');
    expect(product.dimensions).toEqual({ widthMm: 1500, depthMm: 870, heightMm: 820 });
    expect(product.alternateConfiguration).toContain('高さ70cm');
    expect(product.variants.map(v => v.imageUrl)).toEqual(['https://static2.flymee.jp/product_images/brown.jpg', 'https://static2.flymee.jp/product_images/blue.jpg']);
    expect(product.dimensionStatus).toBe('single-source-unverified'); expect(product.modelStatus).toBe('not-generated'); expect(product.contentHash).toMatch(/^[a-f\d]{64}$/);
  });
  it('does not substitute another color photo or incomplete dimensions', () => {
    expect(parseFlymee(html.replace('張地：ブルーグレー', '張地：未知色'), url).variants[1].imageUrl).toBeNull();
    expect(() => parseFlymee(html.replace('82cm', '不明'), url)).toThrow('3軸');
    expect(() => parseFlymee(html + '<div class="spec_area_list _size"><span class="title">高さ</span><span class="text">70cm</span></div>', url)).toThrow('複数');
  });
  it('shares captured fixture expectations with the native parser', () => {
    const captured = readFileSync(new URL('../../../tests/fixtures/flymee-product.html', import.meta.url), 'utf8');
    const product = parseFlymee(captured, url);
    expect(product.productId).toBe('106294');
    expect(product.dimensions).toEqual({ widthMm: 1500, depthMm: 870, heightMm: 820 });
    expect(product.variants.map(v => v.name)).toEqual(['ブラウン', 'ブルーグレー']);
    expect(product.variants.every(v => v.imageUrl?.startsWith('https://static2.flymee.jp/product_images/'))).toBe(true);
  });
  it.each(['https://flymee.jp:443/product/106294/', 'https://flymee.jp/product/106294/?pi_id=1&pi_id=2', 'https://flymee.jp/product/106294/\n'])('rejects URL ambiguity %s', input => expect(() => validateProductUrl(input)).toThrow());
  it.each(['http://flymee.jp/product/106294/', 'https://flymee.jp.evil.test/product/106294/', 'https://localhost/product/106294/', 'https://flymee.jp:9999/product/106294/', 'https://user:password@flymee.jp/product/106294/', 'https://flymee.jp/product/106294/?redirect=https://example.com', 'https://flymee.jp/guide/'])('rejects unsupported URLs %s', input => expect(() => validateProductUrl(input)).toThrow());
});

