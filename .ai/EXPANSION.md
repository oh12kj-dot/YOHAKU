# 1/2/3 実装中の状態

ユーザー指示: 残タスク1（商品写真から家具）、2（窓/ドア・多部屋・非長方形）、3（壁距離交換・詳細干渉・通路）まで進める。途中の「続けて」は同じ作業の継続。追加課金なし、既存データ保持、旧実装を残して継ぎはぎにしない。

- ADR-006が今回の契約。既存正規配布 `build/windows` は前段の安定した素材修正版で、今回のソースはまだ配布していない。
- 新コアはSceneDocument/SceneGeometry/SceneCodec。v2の部屋多角形・壁ID・開口・Asset参照・壁距離交換・部品外形警告・通路。旧v1読込を保持。
- Native SceneProjection/RoomController/RoomEditorWidgetを新コアへ変更。ProceduralMeshComponentを導入し、床三角化・開口付き壁・窓/ドア・生成meshを描画。新LocalModelsが不変モデル保管/検証/プロセス実行を担当。ProductResearchに生成操作を追加中。
- 最新 `artifacts/expansion-build.log` のEditorビルド結果を確認し、コア試験→負例補充→実描画→Shipping→独立レビューへ進む。まだ受入完了ではない。
- `artifacts/local-generation-first` は実商品写真からの実生成結果。Hunyuan3D checkpoint SHA360bc281fc956d4acac0c3d36d5ec0ebf8cdddbf4b8892e894d12419388d479b、ComfyUI既存Python3.13/TorchCUDA、RTX4060Ti8GB。形状のみ。初回327204三角形が上限超過したことを観測し、fast-simplificationをローカル専用packagesへ入れて50000目標へ軽量化後、実生成からmesh/manifestまで成功。別商品のPSF流用ではない。
- Workerの正本は `apps/ue/Tools/local-3d-worker.py`。Runtime profileはLOCALAPPDATA/Yohaku/LocalGeneration/runtime.json。checkpointはworkspace/local-models/hunyuan3d、ComfyUI本体/既存環境を変更せず、追加wheelはworkspace/local-runtime/python_packages。
- 残る接続: build-nativeでworkerを配布Toolsへコピー、setup-local-3dでruntime profileを再現可能に保存、生成用無地材質/窓ガラスを作成。UI・保存・モデルPublishの実機縦断試験。部分共有壁の分割/開口対応を整え、通路と部品検査の負例を確認する。
- 既存ユーザー保存hash: layout B04E780F91F104E17615CB65002DB93B3E92C7A1AACE69165F38B74737C2310E、candidates 05DD3052687B3091632F6ADADFC673006E181A13BE164632B132E3CBBA98A1AB。実データへv2を自動保存しない。
- 削除は前段の自動承認拒否を再試行しない。検証済み重複配布は名前と内部文書で削除対象と明示する。
