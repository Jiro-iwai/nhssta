# Issue #188: expected_prod_pos_expr() の詳細分析

## 調査結果サマリー

`expected_prod_pos_expr()` が1回の呼び出しで **79ノード** を作成している原因を詳細に調査しました。

## 測定結果（call #1）

### 全体のノード数

```
[DEBUG] expected_prod_pos_expr call #1: TOTAL created 79 nodes
```

### 各ステップの内訳

| ステップ | ノード数 | 説明 |
|---------|---------|------|
| a0, a1 | 2 | `mu0 / sigma0`, `mu1 / sigma1` |
| one_minus_rho2, sqrt | 4 | `1 - rho²`, `√(1-ρ²)` |
| Phi2_expr() | 1 | `Φ₂(a0, a1; ρ)` |
| phi_expr() | 14 | `φ(a0)`, `φ(a1)` (2回呼び出し) |
| Phi_expr(cond) | 20 | `Φ((a0 - ρa1)/√(1-ρ²))`, `Φ((a1 - ρa0)/√(1-ρ²))` (2回呼び出し) |
| phi2_expr() | 22 | `φ₂(a0, a1; ρ)` |
| term1 | 2 | `μ0 μ1 Φ₂(a0, a1; ρ)` |
| term2 | 3 | `μ0 σ1 φ(a1) Φ((a0 - ρa1)/√(1-ρ²))` |
| term3 | 3 | `μ1 σ0 φ(a0) Φ((a1 - ρa0)/√(1-ρ²))` |
| term4 | 5 | `σ0 σ1 [ρ Φ₂(a0, a1; ρ) + (1-ρ²) φ₂(a0, a1; ρ)]` |
| result (sum) | 3 | `term1 + term2 + term3 + term4` |
| **合計** | **79** | |

## 詳細な分析

### 1. `phi_expr()` の呼び出し（14ノード）

**実装**: `src/expression.hpp:326-329`
```cpp
inline Expression phi_expr(const Expression& x) {
    static constexpr double INV_SQRT_2PI = 0.3989422804014327;  // 1/√(2π)
    return INV_SQRT_2PI * exp(-(x * x) / 2.0);
}
```

**ノード数の内訳**:
- `x * x`: 1ノード（MUL）
- `(x * x) / 2.0`: 1ノード（DIV）
- `-(x * x) / 2.0`: 1ノード（MINUS）
- `exp(-(x * x) / 2.0)`: 1ノード（EXP）
- `INV_SQRT_2PI * exp(...)`: 1ノード（MUL）

**問題**: `phi_expr()` は **inline 関数**なので、呼び出しごとに新しいノードが作成されます。
- `phi_expr(a0)`: 7ノード
- `phi_expr(a1)`: 7ノード
- **合計**: 14ノード

**改善案**: `phi_expr()` にもキャッシュを追加する（ただし、inline 関数なので実装を変更する必要がある）

### 2. `phi2_expr()` の呼び出し（22ノード）

**実装**: `src/expression.cpp:709-729`

**ノード数の内訳**:
- `one_minus_rho2, sqrt`: 4ノード
  - `rho * rho`: 1ノード（MUL）
  - `Const(1.0) - rho * rho`: 1ノード（MINUS）
  - `sqrt(one_minus_rho2)`: 1ノード（SQRT）
- `coeff`: 4ノード
  - `Const(2.0 * M_PI)`: 1ノード（CONST）
  - `Const(2.0 * M_PI) * sqrt_one_minus_rho2`: 1ノード（MUL）
  - `Const(1.0) / (...)` : 1ノード（DIV）
- `Q`: 9ノード
  - `x * x`: 1ノード（MUL）
  - `rho * x * y`: 2ノード（MUL, MUL）
  - `Const(2.0) * rho * x * y`: 1ノード（MUL）
  - `y * y`: 1ノード（MUL）
  - `x * x - Const(2.0) * rho * x * y`: 1ノード（MINUS）
  - `(x * x - Const(2.0) * rho * x * y + y * y)`: 1ノード（PLUS）
  - `(x * x - Const(2.0) * rho * x * y + y * y) / one_minus_rho2`: 1ノード（DIV）
- `result (coeff * exp)`: 5ノード
  - `Q / Const(2.0)`: 1ノード（DIV）
  - `-Q / Const(2.0)`: 1ノード（MINUS）
  - `exp(-Q / Const(2.0))`: 1ノード（EXP）
  - `coeff * exp(...)`: 1ノード（MUL）

**合計**: 4 + 4 + 9 + 5 = 22ノード

**問題**: `phi2_expr()` はキャッシュされていますが、異なる Expression オブジェクトで呼び出されているため、キャッシュが効いていません。

### 3. `Phi_expr()` の呼び出し（20ノード）

**実装**: `src/expression.hpp:337-340`
```cpp
inline Expression Phi_expr(const Expression& x) {
    static constexpr double INV_SQRT_2 = 0.7071067811865476;  // 1/√2
    return 0.5 * (1.0 + erf(x * INV_SQRT_2));
}
```

