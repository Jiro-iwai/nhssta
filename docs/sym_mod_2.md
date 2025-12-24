# 修正指示書：多段MAXを許容しつつ、順序依存を排除する（ガウス閉包＝平均・分散一致の正規近似）

## 0. 設計判断（ユーザ合意）
- 多段MAX（例：`MAX(MAX(A,B), MAX(A,C))`）は **許容**する。
- ただし MAX の出力は一般に非ガウスになるので、各 MAX 出力は
  **同じ平均・分散を持つガウス（正規）で近似する**（ガウス閉包 / moment matching）。
- 以後の共分散伝播は「すべての変数が jointly Gaussian（共同正規）」である**モデル仮定**のもとで行う。
  - このモデル内では、MAX関連の式は「厳密（モデル整合的）」であり、かつ順序不変にできる。

---

## 1. ゴール（Acceptance Criteria）
### 1.1 順序不変
任意の `X,Y,W` について以下が成り立つこと（許容誤差は `1e-12` 推奨）：
- `mean(MAX(X,Y)) == mean(MAX(Y,X))`
- `var (MAX(X,Y)) == var (MAX(Y,X))`
- `cov(MAX(X,Y), W) == cov(MAX(Y,X), W)`
- `cov(W, MAX(X,Y)) == cov(W, MAX(Y,X))`
- `cov(MAX(X1,Y1), MAX(X2,Y2))` が、各 MAX の引数の並べ替えで不変

### 1.2 共分散の対称性
- `covariance(A,B) == covariance(B,A)` が常に成り立つ

### 1.3 多段MAXの安定性
- `Z = MAX(MAX(A,B), MAX(A,C))` と `Z' = MAX(MAX(A,C), MAX(A,B))` の平均・分散・任意Wとの共分散が一致

---

## 2. 方針（既存コードを活かす）
現在は `OpMAX` が `A + MAX0(B-A)` に固定分解されており、共分散もその分解に再帰依存しています（`covariance.cpp` / `cov_expr`）。
これが順序依存の主要因です。

**修正方針：**
- `OpMAX` は **分解を使わず**、平均・分散を「Clark式（2変数maxの式）」で直接計算する。
- `covariance(MAX(A,B), W)` は **MAX専用の閉包則**で直接計算する：
  \[
    \mathrm{Cov}(\max(A,B),W) = T\,\mathrm{Cov}(A,W) + (1-T)\,\mathrm{Cov}(B,W)
  \]
  ここで
  \[
    \theta = \sqrt{\sigma_A^2+\sigma_B^2-2\mathrm{Cov}(A,B)},\quad
    \alpha=\frac{\mu_A-\mu_B}{\theta},\quad
    T=\Phi(\alpha)
  \]
- これにより、MAXは「ガウス閉包の1演算」として扱われ、**多段MAXでも順序依存が出ない**。

> 注：この方針だと `MAX0` や `covariance_max0_max0`（Gauss-Hermite）を MAX のために使う必要はなくなる。
> ただし MAX0 自体を別機能として使うなら残してよい。

---

## 3. 具体的修正（AI実装者タスク）

