# phi_expr をカスタム関数で書き換える検討

## 現状の実装

### 現在の `phi_expr` の実装

```cpp
// expression.cpp
static std::unordered_map<ExpressionImpl*, Expression> phi_expr_cache;
static size_t phi_expr_cache_hits = 0;
static size_t phi_expr_cache_misses = 0;

Expression phi_expr(const Expression& x) {
    // φ(x) = exp(-x²/2) / √(2π)
    
    // Check cache first
    auto it = phi_expr_cache.find(x.get());
    if (it != phi_expr_cache.end()) {
        phi_expr_cache_hits++;
        return it->second;
    }
    
    phi_expr_cache_misses++;
    
    static constexpr double INV_SQRT_2PI = 0.3989422804014327;  // 1/√(2π)
    Expression result = INV_SQRT_2PI * exp(-(x * x) / Const(2.0));
    
    // Cache the result
    phi_expr_cache[x.get()] = result;
    
    return result;
}
```

### 特徴

1. **pointer-based キャッシュ**: 同じ `ExpressionImpl*` に対して結果をキャッシュ
2. **パフォーマンス最適化**: Issue #188 で実装されたキャッシュ機能
3. **使用箇所**: `expected_prod_pos_expr`, `expected_prod_pos_rho1_expr`, `expected_prod_pos_rho_neg1_expr` など

## カスタム関数で書き換える場合の検討

### オプション 1: カスタム関数で完全に置き換え

#### 実装案

```cpp
// expression.cpp
static CustomFunction phi_func = CustomFunction::create(
    1,
    [](const std::vector<Variable>& v) {
        static constexpr double INV_SQRT_2PI = 0.3989422804014327;
        Expression x = v[0];
        return INV_SQRT_2PI * exp(-(x * x) / Const(2.0));
    },
    "phi"
);

Expression phi_expr(const Expression& x) {
    return phi_func(x);
}
```

#### 利点

1. **統一されたAPI**: カスタム関数のAPIを使用
2. **内部実装の明確化**: 式の構造が明確になる
3. **再利用性**: カスタム関数として独立評価も可能
4. **テスト容易性**: カスタム関数のテストフレームワークを利用可能

#### 欠点

1. **キャッシュ機能の喪失**: 現在の pointer-based キャッシュが失われる
   - カスタム関数は内部式木を一度だけ構築し、再利用するが、
   - 異なる引数（同じ値でも異なる Expression オブジェクト）に対してはキャッシュが効かない
2. **パフォーマンス**: 
   - カスタム関数のオーバーヘッド（内部式木の構築、`set_inputs_and_clear` など）
   - 現在のキャッシュは pointer-based なので、同じ Expression オブジェクトに対しては即座に返せる
3. **互換性**: 既存のコードとの互換性は維持されるが、内部実装が変わる

### オプション 2: カスタム関数 + 外側でキャッシュ

#### 実装案

```cpp
// expression.cpp
static CustomFunction phi_func = CustomFunction::create(
    1,
    [](const std::vector<Variable>& v) {
        static constexpr double INV_SQRT_2PI = 0.3989422804014327;
        Expression x = v[0];
        return INV_SQRT_2PI * exp(-(x * x) / Const(2.0));
    },
    "phi"
);

static std::unordered_map<ExpressionImpl*, Expression> phi_expr_cache;
static size_t phi_expr_cache_hits = 0;
static size_t phi_expr_cache_misses = 0;

Expression phi_expr(const Expression& x) {
    // Check cache first
    auto it = phi_expr_cache.find(x.get());
    if (it != phi_expr_cache.end()) {
        phi_expr_cache_hits++;
        return it->second;
    }
    
    phi_expr_cache_misses++;
    
    // Use custom function
    Expression result = phi_func(x);
    
    // Cache the result
    phi_expr_cache[x.get()] = result;
    
    return result;
}
```

#### 利点

1. **キャッシュ機能の維持**: 現在の pointer-based キャッシュを維持
2. **カスタム関数の利点**: 統一されたAPI、内部実装の明確化
3. **パフォーマンス**: キャッシュが効く場合は現在と同じ性能

#### 欠点

1. **二重の抽象化**: カスタム関数の上にキャッシュレイヤーが追加される
2. **複雑性**: 実装が少し複雑になる

### オプション 3: 現状維持（推奨）

#### 理由

1. **パフォーマンス**: 現在の pointer-based キャッシュは効率的
2. **シンプルさ**: 実装がシンプルで理解しやすい
3. **既存の最適化**: Issue #188 で既に最適化済み
4. **必要性**: `phi_expr` は単純な式なので、カスタム関数の利点が限定的

## 推奨事項

### 現状維持を推奨する理由

1. **パフォーマンス**: 現在の pointer-based キャッシュは効率的で、カスタム関数では同じレベルの最適化が難しい
2. **シンプルさ**: 現在の実装はシンプルで理解しやすい
3. **必要性**: `phi_expr` は単純な式（`exp(-x²/2) / √(2π)`）なので、カスタム関数の利点（複雑な式の再利用、独立評価など）が限定的

### カスタム関数が適しているケース

以下のような場合にカスタム関数が適しています：

1. **複雑な式**: 複数の変数を含む複雑な式
2. **再利用性**: 複数の場所で同じ式を再利用したい場合
3. **独立評価**: メイン式木とは独立に評価したい場合
4. **テスト容易性**: 独立評価APIでテストしたい場合

### 結論

`phi_expr` は現在の実装（pointer-based キャッシュ付きの通常関数）を維持することを推奨します。

カスタム関数への置き換えは、以下の場合に検討すべきです：
- より複雑な式になった場合
- 独立評価APIが必要になった場合
- コードの統一性を優先する場合