**呼び出し箇所**:
- `Phi_expr((a0 - rho * a1) / sqrt_one_minus_rho2)`: 10ノード
- `Phi_expr((a1 - rho * a0) / sqrt_one_minus_rho2)`: 10ノード

**ノード数の内訳**（1回の呼び出しあたり）:
- `a0 - rho * a1`: 2ノード（MUL, MINUS）
- `(a0 - rho * a1) / sqrt_one_minus_rho2`: 1ノード（DIV）
- `(a0 - rho * a1) / sqrt_one_minus_rho2 * INV_SQRT_2`: 1ノード（MUL）
- `erf(...)`: 1ノード（ERF）
- `1.0 + erf(...)`: 1ノード（PLUS）
- `0.5 * (1.0 + erf(...))`: 1ノード（MUL）

**合計**: 7ノード × 2回 = 14ノード（ただし、測定では20ノード）

**問題**: `Phi_expr()` も inline 関数なので、呼び出しごとに新しいノードが作成されます。

### 4. `Phi2_expr()` の呼び出し（1ノード）

**実装**: `src/expression.cpp:775-789`

**ノード数の内訳**:
- `ExpressionImpl::PHI2`: 1ノード（特殊なノードタイプ）

**問題**: `Phi2_expr()` はキャッシュされていますが、異なる Expression オブジェクトで呼び出されているため、キャッシュが効いていません。

## 問題の核心

### 1. キャッシュが効いていない

`expected_prod_pos_expr()` は **12回** 呼び出されていますが、キャッシュヒットは **0回** です。

**原因**:
- Expression オブジェクトのポインタベースのキャッシュを使用
- 異なる Expression オブジェクト（同じ値）で呼び出されているため、キャッシュが効かない

**例**:
```cpp
// 1回目の呼び出し
Expression mu0_1 = d0->mean_expr();  // Expression オブジェクト #1
Expression sigma0_1 = d0->std_expr();  // Expression オブジェクト #2
expected_prod_pos_expr(mu0_1, sigma0_1, ...);  // キャッシュミス

// 2回目の呼び出し（異なる Expression オブジェクト）
Expression mu0_2 = d0->mean_expr();  // Expression オブジェクト #3（異なる）
Expression sigma0_2 = d0->std_expr();  // Expression オブジェクト #4（異なる）
expected_prod_pos_expr(mu0_2, sigma0_2, ...);  // キャッシュミス
```

### 2. inline 関数の呼び出し

`phi_expr()` と `Phi_expr()` は **inline 関数**なので、呼び出しごとに新しいノードが作成されます。

**問題**:
- `phi_expr()` が2回呼び出される → 14ノード
- `Phi_expr()` が2回呼び出される → 20ノード
- 合計: 34ノード（全体の43%）

### 3. 共通部分式の重複計算

**例**: `one_minus_rho2` と `sqrt_one_minus_rho2` が複数回計算される
- `expected_prod_pos_expr()` 内で計算: 4ノード
- `phi2_expr()` 内で再計算: 4ノード
- **合計**: 8ノード（重複）

## 解決策の検討

### オプション 1: 値ベースのキャッシュ

**問題**: Expression オブジェクトのポインタベースのキャッシュでは、異なる Expression オブジェクト（同じ値）でキャッシュが効かない

**解決策**: Expression の値（`value()`）をキーにキャッシュ
- 問題: Expression の値が変更される可能性がある
- 問題: 勾配計算時に問題を起こす可能性がある

### オプション 2: inline 関数のキャッシュ

**問題**: `phi_expr()` と `Phi_expr()` が inline 関数なので、呼び出しごとに新しいノードが作成される

**解決策**: inline 関数を通常の関数に変更し、キャッシュを追加
- 実装が簡単
- 効果が大きい（34ノード削減可能）

### オプション 3: 共通部分式の抽出

**問題**: `one_minus_rho2` と `sqrt_one_minus_rho2` が複数回計算される

**解決策**: 共通部分式を抽出して再利用
- 実装が複雑
- 効果は限定的（8ノード削減）

### オプション 4: Expression ノードの共有

**問題**: 同じ構造の Expression ノードが複数回作成される

**解決策**: Expression ノードの構造共有
- 実装が非常に複雑
- 効果が大きい（全体的なノード数削減）

## 推奨される解決策

**段階的なアプローチ**:

1. **短期**: `phi_expr()` と `Phi_expr()` を通常の関数に変更し、キャッシュを追加
   - 実装が簡単
   - 効果が大きい（34ノード削減可能）

2. **中期**: `expected_prod_pos_expr()` の値ベースキャッシュ（簡易版）
   - Expression の値（`value()`）をキーにキャッシュ
   - ただし、勾配計算時には無効化

3. **長期**: Expression ノードの共有（構造共有）
   - 同じ構造の Expression ノードを共有
   - 実装が複雑だが、最も効果的

## 次のステップ

1. `phi_expr()` と `Phi_expr()` を通常の関数に変更し、キャッシュを追加してテスト
2. `expected_prod_pos_expr()` の値ベースキャッシュを実装してテスト
3. より大きな回路で効果を確認

