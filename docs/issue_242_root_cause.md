# Issue #242: 負の勾配の根本原因分析

## 問題の概要

多段MAX演算（`MAX(MAX(A, B), MAX(A, C))`）において、入力に対する勾配が負になる現象が発生しています。

## デバッグログからの発見

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

## 根本原因の仮説

### 仮説1: MAX0の式木構造による負の勾配

MAX0の実装：
```cpp
E[MAX0] = μ + σ × MeanMax(-μ/σ)
```

ここで、`a = -(μ/σ)`の計算で：
- `Node[2] (value=-1)` - これは`-`演算の結果
- MUL演算で、この負の値が右側の勾配を負にする

### 仮説2: sigmaの勾配が負になる

`sigma = sqrt(variance)`で：
- `variance`の勾配が負の場合、`sigma`の勾配も負になる
- 負の`sigma`勾配がMAX0の勾配計算に影響する

### 仮説3: 多段MAX演算での勾配の累積

多段MAX演算では：
1. 共有入力（A）が複数のパスを通る
2. 各パスから勾配が累積される
3. 負の勾配が累積され、最終的に負になる

## 検証が必要な点

1. **なぜ`upstream=-11.199`が発生するのか？**
   - どのノードで負のupstreamが発生しているか
   - 負のupstreamがどのように伝播しているか

2. **MAX0の勾配計算は正しいか？**
   - `∂E[MAX0]/∂μ = Φ(μ/σ) ∈ [0, 1]`が保証されているか
   - `sigma`の勾配が負になることがあるか

3. **多段MAX演算での勾配の累積は正しいか？**
   - 負の勾配が累積されることがあるか
   - 最終的なmean gradientが非負であるべきか

## 次のステップ

1. **デバッグログの詳細分析**
   - `upstream=-11.199`が発生するノードを特定
   - 負のupstreamがどのように伝播しているかを追跡

2. **MAX0の実装の検証**
   - `sigma = sqrt(variance)`の勾配計算を検証
   - 負の勾配が発生する条件を特定

3. **修正案の検討**
   - sigmaの勾配計算の見直し
   - 多段MAX演算での勾配計算の見直し

