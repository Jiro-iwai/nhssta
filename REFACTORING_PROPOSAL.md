# リファクタリング提案書

**プロジェクト**: nhssta (Statistical Static Timing Analysis)
**バージョン**: 0.3.4
**作成日**: 2025-12-24
**ブランチ**: refactoring-analysis

## 1. エグゼクティブサマリー

nhsstaは、統計的静的タイミング解析（SSTA）を実行する優れた設計のC++17プロジェクトです。791のテスト、包括的なドキュメント、明確なアーキテクチャを持つ高品質なコードベースです。

しかし、以下の領域で改善の余地があります：

1. **グローバルステートの除去** - テスト性と並列性の向上のため
2. **コード重複の削減** - 保守性向上のため
3. **大規模ファイルの分割** - 可読性と保守性向上のため
4. **型安全性の改善** - ランタイムエラーの削減のため
5. **Expression ツリー統合の完了** - 一貫性のある計算パスのため

これらの改善により、コードベースは以下のようになります：
- より保守しやすく
- よりテストしやすく
- より安全で（型安全性）
- より高速（潜在的な最適化機会）

## 2. コードベースの現状分析

### 2.1 強み

- **クリーンなアーキテクチャ**: 責任の明確な分離
- **Handleパターン**: 一貫したメモリ管理
- **包括的なテスト**: 791テスト（72テストスイート）
- **優れたドキュメント**: ユーザーガイド、開発者ガイド、アーキテクチャドキュメント
- **例外安全性**: 適切なエラー報告
- **性能への配慮**: キャッシング、遅延評価

### 2.2 コード統計

```
総ソースコード: ~4,865行（39のC++ソースファイル）
最大ファイル:
  - expression.cpp:     1,147行
  - covariance.cpp:       593行
  - util_numerical.cpp:   499行
  - statistical_functions.cpp: 401行
  - main.cpp:             379行
```

### 2.3 主要コンポーネント

1. **Core Domain Model** (random_variable, normal, add, sub, max, covariance)
2. **Expression Tree & Auto-diff** (expression.cpp)
3. **Circuit Analysis** (parsers, circuit_graph, critical_path_analyzer, sensitivity_analyzer)
4. **SSTA Orchestrator** (ssta.cpp)
5. **CLI Layer** (main.cpp)

## 3. 優先度付きリファクタリング提案

### 優先度 1 (高): グローバルステートの除去

#### 3.1 問題点

**場所**: `src/covariance.cpp:22`

```cpp
static CovarianceMatrix covariance_matrix;
CovarianceMatrix& get_covariance_matrix() {
    return covariance_matrix;
}
```

**影響**:
- 並列テスト実行が困難
- テスト間でステートが共有される（テストの独立性の欠如）
- 並列処理の実装が困難
- ユニットテストでのモック/スタブが困難

#### 3.2 提案解決策

**アプローチ A: Dependency Injection（推奨）**

`CovarianceMatrix`をパラメータとして渡す設計に変更：

```cpp
// Before
double covariance(const RandomVariable& a, const RandomVariable& b) {
    // グローバルな covariance_matrix を使用
}

// After
class CovarianceContext {
    CovarianceMatrix matrix_;
public:
    double covariance(const RandomVariable& a, const RandomVariable& b);
};
```

**メリット**:
- テスト性が大幅に向上
- 並列処理が可能に
- 明示的な依存関係
- テスト用のモック作成が容易

**デメリット**:
- APIの変更が必要（破壊的変更）
- 既存コードの大規模な修正が必要

**影響範囲**:
- `covariance.cpp`: 全面的な修正
- `random_variable.cpp`, `max.cpp`, `add.cpp`, `sub.cpp`: 関数シグネチャの変更
- `circuit_graph.cpp`, `sensitivity_analyzer.cpp`: 呼び出し元の修正
- テストコード: 各テストでContextインスタンスを作成

**実装ステップ**:
1. `CovarianceContext`クラスを作成
2. グローバル関数を`CovarianceContext`のメンバ関数に移行
3. `Ssta`クラスに`CovarianceContext`を保持
4. すべての呼び出し元を更新
5. テストを更新

**アプローチ B: Thread-local Storage（短期的な解決策）**

グローバルステートを維持しつつ、スレッドセーフに：

```cpp
thread_local CovarianceMatrix covariance_matrix;
```

**メリット**:
- 最小限のコード変更
- スレッドセーフ

