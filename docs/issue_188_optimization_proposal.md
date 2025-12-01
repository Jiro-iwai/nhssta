# Issue #188: phi_expr(), Phi_expr(), phi2_expr() の最適化提案

## 現状の問題

### 1. `phi_expr()` - 14ノード（2回呼び出し）

**問題点**:
- inline 関数なので、呼び出しごとに新しいノードが作成される
- `phi_expr(a0)` と `phi_expr(a1)` で合計14ノード
- キャッシュがない

**最適化案**:
1. **inline 関数を通常の関数に変更し、キャッシュを追加**
   - 効果: 2回目の呼び出しで0ノード（キャッシュヒット）
   - 実装: `src/expression.hpp` の inline 関数を `src/expression.cpp` に移動し、キャッシュを追加

### 2. `Phi_expr()` - 20ノード（2回呼び出し）

**問題点**:
- inline 関数なので、呼び出しごとに新しいノードが作成される
- `Phi_expr((a0 - rho * a1) / sqrt_one_minus_rho2)` と `Phi_expr((a1 - rho * a0) / sqrt_one_minus_rho2)` で合計20ノード
- キャッシュがない

**最適化案**:
1. **inline 関数を通常の関数に変更し、キャッシュを追加**
   - 効果: 2回目の呼び出しで0ノード（キャッシュヒット）
   - 実装: `src/expression.hpp` の inline 関数を `src/expression.cpp` に移動し、キャッシュを追加

### 3. `phi2_expr()` - 22ノード

**問題点**:
- `one_minus_rho2` と `sqrt_one_minus_rho2` が `expected_prod_pos_expr()` 内で既に計算済み
- しかし、`phi2_expr()` 内で再計算されている（4ノード）

**最適化案**:
1. **`phi2_expr()` に `one_minus_rho2` と `sqrt_one_minus_rho2` を引数として渡す**
   - 効果: 4ノード削減（`one_minus_rho2` と `sqrt_one_minus_rho2` の再計算を回避）
   - 実装: `phi2_expr()` のシグネチャを変更し、既に計算済みの値を再利用

## 最適化の実装

### オプション 1: `phi_expr()` と `Phi_expr()` を通常の関数に変更（推奨）

**効果**: 最大34ノード削減（`phi_expr()` 14ノード + `Phi_expr()` 20ノード）

**実装**:
1. `src/expression.hpp` の `phi_expr()` と `Phi_expr()` を宣言のみに変更
2. `src/expression.cpp` に実装を追加し、キャッシュを追加

**コード変更**:
```cpp
// expression.hpp
Expression phi_expr(const Expression& x);
Expression Phi_expr(const Expression& x);

// expression.cpp
static std::unordered_map<ExpressionImpl*, Expression> phi_expr_cache;
static std::unordered_map<ExpressionImpl*, Expression> Phi_expr_cache;

Expression phi_expr(const Expression& x) {
    auto it = phi_expr_cache.find(x.get());
    if (it != phi_expr_cache.end()) {
        return it->second;
    }
    
    static constexpr double INV_SQRT_2PI = 0.3989422804014327;
    Expression result = INV_SQRT_2PI * exp(-(x * x) / 2.0);
    phi_expr_cache[x.get()] = result;
    return result;
}

Expression Phi_expr(const Expression& x) {
    auto it = Phi_expr_cache.find(x.get());
    if (it != Phi_expr_cache.end()) {
        return it->second;
    }
    
    static constexpr double INV_SQRT_2 = 0.7071067811865476;
    Expression result = 0.5 * (1.0 + erf(x * INV_SQRT_2));
    Phi_expr_cache[x.get()] = result;
    return result;
}
```

### オプション 2: `phi2_expr()` に共通部分式を渡す

**効果**: 4ノード削減（`one_minus_rho2` と `sqrt_one_minus_rho2` の再計算を回避）

**実装**:
1. `phi2_expr()` のシグネチャを変更し、`one_minus_rho2` と `sqrt_one_minus_rho2` を引数として受け取る
2. `expected_prod_pos_expr()` から既に計算済みの値を渡す

**コード変更**:
```cpp
// expression.hpp
Expression phi2_expr(const Expression& x, const Expression& y, const Expression& rho,
                      const Expression& one_minus_rho2, const Expression& sqrt_one_minus_rho2);

// expression.cpp
Expression phi2_expr(const Expression& x, const Expression& y, const Expression& rho,
                     const Expression& one_minus_rho2, const Expression& sqrt_one_minus_rho2) {
    // one_minus_rho2 と sqrt_one_minus_rho2 は既に計算済みなので、再計算しない
    Expression coeff = Const(1.0) / (Const(2.0 * M_PI) * sqrt_one_minus_rho2);
    Expression Q = (x * x - Const(2.0) * rho * x * y + y * y) / one_minus_rho2;
    Expression result = coeff * exp(-Q / Const(2.0));
    return result;
}

// expected_prod_pos_expr() 内で
Expression phi2_a0_a1 = phi2_expr(a0, a1, rho, one_minus_rho2, sqrt_one_minus_rho2);
```

### オプション 3: 共通部分式の抽出（高度）

**効果**: 追加のノード削減（例: `a0 * a0` と `a1 * a1` の再利用）

**実装**:
1. `expected_prod_pos_expr()` 内で共通部分式を事前に計算
2. 各関数に共通部分式を引数として渡す

**コード変更**:
```cpp
// expected_prod_pos_expr() 内で
Expression a0_sq = a0 * a0;  // 共通部分式
Expression a1_sq = a1 * a1;  // 共通部分式
Expression rho_a0_a1 = rho * a0 * a1;  // 共通部分式

// phi_expr() に a0_sq, a1_sq を渡す
Expression phi_a0 = phi_expr_with_sq(a0, a0_sq);
Expression phi_a1 = phi_expr_with_sq(a1, a1_sq);
```

## 推奨される実装順序

1. **オプション 1**: `phi_expr()` と `Phi_expr()` を通常の関数に変更（最大効果）
2. **オプション 2**: `phi2_expr()` に共通部分式を渡す（簡単な実装）
3. **オプション 3**: 共通部分式の抽出（高度な最適化）

## 期待される効果

### 最適化前
- `phi_expr()`: 14ノード
- `Phi_expr()`: 20ノード
- `phi2_expr()`: 22ノード
- **合計**: 56ノード

### 最適化後（オプション 1 + 2）
- `phi_expr()`: 7ノード（1回目）+ 0ノード（2回目、キャッシュ）= 7ノード
- `Phi_expr()`: 10ノード（1回目）+ 0ノード（2回目、キャッシュ）= 10ノード
- `phi2_expr()`: 18ノード（`one_minus_rho2` と `sqrt_one_minus_rho2` の再計算を回避）
- **合計**: 35ノード（**21ノード削減、37%削減**）

## 実装時の注意点

1. **キャッシュの無効化**: 勾配計算時にはキャッシュを無効化する必要がある可能性がある
2. **メモリ使用量**: キャッシュのメモリ使用量を監視する
3. **テスト**: 最適化前後で結果が一致することを確認する