### 3.1 `max.hpp/max.cpp`：`OpMAX` を「直接MAX（Clark式）」に変更
#### 3.1.1 `OpMAX` から `max0_` を削除（推奨）
- 現状：
  ```cpp
  OpMAX::OpMAX(left,right) : ..., max0_(MAX0(right-left)) { ... }
````

* 修正：

  * `max0_` メンバと `max0()` アクセサを削除（`max.hpp` からも削除）
  * `OpMAX` は `left/right` のみを保持する

> 既存コードを最大活用したい場合、まずは `max0_` を残しても良いが、
> **calc_mean/calc_variance では使わない**ようにする（段階的移行）。

#### 3.1.2 `OpMAX::calc_mean()` を Clark 式に置換（順序不変）

`A=left(), B=right()` として以下を実装：

* 取得：

  * `muA=A->mean()`, `muB=B->mean()`
  * `vA=A->variance()`, `vB=B->variance()`
  * `cAB=covariance(A,B)`
* 計算：
  [
  \theta^2 = vA + vB - 2cAB,\quad \theta=\sqrt{\max(\theta^2,\varepsilon)}
  ]
  `ε` は `MINIMUM_VARIANCE` を使用（`random_variable.hpp` に既にある）
  [
  \alpha=(\muA-\muB)/\theta,\quad T=\Phi(\alpha),\quad p=\phi(\alpha)
  ]
* 平均：
  [
  \mu_Z=\muA T + \muB(1-T) + \theta p
  ]

#### 3.1.3 `OpMAX::calc_variance()` を Clark 式に置換（順序不変）

上と同じ `θ, α, T, p` を使い：

[
E[Z^2] = (vA+\muA^2)T + (vB+\muB^2)(1-T) + (\muA+\muB)\theta p
]
[
\mathrm{Var}(Z)=E[Z^2]-\mu_Z^2
]

最後に `RandomVariableImpl::check_variance(r)` を通す。

#### 3.1.4 `OpMAX::calc_mean_expr/calc_var_expr` も同じ式へ

Expression側も同一式（Φとφ）を使う。`Phi_expr` は既存。
`phi_expr(x)` が無ければ追加する：

[
\phi(x)=\frac{1}{\sqrt{2\pi}}\exp(-x^2/2)
]

Expression側では分岐（θ≈0）を避けたいので、`theta = sqrt(theta2 + Const(MINIMUM_VARIANCE))` など “下駄” 方式でよい。

#### 3.1.5 `MAX(a,b)` の順序正規化は「任意」

この修正後、MAX演算自体が対称になるため、以下のどちらでも正しく動く：

* **推奨**：`MAX(a,b)` はそのまま `OpMAX(a,b)` を返す（単純化）
* 互換維持：Issue #158 の「平均で左を選ぶ」を残す場合は、

  * `|ma-mb| <= eps` のときだけタイブレークを入れて**決定的に**する（後述 3.3）

---

### 3.2 `covariance.cpp`：`OpMAX` 分岐を「MAX専用閉包則」に置換（最重要）

#### 3.2.1 数値版 `covariance(const RandomVariable&, ...)` の `OpMAX` 展開を削除

現状（削除対象）：

```cpp
// MAX(A,B)=A+MAX0(B-A)
const RandomVariable& x = a->left();
const RandomVariable& z = m->max0();
cov = covariance(z,b) + covariance(x,b);
```

これを、以下で置換：

* `a` が `OpMAX` の場合：`A=a->left()`, `B=a->right()`
* `T` を `A,B` の平均・分散・共分散から計算（3.1と同じ）
* 結果：
  [
  \mathrm{Cov}(\max(A,B),W)=T,\mathrm{Cov}(A,W)+(1-T),\mathrm{Cov}(B,W)
  ]

`b` が `OpMAX` の場合も同様に（左右対称）。

> これで `Cov(ガウス, MAX)` と `Cov(MAX, MAX)` は同じ規則で処理され、
> かつ順序に依存しません（`A↔B`で `T↔1-T` に変わるだけで結果は不変）。

#### 3.2.2 Expression版 `cov_expr` も同じ規則へ

現状（削除対象）：

```cpp
// MAX expansion - MAX(A,B) = A + MAX0(B-A)
result = cov_expr(x, b) + cov_expr(z, b);
```

を、同じ閉包則へ置換：

[
\mathrm{Cov}(\max(A,B),W)=T,\mathrm{Cov}(A,W)+(1-T),\mathrm{Cov}(B,W)
]
`T=Phi_expr(alpha)` を使用。

---

### 3.3 タイブレーク（順序正規化）の扱い：ポインタ順 vs 安定ID

この修正（3.1～3.2）を入れると、**結果の順序依存は消える**ので、
タイブレークは「DAGやキャッシュの正規化」のためだけになります。

#### 3.3.1 最小変更（現状踏襲）

* `covariance_matrix` の normalize は既に `a.get() <= b.get()` でポインタ順を使っている（実行間非再現だが、同一プロセスで一貫）。
* 統合テストが「数値一致のみ」を見ているなら、これで十分なことが多い。

#### 3.3.2 統合テストで実行間再現性が必要なら（推奨）

* `RandomVariableImpl` に `uint64_t stable_id_`（生成順インクリメント）を追加し、

  * `MAX()` の正規化・`CovarianceMatrixImpl::normalize()` の両方で stable_id を使う。
* これにより、実行間でもキー順が安定し、ゴールデンテストが安定する。

---

### 3.4 `covariance_x_max0_y`（Φ符号不整合）の扱い

今回の方針では MAX のために MAX0 を使わなくなるので、MAXの順序依存はここに依存しなくなる。
ただし **MAX0 を機能として使う**なら、数値版とExpression版を一致させるべき。

* Expression版は既に：
  [
  \mathrm{Cov}(X,\max(0,Z))=\mathrm{Cov}(X,Z)\Phi(\mu_Z/\sigma_Z)
  ]
* 数値版 `covariance_x_max0_y` も同一式に統一すること（`MeanPhiMax` の定義を確認し、曖昧なら `Phi_std` を明示実装）。

---

## 4. テスト計画（必須）

### 4.1 単体：MAXの順序不変（平均・分散）

* `X,Y` を Normal とし、`get_covariance_matrix()->set(X,Y,c)` で相関を与える。
* `Z1=MAX(X,Y)`, `Z2=MAX(Y,X)` について

  * `|Z1.mean - Z2.mean| < 1e-12`
  * `|Z1.var  - Z2.var | < 1e-12`

### 4.2 単体：Cov(ガウス, MAX) の順序不変

* `W,X,Y` を Normal、相関も適当に設定
* `cov(W, MAX(X,Y)) == cov(W, MAX(Y,X))`
* `cov(MAX(X,Y), W) == cov(MAX(Y,X), W)` も確認

### 4.3 単体：Cov(MAX, MAX) の順序不変（最重要）

* `X1,Y1,X2,Y2` を Normal、相関を設定
* 以下がすべて一致すること：

  * `cov(MAX(X1,Y1), MAX(X2,Y2))`
  * `cov(MAX(Y1,X1), MAX(X2,Y2))`
  * `cov(MAX(X1,Y1), MAX(Y2,X2))`
  * `cov(MAX(Y1,X1), MAX(Y2,X2))`

### 4.4 多段MAX：並べ替え不変（Issue #242 相当）

* `A,B,C` を Normal、相関を設定
* `Z = MAX(MAX(A,B), MAX(A,C))`
* `Z' = MAX(MAX(A,C), MAX(A,B))`
* `mean/var/cov(.,W)` が一致すること（Wも適当な Normal を追加）