**デメリット**:
- グローバルステートは依然として存在
- テストの独立性の問題は残る
- 根本的な解決にはならない

**推奨**: アプローチ A（Dependency Injection）

### 優先度 1 (高): コード重複の削減

#### 3.3 問題点

**場所**: `src/covariance.cpp`

`covariance()` (line 510-592) と `cov_expr()` (line 426-508) が同じロジックを2回実装しています。

**重複コードの例**:

```cpp
// covariance() - line 517-535
else if (dynamic_cast<const OpADD*>(a.get()) != nullptr) {
    double cov0 = covariance(a->left(), b);
    double cov1 = covariance(a->right(), b);
    cov = cov0 + cov1;
} else if (dynamic_cast<const OpADD*>(b.get()) != nullptr) {
    double cov0 = covariance(a, b->left());
    double cov1 = covariance(a, b->right());
    cov = cov0 + cov1;
} else if (dynamic_cast<const OpSUB*>(a.get()) != nullptr) {
    // ...同様のパターン
}

// cov_expr() - line 443-458
else if (dynamic_cast<const OpADD*>(a.get()) != nullptr) {
    Expression cov_left = cov_expr(a->left(), b);
    Expression cov_right = cov_expr(a->right(), b);
    result = cov_left + cov_right;
} else if (dynamic_cast<const OpADD*>(b.get()) != nullptr) {
    // ...同じロジック
}
```

**影響**:
- 保守コストが2倍
- バグ修正を2箇所で行う必要
- ロジックの不一致のリスク

#### 3.4 提案解決策

**アプローチ A: テンプレート関数による共通化（推奨）**

計算ロジックをテンプレート化し、`double`と`Expression`の両方で使用：

```cpp
template<typename T>
T covariance_impl(const RandomVariable& a, const RandomVariable& b,
                  std::function<T(const RandomVariable&, const RandomVariable&)> cov_func) {
    // 共通のロジック
    if (a == b) {
        return get_variance<T>(a);
    }
    else if (dynamic_cast<const OpADD*>(a.get()) != nullptr) {
        T cov0 = cov_func(a->left(), b);
        T cov1 = cov_func(a->right(), b);
        return cov0 + cov1;
    }
    // ... 他の操作
}

// 特殊化
double covariance(const RandomVariable& a, const RandomVariable& b) {
    return covariance_impl<double>(a, b, covariance);
}

Expression cov_expr(const RandomVariable& a, const RandomVariable& b) {
    return covariance_impl<Expression>(a, b, cov_expr);
}
```

**メリット**:
- DRY原則に準拠
- 単一の真実の源（Single Source of Truth）
- バグ修正が1箇所で済む

**デメリット**:
- テンプレートコードは複雑になりがち
- コンパイル時間の増加

**アプローチ B: Expression統合の完了**

すべての計算をExpression経由で行い、最後に値を評価：

```cpp
double covariance(const RandomVariable& a, const RandomVariable& b) {
    Expression expr = cov_expr(a, b);
    return expr->value();
}
```

**メリット**:
- 重複完全除去
- Phase C-5の完了
- 一貫した計算パス

**デメリット**:
- Expression評価のオーバーヘッド
- 既存コードの動作変更リスク

**推奨**: アプローチ B（Expression統合の完了）
理由: Phase C-5の目標と一致し、長期的にメンテナンス性が向上

### 優先度 2 (中): 型安全性の改善

#### 3.5 問題点

**場所**: `covariance.cpp`, `max.cpp`, 他多数

`dynamic_cast`の多用によるランタイム型チェック：

```cpp
// covariance.cpp - 多数の dynamic_cast
const auto* a_normal = dynamic_cast<const NormalImpl*>(a.get());
const auto* b_normal = dynamic_cast<const NormalImpl*>(b.get());

if (dynamic_cast<const OpADD*>(a.get()) != nullptr) { ... }
else if (dynamic_cast<const OpSUB*>(a.get()) != nullptr) { ... }
else if (dynamic_cast<const OpMAX*>(a.get()) != nullptr) { ... }
else if (dynamic_cast<const OpMAX0*>(a.get()) != nullptr) { ... }
// ... 10回以上の dynamic_cast
```

**影響**:
- RTTI（Run-Time Type Information）が必須
- パフォーマンスコスト
- タイプミスや処理漏れのリスク
- コンパイル時の型チェックが効かない

#### 3.6 提案解決策

