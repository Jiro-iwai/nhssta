# 負の勾配の根本原因分析

## 発見事項

### 1. 負のupstream勾配の発生

デバッグログから、`upstream=-11.199`という負のupstream勾配が発生していることが確認されました。

### 2. MUL演算での負の勾配伝播

```
Node[7] (op=MUL)
  Upstream gradient: 0.142927
  Left: Node[2] (value=-1)  ← 負の値！
  Right: Node[6] (value=0.666667)
  Propagating to left: 0.0952849
  Propagating to right: -0.142927  ← 負の勾配！
  ⚠️  NEGATIVE RIGHT GRADIENT!
```

**原因**: MUL演算では、`right_grad = upstream * left`です。
- `upstream = 0.142927` (正)
- `left = -1` (負)
- `right_grad = 0.142927 × (-1) = -0.142927` (負)

これは**数学的に正しい動作**です。

### 3. 負の勾配の連鎖

負の勾配が`Node[6]`に伝播し、さらに負の勾配を生成：

```
Node[6] (op=MUL)
  Upstream gradient: -0.142927  ← 負のupstream
  Left: Node[5] (value=0.816497)
  Right: Node[5] (value=0.816497)
  Propagating to left: -0.1167  ← 負の勾配
  Propagating to right: -0.1167  ← 負の勾配
  ⚠️  NEGATIVE LEFT GRADIENT!
  ⚠️  NEGATIVE RIGHT GRADIENT!
```

### 4. CUSTOM_FUNCTIONへの負の勾配伝播

最終的に、負の勾配がCUSTOM_FUNCTION（MAX0、MeanMaxなど）に伝播：

```
arg[0]: upstream=-11.199 × grad[0]=0.792892 = contrib=-8.87958
  ⚠️  NEGATIVE GRADIENT CONTRIBUTION!
```

## 根本原因

### 問題の本質

1. **MAX0の式**: `E[MAX0] = μ + σ × MeanMax(-μ/σ)`
2. **a = -(μ/σ)の計算**: `a = -(mu / sigma)`で、`mu < 0`の場合、`a > 0`になる
3. **しかし、式木の中間ノードで負の値が発生**:
   - `Node[2] (value=-1)` - これは`-(mu / sigma)`の`-`演算の結果
   - MUL演算で、この負の値が右側の勾配を負にする

### なぜ負の勾配が最終的なmean gradientに影響するのか？

1. **sigmaの勾配が負になる**: `sigma = sqrt(variance)`で、`variance`の勾配が負の場合、`sigma`の勾配も負になる
2. **MAX0の勾配計算**: `∂E[MAX0]/∂σ`が負の場合、`∂E[MAX0]/∂μ`に影響する
3. **多段MAX演算**: 負の勾配が累積され、最終的なmean gradientが負になる

## 数学的な検証

### MAX0の勾配公式

```
E[MAX0] = μ + σ × MeanMax(-μ/σ)
∂E[MAX0]/∂μ = 1 + σ × (∂MeanMax/∂a) × (∂a/∂μ)
            = 1 + σ × Φ(a) × (-1/σ)
            = 1 - Φ(a)
            = Φ(μ/σ)
```

これは常に`[0, 1]`の範囲内です。

### しかし、sigmaがvarianceに依存する場合

```
σ = sqrt(variance)
∂σ/∂variance = 1/(2×sqrt(variance)) = 1/(2σ)

∂E[MAX0]/∂variance = (∂E[MAX0]/∂σ) × (∂σ/∂variance)
                   = (∂E[MAX0]/∂σ) × (1/(2σ))
```

もし`∂E[MAX0]/∂σ`が負なら、`∂E[MAX0]/∂variance`も負になります。

### 問題: varianceの勾配が負になる理由

varianceは入力の分散に依存しますが、**variance自体はmeanに依存しません**。
しかし、多段MAX演算では：
1. `variance`が複数のパスを通る
2. 各パスから勾配が累積される
3. 負の勾配が累積され、最終的に負になる

## 結論

### バグの可能性

1. **MUL演算での負の勾配は数学的に正しい**
   - `right_grad = upstream * left`で、`left < 0`なら`right_grad < 0`は正しい

2. **しかし、最終的なmean gradientが負になるのは問題**
   - MAX0の理論的勾配は`Φ(μ/σ) ∈ [0, 1]`
   - 多段MAX演算でも、mean gradientは非負であるべき

3. **問題の原因**:
   - `sigma = sqrt(variance)`の勾配計算
   - 多段MAX演算での勾配の累積
   - 負のupstream勾配の伝播

### 修正の方向性

1. **sigmaの勾配計算の見直し**
   - `sigma = sqrt(variance)`の勾配が負になることを防ぐ
   - または、MAX0の実装でsigmaの勾配を直接扱う

2. **多段MAX演算での勾配計算の見直し**
   - 負の勾配が累積されないようにする
   - または、最終的なmean gradientが非負になることを保証する

3. **実装の詳細な検証**
   - なぜ`upstream=-11.199`が発生するのか
   - どのノードで負の値が発生しているのか
   - 負の勾配がどのように伝播しているのか

