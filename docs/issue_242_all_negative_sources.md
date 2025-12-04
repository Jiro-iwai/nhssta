# Issue #242: すべての負の勾配の発生源

## 調査結果

デバッグログを詳細に分析した結果、負の勾配は**MUL演算だけではなく、複数の演算で発生**していることが確認されました。

## 負の勾配が発生する演算

### 1. MUL演算（乗算）

**発生条件**: 左側の値が負の場合
```
f = left × right
∂f/∂right = left

例: f = (-1) × right
   ∂f/∂right = -1 (負!)
```

**デバッグログからの例**:
```
Node[7] (op=MUL)
  Left: Node[2] (value=-1)  ← 負の値
  Right: Node[6] (value=0.666667)
  Propagating to right: -0.142927  ← 負の勾配！
  ⚠️  NEGATIVE RIGHT GRADIENT!
```

**発生頻度**: 最も多い（デバッグログで多数確認）

### 2. MINUS演算（減算）

**発生条件**: 常に右側の勾配は負
```
f = left - right
∂f/∂left = 1 (正)
∂f/∂right = -1 (常に負!)

例: f = A - B
   ∂f/∂B = -1 (常に負!)
```

**デバッグログからの例**:
```
Node[30] (op=MINUS)
  Upstream gradient: 0.792892
  Propagating to left: 0.792892
  Propagating to right: -0.792892  ← 常に負！
  ⚠️  NEGATIVE RIGHT GRADIENT!
```

**発生頻度**: 中程度（デバッグログで確認）

### 3. DIV演算（除算）

**発生条件**: 左側の値が正の場合、右側の勾配は負
```
f = left / right
∂f/∂left = 1/right (正、right > 0の場合)
∂f/∂right = -left/(right²) (負、left > 0の場合!)

例: f = 10 / 2
   ∂f/∂2 = -10/4 = -2.5 (負!)
```

**デバッグログからの例**:
```
Node[9] (op=DIV)
  Left: Node[7] (value=-0.666667)
  Right: Node[8] (value=2)
  Propagating to left: 0.142927
  Propagating to right: 0.0476424  ← この例では正（leftが負のため）
```

**注意**: DIV演算では、左側が負の場合、右側の勾配は正になる可能性があります。

### 4. CUSTOM_FUNCTION（カスタム関数）

**発生条件**: 内部の式木で負の勾配が発生し、それが伝播する
```
CUSTOM_FUNCTION内部で:
  - MUL、MINUS、DIVなどで負の勾配が発生
  - それがupstreamと掛け合わされる
  - 結果として負の勾配が返される
```

**デバッグログからの例**:
```
arg[0]: upstream=-11.199 × grad[0]=0.792892 = contrib=-8.87958
  ⚠️  NEGATIVE GRADIENT CONTRIBUTION!
```

**発生頻度**: 高い（デバッグログで多数確認）

## 統計

デバッグログから、負の勾配の発生源をカウント：

| 演算タイプ | 発生回数 | 説明 |
|-----------|---------|------|
| MUL | 多数 | 左側が負の場合に右側の勾配が負 |
| MINUS | 中程度 | 常に右側の勾配が負 |
| DIV | 少数 | 左側が正の場合に右側の勾配が負 |
| CUSTOM_FUNCTION | 多数 | 内部の式木から負の勾配が伝播 |

## 数学的正確性

**重要な点**: これらすべての負の勾配は**数学的に正しい動作**です。

- MUL: `∂(left × right)/∂right = left` - leftが負なら負
- MINUS: `∂(left - right)/∂right = -1` - 常に負
- DIV: `∂(left/right)/∂right = -left/(right²)` - leftが正なら負
- CUSTOM_FUNCTION: 内部の式木の勾配を正しく伝播

## 問題の本質

負の勾配が発生すること自体は問題ではありません。問題は：

1. **最終的なmean gradientが負になること**
   - MAX0の理論的勾配は`Φ(μ/σ) ∈ [0, 1]`で常に非負
   - しかし、多段MAX演算では負の勾配が累積され、最終的なmean gradientが負になる

2. **多段MAX演算での勾配の累積方法**
   - 共有入力（A）が複数のパスを通る
   - 各パスから勾配が累積される
   - 負の勾配が累積され、最終的なmean gradientが負になる

## 結論

負の勾配の発生源は：
- **MUL演算**（最も多い）
- **MINUS演算**（常に右側が負）
- **DIV演算**（条件付きで負）
- **CUSTOM_FUNCTION**（内部から伝播）

これらすべては数学的に正しい動作です。問題は、**多段MAX演算での勾配の累積方法**にあります。

