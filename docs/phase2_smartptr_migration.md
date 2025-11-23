# Phase 2: SmartPtr Migration Summary

最終更新日: 2025-11-24

## ゴール

- プロジェクト全体を段階的に `std::shared_ptr` / `std::unique_ptr` ベースへ移行し、カスタム `SmartPtr` 実装への依存を解消する
- ライフサイクルのテストを整備して、移行後も統計計算の挙動が変わらないことを保証する

## 完了した主な作業

| 対象 | 状態 | 補足 |
| ---- | ---- | ---- |
| `Gate` / `Instance` | ✅ 完了 | `std::shared_ptr` 化し、新旧 API の互換層を整備 |
| `RandomVariable` 系 (`Normal`, `OpADD`, `OpMAX`, `OpSUB`, `CovarianceMatrix`) | ✅ 完了 | ハンドル型を `std::shared_ptr` 薄ラッパに刷新、`CovarianceMatrix` キャッシュも同様 |
| `Expression` 系 (`Const`, `Variable`, `Expression_`) | ✅ 完了 | `std::enable_shared_from_this` を採用し、派生オブジェクトからも `shared_ptr` を復元可能 |
| `SmartPtr` テンプレート | ⚠️ レガシー | デフォルト無効 (`NH_ENABLE_LEGACY_SMARTPTR=1` の場合のみ利用可能)。互換用の `RCObject` のみ温存 |
| ドキュメント整備 | ✅ 完了 | `README.md` / `CONTRIBUTING.md` にメモリ方針を追記、本ファイルで移行状況を集約 |

## 今後の方針

1. `RCObject` の参照カウンタはすでに実質未使用のため、将来的に段階的削除を検討
2. 新規コードでレガシー `SmartPtr` を利用することを禁止（CI でも検出可能なようにフラグ化）
3. 追加のハンドル型が必要な場合は、`RandomVariable` / `Expression` で採用している薄い `std::shared_ptr` ラッパを再利用

## 開発者向けメモ

- 旧 API に依存するコードをビルドしたい場合のみ、対象の翻訳単位の先頭で `#define NH_ENABLE_LEGACY_SMARTPTR 1` を定義してください（テスト専用を推奨）
- ライフサイクルに関する回帰を検知できるよう、`test/test_randomvariable.cpp`・`test/test_expression_assert_migration.cpp` などにフェイルファストなシナリオを追加済みです
- さらなるリファクタリングを行う際は、本ファイルを更新し進捗を共有してください

