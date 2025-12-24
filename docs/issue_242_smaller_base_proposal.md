# Issue #242: smaller + MAX0(larger - smaller) 提案の分析

## 提案内容

現在の実装を逆にする：
- **現在**: `MAX(A,B) = larger + MAX0(smaller - larger)`
- **提案**: `MAX(A,B) = smaller + MAX0(larger - smaller)`

## 勾配分析

### ケース: μ_B < μ_A (例: B=8, A=10)

#### 現在の実装
```
MAX(A,B) = A + MAX0(B - A)

∂MAX/∂B = ∂MAX0(B-A)/∂B
        = ∂MAX0/∂(B-A) × ∂(B-A)/∂B
        = Φ(μ_diff/σ_diff) × 1
        = Φ(μ_diff/σ_diff)

問題点：
- σ_diff = sqrt(Var[B-A])
- ∂σ_diff/∂B が負になる可能性
- 連鎖律で負の勾配が伝播
```

#### 提案の実装
```
MAX(A,B) = B + MAX0(A - B)

∂MAX/∂B = 1 + ∂MAX0(A-B)/∂B
        = 1 + ∂MAX0/∂(A-B) × ∂(A-B)/∂B
        = 1 + Φ(μ_diff/σ_diff) × (-1)
        = 1 - Φ(μ_diff/σ_diff)

利点：
- Φ(x) ∈ [0, 1]
- よって ∂MAX/∂B = 1 - [0,1] ∈ [0, 1]
- 常に非負！
```

## なぜこれが機能するか

### 直接寄与の効果

**提案**: smaller がベースなので、smaller の変化は直接 MAX に影響：
```
∂MAX/∂(smaller) = 1 + (MAX0からの間接寄与)
                = 1 - Φ(...)
```

- 直接寄与 `+1` が最初にある
- MAX0の寄与は減算形式
- **たとえ MAX0 の勾配計算で負の項が生じても、`1 - (...)` で非負が保証される**

**現在**: larger がベースなので、smaller の変化は間接的のみ：
```
∂MAX/∂(smaller) = 0 + (MAX0からの間接寄与のみ)
                = Φ(...)
```

- 間接寄与のみ
- σ_diff の勾配が負なら、全体が負
- **保証なし**

## 理論との整合性

理論的には：
```
∂max(A,B)/∂B ∈ [0, 1]  (常に非負)
```

提案の実装：
```
∂MAX/∂B = 1 - Φ(...) ∈ [0, 1]  ✓ 理論と一致
```

現在の実装：
```
∂MAX/∂B = Φ(...)  (σの勾配により負になりうる)  ✗ 理論と不一致
```

## Issue #158 との関係

Issue #158 のコメント：
> When A > B in mean, B-A has negative mean, so MAX0(B-A) ≈ 0
> This gives better numerical stability

これは**値の計算**に関する最適化でした。

しかし、**勾配の計算**では：
- MAX0(negative_mean) の勾配が問題を引き起こす
- 提案の方が勾配計算では安定

## トレードオフ

### 値の計算

**現在**:
- MAX0(B-A) は小さい値（≈0）
- 数値的に安定

**提案**:
- MAX0(A-B) は大きい値
- 数値的に less stable の可能性

### 勾配の計算

**現在**:
- 勾配が負になる
- 理論と不一致

**提案**:
- 勾配が常に非負
- 理論と一致

## 推奨

**勾配の正確性を優先すべき**。理由：

1. **感度解析の正確性が最重要**
2. 値の計算は既に検証済み（モンテカルロと一致）
3. MAX0(正の値) の計算も十分安定している（Clark近似は正の値でもよく機能する）

## 実装変更

```cpp
RandomVariable MAX(const RandomVariable& a, const RandomVariable& b) {
    // Gradient-correct ordering: smaller + MAX0(larger - smaller)
    // This ensures gradient monotonicity: ∂MAX/∂x ≥ 0
    if (a->mean() <= b->mean()) {  // <= instead of >=
        return RandomVariable(std::make_shared<OpMAX>(a, b));
    }
    return RandomVariable(std::make_shared<OpMAX>(b, a));
}
```

## 検証計画

1. Issue #242 のテストケースで確認
2. 既存のテストスイートで回帰テストなし
3. モンテカルロとの比較で勾配が一致することを確認
4. 値の計算精度への影響を確認

## 次のステップ

1. 実装を変更
2. テストを実行
3. 結果を検証
4. 問題なければコミット
