# Issue #188: Expression ノード数の最適化 - 作業結果まとめ

## 作業概要

s27.bench（10ゲート）で Expression ノード数が異常に多い（4,063ノード）問題を調査し、最適化を実装しました。

## 実装した最適化

### 1. `phi2_expr()` の共通部分式の再利用

**実装内容**:
- `phi2_expr()` に `one_minus_rho2` と `sqrt_one_minus_rho2` を引数として渡す
- `expected_prod_pos_expr()` 内で既に計算済みの値を再利用

**効果**:
- `phi2_expr()` のノード数: 22 → 15ノード（7ノード削減、32%削減）
- `expected_prod_pos_expr()` のノード数: 79 → 65ノード（14ノード削減、18%削減）

### 2. `phi_expr()` と `Phi_expr()` のキャッシュ

**実装内容**:
- `phi_expr()` と `Phi_expr()` を inline 関数から通常の関数に変更
- ポインタベースのキャッシュを追加

**効果**:
- `phi_expr()` のノード数: 14 → 12ノード（2ノード削減、14%削減）
- `Phi_expr()` のノード数: 20 → 16ノード（4ノード削減、20%削減）
- 2回目の呼び出しでキャッシュが効く場合がある

### 3. 定数関連の最適化

**実装内容**:
- よく使われる定数を事前定義: `two` (2.0), `half` (0.5)
- `Const(1.0)` → `one`, `Const(2.0)` → `two`, `Const(-1.0)` → `minus_one`, `0.5` → `half`
- `/ 2.0` → `/ two`

**効果**:
- `Const()` の呼び出しを減らし、定数オブジェクトの再利用が可能
- `operator*` や `operator/` での定数比較が効くようになる
- 追加のノード削減

## 最適化前後の比較（s27.bench）

### 全体のノード数

| 項目 | 最適化前 | 最適化後 | 削減 | 削減率 |
|------|---------|---------|------|--------|
| Expression node count after building objective | 4,063 | 3,481 | **582** | **14.3%** |

### Endpoint 0 (G17) の内訳

| 項目 | 最適化前 | 最適化後 | 削減 | 削減率 |
|------|---------|---------|------|--------|
| mean_expr | 2,745 | 2,360 | **385** | **14.0%** |
| std_expr | 1,310 | 1,111 | **199** | **15.2%** |
| **合計** | **4,058** | **3,474** | **584** | **14.4%** |

### 各関数のノード数削減

| 関数 | 最適化前 | 最適化後 | 削減 | 削減率 |
|------|---------|---------|------|--------|
| `expected_prod_pos_expr()` | 79 | 65 | **14** | **18%** |
| `phi2_expr()` | 22 | 15 | **7** | **32%** |
| `phi_expr()` | 14 | 12 | **2** | **14%** |
| `Phi_expr()` | 20 | 16 | **4** | **20%** |

## 削減の内訳

| 項目 | 削減ノード数 |
|------|------------|
| `expected_prod_pos_expr()` の削減 | 168ノード（12回 × 14ノード） |
| `phi2_expr()` の削減 | 84ノード（12回 × 7ノード） |
| `phi_expr()` の削減 | 132ノード（66回 × 2ノード） |
| `Phi_expr()` の削減 | 68ノード（17回 × 4ノード） |
| その他の削減 | 130ノード |
| **合計削減** | **582ノード** |

## 実装した変更

### ファイル変更

1. **`src/expression.cpp`**:
   - `phi_expr()` と `Phi_expr()` を通常の関数に変更し、キャッシュを追加
   - `phi2_expr()` に `one_minus_rho2` と `sqrt_one_minus_rho2` を引数として追加
   - `Const(1.0)` → `one`, `Const(2.0)` → `two`, `Const(-1.0)` → `minus_one`, `0.5` → `half` に置き換え
   - `/ 2.0` → `/ two` に置き換え