**アプローチ A: Visitor パターン（推奨）**

型判定をVisitorパターンで置き換え：

```cpp
class CovarianceVisitor {
public:
    virtual double visit(const NormalImpl& a, const RandomVariable& b) = 0;
    virtual double visit(const OpADD& a, const RandomVariable& b) = 0;
    virtual double visit(const OpSUB& a, const RandomVariable& b) = 0;
    virtual double visit(const OpMAX& a, const RandomVariable& b) = 0;
    virtual double visit(const OpMAX0& a, const RandomVariable& b) = 0;
};

class RandomVariableImpl {
public:
    virtual double accept_covariance(CovarianceVisitor& visitor,
                                     const RandomVariable& other) const = 0;
};
```

**メリット**:
- コンパイル時型チェック
- 型追加時にコンパイルエラーで検出（処理漏れ防止）
- `dynamic_cast`不要
- Double dispatchで型安全

**デメリット**:
- 大規模なリファクタリングが必要
- Visitorパターンの学習コスト
- コードの複雑性が増す可能性

**アプローチ B: std::variant（C++17）**

`std::variant`でRandomVariableの型を表現：

```cpp
using RandomVariableVariant = std::variant<
    std::shared_ptr<NormalImpl>,
    std::shared_ptr<OpADD>,
    std::shared_ptr<OpSUB>,
    std::shared_ptr<OpMAX>,
    std::shared_ptr<OpMAX0>
>;

double covariance(const RandomVariableVariant& a, const RandomVariableVariant& b) {
    return std::visit(CovarianceVisitor{}, a, b);
}
```

**メリット**:
- 型安全
- `std::visit`による exhaustiveness check
- 現代的なC++アプローチ

**デメリット**:
- 既存のHandleパターンとの統合が困難
- 大規模なアーキテクチャ変更

**推奨**: アプローチ A（Visitorパターン）
理由: 既存のクラス階層と相性が良く、段階的な移行が可能

**代替案**: `dynamic_cast`の現状維持
理由: 機能は正常に動作しており、大規模な変更のリスクを避けたい場合

### 優先度 2 (中): 大規模ファイルの分割

#### 3.7 問題点

**場所**: `src/expression.cpp` (1,147行)

単一のファイルに多くの機能が詰め込まれています：
- 式ノードの実装（Const, Variable, Binary operations）
- 式の評価
- 逆伝播（backward propagation）
- デバッグ機能
- 最適化
- ユーティリティ関数

**影響**:
- 可読性の低下
- 変更時の影響範囲が不明確
- コンパイル時間の増加
- マージコンフリクトのリスク

#### 3.8 提案解決策

**expression.cpp の分割案**:

```
src/expression/
├── expression_base.cpp        (基本クラス、100-150行)
├── expression_nodes.cpp       (Const, Variable, 150-200行)
├── expression_operators.cpp   (Binary operations, 200-250行)
├── expression_functions.cpp   (CustomFunction, 150-200行)
├── expression_backward.cpp    (逆伝播アルゴリズム, 200-250行)
├── expression_optimization.cpp (最適化, 100-150行)
└── expression_debug.cpp       (デバッグ機能, 100-150行)
```

**メリット**:
- 責任の明確な分離
- 可読性の向上
- 変更の影響範囲が限定される
- 並列コンパイルの効率化

**デメリット**:
- ファイル数の増加
- ビルドシステムの変更が必要
- 既存のインクルードパスへの影響

**推奨**: 段階的な分割
1. まず `expression_debug.cpp` を分離（影響が小さい）
2. 次に `expression_backward.cpp` を分離
3. 必要に応じてさらに分割

**covariance.cpp の分割案**:

```
src/covariance/
├── covariance_base.cpp        (基本関数、CovarianceMatrix)
├── covariance_normal.cpp      (Normal-Normal)
├── covariance_linear.cpp      (ADD, SUB)
├── covariance_max.cpp         (MAX)
├── covariance_max0.cpp        (MAX0)
└── covariance_expr.cpp        (Expression版、将来的に統合)
```

### 優先度 3 (低): モダンC++機能の活用

#### 3.9 提案

**現在**: boolフラグによるキャッシュ管理

```cpp
// random_variable.cpp
mutable double mean_;
mutable bool is_set_mean_;

double mean() const {
    if (!is_set_mean_) {
        mean_ = calc_mean();
        is_set_mean_ = true;
    }
    return mean_;
}
```

