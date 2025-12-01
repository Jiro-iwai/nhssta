# Issue #188: 5項演算子による `expected_prod_pos_expr()` の最適化提案

## 概要

`expected_prod_pos_expr()` を5項演算子として実装することで、ノード数を大幅に削減します。

## 現状の問題

### 現在の実装

`expected_prod_pos_expr()` は Expression tree を構築して計算を行っています：

```cpp
Expression expected_prod_pos_expr(const Expression& mu0, const Expression& sigma0,
                                  const Expression& mu1, const Expression& sigma1,
                                  const Expression& rho) {
    Expression a0 = mu0 / sigma0;  // 1ノード
    Expression a1 = mu1 / sigma1;  // 1ノード
    Expression one_minus_rho2 = one - rho * rho;  // 2ノード
    Expression sqrt_one_minus_rho2 = sqrt(one_minus_rho2);  // 1ノード
    Expression Phi2_a0_a1 = Phi2_expr(a0, a1, rho);  // 1ノード + 内部でさらにノード
    Expression phi_a0 = phi_expr(a0);  // 1ノード
    Expression phi_a1 = phi_expr(a1);  // 1ノード
    Expression Phi_cond_0 = Phi_expr((a0 - rho * a1) / sqrt_one_minus_rho2);  // 複数ノード
    Expression Phi_cond_1 = Phi_expr((a1 - rho * a0) / sqrt_one_minus_rho2);  // 複数ノード
    Expression phi2_a0_a1 = phi2_expr(a0, a1, rho, one_minus_rho2, sqrt_one_minus_rho2);  // 複数ノード
    Expression term1 = mu0 * mu1 * Phi2_a0_a1;  // 複数ノード
    Expression term2 = mu0 * sigma1 * phi_a1 * Phi_cond_0;  // 複数ノード
    Expression term3 = mu1 * sigma0 * phi_a0 * Phi_cond_1;  // 複数ノード
    Expression term4 = sigma0 * sigma1 * (rho * Phi2_a0_a1 + one_minus_rho2 * phi2_a0_a1);  // 複数ノード
    Expression result = term1 + term2 + term3 + term4;  // 複数ノード
    
    return result;  // 合計: 約79ノード
}
```

### 問題点

- **ノード数が多い**: 1回の呼び出しで約79ノードが作成される
- **s27.bench での影響**: 12回呼び出されるため、合計で約948ノード（12回 × 79ノード）
- **キャッシュが効きにくい**: 異なる Expression オブジェクトで呼び出されるため、ポインタベースキャッシュのヒット率が低い

## 提案: 5項演算子の導入

### 基本方針

`expected_prod_pos_expr()` を5項演算子 `EXPECTED_PROD_POS(mu0, sigma0, mu1, sigma1, rho)` として実装します。

### 期待される効果

- **ノード数の削減**: 79ノード → 1ノード（**98.7%削減**）
- **s27.bench での削減**: 948ノード → 12ノード（**936ノード削減**）
- **メモリ使用量の削減**: Expression tree の構築を回避

## 実装方針

### 1. `Op` enum への追加

```cpp
// src/expression.hpp
enum Op { 
    CONST = 0, PARAM, PLUS, MINUS, MUL, DIV, POWER, EXP, LOG, ERF, SQRT, PHI2,
    EXPECTED_PROD_POS  // 新規追加
};
```

### 2. `ExpressionImpl` へのメンバ追加

現在は `third_` までしかないため、4番目と5番目の引数を保持するメンバを追加：

```cpp
// src/expression.hpp
class ExpressionImpl {
    // ...
    Expression left_;   // mu0
    Expression right_;  // sigma0
    Expression third_;  // mu1
    Expression fourth_; // sigma1 (新規追加)
    Expression fifth_;  // rho (新規追加)
    // ...
};
```

**代替案**: `std::array<Expression, 5>` や `std::vector<Expression>` を使用する方法もありますが、既存のコードとの一貫性を保つため、個別のメンバ変数として追加することを推奨します。

### 3. コンストラクタの追加

```cpp
// src/expression.hpp
ExpressionImpl(const Op& op, 
               const Expression& first, const Expression& second,
               const Expression& third, const Expression& fourth,
               const Expression& fifth);
```

