# 修正指示書：MAX 関連の共分散を「順序に依存せず」「ガウス入力の範囲で厳密」にする（既存コード活用）

## 0. 前提とゴール

### 前提（この指示書での「ガウス」）
- ここで「ガウス」とは **正規分布に従う確率変数**を指す。
- `MAX` の **入力（left/right の子）は必ずガウス**とする（相関があってよい）。
- `ADD`/`SUB` による線形結合もガウス（子がガウスなら結果もガウス）。

### ゴール
共分散計算のパターン
1. `Cov(ガウス, ガウス)` … 現状でOK（維持）
2. `Cov(ガウス, MAX)` および `Cov(MAX, ガウス)`
3. `Cov(MAX, MAX)`
について、**引数順（MAX(X,Y) vs MAX(Y,X)）に依存せず**、かつ **「MAX の入力がガウス」なら理論式（数値積分含む）として厳密**に計算できるようにする。

---

## 1. 現状の順序依存の原因（要点）

現状 `OpMAX` は
- `MAX(A,B) = A + MAX0(B-A)` に固定分解（`max0_ = MAX0(right-left)`）
- `MAX(a,b)` は「平均が大きい方を left にする」正規化（Issue #158 対策）

これにより、呼び出し順や平均の微小差で left/right が変わると、内部の `B-A` が変わり、
- `Cov(X, MAX0(Z))` や `Cov(MAX0(D0), MAX0(D1))` の計算経路が変化
- さらに **数値版と Expression 版で `Cov(X, MAX0(Z))` の Φ の符号が不整合**
が起き、結果がズレる。

---

## 2. 方針（既存コードを最大活用しつつ順序依存を根絶）

### 方針A（最小改修で順序依存を除去：推奨）
1. `Cov(X, MAX0(Z))` の式を **数値版・Expression 版で完全一致**させる（最重要）。
2. `MAX(a,b)` の left/right 正規化を **決定的（deterministic）**にする
   - 平均比較だけに頼らず、タイブレークに **ポインタ順**を入れる（同一入力の順序反転でも同じ分解に落ちる）。
3. `Cov(MAX, MAX)` を **専用関数で展開して計算**し、一般再帰の分解に依存しないようにする
   - 展開は既存の `covariance_x_max0_y` と `covariance_max0_max0` を使う（入力がガウスなら厳密）。
4. `Cov(ガウス, MAX)` は「展開」でも良いが、順序依存を避けるために **専用関数化**して `MAX` の内部構造に依存させない（推奨）。

> ※「MAX 入力がガウス」なら、`MAX0` の中身 `B-A` もガウスなので、
> `Cov(ガウス, MAX0(ガウス))` と `Cov(MAX0(ガウス), MAX0(ガウス))` を正しく計算できれば、(2)(3) は“理論的に厳密”になります（数値積分は任意精度で収束）。

---

## 3. 具体修正（ファイル別）

## 3.1 `covariance.cpp`：Cov(X, MAX0(Z)) の式統一（最重要）

### 3.1.1 正しい式（ガウス前提で厳密）
`Z ~ N(μ_Z, σ_Z^2)` かつ `(X, Z)` が共同正規のとき

\[
\mathrm{Cov}(X, \max(0,Z)) = \mathrm{Cov}(X, Z)\,\Phi\!\left(\frac{\mu_Z}{\sigma_Z}\right)
\]

### 3.1.2 実装指示
- 関数 `covariance_x_max0_y(const RandomVariable& x, const RandomVariable& y)` の
  - コメントの式（現在 `Φ(-μ/σ)` と書かれている）を上式に修正
  - 計算を **`Phi(mu/sigma)` で統一**
- `cov_expr` 側の `cov_x_max0_expr` は既に `Phi_expr(mu/sigma)` を使っているので、数値側を合わせる。

#### 注意（既存関数 `MeanPhiMax` の意味が不明な場合）
- もし `MeanPhiMax(t)` が `Φ(t)` そのものではない可能性があるなら、**この場で使わない**。
- 代わりに標準正規 CDF を明示実装する：
  \[
  \Phi(x) = \frac{1}{2}\left(1+\mathrm{erf}\left(\frac{x}{\sqrt{2}}\right)\right)
  \]
  C++ では `std::erf` が使える。