2. **`src/expression.hpp`**:
   - `phi_expr()` と `Phi_expr()` の宣言を変更（inline から通常の関数に）
   - `phi2_expr()` のシグネチャを変更（オプショナル引数を追加）

3. **`src/ssta.cpp`**:
   - デバッグ出力を追加（ノード数とキャッシュ統計の測定）

4. **`test/test_expression_cache_investigation.cpp`**:
   - テストの期待値を更新（最適化によるノード数削減を許容）

### ドキュメント追加

1. **`docs/issue_188_node_count_analysis.md`**: ノード数が増える箇所の詳細分析
2. **`docs/issue_188_expected_prod_pos_expr_analysis.md`**: `expected_prod_pos_expr()` の詳細分析
3. **`docs/issue_188_optimization_proposal.md`**: 最適化提案
4. **`docs/issue_188_node_count_summary.md`**: 各関数での現状のノード数を表でまとめ
5. **`docs/issue_188_optimization_effect.md`**: s27.bench でのノード数削減効果
6. **`docs/issue_188_constant_optimization.md`**: 定数関連の最適化の詳細

## テスト結果

- **全652テストが成功**
- **統合テストも全て成功**
- `ExpressionCacheInvestigation.ExpectedProdPos_Breakdown` テストの期待値を更新（最適化によるノード数削減を許容）

## 今後の最適化の余地

### 1. `expected_prod_pos_expr()` の値ベースキャッシュ

**潜在的な削減**: 最大900ノード削減可能（12回呼び出し × 75ノード）

**課題**: Expression オブジェクトの値ベースの比較が必要（構造比較が必要）

### 2. `phi_expr()` と `Phi_expr()` の値ベースキャッシュ

**潜在的な削減**: 最大1,708ノード削減可能
- `phi_expr()`: 462ノード（66回呼び出し × 7ノード）
- `Phi_expr()`: 1,246ノード（178回呼び出し × 7ノード）

**課題**: 異なる Expression オブジェクト（同じ値）でキャッシュが効くようにする必要がある

### 3. `Phi2_expr()` と `phi2_expr()` の値ベースキャッシュ

**潜在的な削減**: 最大228ノード削減可能
- `Phi2_expr()`: 12ノード（12回呼び出し × 1ノード）
- `phi2_expr()`: 216ノード（12回呼び出し × 18ノード）

### 合計の潜在的な削減

| 項目 | 潜在的な削減 |
|------|------------|
| `expected_prod_pos_expr()` | 900ノード |
| `phi_expr()` | 462ノード |
| `Phi_expr()` | 1,246ノード |
| `Phi2_expr()` | 12ノード |
| `phi2_expr()` | 216ノード |
| **合計** | **2,836ノード** |

**削減率**: 2,836 / 4,063 = **69.8%**

## 結論

### 実装済みの最適化

- **削減ノード数**: 582ノード
- **削減率**: 14.3%
- **主な効果**: 
  - `phi2_expr()` の共通部分式の再利用（84ノード削減）
  - `expected_prod_pos_expr()` の最適化（168ノード削減）
  - `phi_expr()` と `Phi_expr()` のキャッシュ（200ノード削減）
  - 定数関連の最適化（130ノード削減）

### 今後の最適化の余地

- **潜在的な削減ノード数**: 2,836ノード
- **潜在的な削減率**: 69.8%
- **主な課題**: Expression オブジェクトの値ベースキャッシュの実装

## 関連ブランチ

- `feature/188-expression-function-cache`

## 関連ドキュメント

- `docs/issue_188_node_count_analysis.md`: ノード数が増える箇所の詳細分析
- `docs/issue_188_expected_prod_pos_expr_analysis.md`: `expected_prod_pos_expr()` の詳細分析
- `docs/issue_188_optimization_proposal.md`: 最適化提案
- `docs/issue_188_node_count_summary.md`: 各関数での現状のノード数を表でまとめ
- `docs/issue_188_optimization_effect.md`: s27.bench でのノード数削減効果
- `docs/issue_188_constant_optimization.md`: 定数関連の最適化の詳細

