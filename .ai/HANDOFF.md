# YOHAKU 現在の引継ぎ

更新: 2026-11-08。UE5本体への移行、旧Web編集画面の退役、実入力修正、FLYMEe商品URLのネイティブ調査に加え、ローカル商品候補の保存/オフライン再表示を実装。通常配布をShippingへ切替え、開発用通信待受けを除去。家具10素材のNanite用設定を保存し、代替素材の警告を解消。アプリ全体の要求完了とは扱わない。

## 起動と正本

- `余白を起動.cmd` → `build/windows/Yohaku.exe`。Windows Shipping版のフォルダー全体が必要。UEエディター不要。
- ソース: `apps/ue/Yohaku.uproject`。UE5.8.2 / MSVC14.44 / Windows SDK10.0.22621。
- 設計: 基本1.8・詳細1.4、ADR-003（本体移行）、ADR-004（商品調査）、ADR-005（ローカル商品候補）。以前の監査文書は過去版の記録であり、今回の実装合格と扱わない。

## 現在の実装

- 1室の長方形を対角ドラッグ/数値で変更。床4仕上げ・各壁5仕上げと安定した5面ID。
- PSF v3の検証用候補2色。プレビュー/クリック配置、掴み位置を保つ移動、45度回転、数値位置/角度、複製/削除、色交換。固定外寸1500x870x820mm/床接地を維持。
- 保存mm/0.001mm、描画UE cm。NORTH=z=depth、SOUTH=z=0、EAST=x=width、WEST=x=0、UE yawは保存yawの符号反転。
- 左は家具、右はカメラ、Shift+右は視点移動、ホイールは拡縮。俯瞰/真上/室内プリセット。InputKeyで離散イベント順を保持し、旧BindKey経路は削除。3px未満のクリックはスナップ移動させない。
- プレビューは確定シーンから分離。終了時1履歴、Esc取消、Undo/Redo100件、通常編集でRedo枝破棄。フォーカス喪失で未確定操作と修飾キーを解除。外形箱の重なり/はみ出し候補を警告し、勝手に縮小/削除しない。
- バージョン付きJSONを明示保存・起動時復元。保存先 `%LOCALAPPDATA%/Yohaku/Layouts/current.yohaku.json`。同一ディレクトリーGUID一時ファイルからMoveFileExWで置換。未保存終了は保存/破棄/取消、手動読込は再クリック確認。不正形式/未知家具版は全体拒否し現在のシーンを保持。履歴は保存しない。
- ヘッダー「商品URL」でFLYMEeの外寸/仕様候補/対応写真URL/別仕様注記/出典/UTC時刻/SHA256を非同期取得。固定HTTPSホスト/商品パス/任意pi_id一つ、転送禁止、2MiB、12秒期限、同時1件。取消/URL変更/所有者終了/古い完了を防御する。調査パネル内のマウス/キー入力は背景編集へ通さない。
- 商品調査はSceneDocument/Dirty/履歴/既存モデルを変更しない。single-source-unverified・not-generated。pi_idと仕様IDの対応を推測しない。Node/Python/ブラウザーランタイムなしで動く。
- 「候補として保存」で出典/取得時刻/本文SHA256/外寸/全仕様と写真URLを別のローカル候補ファイルへ明示保存。同じURL+本文ハッシュは重複させず、変更内容は新しい候補。保存済み一覧から通信せず再表示。100件/4MiB・厳格な全体読込・同一保存先の排他/最新再読込・原子的置換。不正ファイルは保持して上書き禁止。候補の削除/編集/正式採用は未実装。
- 商品候補の保存先 `%LOCALAPPDATA%/Yohaku/ProductCandidates/candidates.yohaku.json`。試験は `-YohakuCandidateRoot=...` で部屋とは独立して隔離する。

PSFは写真参照のコード製モデルとUE専用生成素材。実物校正/正式採用前のstudy候補。ローカル形式 `yohaku-local-study` version1は将来の本番SceneSnapshotとは別。旧Webレイアウトの無断読み替えはしない。

## ソースの境界と退役

- `apps/ue/Source/YohakuCore`: SceneDocument、検証/外形警告、JSON/履歴、ProductSourceのURL/HTML抽出と試験。
- `apps/ue/Source/Yohaku`: UE投影、入力、Slate、固定FurnitureDefinition、LayoutStorage、WinHTTPのProductResearch、独立したProductCandidateStorageと試験。
- `apps/ue/Content`: v3ソファ/専用素材/仕上げ。撮影スタジオや旧マップを本体へ持ち込まない。未使用代替材質12件は削除済み。
- `workers/import`: 将来のサーバー用抽出器と13試験、配布しない。CLI内の重複HTTP処理はfetchProductへ統一。ネイティブとの共有試験資料は `tests/fixtures/flymee-product.html`。`npm run product:inspect -- URL [new-output.json]`。
- `packages/contracts`: 将来のサーバー契約。認証/サーバー/ジョブは未接続。
- `experiments/ue5-sofa`: 元の生成/調査/比較の記録であり現行アプリではない。

