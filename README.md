# 余白 / YOHAKU

UE5.8.2を使うWindowsネイティブの実寸インテリア編集アプリです。現行クライアントはC++/Slateで実装し、旧React/Three.js編集画面は退役しました。

## 起動

ルートの `余白を起動.cmd` をダブルクリックしてください。実行ファイルは `build/windows/Yohaku.exe`。UEエディターを開かずに起動できます。フォルダー内のPak/ライブラリーも必要なのでexeだけを移動しないでください。

初回起動時はシェーダー/描画キャッシュの準備に時間がかかることがあります。既存のUE5検証と同じDX12/SM6環境を使用します。

通常配布はShipping版です。開発用のUE Trace通信待受けを含むDevelopment版は通常起動先へ配布しません。開発用のパッケージが必要な場合のみ `tools/build-native.ps1 -Package -Configuration Development -ArchiveDirectory build/windows-dev` を指定します。

UEエディターでソースプロジェクトを開く場合は `apps/ue/Yohaku.uproject` を開き、「Play」で編集画面を実行します。部屋と家具はランタイムのシーンから構成するため、撮影実験のマップを起動する必要はありません。

## 現在使える機能

- 単一の長方形の部屋を平面ドラッグまたは幅/奥行/天井高の数値で設定。
- 床4仕上げ、北/南/東/西の各壁5仕上げ。部屋の寸法を変えても仕上げを保持。
- PSF SOFA 2P v3の検証用候補をプレビューから配置。移動、45度回転、数値位置/角度、複製、削除、ブラウン/ブルーグレーの固定候補版への交換。
- 家具の固定外寸1500x870x820mm、床接地を維持。色交換で中心・向き・寸法を保持。
- 左ドラッグで家具、右ドラッグで視点、Shift+右で視点移動、ホイールで拡大縮小。俯瞰/真上/室内の視点プリセット。
- ドラッグ確定で1件の履歴。Esc中止、Undo/Redo100件。Ctrl+Z/Ctrl+Y、R、Delete。
- ヘッダーの「商品URL」からFLYMEeの掲載外寸・色/仕様候補・対応写真URL・出典/取得日時を非同期取得。未照合情報として表示し、部屋や既存家具を変更しません。写真からの3D自動生成は未実装です。
- 「候補として保存」で掲載情報を出典付きで保存し、「保存済み候補を開く」で再起動後も通信せず確認できます。同じURLと掲載内容は重複させません。保存先は `%LOCALAPPDATA%/Yohaku/ProductCandidates/candidates.yohaku.json`。候補は未照合・モデル未生成のままです。
- 外形箱による重なり/はみ出し候補を表示。自動的に縮小/削除しない。
- 明示ローカル保存と起動時復元。保存先はユーザー別の `%LOCALAPPDATA%/Yohaku/Layouts/current.yohaku.json`。アプリの更新先とは分離。
- 未保存の終了時は保存/破棄/取消を確認。未保存の変更を手動読込で破棄する場合は6秒以内の再クリックで確認。不正な形式/未知の家具版は読み込み全体を拒否して現在のシーンを保持。

PSFモデルは写真参照のコード製試作品で、実物校正・D/Mゲート・正式採用の前です。部屋寸法は入力値で、実測済みとは扱いません。これはローカルの検証用レイアウトであり、本番商品アセットの採用を意味しません。

## 開発と検証

既存UE5.8.2、Visual Studio C++ツールチェーン、Windows SDKを使用。UEの場所を変える場合は環境変数 `YOHAKU_UE_ROOT` で指定できます。

```powershell
powershell -NoProfile -File tools/build-native.ps1 -Package
powershell -NoProfile -File tools/test-native.ps1
powershell -NoProfile -File tools/test-native-network.ps1
npm.cmd ci
npm.cmd test
npm.cmd run typecheck
```

`test-native.ps1` はAutomation4件（シーン/商品抽出/非同期結果の状態/商品候補の保存保全）と、配布exeでの27項目の実行/別プロセス復元4項目を実施します。両色のソファでNanite用の素材設定が配布版に保持されることも検査します。`tools/test-product-native.ps1` は配布exeから実商品URLを取得・候補保存し、別プロセスで通信せず再表示して、結果とシーン/Dirty/履歴の保全を確認します。試験の保存先はartifacts内に隔離し、ユーザーのレイアウトや候補を上書きしません。Windowsでの保存確定には同一ディレクトリーの一時ファイルとMoveFileExWによる置換を使います。

`test-native-network.ps1` は実URL取得/候補保存中の配布プロセスを観測し、TCP待受けとUDPエンドポイントが0件であることを検査します。Windowsのファイアウォール設定は変更しません。

パッケージ作成前に `apps/ue/Tools/check_materials.py` で家具素材の保存済み描画設定と割当を読み取り検査します。不足時はビルドを止めます。新しい参照資産を準備する `prepare_assets.py` では、同じ `material_usage.py` の処理で必要なNanite用設定を保存します。これらは開発用で、配布アプリの実行時にPythonは不要です。

2026-11-08時点でWindowsビルド、ランタイム27項目・別プロセス復元4項目が成功。同一フレームの入力を処理できるよう整理し、配布exeの実画面でドラッグ/Ctrl+Z・Y/部屋描画/床交換/数値入力/クリック配置/保存・GUI再起動復元を確認しました。[実入力確認記録](artifacts/native-desktop-validation.md)を参照。右ドラッグの実入力と、性能・写実品質の全体受入は未完了です。

## ソース構成

| 場所 | 責務 |
|---|---|
| `apps/ue/Source/YohakuCore` | mmシーン、入力検証、コマンド、Undo/Redo、厳格なJSON |
| `apps/ue/Source/Yohaku` | Slate画面、入力、Actor投影、カメラ、ローカル保存 |
| `apps/ue/Content` | 必要なソファ36メッシュx2色、専用材質、固定候補定義、床壁材質、空の編集マップ |
| `apps/ue/Tools` | ネイティブ資産の準備・来歴。アプリ実行時にはPythonへ依存しない |
| `workers/import` | 将来のサーバー用のURL検査/掲載情報抽出と13試験。配布アプリには組み込まない |
| `packages/contracts` | 将来のサーバーAPI契約。ローカル保存形式とは別 |
| `experiments/ue5-sofa` | 元のソファ作成・撮影・来歴の検証記録。アプリ本体ではない |
| `archive` | 退役したWebソースと旧素材。現行ビルドから参照しない |

商品情報抽出器は保持しており、`npm.cmd run product:inspect -- "https://flymee.jp/product/106294/?pi_id=326211"` で実行できます。掲載値の抽出を公式寸法照合や3D自動生成の完成とは扱いません。

[現状と再開手順](.ai/HANDOFF.md)、[UE切替](docs/ADR-003_UE5ネイティブアプリ.md)、[商品URL調査](docs/ADR-004_商品URLのネイティブ調査.md)、[ローカル商品候補](docs/ADR-005_ローカル商品候補.md)の契約を参照してください。公式資料の追加照合、写真からの自動3D化/モデル候補の品質検査・採用、認証・サーバー保存・ジョブ、窓/ドア、多部屋、壁基準交換、詳細形状判定、照明編集、性能受入は未完了です。

削除してよい重複ビルドは [削除対象一覧](build/削除対象一覧.md) に明記しています。通常起動先の `build/windows` は保持してください。