**実装時の注意点**:
- `eTbl()` への登録
- 各子ノードへの `add_root()` 呼び出し
- デストラクタでの `remove_root()` 呼び出し

### 4. `value()` メソッドの実装

```cpp
// src/expression.cpp
double ExpressionImpl::value() {
    if (!is_set_value()) {
        if (op_ == EXPECTED_PROD_POS) {
            double mu0_val = left()->value();
            double sigma0_val = right()->value();
            double mu1_val = third()->value();
            double sigma1_val = fourth()->value();
            double rho_val = fifth()->value();
            
            // util_numerical.cpp の expected_prod_pos() を使用
            value_ = RandomVariable::expected_prod_pos(
                mu0_val, sigma0_val, mu1_val, sigma1_val, rho_val);
            is_set_value_ = true;
        }
        // ...
    }
    return value_;
}
```

**実装時の注意点**:
- `util_numerical.cpp` の `expected_prod_pos()` 関数を利用
- エラーハンドリング: `sigma0 <= 0` や `sigma1 <= 0` の場合の処理
- `rho` のクランプ処理（`util_numerical.cpp` 内で既に実装済み）

### 5. `propagate_gradient()` メソッドの実装

**最も重要な実装部分**: 解析的勾配を実装する必要があります。

#### オプション1: 解析的勾配（推奨）

`E[D0⁺ D1⁺]` の公式から各パラメータに対する偏微分を導出：

```cpp
// src/expression.cpp
void ExpressionImpl::propagate_gradient() {
    if (op_ == EXPECTED_PROD_POS) {
        double mu0_val = left()->value();
        double sigma0_val = right()->value();
        double mu1_val = third()->value();
        double sigma1_val = fourth()->value();
        double rho_val = fifth()->value();
        
        // 解析的勾配を計算
        // ∂E/∂μ0, ∂E/∂σ0, ∂E/∂μ1, ∂E/∂σ1, ∂E/∂ρ
        
        // 公式:
        // E[D0⁺ D1⁺] = μ0 μ1 Φ₂(a0, a1; ρ)
        //            + μ0 σ1 φ(a1) Φ((a0 - ρa1)/√(1-ρ²))
        //            + μ1 σ0 φ(a0) Φ((a1 - ρa0)/√(1-ρ²))
        //            + σ0 σ1 [ρ Φ₂(a0, a1; ρ) + (1-ρ²) φ₂(a0, a1; ρ)]
        // where a0 = μ0/σ0, a1 = μ1/σ1
        
        double a0 = mu0_val / sigma0_val;
        double a1 = mu1_val / sigma1_val;
        double one_minus_rho2 = 1.0 - (rho_val * rho_val);
        double sqrt_one_minus_rho2 = std::sqrt(std::max(1e-12, one_minus_rho2));
        
        // 各項の勾配を計算（複雑な実装が必要）
        double grad_mu0 = ...;
        double grad_sigma0 = ...;
        double grad_mu1 = ...;
        double grad_sigma1 = ...;
        double grad_rho = ...;
        
        left()->gradient_ += upstream * grad_mu0;
        right()->gradient_ += upstream * grad_sigma0;
        third()->gradient_ += upstream * grad_mu1;
        fourth()->gradient_ += upstream * grad_sigma1;
        fifth()->gradient_ += upstream * grad_rho;
    }
}
```

**実装の複雑さ**:
- 各項の偏微分を手動で実装する必要がある
- チェインルールを適用する必要がある
- `Φ₂`, `φ`, `Φ` の勾配を考慮する必要がある

**参考**: 既存の `PHI2` 演算子の勾配実装（`src/expression.cpp:298-308`）を参考にする

#### オプション2: 数値微分（実装が簡単だが精度・速度のトレードオフ）

```cpp
void ExpressionImpl::propagate_gradient() {
    if (op_ == EXPECTED_PROD_POS) {
        double mu0_val = left()->value();
        double sigma0_val = right()->value();
        double mu1_val = third()->value();
        double sigma1_val = fourth()->value();
        double rho_val = fifth()->value();
        
        constexpr double eps = 1e-6;
        
        // 数値微分
        double grad_mu0 = (RandomVariable::expected_prod_pos(mu0_val + eps, sigma0_val, mu1_val, sigma1_val, rho_val) -
                           RandomVariable::expected_prod_pos(mu0_val - eps, sigma0_val, mu1_val, sigma1_val, rho_val)) / (2 * eps);
        // ... 他のパラメータも同様
        
        left()->gradient_ += upstream * grad_mu0;
        // ...
    }
}
```

