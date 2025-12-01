# Issue #188: MAX演算子の勾配計算専用関数による最適化提案

## 概要

delayの計算には `+`, `-`, `MAX` の演算しかないため、MAX演算子に対する勾配を計算する専用関数を用意することで、ノード数を最小化できます。

## 現状の問題

### 現在の実装

`OpMAX::calc_var_expr()` が `cov_expr(left(), max0())` を呼び出し、それが再帰的に `expected_prod_pos_expr()` を呼び出しています：

```cpp
// src/max.cpp
Expression OpMAX::calc_var_expr() const {
    // Var[MAX(A,B)] = Var[A] + 2*Cov(A, MAX0(B-A)) + Var[MAX0(B-A)]
    Expression var_left = left()->var_expr();
    Expression var_max0 = max0()->var_expr();
    Expression cov = cov_expr(left(), max0());  // ← ここで大量のノードが作成される
    return var_left + Const(2.0) * cov + var_max0;
}
```

**問題点**:
- `cov_expr()` が再帰的に呼び出される
- その中で `expected_prod_pos_expr()` が呼ばれ、79ノードが作成される
- s27.bench では `cov_expr()` が1,000回以上呼び出される

### delay計算の特徴

SSTAでのdelay計算は以下の演算のみ：
- **`+` (ADD)**: 勾配は自明（そのまま伝播）
- **`-` (SUB)**: 勾配は自明（そのまま伝播）
- **`MAX`**: 勾配計算が複雑（`cov_expr()` が必要）

## 提案: MAX演算子の勾配計算専用関数

### 基本方針

MAX演算子に対する勾配を計算する専用関数を実装し、Expression tree を構築せずに直接勾配を計算します。

### 実装イメージ

```cpp
// src/max.cpp または src/expression.cpp

/**
 * @brief MAX演算子の勾配を直接計算する専用関数
 * 
 * MAX(A, B) = A + MAX0(B - A) の勾配を計算
 * 
 * @param max_rv MAX演算の結果（RandomVariable）
 * @param upstream 上流からの勾配
 * @param grad_left 左側（A）への勾配を格納
 * @param grad_right 右側（B）への勾配を格納
 */
void compute_max_gradient(const RandomVariable& max_rv,
                          double upstream,
                          std::unordered_map<RandomVariable, double>& gradients) {
    // MAX(A, B) = A + MAX0(B - A)
    const OpMAX* max_op = dynamic_cast<const OpMAX*>(max_rv.get());
    if (!max_op) {
        return;  // MAX演算でない場合は何もしない
    }
    
    const RandomVariable& A = max_op->left();
    const RandomVariable& max0_B_minus_A = max_op->max0();
    
    // MAX0(B-A) の勾配を計算
    // MAX0(B-A) の勾配は、MAX0演算の勾配計算専用関数を使用
    compute_max0_gradient(max0_B_minus_A, upstream, gradients);
    
    // A への勾配: upstream（MAX = A + MAX0(B-A) なので、A への勾配は upstream）
    gradients[A] += upstream;
    
    // B-A への勾配は既に compute_max0_gradient で計算済み
    // B への勾配を計算（B-A の勾配から B への勾配を導出）
    // ...
}
```

### より具体的な実装

MAX演算の勾配計算は、以下の公式に基づきます：

```
MAX(A, B) = A + MAX0(B - A)

∂MAX/∂A = 1 + ∂MAX0(B-A)/∂A
∂MAX/∂B = ∂MAX0(B-A)/∂B
```

MAX0演算の勾配は、既存の数値計算関数を使用できます：

```cpp
void compute_max0_gradient(const RandomVariable& max0_rv,
                          double upstream,
                          std::unordered_map<RandomVariable, double>& gradients) {
    const OpMAX0* max0_op = dynamic_cast<const OpMAX0*>(max0_rv.get());
    if (!max0_op) {
        return;
    }
    
    const RandomVariable& D = max0_op->left();
    
    // MAX0(D) の勾配を計算
    // E[MAX0(D)] = μ + σ × MeanMax(-μ/σ)
    // ∂E[MAX0(D)]/∂μ, ∂E[MAX0(D)]/∂σ を計算
    
    double mu = D->mean();
    double sigma = D->standard_deviation();
    
    // 数値微分または解析的勾配を使用
    // 既存の util_numerical.cpp の関数を利用可能
    // ...
}
```

## 実装方針

### オプション1: Expression tree を構築せずに勾配を直接計算（推奨）

**利点**:
- ノード数を最小化できる
- `cov_expr()` や `expected_prod_pos_expr()` を呼ばずに済む
- 値計算時も Expression tree を構築しない