**改善案**: `std::optional` の使用

```cpp
mutable std::optional<double> mean_cache_;

double mean() const {
    if (!mean_cache_) {
        mean_cache_ = calc_mean();
    }
    return *mean_cache_;
}
```

**メリット**:
- より意図が明確
- 2つの変数が1つに統合
- メモリ効率がやや向上（bool + doubleの代わりにoptional<double>）
- 未初期化値のアクセス防止

**その他のC++17/20機能活用案**:
- `std::span` でArray viewを表現
- `std::string_view` で文字列の一時コピーを削減
- `if constexpr` でテンプレートの条件分岐を改善
- Structured bindings `auto [mean, variance] = ...`

**推奨**: 優先度は低いが、新規コードでは積極的に採用

### 優先度 3 (低): 数値計算の堅牢性向上

#### 3.10 問題点

数値計算のエッジケース処理が散在：

```cpp
// covariance.cpp
if (vz <= 0.0) {
    throw Nh::RuntimeException("variance must be positive");
}

// random_variable.cpp
if (fabs(v) < MINIMUM_VARIANCE) {
    v = MINIMUM_VARIANCE;
}
```

#### 3.11 提案

**数値計算ユーティリティの統一**:

```cpp
namespace NumericalUtils {
    // 分散のバリデーション
    inline void validate_variance(double var, const std::string& context) {
        if (var < 0.0) {
            throw Nh::RuntimeException(context + ": negative variance: " +
                                     std::to_string(var));
        }
    }

    // 安全な平方根
    inline double safe_sqrt(double value, const std::string& context = "") {
        if (value < 0.0) {
            throw Nh::RuntimeException(context + ": sqrt of negative value: " +
                                     std::to_string(value));
        }
        return std::sqrt(std::max(value, MINIMUM_VARIANCE));
    }

    // 相関係数のクランプ
    inline double clamp_correlation(double rho) {
        return std::clamp(rho, -1.0, 1.0);
    }
}
```

**メリット**:
- エラーメッセージの一貫性
- エッジケース処理の標準化
- テストが容易

## 4. 実装ロードマップ

### Phase 1: 基盤改善（2-3週間）

**目標**: テスト性と保守性の向上

1. **グローバルステートの除去**
   - `CovarianceContext`クラスの導入
   - `Ssta`クラスへの統合
   - テストの更新
   - **リスク**: 中程度（多くのファイルに影響）
   - **効果**: 高（テスト性大幅向上）

2. **数値計算ユーティリティの統一**
   - `NumericalUtils`名前空間の作成
   - エラー処理の標準化
   - **リスク**: 低
   - **効果**: 中（コードの一貫性向上）

### Phase 2: コード重複削減（2-3週間）

**目標**: DRY原則の適用

1. **Expression統合の完了（Phase C-5）**
   - `covariance()` を `cov_expr()` ベースに移行
   - 性能テストとベンチマーク
   - **リスク**: 中程度（計算結果の一貫性確認が必要）
   - **効果**: 高（重複コード削減、一貫性向上）

2. **統合後のコード削除**
   - 古い`covariance()`実装の削除
   - Expression版への完全移行
   - **リスク**: 低
   - **効果**: 中（コードベース簡素化）

### Phase 3: 構造改善（3-4週間）

**目標**: アーキテクチャの洗練

1. **ファイル分割**
   - `expression.cpp` の分割
   - `covariance.cpp` の分割
   - ビルドシステムの調整
   - **リスク**: 低（機能変更なし）
   - **効果**: 中（可読性向上）

2. **型安全性の改善（オプション）**
   - Visitorパターンの導入（段階的）
   - 最も影響の大きい箇所から実施
   - **リスク**: 高（大規模な変更）
   - **効果**: 高（型安全性向上）

### Phase 4: モダナイゼーション（継続的）

**目標**: C++17/20機能の活用

1. **std::optionalの採用**
   - キャッシュ機構の置き換え
   - **リスク**: 低
   - **効果**: 低（コードの明確性向上）

2. **その他のモダンC++機能**
   - 必要に応じて導入
   - 新規コードでは積極的に使用
   - **リスク**: 低
   - **効果**: 低（長期的なコード品質向上）

## 5. リスク評価と緩和策

### 高リスク項目

#### 5.1 グローバルステートの除去

**リスク**:
- 大規模なAPI変更
- 既存コードの広範な修正
- 微妙なバグの混入

