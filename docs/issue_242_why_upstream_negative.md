# Issue #242: なぜupstreamが負になるのか？

## 質問

なぜ `upstream = -11.199` が負になるのか？

## 回答

**upstreamが負になる原因は、MAXの式木構造と勾配の累積メカニズムにあります。**

## MAXの式木構造

### MAX(MAX(A,B), MAX(A,C))の展開

```
MAX(MAX(A,B), MAX(A,C))
= MAX(A,C) + MAX0(MAX(A,B) - MAX(A,C))  [正規化後]
= [C + MAX0(A-C)] + MAX0([A + MAX0(B-A)] - [C + MAX0(A-C)])
```

### 勾配の伝播パス

MAX0(MAX(A,B) - MAX(A,C))へのupstream勾配は、複数のパスから累積されます：

1. **直接パス**: `∂MAX/∂MAX0(diff) = 1` (正)
2. **MAX(A,C)経由のパス**: 負の寄与が発生

## 負のupstreamが発生するメカニズム

### ステップ1: MAX(A,C)の勾配計算

```
MAX = MAX(A,C) + MAX0(diff)
∂MAX/∂MAX(A,C) = 1 + ∂MAX0(diff)/∂MAX(A,C)
```

### ステップ2: MAX0(diff)のMAX(A,C)に対する勾配

```
diff = MAX(A,B) - MAX(A,C)
∂diff/∂MAX(A,C) = -1 (MINUS演算により負!)

∂MAX0(diff)/∂MAX(A,C) = (∂MAX0/∂diff) × (∂diff/∂MAX(A,C))
                      = (正の値) × (-1)
                      = 負
```

### ステップ3: 負の勾配の累積

```
∂MAX/∂MAX(A,C) = 1 + (負の値)
```

もし `|∂MAX0(diff)/∂MAX(A,C)| > 1` なら、`∂MAX/∂MAX(A,C) < 0` になります。

### ステップ4: 負のupstreamの発生

この負の勾配が、MAX0(diff)へのupstream勾配に影響します：

```
upstream = ∂MAX/∂MAX0(diff)
```

しかし、MAX0(diff)はMAX(A,C)にも依存しているため、負の勾配が累積され、upstreamが負になります。

## デバッグログからの証拠

デバッグログでは、以下のような負の勾配の累積が観察されています：

```
Node[52] (op=CUSTOM_FUNCTION)
  Upstream gradient: 6
  ...
  Propagating to child nodes...
  gradient after: -11.199  ← 負の勾配が累積
```

## 問題の本質

1. **MAXの式木構造**: MAX(A,C)とMAX0(diff)が相互に依存している
2. **MINUS演算**: `diff = MAX(A,B) - MAX(A,C)` により、負の勾配が発生
3. **勾配の累積**: 負の勾配が式木を通じて累積され、upstreamが負になる

## 結論

upstreamが負になる原因は：

1. **MAXの式木構造の複雑さ**: MAX(A,C)とMAX0(diff)が相互に依存
2. **MINUS演算による負の勾配**: `∂diff/∂MAX(A,C) = -1`
3. **勾配の累積**: 負の勾配が式木を通じて累積され、upstreamが負になる

これは、MAX0の勾配計算自体の問題ではなく、**MAXの式木構造と勾配の累積メカニズム**の問題です。

## 修正の方向性

1. **MAXの式木構造の見直し**: MAX(A,C)とMAX0(diff)の依存関係を簡素化
2. **勾配の累積方法の見直し**: 負の勾配が累積されないようにする
3. **MAX0の実装の見直し**: 負の入力に対する勾配計算を検証

