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

### ハンドル型の設計意図

`RandomVariableHandle`、`ExpressionHandle`、`Gate`、`Instance`、`CovarianceMatrixHandle` などのハンドル型は、**値型のように見えますが、実際には `std::shared_ptr` による共有所有権を持ちます**。

- **コピーは安価**: ハンドルをコピーしても、内部の `std::shared_ptr` が共有されるだけなので、オブジェクト本体はコピーされません
- **共有セマンティクス**: 複数のハンドルが同じオブジェクトを参照できます
- **自動メモリ管理**: 最後のハンドルが破棄されると、オブジェクトが自動的に解放されます

この設計により、グラフ構造（RandomVariable の演算ツリー、Expression の式ツリーなど）を安全かつ効率的に管理できます。

### dynamic_pointer_cast の使用規約

`RandomVariableHandle::dynamic_pointer_cast<T>()` や `ExpressionHandle::dynamic_pointer_cast<T>()` は、**型チェック済みの前提で呼び出す**ことを推奨します。

```cpp
// 推奨: 事前に型チェックしてから使用
if (dynamic_cast<const OpMAX*>(rv.get()) != nullptr) {
    auto m = rv.dynamic_pointer_cast<const OpMAX>();
    // ここでは例外は発生しない
}

// 非推奨: 型チェックなしで直接呼び出す（例外が発生する可能性がある）
auto m = rv.dynamic_pointer_cast<const OpMAX>(); // 型が合わない場合は例外
```

現在のコード（`Covariance.C` など）では、事前に `dynamic_cast` で型チェックしているため、`dynamic_pointer_cast` は例外を投げません。

### レガシー SmartPtr の有効化方法

旧 API に依存するコードをビルドしたい場合、以下のいずれかの方法で `NH_ENABLE_LEGACY_SMARTPTR` を有効化できます：

1. **翻訳単位の先頭で定義**（テスト専用を推奨）:
   ```cpp
   #define NH_ENABLE_LEGACY_SMARTPTR 1
   #include "SmartPtr.h"
   ```

2. **ビルドフラグで指定**:
   ```bash
   make CXXFLAGS="-g -Wall -std=c++17 -DNH_ENABLE_LEGACY_SMARTPTR=1"
   ```
   
   **注意**: ビルドフラグで指定する場合、すべての翻訳単位で一貫して定義する必要があります。一部のファイルだけで定義すると、ODR（One Definition Rule）違反になる可能性があります。

### RCObject の削除計画

`RCObject` の参照カウンタは現在実質未使用です。将来的な削除計画：

- **Phase 3（将来）**: `RCObject` の完全削除を検討
  - すべてのハンドル型が `std::shared_ptr` ベースに移行済みであることを確認
  - `SmartPtr` テンプレートが完全に削除されていることを確認
  - 互換性テストを実行して、レガシーコードへの影響がないことを確認

### テストとライフサイクル検証

ライフサイクルに関する回帰を検知できるよう、以下のテストを追加済みです：

- `test/test_randomvariable.cpp`: `CopySharesUnderlyingStorage`, `OperationResultSurvivesSourceScope`
- `test/test_expression_assert_migration.cpp`: `ExpressionSurvivesOperandScope`, `SharedSubexpressionEvaluatesConsistently`
- `test/test_gate.cpp`: `InstanceRemainsValidAfterGateGoesOutOfScope`, `InstancesShareGateStateAcrossCopies`

さらなるリファクタリングを行う際は、本ファイルを更新し進捗を共有してください。

