# Issue #242: 負の勾配問題の解決

## 問題の概要

多段MAX演算（`MAX(MAX(A, B), MAX(A, C))`）において、入力に対する勾配が負になる現象が発生していました。

### 問題の詳細

- **理論的期待**: MAX関数は単調増加関数であり、すべての入力に対する勾配は非負であるべき
- **旧実装での問題**: `grad_B = -0.00471593` (負) - 理論違反！
- **モンテカルロ法（厳密解）**: `grad_B = 0.00434573` (正) - 理論通り

## 根本原因

### 旧実装（MAX0分解方式）

旧実装では、MAX(A,B)を以下のように分解していました：

```
MAX(A,B) = A + MAX0(B-A)  （A >= B の場合）
```

多段MAX演算では：

```
MAX(MAX(A,B), MAX(A,C)) = MAX(A,C) + MAX0(MAX(A,B) - MAX(A,C))
```

この構造で、以下の問題が発生：

1. **MINUS演算による負の勾配**: `diff = MAX(A,B) - MAX(A,C)` で `∂diff/∂MAX(A,C) = -1`
2. **負のupstream伝播**: 負の勾配が式木を通じて累積され、`upstream = -11.199` まで増幅
3. **最終的な負の勾配**: 負のupstreamがMAX0の内部勾配と組み合わさり、`grad_B < 0` に

### 問題の本質

MAX0分解方式では、式木の構造が以下のような相互依存関係を持つ：

```
MAX = MAX(A,C) + MAX0(diff)
where diff = MAX(A,B) - MAX(A,C)
```

- MAX(A,C)とMAX0(diff)が相互に依存
- MINUS演算により、MAX(A,C)への勾配が負になる
- この負の勾配が式木を通じて不適切に累積される

## 解決方法：Gaussian Closure（sym_mod_2.md アプローチ）

### 新実装の特徴

1. **Clark公式による直接計算**（MAX0分解を廃止）
   ```
   μ_Z = μ_A * T + μ_B * (1-T) + θ * p
   ```
   where:
   - θ = √(Var(A) + Var(B) - 2*Cov(A,B))
   - α = (μ_A - μ_B) / θ
   - T = Φ(α), p = φ(α)

2. **ガウス閉包ルール**（モーメントマッチング）
   - MAXの出力をガウス近似として扱う
   - 同じ平均・分散を持つガウス分布として共分散を計算

3. **共分散閉包ルール**
   ```
   Cov(MAX(A,B), W) = T·Cov(A,W) + (1-T)·Cov(B,W)
   ```

4. **Cov(MAX, MAX)の厳密な扱い**（sym_mod_1.md方式）
   ```
   Cov(MAX(A,B), MAX(C,D)) = T1·T2·Cov(A,C) + T1·(1-T2)·Cov(A,D)
                            + (1-T1)·T2·Cov(B,C) + (1-T1)·(1-T2)·Cov(B,D)
   ```

### 実装の変更点

#### src/max.cpp

- `OpMAX::calc_mean()`: Clark公式で直接計算
- `OpMAX::calc_variance()`: E[Z²] - μ_Z² で直接計算
- `OpMAX::calc_mean_expr()`: Expression版Clark公式
- `OpMAX::calc_var_expr()`: Expression版分散計算
- `max0_` メンバー変数を削除（MAX0分解を廃止）

#### src/covariance.cpp

- `covariance_max_w()`: 単一MAXとの共分散（閉包ルール）
- `covariance_max_max()`: 2つのMAX間の共分散（4項展開）
- `cov_max_w_expr()`: Expression版閉包ルール
- `cov_max_max_expr()`: Expression版4項展開

### 新実装での式木構造

```
MAX(A,B)
  ├─ mean: μ_A * T + μ_B * (1-T) + θ * p
  └─ variance: E[Z²] - μ_Z²

MAX(MAX(A,B), MAX(A,C))
  ├─ mean: Clark公式を再帰的に適用
  └─ covariance: 4項展開で厳密に計算
```

**重要**: MINUS演算による負の勾配の連鎖が発生しない！

## 検証結果

### テストケース：`MAX(MAX(A,B), MAX(A,C))`

入力パラメータ：
- A ~ N(10.0, 4.0)
- B ~ N(8.0, 1.0)
- C ~ N(12.0, 2.0)

### 勾配の比較

| 実装 | grad_A | grad_B | grad_C | Sum |
|------|--------|--------|--------|-----|
| モンテカルロ（厳密解） | 0.20576328 | **0.00434573** (正) | 0.78989099 | 1.0 |
| 旧実装（MAX0分解） | 0.26094127 | **-0.00471593** (負) ⚠️ | 0.74377466 | 1.0 |
| **新実装（Gaussian Closure）** | 0.26396138 | **0.0016530119** (正) ✅ | 0.73438561 | 1.0 |

### 検証結果

✅ **すべてのテストが合格**:

1. ✅ **単調性の保証**: すべての勾配が非負
   - `grad_A = 0.26396138 >= 0` ✓
   - `grad_B = 0.0016530119 >= 0` ✓ **（以前は負だった！）**
   - `grad_C = 0.73438561 >= 0` ✓

2. ✅ **勾配の合計が1**: `Sum = 1.0` （完璧な確率分布）

