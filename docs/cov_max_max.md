````markdown
# Cov(max, max) 修正仕様書 (STAC ベース SSTA)

## 0. 背景

対象コードは `RandomVariable` 名前空間における共分散計算ロジックです。

```cpp
double covariance(const RandomVariable& a, const RandomVariable& b);
````

* `OpMAX` : `max(x, y)` を表す RandomVariable
* `OpMAX0`: `max(0, z)` を表す RandomVariable
* STAC 流の分解

  * `max(X,Y) = X + max(0, Y-X)`
  * `Cov(X + D⁺, B) = Cov(X,B) + Cov(D⁺,B)`

現状コードでは
**`OpMAX0 × OpMAX0` の共分散を `covariance_x_max0_y` で計算しており、理論的前提を破っているため `Cov(max, max)` が不正確** になります。

---

## 1. 問題点（現状コードの誤り）

### 1.1 `covariance_x_max0_y` の前提

```cpp
static double covariance_x_max0_y(const RandomVariable& x,
                                  const RandomVariable& y);
```

* `y` は `OpMAX0` (`y = max(0, Z)`) を想定
* `z = y->left()` がガウス（正規）乱数
* `x` と `z` は **2 変量正規 (joint Gaussian)** と仮定
* 理論的には
  [
  \mathrm{Cov}(x,\max(0,Z))
  = \mathrm{Cov}(x,Z)\cdot \Phi(\mu_Z/\sigma_Z)
  ]
* よって **`x` はガウス（線形結合）でなければならない**

### 1.2 誤用されている箇所

```cpp
} else if (dynamic_cast<const OpMAX0*>(a.get()) != nullptr &&
           dynamic_cast<const OpMAX0*>(b.get()) != nullptr) {
    if (a->left() == b->left()) {
        cov = a->variance();
    } else if (a->level() < b->level()) {
        cov = covariance_x_max0_y(a, b);
    } else if (b->level() < a->level()) {
        cov = covariance_x_max0_y(b, a);
    } else {
        double cov0 = covariance_x_max0_y(a, b);
        double cov1 = covariance_x_max0_y(b, a);
        cov = (cov0 + cov1) * 0.5;
    }
}
```

* `a` / `b` がともに `OpMAX0` なので、
  片方を `x` として `covariance_x_max0_y` に渡すと、
  **非ガウス (max を通した後) に対して joint Gaussian 用の公式を適用している**。
* これは理論的に誤りで、`Cov(max0, max0)` → `Cov(max, max)` が崩れる。

---

## 2. 修正方針（高レベル）

1. `OpMAX0 × OpMAX0` の共分散は
   **専用の数値積分ロジックで計算**する。
2. それ以外 (`ガウス × max0`, `max × 任意`, `加算/減算`) は、既存ロジックを維持。
3. 数値積分は

   * 差分 `D0, D1` が 2 変量正規であると仮定
   * `D0⁺ = max(0,D0)`, `D1⁺ = max(0,D1)` を定義
   * (\mathrm{Cov}(D_0^+,D_1^+) = E[D_0^+D_1^+] - E[D_0^+]E[D_1^+])
   * `E[D⁺]` は解析式を使用
   * `E[D0⁺ D1⁺]` は **1 次元 Gauss–Hermite 積分**で評価

---

## 3. 数学仕様

### 3.1 前提

* `D0 = a->left()` の値

  * 平均 `μ0 = E[D0]`
  * 分散 `σ0² = Var(D0) > 0`
* `D1 = b->left()` の値

  * 平均 `μ1 = E[D1]`
  * 分散 `σ1² = Var(D1) > 0`
* 共分散 `covD = Cov(D0, D1)`
* 相関係数
  [
  \rho = \frac{\mathrm{Cov}(D_0,D_1)}{\sigma_0 \sigma_1}
  ]
  は数値誤差を考慮して `[-1,1]` にクリップすること。

### 3.2 1 変量：E[D⁺]

[
E[D^+]
= \sigma,\varphi(\mu/\sigma) + \mu,\Phi(\mu/\sigma)
]

* `φ(t)` : 標準正規 PDF
  [
  \varphi(t) = \frac{1}{\sqrt{2\pi}}\exp(-t^2/2)
  ]
* `Φ(t)` : 標準正規 CDF

### 3.3 2 変量：E[D0⁺ D1⁺] を 1 次元積分で

`Z, Z1` 独立標準正規として

[
\begin{aligned}
D_0 &= \mu_0 + \sigma_0 Z,\
D_1 &= \mu_1 + \sigma_1(\rho Z + \sqrt{1-\rho^2},Z_1).
\end{aligned}
]

このとき

[
E[D_0^+D_1^+] = E_Z\bigl[(\mu_0+\sigma_0 Z)^+ \cdot E[D_1^+ \mid Z]\bigr].
]

条件付き分布

[
D_1\mid Z=z \sim \mathcal{N}(m(z), s^2)
]
[
m(z) = \mu_1 + \rho\sigma_1 z,\quad s = \sigma_1\sqrt{1-\rho^2}.
]

1 変量結果を用いて

[
E[D_1^+ \mid Z=z]
= s,\varphi!\Bigl(\frac{m(z)}{s}\Bigr)

* m(z),\Phi!\Bigl(\frac{m(z)}{s}\Bigr).
  ]

したがって

[
E[D_0^+D_1^+]
= \int_{-\infty}^{\infty}
(\mu_0+\sigma_0 z)^+,
g(m(z), s),
\varphi(z),dz,
]
[
g(m,s) = s,\varphi(m/s) + m,\Phi(m/s).
]

### 3.4 共分散

[
\mathrm{Cov}(D_0^+, D_1^+)
= E[D_0^+D_1^+] - E[D_0^+]E[D_1^+].
]

`OpMAX0(a)` と `OpMAX0(b)` の共分散は
`Cov(a,b) = Cov(D0⁺, D1⁺)` とする。

---

## 4. 実装仕様（関数レベル）

### 4.1 新規ヘルパー関数

#### 4.1.1 標準正規 PDF / CDF

※ プロジェクトに既存実装がある場合はそれを使用。ここではインターフェースのみ指定。

```cpp
// φ(x) = N(0,1) の PDF
double normal_pdf(double x);

