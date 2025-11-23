# 例外クラス統一化の移行計画

## 現在の状況

以下の例外クラスが使用されています：
1. `Ssta::exception` - Sstaクラス内
2. `Parser::exception` - Parserクラス内
3. `Gate::exception` - Gateクラス内
4. `RandomVariable::Exception` - RandomVariable名前空間内
5. `SmartPtrException` - SmartPtrクラス
6. `ExpressionException` - Expressionクラス
7. `Nh::Exception` - 統一例外クラス（既に存在、Normal.Cで使用）

## 移行計画

### Phase 1: Parser::exception → Nh::ParseException / Nh::FileException
- `Parser::checkFile()` → `Nh::FileException`
- `Parser::checkTermination()` → `Nh::ParseException`
- `Parser::unexpectedToken_()` → `Nh::ParseException`
- `Ssta::read_dlib()`と`Ssta::read_bench()`のキャッチ節を更新

### Phase 2: Gate::exception → Nh::RuntimeException
- `_Gate_::delay()` → `Nh::RuntimeException`
- `_Instance_::output()` → `Nh::RuntimeException`
- `Ssta::read_bench()`のキャッチ節を更新

### Phase 3: RandomVariable::Exception → Nh::RuntimeException
- `_RandomVariable_::check_variance()` → `Nh::RuntimeException`
- `_Normal_`コンストラクタ → `Nh::RuntimeException`
- `Ssta::read_bench()`と`Ssta::report()`のキャッチ節を更新

### Phase 4: SmartPtrException → Nh::RuntimeException
- `SmartPtr::operator SmartPtr<U>()` → `Nh::RuntimeException`
- `Ssta::read_bench()`と`Ssta::report()`のキャッチ節を更新

### Phase 5: Ssta::exception → Nh::Exception / Nh::RuntimeException
- `Ssta::node_error()` → `Nh::RuntimeException`
- `Ssta::connect_instances()` → `Nh::RuntimeException`
- `main.C`のキャッチ節を更新

### Phase 6: ExpressionException → Nh::RuntimeException
- `Expression.C`のすべてのthrow → `Nh::RuntimeException`
- 注意: ExpressionExceptionはassert(0)を含むため、慎重に扱う

## テスト戦略

各Phaseで以下を確認：
1. 既存のテストがすべてパスする
2. エラーメッセージの形式が維持される
3. 例外の型が正しく変換される

