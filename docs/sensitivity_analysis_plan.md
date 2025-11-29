# 感度解析機能 実装計画

## 概要

SSTA のクリティカルパスに対する感度解析機能を実装する。
自動微分（Reverse-mode AD）を用いて、入力パラメータ（μ, σ）に対する
パス統計量（平均、分散）の偏微分を計算する。

## 現状（2024年11月）

### 完了済み

| Phase | 内容 | PR |
|-------|------|-----|
| Phase 5 | Expression 数学関数（Φ₂, φ₂, expected_prod_pos_expr 等）| #178 |
| C-1 | NormalImpl の mean_expr(), var_expr(), std_expr() | #179 |
| C-2 | OpADD/OpSUB の mean_expr(), var_expr() | #179 |
| C-3 | OpMAX0 の mean_expr(), var_expr() | #179 |
| C-4 | OpMAX の mean_expr(), var_expr() | #179 |

### 現状の制限

`var_expr()` では共分散を数値定数として扱っている：

```cpp
Expression OpADD::var_expr() const {
    double cov = covariance(left(), right());  // 数値計算
    return var_left + Const(2.0 * cov) + var_right;  // 定数扱い
}
```

**影響**: 入力を共有するゲートがある場合（SSTA では一般的）、
`var_expr()` の勾配が不正確になる。

**正確に動作するもの**:
- `mean_expr()` の勾配（∂E[Path]/∂μ, ∂E[Path]/∂σ）

---

## C-5: 共分散の Expression ベース化

### 目的

`cov_expr(a, b)` を実装し、共分散も Expression として計算することで、
`var_expr()` の勾配を正確にする。

### 設計方針

1. **同一性判定**: `RandomVariable::operator==`（ポインタ比較）を使用
2. **キャッシュ**: `std::unordered_map<RowCol, Expression>` で重複回避
3. **クランプ省略**: ρ の [-1, 1] クランプは省略（微分不可能のため）

### 関数シグネチャ

```cpp
// メイン関数
Expression cov_expr(const RandomVariable& a, const RandomVariable& b);

// 内部ヘルパー（インターフェース変更）
Expression cov_max0_max0_expr(const RandomVariable& a, const RandomVariable& b);
Expression cov_x_max0_expr(const RandomVariable& x, const RandomVariable& y);

// 既存の内部ヘルパー（変更なし）
Expression expected_prod_pos_expr(const Expression& mu0, const Expression& sigma0,
                                  const Expression& mu1, const Expression& sigma1,
                                  const Expression& rho);
```

### 実装ステップ

#### C-5.1: 基本構造とキャッシュ

```cpp
// covariance.hpp に追加
class CovarianceExprCache {
    using RowCol = std::pair<RandomVariable, RandomVariable>;
    std::unordered_map<RowCol, Expression, PairHash> cache_;
public:
    bool lookup(const RandomVariable& a, const RandomVariable& b, Expression& result);
    void set(const RandomVariable& a, const RandomVariable& b, Expression expr);
    void clear();
};

// グローバルキャッシュ
CovarianceExprCache& get_cov_expr_cache();
```

**テスト**: キャッシュの動作確認

#### C-5.2: Normal × Normal

```cpp
if (a == b) {
    return a->var_expr();  // Cov(X, X) = Var(X)
}
if (is_normal(a) && is_normal(b)) {
    return Const(0.0);     // 異なる Normal は独立
}
```

**テスト**: 
- `cov_expr(N, N) == N->var_expr()`
- `cov_expr(N1, N2) == 0` （N1 ≠ N2）

#### C-5.3: ADD/SUB の線形展開

```cpp
// Cov(A + B, C) = Cov(A, C) + Cov(B, C)
if (is_add(a)) {
    return cov_expr(a->left(), b) + cov_expr(a->right(), b);
}
// Cov(A - B, C) = Cov(A, C) - Cov(B, C)
if (is_sub(a)) {
    return cov_expr(a->left(), b) - cov_expr(a->right(), b);
}
// 対称性
if (is_add(b)) { return cov_expr(b, a); }
if (is_sub(b)) { return cov_expr(b, a); }
```

**テスト**:
- `cov_expr(A+B, C)` の値が `covariance(A+B, C)` と一致
- 勾配の検証

