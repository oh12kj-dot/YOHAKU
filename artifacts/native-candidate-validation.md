# 商品候補保存の検証

2026-11-08。Windows配布版を通常起動先へ反映済み。

- `native-candidates-package.log`: UE5 Editor/Windowsビルド・Cook・Stage・Archive成功。
- `native-core-tests/index.json`: 2026.11.08-07.39.22 UTC、4成功/0失敗。候補の全項目復元、同一内容の重複防止、ハッシュ変更とpi_id変更の別保存、別所有者の追加保持、置換失敗時のバイト/メモリー保持、不正/未知/過大ファイル保全を含む。
- `native-candidates-room-tests.log`: 配布版の部屋編集26項目と別プロセス復元4項目成功。部屋JSON完全一致。
- `native-candidates-live-tests.log`: 実URLを取得して1500×870×820mm/2仕様を候補保存、再保存で重複なし、保存後の再表示に成功。
- `native-candidate-validation/3feb7575-53bb-4097-9e03-67c4b1d36341/candidate-restart.json`: 新しい配布exeプロセスで保存済み候補を通信せず開き、全ProductSource JSONが元の取得結果と完全一致。両プロセスともシーン/Dirty/履歴は変更なし。
- 同ディレクトリーの `product-research.png`: 実描画で保存済み1件、未照合・3Dモデル未生成、寸法/仕様ID/写真URLを表示。保存・候補一覧・再読込ボタンとスクロール領域の配置を目視確認。
- `native-candidates-release.json`: 通常版へ配布54ファイルのSHA256一致を確認。通常版の追加ファイルはUEのSaved内のログ/設定のみ。
- ユーザーの部屋保存SHA256は `B04E780F91F104E17615CB65002DB93B3E92C7A1AACE69165F38B74737C2310E` を保持。試験は部屋と候補の両保存先をartifacts内へ隔離。
- `native-candidate-review.md`: 新しい保存境界の独立ソースレビュー、重大指摘なし。

ユーザーがWindowsネットワーク許可画面を閉じた後、通常版を @oai/sky で操作。掲載情報の取得→候補保存→保存済み一覧の表示→候補選択と再表示を確認。PSF候補ID A5FC07E7-4E51-52E4-C3A9-FBBE9D737AD3、保存日時2026-11-08T08:38:29.504Z。寸法/2仕様/写真URLを表示し、部屋は保存済み1点を保持。部屋保存のSHA256も元と一致。

既存資産を読み込む部屋試験では、一部材質にNanite usage flag不足の警告が記録された。今回の候補保存変更は素材を変更していない。全仕上げと2色を配布版で目視する品質受入の際に調査する。並行別プロセスの同時書込・停電時の耐久性も未実測。
