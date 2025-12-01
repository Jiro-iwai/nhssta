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

## 提案: MAX演算子専用の Expression ノードタイプ

### 基本方針

MAX演算全体を1つの Expression ノードとして扱い、その中で勾配を計算します。内部では Expression を使いますが、個別の演算子（+、-、MAXなど）ごとに Expression tree を作ることはしません。

### 実装イメージ

MAX演算全体を1つの Expression ノードとして扱います：

```cpp
// src/expression.hpp
enum Op { 
    CONST = 0, PARAM, PLUS, MINUS, MUL, DIV, POWER, EXP, LOG, ERF, SQRT, PHI2,
    MAX_OP  // 新規追加: MAX演算全体を1つのノードとして扱う
};

// src/expression.cpp
double ExpressionImpl::value() {
    if (!is_set_value()) {
        if (op_ == MAX_OP) {
            // MAX演算の値を計算
            // left() = A, right() = B (RandomVariable への参照を保持)
            const RandomVariable* max_rv = ...;  // MAX演算の RandomVariable
            value_ = max_rv->mean();  // または max_rv->variance()
            is_set_value_ = true;
        }
        // ...
    }
    return value_;
}

void ExpressionImpl::propagate_gradient() {
    if (op_ == MAX_OP) {
        // MAX演算の勾配を計算
        // 内部で Expression を使うが、個別の演算子ごとの tree は作らない
        
        const RandomVariable* max_rv = ...;  // MAX演算の RandomVariable
        const OpMAX* max_op = dynamic_cast<const OpMAX*>(max_rv.get());
        
        const RandomVariable& A = max_op->left();
        const RandomVariable& B = max_op->right();
        const RandomVariable& max0_B_minus_A = max_op->max0();
        
        // MAX(A, B) = A + MAX0(B - A) の勾配を計算
        // 内部で Expression を使うが、+、-、MAX0 ごとの tree は作らない
        
        // A への勾配: upstream（MAX = A + MAX0(B-A) なので、A への勾配は upstream）
        // ただし、MAX0(B-A) の勾配も考慮する必要がある
        // MAX0(B-A) の勾配を計算する際も、内部で Expression を使うが、
        // 個別の演算子ごとの tree は作らない
        
        // 実装の詳細は、MAX0演算専用の Expression ノードタイプも追加する必要がある
        // ...
    }
}
```

### より具体的な実装

MAX演算の勾配計算は、以下の公式に基づきます：

```
MAX(A, B) = A + MAX0(B - A)

∂MAX/∂A = 1 + ∂MAX0(B-A)/∂A
∂MAX/∂B = ∂MAX0(B-A)/∂B
```

**重要なポイント**:
- MAX演算全体を1つの Expression ノードとして扱う
- 内部で Expression を使うが、個別の演算子（+、-、MAX0）ごとの Expression tree は作らない
- MAX0演算も同様に、1つの Expression ノードとして扱う

## 実装方針

### MAX演算専用の Expression ノードタイプを追加（推奨）

**基本方針**:
- MAX演算全体を1つの Expression ノードとして扱う
- 内部で Expression を使うが、個別の演算子（+、-、MAX0）ごとの Expression tree は作らない
- `cov_expr()` や `expected_prod_pos_expr()` を呼ばずに済む

**実装**:
1. `Op` enum に `MAX_OP` を追加
2. `ExpressionImpl` に MAX演算用のメンバを追加（RandomVariable への参照を保持）
3. `value()` メソッド: `RandomVariable::mean()` や `RandomVariable::variance()` を使用
4. `propagate_gradient()` メソッド: MAX演算の勾配を直接計算
   - 内部で Expression を使うが、個別の演算子ごとの tree は作らない
   - MAX0演算も同様に、1つの Expression ノードとして扱う

**利点**:
- ノード数を最小化できる
- Expression tree の構造を維持できる
- 既存の `backward()` メソッドをそのまま使用できる
- `cov_expr()` を呼ばずに済む

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

### フェーズ1: MAX演算専用の Expression ノードタイプの追加

1. **`Op` enum に `MAX_OP` を追加**
   - `src/expression.hpp` の `enum Op` に追加

2. **`ExpressionImpl` に MAX演算用のメンバを追加**
   - RandomVariable への参照を保持するメンバを追加
   - または、既存のメンバを再利用

3. **MAX演算用のコンストラクタを追加**
   - `src/expression.hpp` に宣言を追加
   - `src/expression.cpp` に実装を追加

### フェーズ2: 値計算と勾配計算の実装

4. **`value()` メソッドに `MAX_OP` の処理を追加**
   - `RandomVariable::mean()` や `RandomVariable::variance()` を使用
   - `cov_expr()` を呼ばずに済む

5. **`propagate_gradient()` メソッドに `MAX_OP` の処理を追加**
   - MAX演算の勾配を直接計算
   - 内部で Expression を使うが、個別の演算子ごとの tree は作らない
   - MAX0演算も同様に、1つの Expression ノードとして扱う

6. **`OpMAX::calc_var_expr()` を変更**
   - MAX演算専用の Expression ノードを作成するように変更

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