// Φ(x) = N(0,1) の CDF
double normal_cdf(double x);
```

#### 4.1.2 E[D⁺] のヘルパー

```cpp
// D ~ N(mu, sigma^2) のとき E[max(0,D)] を返す
double expected_positive_part(double mu, double sigma);
```

仕様:

* `sigma > 0` を前提（呼び出し側でチェック）
* 実装式:

  ```cpp
  double t = mu / sigma;
  return sigma * normal_pdf(t) + mu * normal_cdf(t);
  ```

#### 4.1.3 Gauss–Hermite 10点ルール

標準正規ウェイト用のノード・重み（定数配列）を追加:

```cpp
// ∫_{-∞}^{∞} φ(z) f(z) dz ≒ Σ wphi[i] * f(zphi[i])
static constexpr int GH_N = 10;
static const double zphi[GH_N] = { ... }; // ノード
static const double wphi[GH_N] = { ... }; // 重み (sum ≒ 1.0)
```

（具体値はコード側で埋め込み。`sum(wphi)` が 1 になるようにする）

任意関数 f に対して `E[f(Z)]` を近似する汎用ヘルパー:

```cpp
template <class F>
double gh_expectation_phi(F f) {
    double s = 0.0;
    for (int i = 0; i < GH_N; ++i) {
        s += wphi[i] * f(zphi[i]);
    }
    return s;
}
```

#### 4.1.4 E[D0⁺D1⁺] の数値積分

```cpp
// 指定されたパラメータに対し E[D0^+ D1^+] を評価する
double expected_prod_pos(double mu0, double sigma0,
                         double mu1, double sigma1,
                         double rho);
```

仕様:

* 入力: `sigma0 > 0`, `sigma1 > 0`
* `rho` は内部で [-1,1] にクリップして使用
* 実装:

  1. `one_minus_rho2 = max(0, 1 - rho*rho)`

  2. `s1_cond = sigma1 * sqrt(one_minus_rho2)`

  3. integrand:

     ```cpp
     f(z) = D0plus * E_D1pos_givenZ
     ```

     where

     * `d0 = mu0 + sigma0*z`
     * if `d0 <= 0` → `return 0`
     * `m1z = mu1 + rho*sigma1*z`
     * if `s1_cond > 0`:

       * `t = m1z / s1_cond`
       * `E_D1pos_givenZ = s1_cond * φ(t) + m1z * Φ(t)`
     * else (`s1_cond == 0`, ρ=±1 の極限):

       * `E_D1pos_givenZ = max(0, m1z)` とする
     * `D0plus = d0`（d0>0 の場合）
     * `f(z) = D0plus * E_D1pos_givenZ`

  4. `expected_prod_pos = gh_expectation_phi(f)`

#### 4.1.5 Cov(max0, max0) 専用関数

```cpp
// a = max(0, D0), b = max(0, D1) を想定し Cov(a,b) を返す
double covariance_max0_max0(const RandomVariable& a,
                            const RandomVariable& b);