**問題点**:
- **精度**: 有限差分の誤差が発生する
- **速度**: 各パラメータごとに2回の関数評価が必要（合計10回の関数評価）
- **数値安定性**: `eps` の値の選択が難しい

#### オプション3: ハイブリッド（Expression を内部で使用）

勾配計算時だけ Expression tree を構築する方法：

```cpp
void ExpressionImpl::propagate_gradient() {
    if (op_ == EXPECTED_PROD_POS) {
        // 一時的に Expression tree を構築して勾配を計算
        Variable v_mu0, v_sigma0, v_mu1, v_sigma1, v_rho;
        v_mu0 = left()->value();
        v_sigma0 = right()->value();
        v_mu1 = third()->value();
        v_sigma1 = fourth()->value();
        v_rho = fifth()->value();
        
        Expression e = expected_prod_pos_expr_old(v_mu0, v_sigma0, v_mu1, v_sigma1, v_rho);
        e->backward();
        
        left()->gradient_ += upstream * v_mu0->gradient();
        right()->gradient_ += upstream * v_sigma0->gradient();
        third()->gradient_ += upstream * v_mu1->gradient();
        fourth()->gradient_ += upstream * v_sigma1->gradient();
        fifth()->gradient_ += upstream * v_rho->gradient();
    }
}
```

**問題点**:
- ノード削減の効果が薄れる（勾配計算時にノードが作成される）
- 値計算時は1ノードだが、勾配計算時に79ノードが作成される可能性がある

### 6. `expected_prod_pos_expr()` 関数の変更

```cpp
// src/expression.cpp
Expression expected_prod_pos_expr(const Expression& mu0, const Expression& sigma0,
                                  const Expression& mu1, const Expression& sigma1,
                                  const Expression& rho) {
    // キャッシュチェック（ポインタベース）
    auto key = std::make_tuple(mu0.get(), sigma0.get(), mu1.get(), sigma1.get(), rho.get());
    auto it = expected_prod_pos_expr_cache.find(key);
    if (it != expected_prod_pos_expr_cache.end()) {
        expected_prod_pos_cache_hits++;
        return it->second;
    }
    
    expected_prod_pos_cache_misses++;
    
    // 5項演算子として1ノードだけ作成
    Expression result = Expression(std::make_shared<ExpressionImpl>(
        ExpressionImpl::EXPECTED_PROD_POS, mu0, sigma0, mu1, sigma1, rho));
    
    // キャッシュに保存
    expected_prod_pos_expr_cache[key] = result;
    
    return result;
}
```

## 実装手順

### フェーズ1: 基本構造の実装

1. **`Op` enum に `EXPECTED_PROD_POS` を追加**
   - `src/expression.hpp` の `enum Op` に追加

2. **`ExpressionImpl` に `fourth_` と `fifth_` メンバを追加**
   - `src/expression.hpp` に追加
   - デフォルトコンストラクタで初期化

3. **5項演算子用のコンストラクタを追加**
   - `src/expression.hpp` に宣言を追加
   - `src/expression.cpp` に実装を追加
   - `eTbl()` への登録と `add_root()` の呼び出し

4. **デストラクタの更新**
   - `fourth_` と `fifth_` の `remove_root()` を追加

### フェーズ2: 値計算の実装

5. **`value()` メソッドに `EXPECTED_PROD_POS` の処理を追加**
   - `util_numerical.cpp` の `expected_prod_pos()` を使用
   - エラーハンドリングを追加

6. **`expected_prod_pos_expr()` 関数を変更**
   - 5項演算子を使用するように変更
   - キャッシュの動作を確認

### フェーズ3: 勾配計算の実装

7. **`propagate_gradient()` メソッドに `EXPECTED_PROD_POS` の処理を追加**
   - 解析的勾配を実装（推奨）
   - または数値微分を実装（簡単だが精度・速度のトレードオフ）

8. **テストの追加・更新**
   - 既存のテストが通ることを確認
   - 勾配計算のテストを追加

### フェーズ4: 検証と最適化

9. **ノード数の測定**
   - `s27.bench` でノード数を測定
   - 期待される削減（936ノード削減）を確認