旧React/Three.js画面・Vite API・旧scene-domain/geometry-kernel・Web試験/生成JS/HTMLは現行ソース/起動経路から除去。React依存と5173の旧プロセスも退役済み。`archive/web-editor-2026-11-08.zip` の32元ファイルをSHA256照合して保管。比較用アルベドは `artifacts/materials/web-study-v2`。商品写真/出典/生成来歴は保持。Three.jsは過去のモデル作成ツール用のみ。

## 検証と残る境界

- `artifacts/native-material-package.log`: 最新Windows Shipping build/cook/stage/archive成功。Editorは引き続きDevelopment。通常配布は `tools/build-native.ps1 -Package`、開発用の配布が必要な場合だけ別の出力先へ `-Configuration Development` を指定する。
- `artifacts/native-material-release.json`: 素材修正を通常起動先へ反映し、配布30ファイルのSHA256一致を確認。元のDevelopment版はフォルダーごと退役し、現行フォルダーに旧実行ファイルを残していない。部屋保存SHA256 B04E780F91F104E17615CB65002DB93B3E92C7A1AACE69165F38B74737C2310E、商品候補SHA256 05DD3052687B3091632F6ADADFC673006E181A13BE164632B132E3CBBA98A1ABを保持。
- `artifacts/native-network-validation.md`: DevelopmentでTCP 0.0.0.0:1985を実測→Shippingへ切替→実商品取得/候補保存中の34回の観測でTCP待受け/UDPエンドポイント0件。Windowsの許可ルール/通知は変更していない。通常版の実画面でも許可画面なしで起動、既存の部屋1点を復元。現在の待受け0件は `native-shipping-normal-network.json`。全環境・将来の許可画面の不存在保証とは区別する。
- `artifacts/native-material-room-tests.log`: 配布版27ランタイム項目（両色72パーツの保存済み素材usageと実行時互換性を含む）と別プロセス復元4項目成功。JSONのID/仕様/位置/向き/仕上げの完全一致。保存先はartifacts内へ隔離。
- `artifacts/native-shipping-product-tests.log` と `artifacts/native-candidate-validation/eb262667-ca72-4637-affb-30cb8b0b28f6`: 実URLを取得し1500x870x820mm/2色、候補保存/重複防止/再表示、別プロセスで通信せず全商品JSONを完全復元。シーン/Dirty/履歴非変更。
- `artifacts/native-core-tests/index.json`: 直近のEditor Automation4件成功（シーン/商品抽出/非同期状態/候補保存）。今回の追加変更は素材10件の描画設定、資産準備/検査スクリプトとランタイムの素材検査。アプリの保存形式/編集コマンド/通信処理は変更なし。サーバー抽出13試験と型検査も前段で成功。
- 実入力の根拠は `native-desktop-validation.md`、`native-product-desktop-validation.md`、`native-candidate-validation.md`。候補の取得/保存/一覧選択/再表示まで通常版で確認済み。保存境界/商品通信/候補保存は各一回の独立レビューで重大指摘なし。
- `artifacts/native-material-network-tests.log`: 最新の素材修正版でも実商品取得/候補保存成功、15回の採取でTCP待受け/UDPエンドポイント0件。両保存先は隔離。商品候補の別プロセス完全復元は直前のShipping版で検証済み。
- 重複/旧配布フォルダーは `build/削除対象一覧.md` と各内部の説明に明記。通常版 `build/windows` は削除対象ではない。自動承認で削除が拒否されたため、削除は再試行せず名前で区別している。
- TLS異常/転送応答/取消時刻の実回線試験、同時別プロセスの候補書込/停電耐久、右ドラッグ/Shift+右の実入力、全仕上げ目視、写実品質/20点FPS/長時間GPU安定性は未完了。
- `artifacts/native-material-validation.md`: Nanite有効72メッシュに対し10素材のusageがfalseであることを確認して修正。修正後は全割当/usage一致、変更資産は10素材だけで形状/テクスチャ等のその他資産はSHA256不変。描画付きのEditor game試験27項目でも不足/代替素材警告0件。配布版のブルーグレーは `native-material-bluegray.png`、通常版のブラウンは実画面で確認。全写実品質合格とは扱わない。パッケージ前に読取専用の素材検査を追加。

## 次の作業

1. 保存した商品候補を公式追加資料の照合とモデル生成/品質ゲートへ進める。写真から任意商品の自動3D化は未実装。既存studyモデルを無関係なURLのモデルとして流用せず、仕様と色の根拠を固定する。有料API接続を既定にしない。
2. 窓/ドア、多部屋、壁基準交換、詳細形状判定、照明編集、性能/写実受入、将来の認証/保存API/ジョブ。

```powershell
powershell -NoProfile -File tools/build-native.ps1 -Package
powershell -NoProfile -File tools/test-native.ps1
powershell -NoProfile -File tools/test-product-native.ps1
powershell -NoProfile -File tools/test-native-network.ps1
npm.cmd test
npm.cmd run typecheck
```

Git初期化済み。originは https://github.com/oh12kj-dot/YOHAKU.git。クラウド配備/有料3D生成APIは実行していない。
