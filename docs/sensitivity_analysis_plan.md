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
| C-5 | cov_expr() による Expression ベースの共分散計算 | #181 |

### 正確に動作するもの

- `mean_expr()` の勾配（∂E[Path]/∂μ, ∂E[Path]/∂σ）
- `var_expr()` の勾配（∂Var[Path]/∂μ, ∂Var[Path]/∂σ）
- `std_expr()` の勾配（∂σ[Path]/∂μ, ∂σ[Path]/∂σ）

### 注意事項

高相関ケース（|ρ| > 0.97）では Expression の解析的計算と
RandomVariable の Gauss-Hermite 計算で最大 3% の差が生じる。
Expression の方が高精度（Issue #182 参照）。

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
クリティカルパス群の「最悪ケース」に対する各ゲートの寄与度を計算する。

### オプション設計

```bash
nhssta -d delays.dlib -b circuit.bench -l -p -s
nhssta -d delays.dlib -b circuit.bench -l -p -s -n 10

# -s, --sensitivity: 感度解析を出力
# -n N: top N パスを対象（デフォルト: 5）
```

### 設計方針

#### 1. パス選出基準

```
スコア = LAT + σ   (大きい順に top N を選出)
```

従来の LAT のみではなく、ばらつきも考慮することで、
歩留まりに影響するパスを正しく選出する。

#### 2. 目的関数

```
F = log(Σᵢ exp(LATᵢ + σᵢ))
```

これは log-sum-exp（ソフトマックス）であり、max(LATᵢ + σᵢ) の滑らかな近似。
温度パラメータ T=1.0 を使用。

#### 3. 計算する感度

各ゲート j に対して：
- `∂F/∂μⱼ`: ゲート j の平均遅延に対する目的関数の感度
- `∂F/∂σⱼ`: ゲート j の遅延ばらつきに対する目的関数の感度

### 出力フォーマット

```
# Sensitivity Analysis
# Objective: log(Σ exp(LAT + σ)) = 22.15
# Top 3 Paths (by LAT + σ):

Path 1: out1
  LAT=20.3  σ=1.5  score=21.8

Path 2: out2
  LAT=19.8  σ=1.8  score=21.6

Path 3: out3
  LAT=20.0  σ=1.2  score=21.2

# Gate Sensitivities
Gate        ∂F/∂μ       ∂F/∂σ
G1          0.8523      0.4512
G2          0.7234      0.3821
G3          0.6891      0.3654
G4          0.5123      0.2789
...
```

### 実装詳細

#### 目的関数の Expression 構築

```cpp
Expression computeObjectiveFunction(const std::vector<RandomVariable>& paths) {
    // F = log(Σ exp(LAT + σ))
    Expression sum = Const(0.0);
    for (const auto& path : paths) {
        Expression lat = path->mean_expr();
        Expression sigma = path->std_expr();
        Expression score = lat + sigma;
        sum = sum + exp(score);
    }
    return log(sum);
}
```

#### 勾配計算

```cpp
void computeSensitivities(const std::vector<Gate>& gates, 
                          Expression objective) {
    objective->backward();
    
    for (const auto& gate : gates) {
        double grad_mu = gate->delay()->mean_expr()->gradient();
        double grad_sigma = gate->delay()->std_expr()->gradient();
        // 出力
    }
}
```

### 実装ステップ

1. `-s`, `-n` オプションのパース（main.cpp）
2. パス選出ロジック（ssta.cpp）
   - スコア = LAT + σ で降順ソート
   - top N を選出
3. 目的関数の構築
   - log-sum-exp の Expression 構築
4. 感度計算
   - backward() で全ゲートの勾配を計算
5. 出力フォーマット
6. テスト

---

## 進捗状況

| Task | 状態 | PR |
|------|------|-----|
| C-5 | ✅ 完了 | #181 |
| C-6 | 📋 未着手 | - |

---

## 参考

- Issue #167: 感度解析機能の議論
- Issue #182: GH 精度問題（高相関ケース）
- PR #178: Phase 5（Φ₂, φ₂ 等の実装）
- PR #179: Phase C-1〜C-4（mean_expr, var_expr 実装）
- PR #181: Phase C-5（cov_expr 実装）