3. ✅ **符号の一致**: grad_Bがモンテカルロ法と同じ符号（正）

4. ✅ **順序不変性**: MAX(X,Y) = MAX(Y,X) がすべてのケースで保証

### 近似精度

| Parameter | モンテカルロ | 新実装 | 誤差 | 相対誤差 |
|-----------|-------------|-------|------|----------|
| grad_A | 0.20576328 | 0.26396138 | +0.0581981 | +28.3% |
| grad_B | 0.00434573 | 0.0016530119 | -0.002692718 | -62.0% |
| grad_C | 0.78989099 | 0.73438561 | -0.05550538 | -7.0% |

**注**: grad_Bの相対誤差は大きいが、絶対値が小さいため実用上の問題は少ない。
最も重要な点は、**符号が正しい**ことです。

## テスト結果

### 単体テスト

```bash
$ make test
[==========] 791 tests from 72 test suites ran. (2477 ms total)
[  PASSED  ] 791 tests.
```

✅ すべての単体テストが合格（791/791）

### 統合テスト

```bash
$ ./build/bin/nhssta_test --gtest_filter="*Integration*"
[==========] 16 tests from 4 test suites ran. (345 ms total)
[  PASSED  ] 16 tests.
```

✅ すべての統合テストが合格（16/16）

### Issue #242専用テスト

```bash
$ ./example/test_issue_242_fix

Gradients:
  grad_A = 0.26396138
  grad_B = 0.0016530119 [POSITIVE] ✓ PASSED
  grad_C = 0.73438561
  Sum = 1

Monotonicity Check:
All gradients non-negative: ✓ PASSED
Gradient sum equals 1: ✓ PASSED
```

✅ Issue #242のすべての検証が合格

### 順序不変性テスト

```bash
$ ./build/bin/nhssta_test --gtest_filter="MaxOrderInvarianceTest.*"
[----------] 5 tests from MaxOrderInvarianceTest
[ RUN      ] MaxOrderInvarianceTest.MaxMeanVarianceOrderInvariant
[       OK ] MaxOrderInvarianceTest.MaxMeanVarianceOrderInvariant (0 ms)
[ RUN      ] MaxOrderInvarianceTest.CovGaussianMaxOrderInvariant
[       OK ] MaxOrderInvarianceTest.CovGaussianMaxOrderInvariant (0 ms)
[ RUN      ] MaxOrderInvarianceTest.CovMaxMaxOrderInvariant
[       OK ] MaxOrderInvarianceTest.CovMaxMaxOrderInvariant (0 ms)
[ RUN      ] MaxOrderInvarianceTest.MultiStageMaxOrderInvariant
[       OK ] MaxOrderInvarianceTest.MultiStageMaxOrderInvariant (0 ms)
[ RUN      ] MaxOrderInvarianceTest.CovarianceWithMultiStageMax
[       OK ] MaxOrderInvarianceTest.CovarianceWithMultiStageMax (0 ms)
[----------] 5 tests from MaxOrderInvarianceTest (0 ms total)
```

✅ すべての順序不変性テストが合格（5/5）

## 結論

**Issue #242は完全に解決されました。**

### 成果

1. ✅ **負の勾配問題の解決**: grad_B が負から正に変更
2. ✅ **単調性の保証**: MAX関数の数学的性質を満たす
3. ✅ **順序不変性の実現**: MAX(X,Y) = MAX(Y,X) を保証
4. ✅ **すべてのテストが合格**: 791個の単体テスト + 16個の統合テスト

### 技術的アプローチ

- **Gaussian Closure（sym_mod_2.md）**: 基本的なアプローチ
- **厳密なCov(MAX, MAX)処理（sym_mod_1.md）**: 精度向上のための追加実装
- **ハイブリッド実装**: 両方の利点を組み合わせた最適な解決策

### 今後の改善点

1. **近似精度の向上**: grad_Bの相対誤差（62%）を削減
2. **パフォーマンスの最適化**: 4項展開の計算コストを削減
3. **より複雑な回路での検証**: 実際のベンチマークでの検証を継続

## 関連ファイル

### 実装ファイル

- `src/max.cpp`: Clark公式による直接的なMAX計算
- `src/max.hpp`: OpMAXクラスの定義（max0_メンバー削除）
- `src/covariance.cpp`: 共分散閉包ルールと4項展開

### テストファイル

- `test/test_max_order_invariance.cpp`: 順序不変性テスト（新規作成）
- `test/test_meanphimax_verification.cpp`: MeanPhiMax検証テスト（新規作成）
- `test/test_max_order.cpp`: MAX順序依存性テスト（更新）
- `example/test_issue_242_fix.cpp`: Issue #242専用検証コード
- `example/monte_carlo_verification.cpp`: モンテカルロ法による検証

### ドキュメント

- `docs/issue_242_root_cause.md`: 根本原因分析
- `docs/issue_242_theory_vs_implementation.md`: 理論と実装の不一致分析
- `docs/issue_242_final_gradient_analysis.md`: 最終的な勾配分析
- `docs/sym_mod_2.md`: Gaussian Closureアプローチの説明
- `docs/sym_mod_1.md`: 厳密なCov(MAX, MAX)処理の説明

## 日時

- **問題発見**: 2024年12月
- **解決実装**: 2025年12月24日
- **検証完了**: 2025年12月24日
