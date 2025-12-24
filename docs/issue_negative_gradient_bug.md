# Issue: Negative Gradients in Multi-Stage MAX Operations

## 問題の概要

多段MAX演算（例：`MAX(MAX(A, B), MAX(A, C))`）において、入力に対する勾配が負になる現象が発生しています。理論的には、MAX演算の勾配は常に非負であるべきですが、実際の実装では負の勾配が観察されています。

## 再現手順

### テストケース
```cpp
Normal A(10.0, 4.0);
Normal B(8.0, 1.0);   // mu_B < mu_A
Normal C(12.0, 2.0);  // mu_C > mu_A

auto MaxAB = MAX(A, B);
auto MaxAC = MAX(A, C);
auto MaxABC = MAX(MaxAB, MaxAC);

zero_all_grad();
auto mean_expr = MaxABC->mean_expr();
mean_expr->backward();

double grad_B = B->mean_expr()->gradient();
// grad_B = -0.004716 (負の値!)
```

### 実行結果
```
A ~ N(10.0, 4.0)
B ~ N(8.0, 1.0)
C ~ N(12.0, 2.0)

grad_A = 0.260941
grad_B = -0.004716  [NEGATIVE]
grad_C = 0.743775
Sum = 1.000000
```

## 期待される動作

### 理論的期待
1. **MAX0の勾配は常に非負**
   - `∂E[max(0, D)]/∂μ = Φ(μ/σ) ∈ [0, 1]`
   - これは数学的に証明されています

2. **単段MAX演算の勾配は常に非負**
   - `∂MAX(A,B)/∂A ∈ [0, 1]`、`∂MAX(A,B)/∂B ∈ [0, 1]`
   - `∂MAX(A,B)/∂A + ∂MAX(A,B)/∂B = 1`

3. **多段MAX演算の勾配も非負であるべき**
   - 連鎖律により、非負の勾配の組み合わせは非負であるべき
   - しかし、実際には負の勾配が発生している

## 実際の動作

### 観察された現象
- 1089ケースのテストで、82ケース（7.5%）で負の勾配が発生
- 負の勾配が発生する条件：
  - `mu_B < mu_A`かつ`mu_C > mu_A`の場合に`grad_B < 0`
  - `mu_C < mu_A`かつ`mu_B > mu_A`の場合に`grad_C < 0`

### 重要な点
- 勾配の和は常に1.0（これは正しい）
- しかし、個々の勾配が負になることは理論的に説明できない

## 原因の仮説

### 仮説1: MAX0の実装における`σ = sqrt(variance)`の勾配計算

MAX0は以下のように実装されています：
```cpp
E[max(0, D)] = μ + σ × MeanMax(-μ/σ)
```

ここで、`σ = sqrt(variance)`です。

問題の可能性：
- `variance`が入力の分散に依存する
- `σ = sqrt(variance)`の勾配が`variance`の勾配を通じて計算される
- 連鎖律により、負の勾配が発生する可能性がある

### 仮説2: 自動微分の実装における勾配の累積

多段MAX演算では：
- 共有入力（A）が複数のパスを通る
- 勾配が複数のパスから累積される
- 自動微分の実装で、負の勾配が発生する可能性がある

### 仮説3: MAX関数の正規化と勾配計算の不整合

MAX関数は正規化を行います：
```cpp
if (a->mean() >= b->mean()) {
    return MAX(a, b);  // = a + MAX0(b-a)
} else {
    return MAX(b, a);  // = b + MAX0(a-b)
}
```

問題の可能性：
- 正規化により、勾配の計算パスが変わる
- 多段MAX演算で、正規化と勾配計算の不整合が発生する可能性がある

## 影響範囲

### 影響を受ける機能
1. **感度解析（Sensitivity Analysis）**
   - 負の勾配は、入力パラメータが出力に負の影響を与えることを示す
   - しかし、理論的にはこれは正しくない可能性がある

2. **最適化**
   - 負の勾配に基づく最適化が誤った方向に進む可能性がある

### 影響を受けない機能
- 勾配の和は常に1.0（これは正しい）
- 平均値と分散の計算は正しい

## 関連するコード

### MAX0の実装
- `src/max.cpp`: `OpMAX0::calc_mean_expr()`
- `src/statistical_functions.cpp`: `max0_mean_expr()`
- `src/random_variable.cpp`: `calc_std_expr()` (σ = sqrt(variance))

### MAXの実装
- `src/max.cpp`: `MAX()`関数（正規化）
- `src/max.cpp`: `OpMAX::calc_mean_expr()`

### 自動微分の実装
- `src/expression.cpp`: `backward()`, `propagate_gradient()`
- `src/expression.hpp`: `CustomFunctionImpl::eval_with_gradient()`

## 検証方法

### テストコード
- `example/test_multi_stage_max.cpp`: 多段MAX演算のテスト
- `example/test_negative_gradient_sum.cpp`: 負の勾配の検証
- `example/analyze_negative_gradient_conditions.cpp`: 条件の分析

### 検証結果
- 1089ケースのテストで、82ケースで負の勾配が発生
- すべてのケースで勾配の和は1.0（これは正しい）

## 修正の方向性

### オプション1: MAX0の勾配計算の修正
- `σ = sqrt(variance)`の勾配計算を修正
- `variance`が入力の分散に依存しないようにする

### オプション2: MAX関数の正規化の見直し
- 正規化と勾配計算の整合性を確保
- 多段MAX演算での正規化の影響を調査

### オプション3: 自動微分の実装の修正
- 勾配の累積方法を修正
- 負の勾配が発生しないようにする

## 優先度

**高**: 感度解析の正確性に影響する可能性がある

## 関連Issue

- Issue #188: MAX演算の勾配計算に関する議論
- 感度解析の実装に関する議論

## 参考資料

- `example/mathematical_proof_max0_gradient.md`: MAX0の勾配の数学的証明
- `example/ADMISSION_NO_RIGOROUS_PROOF.md`: 負の勾配の条件の証明の困難さ
- `example/NEGATIVE_GRADIENT_CONDITIONS.md`: 負の勾配が発生する条件の分析