```

仕様:

1. `d0 = a->left()`, `d1 = b->left()` を取得
2. `mu0 = d0->mean()`, `v0 = d0->variance()`
3. `mu1 = d1->mean()`, `v1 = d1->variance()`
4. `v0 <= 0` または `v1 <= 0` の場合は例外をスロー
5. `sigma0 = sqrt(v0)`, `sigma1 = sqrt(v1)`
6. `covD = covariance(d0, d1)`（既存の再帰的 covariance を呼ぶ）
7. `rho = covD / (sigma0 * sigma1)` を計算し、数値誤差込みで [-1,1] にクリップ
8. `E0pos = expected_positive_part(mu0, sigma0)`
9. `E1pos = expected_positive_part(mu1, sigma1)`
10. `Eprod = expected_prod_pos(mu0, sigma0, mu1, sigma1, rho)`
11. 戻り値:

    ```cpp
    return Eprod - E0pos * E1pos;
    ```

---

## 5. `covariance()` 本体の修正

### 5.1 修正対象ブロック

現在の `OpMAX0 × OpMAX0` ブロックを削除し、以下に差し替える。

#### Before

```cpp
} else if (dynamic_cast<const OpMAX0*>(a.get()) != nullptr &&
           dynamic_cast<const OpMAX0*>(b.get()) != nullptr) {
    if (a->left() == b->left()) {
        cov = a->variance();  // maybe here is not reachable

    } else if (a->level() < b->level()) {
        cov = covariance_x_max0_y(a, b);

    } else if (b->level() < a->level()) {
        cov = covariance_x_max0_y(b, a);

    } else {
        double cov0 = covariance_x_max0_y(a, b);
        double cov1 = covariance_x_max0_y(b, a);
        cov = (cov0 + cov1) * 0.5;
    }
}
```

#### After

```cpp
} else if (dynamic_cast<const OpMAX0*>(a.get()) != nullptr &&
           dynamic_cast<const OpMAX0*>(b.get()) != nullptr) {

    if (a->left() == b->left()) {
        // max(0,D) 同士なら Cov = Var(max(0,D))
        cov = a->variance();
    } else {
        cov = covariance_max0_max0(a, b);
    }
}
```

### 5.2 `covariance_x_max0_y` の適用範囲コメント

`covariance_x_max0_y` の定義にコメントを追加する。

```cpp
// covariance_x_max0_y(x, y):
// y が OpMAX0 (y = max(0, Z)) のとき、
// (x, Z) がjoint Gaussian であると仮定して Cov(x, max(0, Z)) を計算する。
// x が OpMAX / OpMAX0 （非ガウス）である場合には使用してはならない。
static double covariance_x_max0_y(const RandomVariable& x,
                                  const RandomVariable& y);
```

---

## 6. テスト仕様（最低限）

### 6.1 一致ケース (同一変数)

* 条件:

  * `a = max(0, D)` `b = max(0, D)`（同じ `D` を共有）
* 期待値:

  * `Cov(a,b) == Var(a)`（分散と共分散が一致）
* 実装:

  * `if (a->left() == b->left())` ケースで `cov = a->variance()` によって保証される。

### 6.2 対称性チェック

* 条件:

  * 任意の `OpMAX0 a, b`
* 期待値:

  * `covariance(a,b) == covariance(b,a)`（対称性）
* 確認:

  * `covariance_max0_max0(a,b)` と `covariance_max0_max0(b,a)` の差が `eps` 以下であることを確認。

### 6.3 特殊相関

1. `ρ = 0` のケース

   * `D0, D1` 独立 → `Cov(D0⁺, D1⁺)` は 0 に近いはず
     （期待値は理論的には 0 ではないが、相関 0 なので small。簡易 sanity check に使用）

2. `ρ = 1` のケース

   * `D1 = a*D0 + b` の線形関係で `ρ=±1` を実現
   * 数値的に極限 (`s1_cond → 0`) の処理が安定しているかを確認

### 6.4 Cov(max, max) の簡単な回路例

* `max(X,Y)` と `max(X,Y)` を作り、以下を検証:

  * `Cov(max(X,Y), max(X,Y)) == Var(max(X,Y))`（自己共分散）
  * `X=Y` のとき、`max(X,Y)=X` → `Cov(max,max) == Var(X)` になること

---

## 7. パフォーマンスと精度

* デフォルトの GH 点数：`GH_N = 10`

  * 10 点 Gauss–Hermite で通常の SSTA 精度には十分と想定
* 精度が不足する場合（レポートがあれば）:

  * `GH_N` を 16 点などに拡張可能なよう、実装を `GH_N` 定数に依存させること
* 数値安定性

  * 分散 ≦ 0 の場合は例外をスローし、上位で処理
  * `rho` を `[-1,1]` にクリップ
  * `1 - rho^2` を計算するときは `max(0.0, 1.0 - rho*rho)` で負数を防ぐ

---

## 8. まとめ

* **目的**: `Cov(max, max)` を理論的に正しい形に修正する。
* **方針**:

  * `OpMAX0 × OpMAX0` の共分散を、
    差分 `D0,D1` の 2 変量正規を前提とした 1D Gauss–Hermite 積分で計算する。
  * `covariance_x_max0_y` は **ガウス×max0 専用** に限定する。
* **変更点**:

  * `covariance()` の `OpMAX0×OpMAX0` ブロックを `covariance_max0_max0()` 呼び出しに差し替え。
  * 新規ヘルパー関数: `expected_positive_part`, `expected_prod_pos`, `covariance_max0_max0` 等を実装。

この仕様に従って修正すれば、STAC 流の前提（2 変量正規）を守りつつ、
`Cov(max,max)` を高精度に計算できるようになります。

```
::contentReference[oaicite:0]{index=0}
```