**緩和策**:
1. 段階的な移行
   - まず新しいAPIを追加（古いAPIと並存）
   - 段階的に移行
   - 古いAPIを削除
2. 包括的なテスト
   - 各ステップで全テストを実行
   - リグレッションテストの追加
3. コードレビュー
   - すべての変更をレビュー
   - ペアプログラミングの活用

#### 5.2 Expression統合の完了

**リスク**:
- 性能低下の可能性
- 計算結果の微妙な差異
- Expressionツリーのメモリオーバーヘッド

**緩和策**:
1. 性能ベンチマーク
   - 統合前後で性能測定
   - 許容できない劣化があれば最適化
2. 数値精度テスト
   - 既存実装との結果比較
   - 許容誤差内であることを確認
3. メモリプロファイリング
   - メモリ使用量の監視
   - 必要に応じてExpression pruni実装

### 中リスク項目

#### 5.3 ファイル分割

**リスク**:
- ビルドの問題
- インクルードの循環依存
- リンクエラー

**緩和策**:
1. 慎重な設計
   - 分割前に依存関係を明確化
   - 前方宣言の活用
2. 段階的な実施
   - 1ファイルずつ分割
   - 各ステップでビルド確認

### 低リスク項目

- 数値計算ユーティリティの統一
- std::optionalの採用
- その他のモダンC++機能

これらは影響範囲が限定的で、既存の機能を壊さずに実施可能。

## 6. 成功基準

### 定量的指標

1. **テストカバレッジ**: 現状維持または向上（現在：791テスト）
2. **コンパイル時間**: 増加幅は10%以内
3. **実行時性能**: 劣化は5%以内（ベンチマークで測定）
4. **コード行数**: 10-15%削減（重複除去により）
5. **サイクロマティック複雑度**: 平均20%削減

### 定性的指標

1. **可読性**: コードレビューでの肯定的フィードバック
2. **保守性**: 新機能追加時の変更箇所の削減
3. **テスト性**: テスト作成時間の短縮
4. **ドキュメント**: アーキテクチャドキュメントの更新

## 7. 推奨アクション

### 即座に実施可能（低リスク、高効果）

1. **数値計算ユーティリティの統一**
   - 実装時間: 1-2日
   - レビュー: 1日
   - テスト: 既存テストで十分

2. **Expression デバッグコードの分離**
   - 実装時間: 2-3日
   - `expression_debug.cpp`を作成
   - リスクほぼゼロ

### 短期的に実施（中リスク、高効果）

3. **グローバルステートの除去**
   - 計画: 1週間
   - 実装: 2週間
   - テスト: 1週間
   - 優先度: 最高

4. **Expression統合の完了（Phase C-5）**
   - 計画: 1週間
   - 実装: 2週間
   - 性能測定とチューニング: 1週間
   - 優先度: 高

### 長期的に検討（高リスク、高効果）

5. **型安全性の改善（Visitorパターン）**
   - 設計: 2週間
   - プロトタイプ: 2週間
   - 段階的実装: 4-6週間
   - 優先度: 中（時間に余裕がある場合）

6. **大規模ファイルの分割**
   - expression.cpp: 1週間
   - covariance.cpp: 1週間
   - 優先度: 中

## 8. 結論

nhsstaは既に高品質なコードベースですが、上記のリファクタリングにより、以下が実現できます：

1. **テスト性の向上**: グローバルステートの除去により、並列テストとモック化が容易に
2. **保守性の向上**: コード重複の削減により、バグ修正と機能追加が容易に
3. **型安全性の向上**: Visitorパターンにより、コンパイル時のエラー検出が可能に
4. **可読性の向上**: ファイル分割により、コードナビゲーションが容易に

**推奨される実施順序**:

1. **Phase 1**: グローバルステート除去 + 数値ユーティリティ統一（最優先）
2. **Phase 2**: Expression統合完了（Phase C-5の完了）
3. **Phase 3**: ファイル分割（時間に余裕がある場合）
4. **Phase 4**: 型安全性改善（長期的な目標）

各フェーズの後に、包括的なテストとベンチマークを実施し、品質と性能を保証します。

---

**次のステップ**:
このリファクタリング提案についてチームで議論し、優先順位と実施タイミングを決定してください。PoC（Proof of Concept）の実装から始めることをお勧めします。

特に**Phase 1の「グローバルステート除去」**は、テスト性と並列性に大きな影響があるため、最優先で取り組むべきです。
