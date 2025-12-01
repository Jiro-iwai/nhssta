# Issue #188: Expression ノード数の詳細分析

## 調査結果サマリー

s27.bench（10ゲート）で Expression ノード数が **4,063ノード** と異常に多い原因を詳細に調査しました。

## 測定結果（s27.bench）

### 全体のノード数

```
[DEBUG] Expression node count before building objective: 3
[DEBUG] Expression node count after building objective: 4063
```

### Endpoint 0 (G17) の内訳

```
[DEBUG] Endpoint 0 (G17): mean_expr=2745 nodes, std_expr=1310 nodes, total=4058 nodes
```

**単一の endpoint だけで 4,058ノード**が作成されています。

## ノード数が増える主要な箇所

### 1. `cov_expr()` の再帰的な呼び出し

`cov_expr()` が **1,000回以上** 呼び出されています。

**統計**:
- 呼び出し回数: 1,000回以上
- キャッシュヒット: 約200回
- キャッシュミス: 約800回

**問題**: `cov_expr()` の再帰的な展開が、深い RandomVariable ツリーに対して指数関数的にノード数を増やしています。

#### ADD expansion の例

```cpp
// C-5.3: ADD linear expansion - Cov(A+B, X) = Cov(A,X) + Cov(B,X)
result = cov_expr(a->left(), b) + cov_expr(a->right(), b);
```

**測定結果**:
- call #303: left=257 nodes, right=19 nodes
- call #890: left=445 nodes, right=12 nodes
- call #1465: left=404 nodes, right=25 nodes
- call #1506: 605 nodes → 728 nodes → 1138 nodes（累積的に増加）

**問題**: `cov_expr(a->left(), b)` と `cov_expr(a->right(), b)` が異なる RandomVariable ペアで呼び出されるため、それぞれが新しい Expression ノードを作成します。

### 2. `cov_max0_max0_expr_impl()` の呼び出し

`cov_max0_max0_expr_impl()` が呼び出されると、大量のノードが作成されます。

**内訳**（call #12 の例）:
- `cov_expr(d0, d1)`: 432-605 nodes（再帰的な呼び出し）
- `max0_mean_expr()`: 40 nodes
- `expected_prod_pos_expr()`: 81 nodes
- **合計**: 133-728 nodes

**問題**: `cov_expr(d0, d1)` が再帰的に呼び出され、その中でさらに `cov_expr()` が呼び出されるため、累積的にノード数が増加します。

### 3. `expected_prod_pos_expr()` の呼び出し

`expected_prod_pos_expr()` が **12回** 呼び出されています。

**統計**:
- キャッシュヒット: 0回
- キャッシュミス: 12回

**問題**: 異なる Expression オブジェクトで呼び出されているため、キャッシュが効いていません。毎回 **81ノード** が作成されます。

## 呼び出しチェーン

```
std_expr()
  → var_expr()
    → calc_var_expr()
      → cov_expr()  [再帰的に呼び出される]
        → cov_expr(a->left(), b)  [新しいノード作成]
        → cov_expr(a->right(), b)  [新しいノード作成]
        → cov_max0_max0_expr_impl()
          → cov_expr(d0, d1)  [再帰的に呼び出される]
          → expected_prod_pos_expr()  [81ノード]
```

## 問題の核心

### 1. 再帰的な `cov_expr()` 呼び出し

`cov_expr()` が再帰的に展開される際に、毎回新しい Expression ノードが作成されます。

**例**: `cov_expr(A+B, X)` の場合
```cpp
cov_expr(A+B, X) = cov_expr(A, X) + cov_expr(B, X)
```

- `cov_expr(A, X)` と `cov_expr(B, X)` は異なる RandomVariable ペア
- それぞれが新しい Expression ノードを作成
- 深い RandomVariable ツリーでは、指数関数的にノード数が増加

### 2. `cov_expr()` のキャッシュが効かないケース

`cov_expr()` 自体はキャッシュされていますが、以下の理由でキャッシュが効かないケースがあります：

1. **異なる RandomVariable ペア**: `cov_expr(A, X)` と `cov_expr(B, X)` は異なるペア
2. **再帰的な展開**: `cov_expr(A+B, X)` が `cov_expr(A, X)` と `cov_expr(B, X)` に展開される
3. **深いツリー**: 深い RandomVariable ツリーでは、多くの異なるペアが作成される

### 3. `expected_prod_pos_expr()` のキャッシュが効かない

`expected_prod_pos_expr()` は Expression オブジェクトのポインタベースのキャッシュを使用していますが、異なる Expression オブジェクトで呼び出されているため、キャッシュが効いていません。

**原因**:
- `cov_max0_max0_expr_impl()` 内で `d0->mean_expr()` と `d1->mean_expr()` が呼び出される
- これらの Expression オブジェクトは、`cov_expr()` の再帰的な呼び出しによって異なるものが作成される可能性がある

## 解決策の検討

### オプション 1: `cov_expr()` の再帰的な呼び出しを最適化

**問題**: `cov_expr(A+B, X)` が `cov_expr(A, X)` と `cov_expr(B, X)` に展開される際に、それぞれが新しい Expression ノードを作成する

**解決策**: Expression ノードの共有（構造共有）
- 同じ構造の Expression ノードを共有
- 実装が複雑（構造比較が必要）

### オプション 2: `expected_prod_pos_expr()` の値ベースキャッシュ

**問題**: Expression オブジェクトのポインタベースのキャッシュでは、異なる Expression オブジェクト（同じ値）でキャッシュが効かない

**解決策**: Expression の値ベースのキャッシュ
- Expression の値（`value()`）をキーにキャッシュ
- 問題: Expression の値が変更される可能性がある
- 問題: 勾配計算時に問題を起こす可能性がある

### オプション 3: RandomVariable レベルでの最適化

**問題**: `cov_expr()` の再帰的な呼び出しが、深い RandomVariable ツリーに対して指数関数的にノード数を増やしている

**解決策**: RandomVariable ツリーの構造を最適化
- フラット化や共通部分式の抽出
- 実装が複雑

### オプション 4: `cov_expr()` の呼び出し回数を削減

**問題**: `cov_expr()` が1,000回以上呼び出されている

**解決策**: `var_expr()` の計算を最適化
- `var_expr()` が `cov_expr()` を呼び出す回数を削減
- キャッシュをより積極的に活用

## 推奨される解決策

**段階的なアプローチ**:

1. **短期**: `expected_prod_pos_expr()` の値ベースキャッシュ（簡易版）
   - Expression の値（`value()`）をキーにキャッシュ
   - ただし、勾配計算時には無効化

2. **中期**: `cov_expr()` の呼び出し回数を削減
   - `var_expr()` の計算を最適化
   - キャッシュをより積極的に活用

3. **長期**: Expression ノードの共有（構造共有）
   - 同じ構造の Expression ノードを共有
   - 実装が複雑だが、最も効果的

## 次のステップ

1. `expected_prod_pos_expr()` の値ベースキャッシュを実装してテスト
2. `cov_expr()` の呼び出し回数を削減する方法を検討
3. より大きな回路で効果を確認