10. **パフォーマンステスト**
    - 値計算の速度を測定
    - 勾配計算の速度を測定
    - メモリ使用量を測定

## 注意点とリスク

### 1. 勾配計算の実装の複雑さ

**リスク**: 解析的勾配の実装が複雑で、バグが混入する可能性がある

**対策**:
- 既存のテスト（`test_cov_max0_max0_expr.cpp`）で勾配計算を検証
- 数値微分との比較テストを追加
- 段階的に実装し、各段階でテストを実行

### 2. 既存コードへの影響

**リスク**: `ExpressionImpl` の構造変更が既存コードに影響を与える可能性がある

**対策**:
- 既存のテストを全て実行して回帰がないことを確認
- `third_` までの既存コードとの互換性を保つ
- 段階的に実装し、各段階でテストを実行

### 3. メモリ使用量の増加

**リスク**: `fourth_` と `fifth_` メンバの追加により、メモリ使用量が増加する

**対策**:
- メモリ使用量を測定
- 必要に応じて `std::array<Expression, 5>` などの代替案を検討

### 4. デバッグの難しさ

**リスク**: 5項演算子は Expression tree を構築しないため、デバッグが難しくなる

**対策**:
- `print()` メソッドを更新して5項演算子をサポート
- デバッグ用のログを追加
- テストを充実させる

### 5. 数値安定性

**リスク**: `util_numerical.cpp` の `expected_prod_pos()` が数値的に不安定な場合がある

**対策**:
- `util_numerical.cpp` の実装を確認
- エッジケース（`rho ≈ ±1`）の処理を確認
- テストでエッジケースをカバー

## テスト計画

### 1. 値計算のテスト

- [ ] 既存の `test_cov_max0_max0_expr.cpp` のテストが通ることを確認
- [ ] `expected_prod_pos_expr()` の値が `util_numerical.cpp` の `expected_prod_pos()` と一致することを確認
- [ ] エッジケース（`rho ≈ ±1`, `sigma0 ≈ 0`, `sigma1 ≈ 0`）のテスト

### 2. 勾配計算のテスト

- [ ] 既存の勾配計算テストが通ることを確認
- [ ] 数値微分との比較テストを追加
- [ ] 各パラメータ（`mu0`, `sigma0`, `mu1`, `sigma1`, `rho`）に対する勾配のテスト

### 3. ノード数のテスト

- [ ] `expected_prod_pos_expr()` の呼び出しで1ノードだけが作成されることを確認
- [ ] `s27.bench` でノード数が削減されることを確認

### 4. パフォーマンステスト

- [ ] 値計算の速度を測定
- [ ] 勾配計算の速度を測定
- [ ] メモリ使用量を測定

## 期待される効果

### s27.bench での削減

| 項目 | 現在 | 5項演算子導入後 | 削減 |
|------|------|---------------|------|
| `expected_prod_pos_expr()` の呼び出し回数 | 12回 | 12回 | - |
| 1回あたりのノード数 | 79ノード | 1ノード | 78ノード |
| 合計ノード数 | 948ノード | 12ノード | **936ノード** |

### 全体への影響

現在の最適化後: 3,481ノード
5項演算子導入後: 3,481 - 936 = **2,545ノード**

**削減率**: 936 / 3,481 = **26.9%**

## 関連ドキュメント

- `docs/issue_188_summary.md`: 作業結果のまとめ
- `docs/issue_188_expected_prod_pos_expr_analysis.md`: `expected_prod_pos_expr()` の詳細分析
- `docs/issue_188_optimization_proposal.md`: 最適化提案
- `docs/issue_188_node_count_analysis.md`: ノード数が増える箇所の詳細分析

## 実装の優先度

**高**: この最適化は大きな効果が期待できるため、優先的に実装することを推奨します。

## 実装の難易度

**中〜高**: 
- 基本構造の実装は中程度の難易度
- 勾配計算の実装は高難易度（解析的勾配の場合）

## 実装の推奨順序

1. **フェーズ1**: 基本構造の実装（1-2日）
2. **フェーズ2**: 値計算の実装（1日）
3. **フェーズ3**: 勾配計算の実装（2-3日、解析的勾配の場合）
4. **フェーズ4**: 検証と最適化（1-2日）

**合計**: 約5-8日