#### 擬似コード
```cpp
static inline double Phi_std(double x) {
  return 0.5 * (1.0 + std::erf(x / std::sqrt(2.0)));
}

static double covariance_x_max0_y(const RandomVariable& x, const RandomVariable& y) {
  // y is OpMAX0: y = max(0, Z)
  const RandomVariable& z = y->left();
  double c  = covariance(x, z);
  double mu = z->mean();
  double vz = z->variance();
  if (vz <= 0.0) throw ...
  double sz = std::sqrt(vz);
  double t  = mu / sz;
  double cov = c * Phi_std(t);
  return cov;
}
````

---

## 3.2 `max.cpp`：MAX の引数正規化を「決定的」にする（既存の Issue #158 を壊さない）

### 3.2.1 要件

* `MAX(X,Y)` と `MAX(Y,X)` が **同じ left/right の並び**になるようにする。
* ただし Issue #158 の意図（平均差が大きいときに数値安定）も可能なら維持する。

### 3.2.2 実装指示（平均＋タイブレーク方式）

* `a->mean()` と `b->mean()` が十分に離れている場合は従来どおり平均で決める
* ほぼ同じなら **ポインタ順で決める**

擬似コード：

```cpp
RandomVariable MAX(const RandomVariable& a, const RandomVariable& b) {
  constexpr double MEAN_TIE_EPS = 1e-12; // あるいは要求精度に合わせて
  double ma = a->mean();
  double mb = b->mean();

  if (ma > mb + MEAN_TIE_EPS) return RV(OpMAX(a,b));
  if (mb > ma + MEAN_TIE_EPS) return RV(OpMAX(b,a));

  // tie-break: deterministic
  if (a.get() <= b.get()) return RV(OpMAX(a,b));
  return RV(OpMAX(b,a));
}
```

> これで、平均がほぼ同じケースでも左右がぶれず、順序依存の温床を除去できます。

---

## 3.3 `covariance.cpp`：Cov(MAX, MAX) を専用で厳密計算（既存の MAX0 計算を活かす）

### 3.3.1 目標

`Cov(MAX1, MAX2)` を一般再帰（`covariance()` の OpMAX 分岐）に任せず、**必ず同じ展開で計算**する。

### 3.3.2 厳密展開（MAX 入力ガウス前提）

`MAX(A,B) = A + MAX0(B-A)` を、`OpMAX` が保持する `left()` と `max0()` を使って固定展開する。

* `M1 = MAX(A,B) = A + Z1` （`Z1 = m1->max0()`）
* `M2 = MAX(C,D) = C + Z2` （`Z2 = m2->max0()`）

このとき
[
\mathrm{Cov}(M1, M2) =
\mathrm{Cov}(A, C) + \mathrm{Cov}(A, Z2) + \mathrm{Cov}(Z1, C) + \mathrm{Cov}(Z1, Z2)
]

ここで

* `Cov(A,Z2)` は既存 `covariance_x_max0_y(A, Z2)` が（式を修正すれば）ガウス前提で厳密
* `Cov(Z1,Z2)` は既存 `covariance_max0_max0(Z1, Z2)` が（入力ガウスなら）数値積分で厳密

### 3.3.3 実装指示

* `static double covariance_max_max(const RandomVariable& a_max, const RandomVariable& b_max)` を新設し、

  * `a_max` と `b_max` が `OpMAX` であることを `dynamic_cast` で検証
  * 上の4項和で計算し `check_covariance` に通す
* `covariance()` の `OpMAX`×`OpMAX` の分岐をこの専用関数へ置換する。

擬似コード：

```cpp
static double covariance_max_max(const RandomVariable& a, const RandomVariable& b) {
  auto am = a.dynamic_pointer_cast<const OpMAX>();
  auto bm = b.dynamic_pointer_cast<const OpMAX>();
  const RandomVariable& A  = a->left();
  const RandomVariable& C  = b->left();
  const RandomVariable& Z1 = am->max0(); // MAX0(B-A)
  const RandomVariable& Z2 = bm->max0(); // MAX0(D-C)

  double t1 = covariance(A, C);
  double t2 = covariance(A, Z2);  // -> uses covariance_x_max0_y after fixing
  double t3 = covariance(Z1, C);
  double t4 = covariance(Z1, Z2); // -> covariance_max0_max0
  return t1 + t2 + t3 + t4;
}
```

---

## 3.4 `cov_expr` 側も順序依存を避ける（同じ構造で）

Expression 版 `cov_expr` の OpMAX 分岐は現在、
`Cov(MAX(A,B), X) = Cov(A,X) + Cov(MAX0(B-A), X)` の再帰展開になっている。

### 指示

* Expression 版でも `Cov(MAX,MAX)` は専用関数（上と同じ4項）で組み立てる。
* `Cov(ガウス, MAX)` についても、可能なら専用関数化して再帰分解依存を減らす（ただし必須ではない）。

> 目的は「順序依存排除」なので、少なくとも `Cov(MAX,MAX)` は必ず同じ展開を踏むようにする。

---

## 4. 追加の安全策（推奨）

### 4.1 「MAX の入力がガウス」をコードで守る

* `OpMAX` のコンストラクタ（または `MAX()`）で、

  * 入力が `OpMAX` / `OpMAX0` を含む（＝非ガウス）なら例外 or assert を入れる
    （あなたのアプリ前提をコードで保証する）

例：

```cpp
static bool is_gaussian_input(const RandomVariable& v) {
  return dynamic_cast<const NormalImpl*>(v.get()) != nullptr
      || dynamic_cast<const OpADD*>(v.get()) != nullptr
      || dynamic_cast<const OpSUB*>(v.get()) != nullptr;
  // 必要なら ADD/SUB を再帰チェックして「線形結合のみ」を保証
}
```

---

## 5. テスト計画（順序依存が消えたことを証明する）

### 5.1 ユニットテスト：MAX 自体の順序不変

**目的**：`MAX(X,Y)` と `MAX(Y,X)` が同じ結果になること。

* 入力：`X,Y` をガウス（Normal）として作成
* 期待：

  * `mean(MAX(X,Y)) == mean(MAX(Y,X))`
  * `var (MAX(X,Y)) == var (MAX(Y,X))`

※ 可能なら、`MAX()` が作る `OpMAX` の `left/right` が順序反転でも同一になることも検査（ポインタ比較）。

---

### 5.2 ユニットテスト：Cov(ガウス, MAX) の順序不変

**目的**：パターン2が順序に依存しないこと。

* 入力：ガウス `W,X,Y`
* 準備：必要なら `get_covariance_matrix()->set(...)` で相関を与える
* 期待：

  * `cov(W, MAX(X,Y)) == cov(W, MAX(Y,X))`
  * `cov(MAX(X,Y), W) == cov(MAX(Y,X), W)`（対称性も含める）

---

### 5.3 ユニットテスト：Cov(MAX, MAX) の順序不変（最重要）

**目的**：パターン3が順序に依存しないこと。

* 入力：ガウス `X1,Y1,X2,Y2`
* 期待：以下のすべてが一致（許容誤差内）

  * `cov(MAX(X1,Y1), MAX(X2,Y2))`
  * `cov(MAX(Y1,X1), MAX(X2,Y2))`
  * `cov(MAX(X1,Y1), MAX(Y2,X2))`
  * `cov(MAX(Y1,X1), MAX(Y2,X2))`

---

### 5.4 回帰テスト：`covariance_x_max0_y` の符号整合（数値 vs Expression）

**目的**：数値版と Expression 版の式不整合を二度と起こさない。

* 入力：ガウス `X,Z` と `MAX0(Z)`
* 期待：

  * `covariance(X, MAX0(Z))` と `cov_expr(X, MAX0(Z))->value()` が一致（許容誤差内）

---

### 5.5 退化ケース（数値安定）

* `Var(D)=Var(X-Y)` が極小（≒0）でも NaN/Inf を出さない
* 相関係数が ±1 に極めて近いケース（`covariance_max0_max0` が既に特別扱いしているなら同様に）

---

### 5.6 キャッシュテスト（性能＋順序不変）

**目的**：順序反転でキャッシュが二重化しないこと。

* `get_covariance_matrix()->clear()`
* `cov(MAX(X,Y), MAX(U,V))` を計算
* `cov(MAX(Y,X), MAX(U,V))` を計算
* 期待：

  * キャッシュサイズが不必要に増えない（少なくとも爆発しない）

---

## 6. 変更点まとめ（チェックリスト）

* [ ] `covariance_x_max0_y` を `Cov(X,Z)*Phi(muZ/sigmaZ)` に統一（数値版）
* [ ] コメント（Φの符号）も統一
* [ ] `MAX()` の正規化に tie-break（ポインタ順）を追加して決定的に
* [ ] `Cov(MAX,MAX)` を専用関数で 4項展開し、常に同じ経路で計算
* [ ] Expression 版 `cov_expr` でも `Cov(MAX,MAX)` を同じ4項で構成
* [ ] 上記 5.1〜5.6 のテストを追加

---

## 7. 補足（なぜこれで「厳密」になるか）

この修正は「MAX 入力がガウス」という前提のもとで、

* `B-A` や `D-C` もガウス
* `Cov(ガウス, MAX0(ガウス))` は Φ を用いた閉形式で厳密
* `Cov(MAX0(ガウス), MAX0(ガウス))` は既存の Gauss-Hermite 求積で任意精度に収束（数値評価として厳密）
  となるため、(2)(3) の計算は **理論式の数値評価**として厳密になる。

以上。