#### C-5.4: MAX0 × X

```cpp
Expression cov_x_max0_expr(const RandomVariable& x, const RandomVariable& y) {
    // y = MAX0(Z)
    const RandomVariable& z = y->left();
    Expression cov_xz = cov_expr(x, z);
    Expression mu_z = z->mean_expr();
    Expression sigma_z = z->std_expr();
    
    // Cov(X, max0(Z)) = Cov(X, Z) × Φ(-μ_Z/σ_Z)
    // = Cov(X, Z) × MeanPhiMax(-μ_Z/σ_Z)
    Expression a = (Const(0.0) - mu_z) / sigma_z;
    return cov_xz * MeanPhiMax_expr(a);
}
```

**テスト**:
- `cov_x_max0_expr(x, MAX0(z))` の値が `covariance_x_max0_y()` と一致

#### C-5.5: MAX0 × MAX0

```cpp
Expression cov_max0_max0_expr(const RandomVariable& a, const RandomVariable& b) {
    // a = MAX0(D0), b = MAX0(D1)
    if (a->left() == b->left()) {
        return a->var_expr();  // 同一の D
    }
    
    const RandomVariable& d0 = a->left();
    const RandomVariable& d1 = b->left();
    
    Expression mu0 = d0->mean_expr();
    Expression sigma0 = d0->std_expr();
    Expression mu1 = d1->mean_expr();
    Expression sigma1 = d1->std_expr();
    
    Expression cov_d0d1 = cov_expr(d0, d1);
    Expression rho = cov_d0d1 / (sigma0 * sigma1);
    
    Expression E_D0_pos = max0_mean_expr(mu0, sigma0);
    Expression E_D1_pos = max0_mean_expr(mu1, sigma1);
    Expression E_prod = expected_prod_pos_expr(mu0, sigma0, mu1, sigma1, rho);
    
    return E_prod - E_D0_pos * E_D1_pos;
}
```

**テスト**:
- `cov_max0_max0_expr(a, b)` の値が `covariance_max0_max0()` と一致

#### C-5.6: MAX の展開

```cpp
// MAX(A, B) = A + MAX0(B - A)
if (is_max(a)) {
    auto m = dynamic_cast<const OpMAX*>(a.get());
    return cov_expr(a->left(), b) + cov_expr(m->max0(), b);
}
```

**テスト**:
- `cov_expr(MAX(A,B), C)` の値が `covariance(MAX(A,B), C)` と一致

#### C-5.7: var_expr() の書き換え

```cpp
Expression OpADD::var_expr() const {
    Expression cov = cov_expr(left(), right());  // Expression として計算
    return left()->var_expr() + Const(2.0) * cov + right()->var_expr();
}
```

**テスト**:
- 入力共有ケースで `var_expr()` の勾配が正確になることを確認

---

## C-6: CLI 感度解析オプション

### 目的

コマンドラインから感度解析を実行できるようにする。

### オプション設計

```bash
nhssta -d delays.dlib -b circuit.bench -l -p -s

# -s, --sensitivity: 感度解析を出力
```

### 出力フォーマット

```
# Critical Paths (top 5)
#
# Path 1: G1 -> G2 -> G3 -> OUT
#   Mean: 45.23  Std: 5.67
#
#   Sensitivity Analysis:
#   Gate        ∂mean/∂μ    ∂mean/∂σ    ∂std/∂μ     ∂std/∂σ
#   G1          1.000       0.234       0.012       0.445
#   G2          0.823       0.156       0.008       0.312
#   G3          1.000       0.312       0.015       0.521
```

### 実装ステップ

1. `-s` オプションのパース（main.cpp）
2. 感度計算ロジック（ssta.cpp）
3. 出力フォーマット
4. テスト

---

## 優先度

| Task | 優先度 | 理由 |
|------|--------|------|
| C-5 | 高 | var_expr() の正確性向上 |
| C-6 | 中 | ユーザビリティ向上 |

---

## 参考

- Issue #167: 感度解析機能の議論
- PR #178: Phase 5（Φ₂, φ₂ 等の実装）
- PR #179: Phase C-1〜C-4（mean_expr, var_expr 実装）