### 4.5 共分散対称性

* 任意の `U,V` について `|cov(U,V) - cov(V,U)| < 1e-12`

### 4.6 PSDチェック（推奨）

ガウス閉包では「厳密なPSD」は理論上必ずしも保証されない場合がある（モデル誤差）。
ただし実装バグ検出に有効なので、テストで以下を推奨：

* 5～20個の変数（NormalとMAX混在）を作り、共分散行列 `Σ` を構成
* 固有値が `>= -1e-10` 程度であることをチェック（小さい負は丸めとして許容）

---

## 5. 変更点チェックリスト

* [ ] `OpMAX` が `A + MAX0(B-A)` に依存しない（calc_mean/varianceをClark式へ）
* [ ] `covariance()` と `cov_expr()` の `OpMAX` 分岐が「閉包則」になっている
* [ ] （任意）`MAX()` の順序正規化があっても、結果が順序不変
* [ ] テスト 4.1～4.5 がすべてパス
* [ ] （推奨）テスト 4.6 で大きな負の固有値が出ない

---

## 6. 実装メモ（関数の用意）

* `Phi(double)` と `phi(double)` は、既存 `statistical_functions.hpp` にあるなら使う。
* 無い/曖昧なら、数値版は以下で明示実装：

  * `Phi(x) = 0.5 * (1 + erf(x / sqrt(2)))`
  * `phi(x) = inv_sqrt_2pi * exp(-0.5*x*x)`

---

これで「多段MAXを許容（ガウス閉包）」しつつ、(2)(3) の共分散が **順序に依存しない**形になります。

```
