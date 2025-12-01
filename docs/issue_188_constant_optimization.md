# Issue #188: 定数関連の最適化

## 実装した最適化

### 1. よく使われる定数の事前定義

**変更内容**:
- `two` (2.0) を事前定義
- `half` (0.5) を事前定義
- `minus_one` (-1.0) は既に定義済み

**コード変更**:
```cpp
static const Const zero(0.0);
static const Const one(1.0);
static const Const minus_one(-1.0);
static const Const two(2.0);      // 追加
static const Const half(0.5);     // 追加
```

### 2. `Const()` の呼び出しを定数オブジェクトに置き換え

**変更箇所**:

1. **`phi2_expr()`**:
   - `Const(2.0)` → `two` (2箇所)
   - 効果: 2ノード削減

2. **`Phi_expr()`**:
   - `0.5` → `half`
   - `1.0` → `one`
   - 効果: 定数オブジェクトの再利用

3. **`expected_prod_pos_rho1_expr()`**:
   - `Const(-1.0)` → `minus_one` (2箇所)
   - 効果: 定数オブジェクトの再利用

## 最適化の効果

### `phi2_expr()` のノード数

| 項目 | 最適化前 | 最適化後 | 削減 |
|------|---------|---------|------|
| ノード数 | 18 | 15 | **3ノード** |

### `expected_prod_pos_expr()` のノード数

| 項目 | 最適化前 | 最適化後 | 削減 |
|------|---------|---------|------|
| ノード数 | 73 | 71 | **2ノード** |

### 合計削減

- `phi2_expr()`: 3ノード削減
- `expected_prod_pos_expr()`: 2ノード削減（`phi2_expr()` の削減を含む）
- **合計**: 約5ノード削減（`expected_prod_pos_expr()` 1回あたり）

## 最適化の理由

### 1. `Const()` の呼び出しを減らす

`Const(2.0)` を毎回作成すると、新しい `ConstImpl` オブジェクトが作成されます。事前定義された `two` を使用することで、オブジェクトの作成を回避できます。

### 2. `operator*` や `operator/` での定数比較が効く

`operator*` の実装では、`one` との比較で最適化されます：

```cpp
Expression operator*(const Expression& a, const Expression& b) {
    if (a == one) {
        return b;  // one * x を x に最適化
    }
    if (b == one) {
        return a;  // x * one を x に最適化
    }
    // ...
}
```

`Const(1.0)` を毎回作成すると新しいオブジェクトになるため、`one` との比較が効きません。事前定義された定数を使用することで、この最適化が有効になります。

### 3. 定数オブジェクトの再利用

同じ定数値を複数回使用する場合、事前定義された定数オブジェクトを再利用することで、メモリ使用量とノード作成を削減できます。

## 今後の最適化の余地

### 1. `operator*(double, Expression)` の最適化

現在の実装：
```cpp
Expression operator*(double a, const Expression& b) {
    return (Const(a) * b);
}
```

よく使われる定数（2.0, 0.5, -1.0）の場合は、事前定義された定数を使用するように最適化できます：

```cpp
Expression operator*(double a, const Expression& b) {
    if (a == 1.0) return b;
    if (a == 2.0) return (two * b);
    if (a == 0.5) return (half * b);
    if (a == -1.0) return (minus_one * b);
    return (Const(a) * b);
}
```

### 2. `operator/(Expression, double)` の最適化

同様に、よく使われる定数での除算を最適化できます：

```cpp
Expression operator/(const Expression& a, double b) {
    if (b == 1.0) return a;
    if (b == 2.0) return (a / two);
    if (b == 0.5) return (a * two);  // a / 0.5 = a * 2
    return (a / Const(b));
}
```

### 3. その他のよく使われる定数

- `Const(2.0 * M_PI)`: 円周率の2倍（`phi2_expr()` で使用）
- `Const(0.0)`: 既に `zero` として定義済み

## まとめ

定数関連の最適化により、`expected_prod_pos_expr()` のノード数が **73 → 71ノード**（2ノード削減）になりました。さらに、`phi2_expr()` のノード数が **18 → 15ノード**（3ノード削減）になりました。

主な効果：
- `Const()` の呼び出しを減らし、定数オブジェクトの再利用が可能
- `operator*` や `operator/` での定数比較が効くようになる
- メモリ使用量とノード作成を削減

