# PSF SOFA v2 素材画像と生成条件

方式: 内蔵`image_gen`。CLIや外部のfal APIは使用していない。

出力は3Dモデルの画像ではなく、ブラウザー/GLBへ実際に使うアルベド画像2枚。形状は写真参照のコード製のまま。生成画像は実物のスキャンや測定結果ではなく、写真からの近似である。

## 保存先

- `apps/web/public/textures/psf-study-v2/brown-albedo.png`
- `apps/web/public/textures/psf-study-v2/blue-gray-albedo.png`
- 同ディレクトリーの`provenance.json`: 画像寸法、SHA-256、参照写真、来歴。

各部品のUVは約340mm角に1画像となる実寸尺度で割り当てる。ブラウザーで画像の濃淡から法線/粗さマップを計算する。これは実測PBRではない。法線は標準glTF normalTexture、粗さは標準metallicRoughnessTextureとしてGLBへ書き出し、旧版のEXT_materials_bumpへの依存をなくす。GLBの画像は最大1024pxに制限する。

## ブラウンの生成プロンプト

参照: `artifacts/reference/psf-1.jpg`（全体）と`detail-18.jpg`（側面）。

```text
Create a production texture asset for a 3D sofa material, not a sofa render. Input images are material references only: the exact brown leather-look water-repellent polyester upholstery of the PSF SOFA 2P. Output one square seamless tileable 1024x1024 PBR BASE COLOR / ALBEDO texture, flat orthographic scan of this upholstery covering about 30cm x 30cm. Match the reference's medium dark warm cognac/russet brown, fine irregular pale branching crackle/vein pattern and tiny fine dense micro-grain, subtle darker mottling. This is leather-look polyester fabric: supple, dry, slightly brushed, matte to low satin; no large woven checker pattern. The pale vein lines are hairline, thin and irregular, branching in varied small cells roughly 3-10mm across; do not make reptile scales. Even diffuse cross-polarized illumination, remove all directional shadows, highlights, folds, perspective, seams, edges, cushions, furniture, stitching, text and borders. Edge-to-edge consistent scale, seamless at all edges. Preserve nuanced realistic surface detail, restrained contrast and natural subdued color. Only the flat brown upholstery albedo texture should fill the entire image.
```

## ブルーグレーの生成プロンプト

参照: 生成したブラウン画像、`artifacts/reference/detail-26.jpg`（電源部付近の張地）、`detail-23.jpg`（背面）。

```text
Generate a single square seamless tileable 1024x1024 PBR BASE COLOR / ALBEDO texture for the BLUE-GRAY version of PSF SOFA 2P leather-look polyester upholstery. Image 1 is the exact brown texture pattern to follow in scale and complexity, images 2 and 3 are references for the blue-gray color and fine dense micrograin on the real sofa. Keep the dense tiny granular fabric pores and very fine irregular branching crackle veins, but change the color to the real reference's deep desaturated navy-slate/charcoal blue-gray. This is a flat material swatch, filling the whole frame, diffuse cross-polarized even lighting with no shadows, no gloss reflections baked into albedo, no seams, no folds, no furniture, no socket, no text or border. The branching veins should be subtle low-contrast slate, not bright white, not embossed crocodile scales. Overall subdued, dark blue-gray leather-look synthetic upholstery surface, slightly mottled, seamless at all edges. Do not produce an image of a sofa. Produce only the seamless base color material texture.
```

## 形状の改修と検証

参照写真と照合して、肘を上部が丸い押出断面へ、背クッションと座面を広い面にも頂点がある形状へ変更した。RoundedBoxGeometryのsegmentsは主に角の丸みで、中央の大きな面へ変形用グリッドを追加しないことをインストール済みThree.jsの実装から確認した。面全体のふくらみ、端の寄りじわ、座面の沈み込みを作り、位置を共有する頂点の法線を平均する。

座面の区画/縫い目、背板の高さ、脚の内寄せ、前面電源部の輪郭も写真へ近づけた。個々の微細なしわや裏面構造は推定を含む。外寸1500×870×820mmと床原点を維持し、正式な配置アセットには採用していない。

影のカメラ範囲とバイアスをm単位のソファに合わせて補正した。光源位置・強度は旧ブラウザー版と同じ。UIに「質感を拡大」を追加し、読み込みに失敗した素材を成功扱いせず表示する。色切替の後着結果で別の色を上書きしない。

GLBは`artifacts/psf-sofa-study-{brown,blue-gray}-v2.glb`へ別名で保存。旧GLBとUE5検証プロジェクトは変更していない。比較用の旧スクリーンショットは`artifacts/sofa-study-v1-{brown,blue-gray}.png`。
