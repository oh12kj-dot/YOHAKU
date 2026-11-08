# UE5専用素材 v2

作成日: 2026-11-08 (JST)。内蔵 `image_gen` 使用、CLI/API課金経路は使用していない。

ユーザー指示に従い、Web版の素材画像を再利用せず、公式商品写真だけを参照して2色を新規生成。形状は改修済みコード製v2を使用し、取り込み用GLBからWeb版の全テクスチャ/素材設定を除去した。UEで全36メッシュのマテリアルを新規作成したネイティブ材質へ上書きする。

## 保存先

`experiments/ue5-sofa/material-source-v2/`:

- `brown-basecolor.png`、`blue-gray-basecolor.png`: 新規生成画像各1254×1254。
- `brown-normal.png`、`blue-gray-normal.png`: 新規画像から濃度勾配で導出したDirectX法線。
- `brown-roughness.png`、`blue-gray-roughness.png`: 新規画像から導出した線形粗さ。
- `sofa-geometry-only.glb`: 画像を含まないv2形状のみ。
- `provenance.json`: 原画像/導出画像/形状のSHA-256。

UE資産は `/Game/UEFabricV2`。色はsRGB、法線はTC_NORMALMAP/非sRGB/DirectX green、粗さはTC_MASKS/非sRGBで設定。張地のほか、縫い糸・黒い脚・樹脂・USB金属もUEネイティブ材質として新規作成する。`prepare_v2.cjs` は技術用PBRデータを導出するのみで、写真や生成アルベドの描き替えはしない。

実物スキャン/測定PBRではない。生成画像の微細模様・色と物理的な反射値は近似であり、再現品質未検証。UEのスタジオ照明で白っぽく見えたため、ベースカラーにブラウン0.72/ブルーグレー0.5の線形ゲインを適用している。写真との目視調整で、測色校正ではない。3D自動復元や正式配置アセットへの採用は行っていない。

## ブラウン: 完全な生成プロンプト

参照画像: `artifacts/reference/detail-18.jpg` (公式側面写真)。参照のみ、新規生成。

```text
Create a NEW seamless square base-color texture specifically for an Unreal Engine 5 material study. Reference is the attached official PSF SOFA 2P brown side product photograph. Reproduce ONLY the upholstery surface: warm medium muted caramel brown polyester with leather-look water-repellent finish, dense irregular delicate pale branching crackle network, small subdued darker brown islands and fine fibrous stippling. Important: closely match the photo's subtle brown tone and dense fine irregular web, not generic orange suede or coarse woven fabric. Flat orthographic texture scan of approximately 34cm square of upholstery, edge-to-edge detail, perfectly even neutral diffuse illumination; no folds, no seams, no sofa, no perspective, no highlights, no cast shadows, no large stains, no vignette, no writing. Seamlessly tileable at all edges. This is a fresh UE5-specific albedo asset, not an edit/reuse of earlier generated textures. Detailed photorealistic PBR base color only.
```

## ブルーグレー: 完全な生成プロンプト

参照画像: `artifacts/reference/detail-26.jpg` (公式張地/電源部写真)。参照のみ、新規生成。

```text
Use case: product-mockup texture asset. Create a NEW seamless square PBR base-color texture specifically for Unreal Engine 5. Input image 1 is reference ONLY: official PSF SOFA 2P blue-gray upholstery close-up; exclude the socket and all geometry. Reproduce the upholstery's slate blue-gray polyester leather-look finish: extremely fine dark irregular stippled grain and delicate subtle branching worn micro-lines, very slight muted charcoal mottling. More fine stippled fabric-like grain than large cracked leather. Match the reference cool gray-blue tone, no orange. Flat orthographic evenly lit material scan, equivalent to 34cm square, edge-to-edge. Uniform diffuse lighting without shadows or highlights. No seams, wrinkles, socket, text, objects, borders, vignette, perspective, coarse weave. Seamlessly tileable. Fresh standalone UE5 material asset, not a modification of previous generated textures.
```

