# 家具素材の描画設定修正

2026-11-08。対象はUE専用v3素材の描画互換性。写真参照モデルの実物校正・形状再作成・新しい素材画像の生成ではない。

## 根拠と変更

- 前段の配布ログで、革/布/縫目/脚/足/電源部品など10素材の `missing usage flag Nanite! Default Material will be used in game` を確認。
- `native-material-audit-before.json`: 72メッシュすべてNanite有効、素材割当は一致するが10素材すべての保存済みNanite usageがfalse。
- 必要なusageを素材側へ保存し再コンパイル。生成経路の `prepare_assets.py` も同じ `material_usage.py` を利用する。重複した別の描画経路を追加しない。
- `native-material-audit-after.json`: 10素材すべてusage有効、72メッシュとの割当一致。
- `native-material-change-evidence.json`: Content内の変更は10素材だけ。形状/テクスチャ/カタログ/マップなどその他資産は修正前のSHA256と一致。修正前の10素材は `material-usage-backup` に保持。
- パッケージ前に読取専用 `check_materials.py` を実行し、不足・割当違いがある場合はビルドを止める。ランタイム試験にも、両色の72パーツで保存済みusageと実行時互換性が保持されることを追加。

## 配布版の検証

- `native-material-package.log`: Shippingビルド/Cook/Stage/Archive成功。
- `native-material-room-tests.log`: 配布exeで27項目・別プロセス復元4項目成功。両色の素材互換性も合格。
- `native-material-bluegray.png`: 配布版の実描画。布の表面・縫目・脚の表示を確認。全素材の写実品質合格と扱わない。
- `native-material-network-tests.log`: 商品URL取得/候補保存成功、15回の採取でTCP待受け/UDPエンドポイント0件。検証保存先は隔離。
- `native-material-render/render.log`: 描画を有効にしたEditor game実行でも27項目成功。Nanite usage不足/代替素材を使うという警告は0件。Shippingではログ出力が限定されるため、この確認を分けて実施。
- `native-material-release.json`: 通常起動先へ配布30ファイルのSHA256一致を確認。部屋と商品候補のSHA256も修正前と一致。通常版の実画面でブラウンの革・縫目・脚、既存の部屋と保存済み1点を確認。ネットワーク許可画面なしで起動。
- 検証用の重複ビルドは `build/削除対象_windows-materials_検証済み`。通常起動先には使用せず、削除理由/照合根拠を内部と削除対象一覧に記載。

通常の部屋編集・商品候補の保存形式とモデル候補の未照合状態は変更しない。
