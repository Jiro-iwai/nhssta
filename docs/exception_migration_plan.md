# 例外クラス統一化の移行計画

**最終更新**: 2025-11-24
**状態**: ✅ 完了

## 移行完了

すべての例外エイリアスが削除され、コードベース全体で統一された`Nh::Exception`階層が使用されています。

### 削除されたエイリアス

1. ✅ `Ssta::exception` - Sstaクラス内（Issue #99で削除）
2. ✅ `Parser::exception` - Parserクラス内（Issue #99で削除）
3. ✅ `Gate::exception` - Gateクラス内（公開API: Issue #99、内部実装: Issue #107で削除）
4. ✅ `RandomVariable::Exception` - RandomVariable名前空間内（Issue #99で削除）
5. ✅ `SmartPtrException` - SmartPtrクラス（削除済み）
6. ✅ `ExpressionException` - Expressionクラス（Issue #107で削除）

### 現在の状況（過去の記録）

以下の例外クラスが使用されていました（すべて削除済み）：
1. `Ssta::exception` - Sstaクラス内
2. `Parser::exception` - Parserクラス内
3. `Gate::exception` - Gateクラス内
4. `RandomVariable::Exception` - RandomVariable名前空間内
5. `SmartPtrException` - SmartPtrクラス
6. `ExpressionException` - Expressionクラス
7. `Nh::Exception` - 統一例外クラス（現在も使用中）

## 移行計画（完了）

### Phase 1: Parser::exception → Nh::ParseException / Nh::FileException ✅
- `Parser::checkFile()` → `Nh::FileException`
- `Parser::checkTermination()` → `Nh::ParseException`
- `Parser::unexpectedToken_()` → `Nh::ParseException`
- `Ssta::read_dlib()`と`Ssta::read_bench()`のキャッチ節を更新

### Phase 2: Gate::exception → Nh::RuntimeException ✅
- `_Gate_::delay()` → `Nh::RuntimeException`
- `_Instance_::output()` → `Nh::RuntimeException`
- `Ssta::read_bench()`のキャッチ節を更新

### Phase 3: RandomVariable::Exception → Nh::RuntimeException ✅
- `_RandomVariable_::check_variance()` → `Nh::RuntimeException`
- `_Normal_`コンストラクタ → `Nh::RuntimeException`
- `Ssta::read_bench()`と`Ssta::report()`のキャッチ節を更新

### Phase 4: SmartPtrException → Nh::RuntimeException ✅
- `SmartPtr::operator SmartPtr<U>()` → `Nh::RuntimeException`
- `Ssta::read_bench()`と`Ssta::report()`のキャッチ節を更新

### Phase 5: Ssta::exception → Nh::Exception / Nh::RuntimeException ✅
- `Ssta::node_error()` → `Nh::RuntimeException`
- `Ssta::connect_instances()` → `Nh::RuntimeException`
- `main.C`のキャッチ節を更新

### Phase 6: ExpressionException → Nh::RuntimeException ✅
- `Expression.C`のすべてのthrow → `Nh::RuntimeException`
- 注意: ExpressionExceptionはassert(0)を含むため、慎重に扱う

### Phase 7: 内部実装ファイルのエイリアス削除 ✅（Issue #107）
- `src/gate.hpp`から`Gate::exception`エイリアスを削除
- `src/expression.hpp`から`ExpressionException`エイリアスを削除

## テスト戦略

各Phaseで以下を確認：
1. 既存のテストがすべてパスする
2. エラーメッセージの形式が維持される
3. 例外の型が正しく変換される