**実装**:
1. `OpMAX::calc_var_expr()` を変更せず、勾配計算時に専用関数を使用
2. `backward()` メソッドで MAX演算を検出し、専用関数を呼び出す
3. または、MAX演算専用の Expression ノードタイプを追加

### オプション2: MAX演算専用の Expression ノードタイプを追加

**実装**:
1. `Op` enum に `MAX_OP` を追加
2. `ExpressionImpl` に MAX演算用のメンバを追加
3. `value()` メソッド: `util_numerical.cpp` の関数を使用
4. `propagate_gradient()` メソッド: 専用関数を使用して勾配を計算

**利点**:
- Expression tree の構造を維持できる
- 既存の `backward()` メソッドをそのまま使用できる

## 期待される効果

### ノード数の削減

現在の問題:
- `cov_expr()` が1,000回以上呼び出される
- 各呼び出しで `expected_prod_pos_expr()` が呼ばれ、79ノードが作成される
- 合計で約948ノード（12回 × 79ノード）が `expected_prod_pos_expr()` で作成される

MAX演算子の勾配計算専用関数を実装した場合:
- `cov_expr()` を呼ばずに直接勾配を計算
- `expected_prod_pos_expr()` を呼ばずに済む
- **948ノード削減**（`expected_prod_pos_expr()` で作成されるノード）

### 全体への影響

現在の最適化後: 3,481ノード
MAX演算子の勾配計算専用関数導入後: 3,481 - 948 = **2,533ノード**

**削減率**: 948 / 3,481 = **27.2%**

## 実装の難易度

**中**: 
- MAX演算の勾配計算の公式は既知
- 既存の数値計算関数（`util_numerical.cpp`）を利用可能
- Expression tree を構築しないため、実装が比較的簡単

## 5項演算子との比較

| 項目 | 5項演算子 | MAX演算子専用関数 |
|------|----------|------------------|
| **対象** | `expected_prod_pos_expr()` | `OpMAX::calc_var_expr()` |
| **削減ノード数** | 936ノード | 948ノード |
| **実装の難易度** | 中〜高 | 中 |
| **実装の期間** | 4-6日 | 3-5日 |
| **影響範囲** | `expected_prod_pos_expr()` のみ | MAX演算全体 |

**推奨**: MAX演算子専用関数の方が、より根本的な最適化で、実装も簡単です。

## 実装手順

### フェーズ1: MAX演算の勾配計算専用関数の実装

1. **`compute_max_gradient()` 関数を実装**
   - MAX演算の勾配を直接計算
   - `cov_expr()` を呼ばずに済む

2. **`compute_max0_gradient()` 関数を実装**
   - MAX0演算の勾配を直接計算
   - 既存の数値計算関数を利用

### フェーズ2: Expression tree への統合

3. **`backward()` メソッドを拡張**
   - MAX演算を検出して専用関数を呼び出す
   - または、MAX演算専用の Expression ノードタイプを追加

4. **`OpMAX::calc_var_expr()` を最適化**
   - 可能であれば、Expression tree を構築せずに値を計算

### フェーズ3: 検証と最適化

5. **テストの追加・更新**
   - 既存のテストが通ることを確認
   - 勾配計算のテストを追加

6. **ノード数の測定**
   - `s27.bench` でノード数を測定
   - 期待される削減（948ノード削減）を確認

## 注意点

### 1. 既存コードへの影響

**リスク**: `backward()` メソッドの変更が既存コードに影響を与える可能性がある

**対策**:
- 既存のテストを全て実行して回帰がないことを確認
- 段階的に実装し、各段階でテストを実行

### 2. 勾配計算の正確性

**リスク**: 専用関数の実装が既存の実装と異なる結果を返す可能性がある

**対策**:
- 既存の実装との比較テストを追加
- 数値微分との比較テストを追加

### 3. 実装の複雑さ

**リスク**: MAX演算の勾配計算が複雑で、バグが混入する可能性がある

**対策**:
- 既存の数値計算関数を最大限に活用
- 段階的に実装し、各段階でテストを実行

## 関連ドキュメント

- `docs/issue_188_summary.md`: 作業結果のまとめ
- `docs/issue_188_node_count_analysis.md`: ノード数が増える箇所の詳細分析
- `docs/issue_188_five_arg_operator_proposal.md`: 5項演算子導入の開発方針

## 実装の優先度

**高**: この最適化は5項演算子よりも根本的で、実装も簡単なため、優先的に実装することを推奨します。

