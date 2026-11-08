# UE5 PSF SOFA 改修 v3

2026-11-08 JST。対象はUE専用モデル。Web版やv1/v2資産は保持する。

## 写真から確認した差と修正

参照: `artifacts/reference/psf-4.jpg` 正面、`detail-18.jpg` 側面、`detail-26.jpg` 張地/電源部。

- 下部ベースは170mmから130mmへ。座面クッションはふくらみを含めた外形で約129mmから約164mmへ。全幅1500/奥行870/全高820mm、座面高410mm、床原点を保持。部品厚は写真の比率に基づく推定で、メーカーの部品寸法や実測値ではない。
- ベース上端と座面下面は約3.7mm重ね、浮きや隙間を避ける。電源板の高さは180mmへ追従。
- 肘の上部は完全な半円から浅い丸肩へ。背クッションは中央を厚くし、左右/内外で異なるしわ、端の寄り、上辺の僅かな沈みを作る。均等な横棒状のしわを減らす。
- 座面キルト溝と前端の柔らかさを調整し、糸の径を約半分へ。小さな電源ソケットの厚み/丸め半径も修正。

## UE専用素材

内蔵 `image_gen` で公式写真だけを参照し、2色のv3アルベドを新規生成。Web版とv2画像は使わない。粗いまだら/粒感を抑え、革調ポリエステルの細い不規則な模様を目標にする。

保存先: `experiments/ue5-sofa/material-source-v3/` の `brown-basecolor.png` / `blue-gray-basecolor.png`、各色 `normal.png` / `roughness.png`、`provenance.json`、`sofa-geometry-only.glb`。アルベド原画像はそのまま保存。法線と粗さは新規画像の局所コントラストから技術的に導出し、広い色むらを形状凹凸へ誤変換しない。導出マップは2048角。340mmタイル、ブラウン粗さ中心0.61/ブルーグレー0.69、細線で粗さを変え、specular0.42。UE取り込み時のStretchToPowerOfTwoにより、余白を作らずmip対応する。実物スキャン/測定PBRではない。

UE材質は `/Game/UEFabricV3`。通常の色/法線/粗さ入力へ接続し、ソファ全36メッシュへ専用材質を割り当てる。形状・全高・厚み比・座面高は生成時とUE取り込み後の双方で検査する。

## 再現

`node experiments/ue5-sofa/prepare_v3.mjs` → UE Python commandlet `build_scene_v3.py`（ブラウン、`-YohakuBlueGray`で青）→ UE起動 `-YohakuCapture -YohakuV3`（青も同旗追加）→ `node experiments/ue5-sofa/verify_v3.cjs`。

マップ `/Game/SofaStudyV3Brown` / `SofaStudyV3BlueGray`。実際のUE描画は `artifacts/ue5-sofa-brown-v3.png`、`-v3-front.png`、`-v3-detail.png` と青の全体/接写。新規ファイル時刻/SHA-256を `render-evidence-v3-*.json` に記録。DX12/SM6、software Lumen、VSM、同じ3灯/露出-6を使用。影のサンプルはローカルエンジンの変数説明を確認し、SMRT ray count24/samples per ray12へ増やす。リアルタイム性能の受入ではない。

## 検証

検証結果: 2色のUE取り込み、全5枚の新規撮影、目視確認と `verify_v3.cjs` が成功。UE取り込み後の座面厚は16.3665cm、ベース13cm、座面高41cmを確認。ソファ全体の外寸/床原点と各メッシュの専用材質割当も確認。今回はv2で出ていたソケットの縮退三角形警告は出ていない。撮影ログにマテリアルコンパイル失敗/Pythonエラーは検出されず、撮影後は自動終了した。接触影には粒状ノイズが残る。これらは実物照合や性能の受入検査ではない。

## ブラウン生成プロンプト（全文）

参照専用: `artifacts/reference/detail-18.jpg`。

```text
Use case: product-mockup material asset. NEW UE5 v3 upholstery basecolor. Reference image 1 is official PSF SOFA 2P side photo, reference ONLY, use its actual upholstery surface character. Generate ONLY a seamless evenly lit flat square albedo scan representing exactly 34 x 34 cm of that material. Muted medium saddle-brown, neutral brown, darker than orange caramel. Smooth polyester with worn leather-look finish, delicate pale irregular branching micro-crackle network: varied irregular cells 4–12mm wide, a few larger 15mm, narrow faint tan lines, lower color contrast, fine barely visible fibrous stipple inside cells. Large areas should be smooth subdued brown; not noisy mottled suede, not fluffy, not pebbled leather. Match the fine web-shaped cracks seen on the outer arm in reference. No giant damage or deep cracks. Strict diffuse base color only, flat uniform neutral illumination, no specular highlight, no directional lighting, no shadow, no wrinkles, no seams, no geometry, no objects, no text, no vignette. Edge-to-edge seamlessly tileable. High resolution material scan for physically shaded UE5 close-ups; do not generate a sofa render.
```

## ブルーグレー生成プロンプト（全文）

参照専用: `artifacts/reference/detail-26.jpg`。

```text
Use case: product-mockup material asset. NEW independent UE5 v3 basecolor. Input image is official PSF SOFA 2P blue-gray close-up, reference ONLY. Create an edge-to-edge seamless flat albedo scan of a 34cm square of the blue-gray upholstery without the power socket or any objects. Muted dark slate gray-blue polyester leather-look finish. Extremely fine understated dark fibrous stipple, delicate discontinuous lighter hairline micro-crackle in irregular cells, very low contrast smooth blue-gray base. Match the actual photo's character; no coarse pebbles, no fluffy suede, no salt-and-pepper noise, no large mottled stains or visible woven grid. Neutral uniform diffuse light; no specular highlights, shadows, seams, folds, text, logos, geometry, perspective or vignette. Natural-scale subtle details suitable for UE5 close-up physical shading, seamlessly tileable at all edges. This is a fresh texture, not a sofa rendering.
```

