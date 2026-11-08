# 配布版の開発用待受け除去

2026-11-08。

## 原因を支持する観測

更新前のDevelopment配布版でTCP `0.0.0.0:1985` を観測。UDPエンドポイントは0件。実プロセス/ポートの記録は `native-listeners-before.json`。UEのログにも `Control listening on port 1985` がある。

UE5.8のエンジンソース `Runtime/TraceLog/Public/Trace/Config.h` は、通常のTracingをShippingで無効にし、TCP Controlもそれに従う。アプリに必要な商品URL取得はWinHTTPの外向きHTTPSであり、この開発用診断ポートを使用しない。

Windowsの拒否イベントとの突合は行っていないため、画面表示との因果を直接証明したとはしない。検証対象は、不要な待受けそのものの除去とアプリ機能の保持。

## 変更と検証方針

`tools/build-native.ps1 -Package` の標準をShippingへ変更。Editorの開発用ビルドは維持し、必要時のみ `-Configuration Development` を指定できる。エンジンの共有バイナリーやWindowsの設定は変更しない。まずビルド形式だけを変更して比較し、根拠なく通信プラグインを追加で無効化しない。

`tools/test-native-network.ps1` は隔離した保存先で配布版を起動し、起動したプロセスとその実行子プロセスについてTCP Listen/UDPエンドポイントを複数回採取。実子プロセスの観測、3回以上の採取、通信待受け0件、実URLの取得/候補保存成功を必須とする。調査API自体の失敗もエラーとし、未観測を成功と扱わない。

## 結果

- `native-shipping-package.log`: Shippingビルド/Cook/Stage/Archive成功。
- `native-shipping-network-tests.log`: 実子プロセスを含む34回の観測でTCP Listen/UDPエンドポイントがいずれも0件。実商品URLの取得・候補保存も成功。
- `native-network-validation/9c6b2840-6e1d-4e2b-9b09-91ab300cac91/network-evidence.json`: 観測時刻・所有プロセス・TCP/UDP全採取結果、exitCode0。`product-evidence.json` は掲載外寸/2仕様/部屋とDirty/履歴の保全。
- `native-shipping-room-tests.log`: Shipping配布exeで部屋編集26項目・別プロセス復元4項目、部屋JSON完全一致が成功。
- `native-shipping-product-tests.log`: 実URLの取得・候補保存と、別プロセスで通信せず商品情報を完全復元。部屋/Dirty/履歴を保持。証拠は `native-candidate-validation/eb262667-ca72-4637-affb-30cb8b0b28f6`。
- `native-shipping-release.json`: 通常起動先をShippingへ切替。配布30ファイルのSHA256一致、既存の部屋と商品候補のSHA256不変を確認。旧Development版は削除対象名へ退避し、現在の配布フォルダーに古い実行ファイルを残さない。
- `native-shipping-normal-network.json`: 通常起動先でもTCP待受け/UDPエンドポイント0件。通常の画面起動を @oai/sky で確認し、この起動ではWindowsのネットワーク許可画面は表示されなかった。保存済みの部屋5000×4000×2600mm、ブラウンソファ1点を復元。

待受け0件の結果は採取した起動と通信中の観測範囲を示し、すべてのWindows環境で将来の許可画面が出ないことの保証とは区別する。
