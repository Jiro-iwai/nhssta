# Issue #188: Expression キャッシュの詳細調査

## 問題の概要

簡単な回路（例: s27.bench, 10ゲート）でも Expression ノード数が異常に多い（4,254ノード）。RandomVariable のキャッシュは機能しているが、Expression レベルの関数（`expected_prod_pos_expr()` など）にはキャッシュがない。

## 調査結果

### 1. RandomVariable のキャッシュ（機能している）

**実装**: `src/random_variable.cpp:140-145`
```cpp
Expression RandomVariableImpl::mean_expr() const {
    if (!cached_mean_expr_) {
        cached_mean_expr_ = calc_mean_expr();
    }
    return cached_mean_expr_;
}
```

**動作**:
- 同じ RandomVariable オブジェクトに対して `mean_expr()` を複数回呼び出すと、2回目以降はキャッシュされた Expression を返す
- **オブジェクトベースのキャッシュ**: 同じ RandomVariable オブジェクト → 同じ Expression

**テスト結果**:
```
First mean_expr() call: 5 nodes
Second mean_expr() call (same object): 0 nodes  ← キャッシュが機能
```

### 2. Expression レベルの関数（キャッシュなし）

**実装**: `src/expression.cpp:684-724`
```cpp
Expression expected_prod_pos_expr(const Expression& mu0, const Expression& sigma0,
                                  const Expression& mu1, const Expression& sigma1,
                                  const Expression& rho) {
    // ... 毎回新しい Expression ノードを作成
    Expression a0 = mu0 / sigma0;
    Expression a1 = mu1 / sigma1;
    // ...
    return term1 + term2 + term3 + term4;
}
```

**動作**:
- 同じ Expression オブジェクトで呼び出しても、毎回新しい Expression ノードを作成
- **キャッシュなし**: 同じ Expression オブジェクトでも、毎回新しいノードが作成される

**テスト結果**:
```
First expected_prod_pos_expr() call: 79 nodes
Second call (same Expression objects): 79 nodes  ← キャッシュなし
e1.get() = 0x12be4bbc8, e2.get() = 0x12be51158
Are they the same? NO  ← 異なる Expression オブジェクト
```

### 3. cov_expr のキャッシュ（機能している）

**実装**: `src/covariance.cpp:224-232`
```cpp
Expression cov_expr(const RandomVariable& a, const RandomVariable& b) {
    // Check cache first (with symmetry)
    auto it = cov_expr_cache.find({a, b});
    if (it != cov_expr_cache.end()) {
        return it->second;
    }
    // ...
    // Cache the result
    cov_expr_cache[{a, b}] = result;
    return result;
}
```

**動作**:
- RandomVariable ペアをキーにキャッシュ
- 同じ RandomVariable ペアで呼び出すと、キャッシュされた Expression を返す

**テスト結果**:
```
First cov_expr() call: 128 nodes
Second cov_expr() call (same RandomVariable objects): 0 nodes  ← キャッシュが機能
e1.get() = 0x12be4f438, e2.get() = 0x12be4f438
Are they the same? YES  ← 同じ Expression オブジェクト
```

## 問題の核心

### なぜ RandomVariable のキャッシュは機能するのか？

1. **オブジェクトベースのキャッシュ**: 同じ RandomVariable オブジェクトに対して、メンバ変数 `cached_mean_expr_` にキャッシュ
2. **オブジェクトの同一性**: 同じ RandomVariable オブジェクトは同じメモリアドレスを持つ
3. **ライフサイクル**: RandomVariable オブジェクトのライフサイクル内でキャッシュが有効

### なぜ Expression レベルの関数はキャッシュされないのか？

1. **関数レベルのキャッシュがない**: `expected_prod_pos_expr()` などの関数には、パラメータに基づくキャッシュがない
2. **値ベースのキャッシュが必要**: 同じ Expression 値（異なるオブジェクト）でも、同じ Expression ノードを返すべき
3. **実装の複雑さ**: Expression の値ベースの比較は複雑（DAG の構造比較が必要）

### 実際の問題シナリオ

`cov_max0_max0_expr_impl()` 内で `expected_prod_pos_expr()` が呼び出される:

```cpp
// src/covariance.cpp:186-221
static Expression cov_max0_max0_expr_impl(const RandomVariable& a, const RandomVariable& b) {
    // ...
    Expression E_prod = expected_prod_pos_expr(mu0, sigma0, mu1, sigma1, rho);
    // ...
}
```

**問題**:
- 同じ RandomVariable ペアに対して `cov_expr()` が複数回呼び出される場合、`cov_max0_max0_expr_impl()` も複数回呼び出される
- しかし、`expected_prod_pos_expr()` にはキャッシュがないため、毎回79ノードが作成される
- `cov_expr()` 自体はキャッシュされているが、その内部で呼び出される `expected_prod_pos_expr()` はキャッシュされていない

## 解決策の検討

### オプション 1: Expression レベルの関数にキャッシュを追加

**実装方法**:
```cpp
// グローバルキャッシュ（値ベース）
static std::unordered_map<
    std::tuple<Expression, Expression, Expression, Expression, Expression>,
    Expression,
    ExpressionTupleHash
> expected_prod_pos_expr_cache;

Expression expected_prod_pos_expr(const Expression& mu0, const Expression& sigma0,
                                  const Expression& mu1, const Expression& sigma1,
                                  const Expression& rho) {
    auto key = std::make_tuple(mu0, sigma0, mu1, sigma1, rho);
    auto it = expected_prod_pos_expr_cache.find(key);
    if (it != expected_prod_pos_expr_cache.end()) {
        return it->second;
    }
    
    Expression result = /* ... 計算 ... */;
    expected_prod_pos_expr_cache[key] = result;
    return result;
}
```

**問題点**:
- Expression のハッシュ関数と等価性比較が必要
- Expression は DAG 構造のため、構造比較が複雑
- メモリ使用量の増加

### オプション 2: RandomVariable レベルでのキャッシュ活用

**実装方法**:
- `cov_max0_max0_expr_impl()` 内で、`expected_prod_pos_expr()` の結果をキャッシュ
- RandomVariable ペアをキーにキャッシュ

**問題点**:
- `expected_prod_pos_expr()` は Expression パラメータを受け取るため、RandomVariable レベルでのキャッシュは難しい

### オプション 3: Expression ノードの共有（構造共有）

**実装方法**:
- Expression ノードの構造を比較して、同じ構造のノードを共有
- `eTbl_` に既にノードが存在するかチェック

**問題点**:
- 構造比較のコストが高い
- 実装が複雑

### オプション 4: 呼び出し元でのキャッシュ

**実装方法**:
- `cov_max0_max0_expr_impl()` 内で、`expected_prod_pos_expr()` の結果をキャッシュ
- Expression パラメータの値（`value()`）をキーにキャッシュ

**問題点**:
- Expression の値が変更される可能性がある
- 値ベースのキャッシュは、勾配計算時に問題を起こす可能性がある

## 推奨される解決策

**オプション 1 の簡易版**: Expression オブジェクトのポインタベースのキャッシュ

```cpp
// Expression オブジェクトのポインタベースのキャッシュ
static std::unordered_map<
    std::tuple<ExpressionImpl*, ExpressionImpl*, ExpressionImpl*, ExpressionImpl*, ExpressionImpl*>,
    Expression
> expected_prod_pos_expr_cache;

Expression expected_prod_pos_expr(const Expression& mu0, const Expression& sigma0,
                                  const Expression& mu1, const Expression& sigma1,
                                  const Expression& rho) {
    auto key = std::make_tuple(
        mu0.get(), sigma0.get(), mu1.get(), sigma1.get(), rho.get()
    );
    auto it = expected_prod_pos_expr_cache.find(key);
    if (it != expected_prod_pos_expr_cache.end()) {
        return it->second;
    }
    
    Expression result = /* ... 計算 ... */;
    expected_prod_pos_expr_cache[key] = result;
    return result;
}
```

**利点**:
- 実装が簡単（ポインタ比較のみ）
- 同じ Expression オブジェクトで呼び出された場合にキャッシュが機能

**欠点**:
- 異なる Expression オブジェクト（同じ値）ではキャッシュが機能しない
- しかし、実際の使用では、RandomVariable から取得した Expression オブジェクトは同じである可能性が高い

## 次のステップ

1. オプション 1 の簡易版を実装してテスト
2. 実際の回路（s27.bench）でノード数の改善を確認
3. 必要に応じて、より高度なキャッシュ戦略を検討

