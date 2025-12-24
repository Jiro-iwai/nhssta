# MAX演算の感度解析実験

## 概要

この実験では、MAX演算において入力のdelayの平均・分散を変化させたとき、出力のdelayの感度がどのように変化するかを調べます。

## 実験内容

### Experiment 1: Aの平均を変化
- 固定値: `var_A=4.0`, `mu_B=8.0`, `var_B=1.0`
- 変化範囲: `mu_A = 5.0 ~ 12.0` (0.5刻み)

### Experiment 2: Aの分散を変化
- 固定値: `mu_A=10.0`, `mu_B=8.0`, `var_B=1.0`
- 変化範囲: `var_A = 0.5 ~ 10.0` (0.5刻み)

### Experiment 3: Bの平均を変化
- 固定値: `mu_A=10.0`, `var_A=4.0`, `var_B=1.0`
- 変化範囲: `mu_B = 5.0 ~ 12.0` (0.5刻み)

### Experiment 4: Bの分散を変化
- 固定値: `mu_A=10.0`, `var_A=4.0`, `mu_B=8.0`
- 変化範囲: `var_B = 0.5 ~ 10.0` (0.5刻み)

### Experiment 5: 平均が等しい場合、両方の分散を変化
- 固定値: `mu_A=10.0`, `mu_B=10.0`
- 変化範囲: `var_A = 0.5 ~ 10.0`, `var_B = 0.5 ~ 10.0` (0.5刻み)

## 出力データ

各実験の結果はCSV形式で出力されます。以下の列が含まれます：

- `mu_A`, `var_A`: 入力Aの平均と分散
- `mu_B`, `var_B`: 入力Bの平均と分散
- `output_mean`, `output_var`: MAX演算の出力の平均と分散
- `grad_mu_A`, `grad_var_A`: 入力Aの平均・分散に対する感度（∂E[MAX]/∂μ_A, ∂Var[MAX]/∂σ²_A）
- `grad_mu_B`, `grad_var_B`: 入力Bの平均・分散に対する感度（∂E[MAX]/∂μ_B, ∂Var[MAX]/∂σ²_B）
- `grad_sigma_A`, `grad_sigma_B`: 入力A・Bの標準偏差に対する感度（∂Var[MAX]/∂σ_A, ∂Var[MAX]/∂σ_B）

## 実行方法

**実行ディレクトリ**: `example/` ディレクトリから実行します。

```bash
# exampleディレクトリに移動
cd example

# コンパイル（初回のみ、またはソース変更時）
g++ -std=c++17 -O -I../src -I../include -I. \
    ../src/random_variable.cpp ../src/normal.cpp ../src/add.cpp \
    ../src/sub.cpp ../src/max.cpp ../src/covariance.cpp \
    ../src/expression.cpp ../src/statistical_functions.cpp \
    ../src/util_numerical.cpp experiment_max_sensitivity.cpp \
    -o experiment_max_sensitivity

# すべての実験を実行
./experiment_max_sensitivity

# 特定の実験のみ実行（1-5）
./experiment_max_sensitivity 1  # Experiment 1のみ
./experiment_max_sensitivity 2  # Experiment 2のみ
./experiment_max_sensitivity 3  # Experiment 3のみ
./experiment_max_sensitivity 4  # Experiment 4のみ
./experiment_max_sensitivity 5  # Experiment 5のみ（時間がかかります）

# 結果をCSVファイルに保存
./experiment_max_sensitivity 1 > max_sensitivity_exp1.csv
./experiment_max_sensitivity 2 > max_sensitivity_exp2.csv
./experiment_max_sensitivity 3 > max_sensitivity_exp3.csv
./experiment_max_sensitivity 4 > max_sensitivity_exp4.csv
```

## 観察結果

### Experiment 1: Aの平均を変化させた場合
- Aの平均が大きくなるほど、`grad_mu_A`が増加（1.0に近づく）
- Aの平均が大きくなるほど、`grad_mu_B`が減少（0.0に近づく）
- Aの平均がBの平均より大きい場合、Aが支配的になり、`grad_mu_A ≈ 1.0`, `grad_mu_B ≈ 0.0`に近づく

**MAX関数の引数順序正規化の影響**:
- `mu_A < mu_B`の場合、MAX関数は内部的に`MAX(B, A) = B + MAX0(A-B)`として計算されます
- この場合、`grad_mu_A`は`MAX0(A-B)`の勾配から来るため、小さくなります（例：mu_A=7.5でgrad_mu_A=0.411532）
- `grad_mu_B`は`1 + MAX0(A-B)`の勾配から来るため、大きくなります（例：mu_A=7.5でgrad_mu_B=0.588468）
- これは数学的に正しく、`grad_mu_A + grad_mu_B = 1.0`が常に成り立ちます

### Experiment 2: Aの分散を変化させた場合
- Aの分散が大きくなるほど、`grad_var_A`が減少傾向
- Aの分散が大きい場合でも、`grad_mu_A`はほぼ1.0に近い（Aが支配的）

### Experiment 3: Bの平均を変化させた場合
- Bの平均が大きくなるほど、`grad_mu_B`が増加
- Bの平均がAの平均に近づくほど、両方の感度が0.5に近づく（対称性）

### Experiment 4: Bの分散を変化させた場合
- Bの分散が大きくなるほど、`grad_var_B`が増加傾向
- Bの分散が大きい場合でも、`grad_mu_B`は増加するが、Aが支配的な場合は負の値になることもある

## 技術的詳細

MAX演算はClark近似を使用して実装されています：
```
MAX(A, B) = A + MAX0(B - A)  (when E[A] >= E[B])
MAX(A, B) = B + MAX0(A - B)  (when E[A] < E[B])
```

ここで、`MAX0(X) = max(0, X)`です。

**引数の順序正規化**: MAX関数は、平均が大きい方をleft（基底）として統一します（Issue #158）。これにより、`MAX(A, B)`と`MAX(B, A)`は同じ結果を返しますが、感度解析の結果は元の変数AとBに対する勾配として正しく計算されます。

**勾配の範囲**: 
- `grad_mu_A + grad_mu_B = 1.0`が常に成り立ちます（数学的に保証されています）
- `grad_mu_A`と`grad_mu_B`はそれぞれ0.0から1.0の範囲にあります
- `mu_A > mu_B`の場合：`grad_mu_A`が大きく（1.0に近い）、`grad_mu_B`が小さい（0.0に近い）
- `mu_A < mu_B`の場合：`grad_mu_A`が小さく（0.0に近い）、`grad_mu_B`が大きい（1.0に近い）
- `mu_A = mu_B`の場合：両方とも約0.5になります（対称性）

感度解析は自動微分（Reverse-mode AD）を使用して計算されており、`Expression`クラスを通じて実装されています。

