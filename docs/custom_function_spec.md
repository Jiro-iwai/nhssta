# カスタム関数機能 仕様書（AI 実装者向け・完全版）

## 0. 前提

既存コードベースには少なくとも以下の型・機能が存在する（`expression.hpp` / `expression.cpp`）：

* `class ExpressionImpl`
* `using Expression = ...`
  （`ExpressionImpl` へのハンドル型。`e->value()`, `e->backward()` などが使える）
* `class Variable`

  * 内部的に `Expression` を保持しているラッパ。
  * `Variable` から `Expression` への暗黙変換も存在する（既存仕様に従う）。
* `enum class Op { CONST, PARAM, PLUS, ..., PHI2, ... }`
* 逆伝播・評価ロジック：

  * `double ExpressionImpl::value()`
  * `void ExpressionImpl::backward(double upstream)`
  * `void ExpressionImpl::zero_grad()`
  * `void zero_all_grad()`（全ノード勾配リセット）
  * トポロジカルソート / DFS 関数（`topo_sort_dfs` 等）

すべての `ExpressionImpl` はグローバルなコンテナ（`Expressions eTbl()` など）に保存される想定。

---

## 1. 目的

1. ユーザーが **式木 API（Expression / Variable）を使って** カスタム関数
   ( f: \mathbb{R}^n \to \mathbb{R} ) を定義できるようにする。
2. カスタム関数内部の式木は **一度だけ構築** し、後は再利用する。
3. カスタム関数は

   * (A) `value()` / `gradient()` により、メイン式木と独立して評価できる。
   * (B) メイン式木の中から「ノード」として呼び出せる（**今回の主目的**）。
4. カスタム関数内部式木の値・勾配計算は、メイン式木の勾配リセットや `backward()` と独立して動作する。
5. スレッドセーフティは今回の実装では必須ではないが、後から対応しやすいよう設計を分離する。

---

## 2. Builder の定義

### 2.1 型

```cpp
using CustomFunctionBuilder =
    std::function<Expression(const std::vector<Variable>&)>;
```

* 引数：`const std::vector<Variable>& v`

  * `v[i]` が「関数の i 番目の引数」を表すローカルな `Variable`。
* 戻り値：`Expression`

  * 関数の出力を表す **式木の根ノード** を返す。

### 2.2 役割

* Builder は「**関数の定義を書いたラムダ**」である。
* ライブラリが用意した `n` 個のローカル `Variable` を受け取り、
  それらを使って式木（`Expression`）を構築して返す。
* 数値評価や勾配計算は Builder 自身は行わず、**ただ式木を組み立てるだけ**。

### 2.3 呼ばれるタイミング

* `CustomFunction` 作成時に **1回だけ** 呼び出される。
* 実際の数値評価中（`value()`, `gradient()`, メイン式木の `backward()`）では Builder は呼ばない。

---

## 3. 内部実装クラス `CustomFunctionImpl`

### 3.1 宣言

```cpp
class CustomFunctionImpl {
public:
    using Builder = CustomFunctionBuilder;

    CustomFunctionImpl(size_t input_dim,
                       Builder builder,
                       const std::string& name = "");

    size_t input_dim() const noexcept;
    const std::string& name() const noexcept;

    // --- 独立評価 API ---

    // x.size() != input_dim() の場合 std::invalid_argument を throw
    double value(const std::vector<double>& x);
    std::vector<double> gradient(const std::vector<double>& x);
    std::pair<double, std::vector<double>>
    value_and_gradient(const std::vector<double>& x);

    // メイン式木からの呼び出し用:
    // args_values.size() != input_dim() なら std::invalid_argument
    std::pair<double, std::vector<double>>
    eval_with_gradient(const std::vector<double>& args_values);

private:
    size_t input_dim_;
    std::string name_;                 // 関数名（print 用）
    std::vector<Variable> local_vars_; // 内部入力変数（Builder で使う）
    Expression output_;                // 内部式木の出力 Expression

    // 内部式木ノード一覧（output_ から DFS して一度だけ構築）
    std::vector<ExpressionImpl*> nodes_;

    // 内部ヘルパ
    void build_nodes_list();
    void collect_nodes_dfs(ExpressionImpl* node,
                           std::unordered_set<ExpressionImpl*>& visited);

    // 共通前処理: 入力のセット＋内部式木の値/勾配クリア
    void set_inputs_and_clear(const std::vector<double>& x);
};
```

### 3.2 コンストラクタの挙動

擬似コード：

```cpp
CustomFunctionImpl::CustomFunctionImpl(size_t input_dim,
                                       Builder builder,
                                       const std::string& name)
    : input_dim_(input_dim)
{
    // 名前の決定
    if (name.empty()) {
        static std::atomic<size_t> counter{0};
        size_t id = counter++;
        name_ = "custom_f_" + std::to_string(id);
    } else {
        name_ = name;
    }

    // 1. ローカル入力変数の生成
    local_vars_.resize(input_dim_);

    // 2. Builder 呼び出し（式木構築）
    //    Builderは「このローカル変数たちを使って Expression を組み立てて返す」
    output_ = builder(local_vars_);

    // 3. 内部ノード一覧の構築
    build_nodes_list();
}
```

#### 注意点

* Builder 内では、**原則として `local_vars_` のみを使って式木を構築する** 前提（外部の `Variable` / `Expression` を使うと独立性が壊れる）。
* `build_nodes_list()` は `output_` から DFS/BFS で辿れる `ExpressionImpl*` をすべて `nodes_` に格納する。

  * 重複は `std::unordered_set` などで排除する。

### 3.3 set_inputs_and_clear()

```cpp
void CustomFunctionImpl::set_inputs_and_clear(const std::vector<double>& x) {
    if (x.size() != input_dim_) {
        throw std::invalid_argument(
            "CustomFunctionImpl::set_inputs_and_clear: size mismatch");
    }

    // 1. ローカル入力変数へ値をセット
    for (size_t i = 0; i < input_dim_; ++i) {
        local_vars_[i] = x[i];   // 既存の Variable の代入規約に従う
    }

    // 2. 内部式木の value キャッシュ & 勾配をクリア
    for (auto* node : nodes_) {
        // ここは既存実装に合わせる：
        // - 値キャッシュの無効化 (unset_value())
        // - 勾配フィールドのゼロクリア (zero_grad())
        node->unset_value();   // ない場合は適切な実装を追加する
        node->zero_grad();
    }
}
```

### 3.4 value / gradient / value_and_gradient

```cpp
double CustomFunctionImpl::value(const std::vector<double>& x) {
    set_inputs_and_clear(x);
    return output_->value();
}

std::vector<double>
CustomFunctionImpl::gradient(const std::vector<double>& x) {
    set_inputs_and_clear(x);
    output_->value();
    output_->backward(1.0);

    std::vector<double> grad(input_dim_);
    for (size_t i = 0; i < input_dim_; ++i) {
        grad[i] = local_vars_[i]->gradient();
    }
    return grad;
}

std::pair<double, std::vector<double>>
CustomFunctionImpl::value_and_gradient(const std::vector<double>& x) {
    set_inputs_and_clear(x);
    double v = output_->value();
    output_->backward(1.0);

    std::vector<double> grad(input_dim_);
    for (size_t i = 0; i < input_dim_; ++i) {
        grad[i] = local_vars_[i]->gradient();
    }
    return {v, std::move(grad)};
}
```

### 3.5 eval_with_gradient（メイン式木から利用）

```cpp
std::pair<double, std::vector<double>>
CustomFunctionImpl::eval_with_gradient(const std::vector<double>& args_values) {
    return value_and_gradient(args_values);  // ラッパで OK
}
```

---

## 4. ユーザー向けクラス `CustomFunction`（ラッパ）

### 4.1 ハンドル型

```cpp
using CustomFunctionHandle = std::shared_ptr<CustomFunctionImpl>;
```

### 4.2 ラッパークラス

```cpp
class CustomFunction {
public:
    using Builder = CustomFunctionImpl::Builder;

    CustomFunction() = default;

    explicit CustomFunction(CustomFunctionHandle impl)
        : impl_(std::move(impl)) {}

    // ファクトリ
    static CustomFunction create(size_t input_dim,
                                 Builder builder,
                                 const std::string& name = "") {
        return CustomFunction(
            std::make_shared<CustomFunctionImpl>(
                input_dim, std::move(builder), name));
    }

    bool valid() const noexcept { return static_cast<bool>(impl_); }
    explicit operator bool() const noexcept { return valid(); }

    size_t input_dim() const {
        ensure_valid();
        return impl_->input_dim();
    }

    const std::string& name() const {
        ensure_valid();
        return impl_->name();
    }

    // --- 独立評価 API ---

    double value(const std::vector<double>& x) const {
        ensure_valid();
        return impl_->value(x);
    }

    std::vector<double> gradient(const std::vector<double>& x) const {
        ensure_valid();
        return impl_->gradient(x);
    }

    std::pair<double, std::vector<double>>
    value_and_gradient(const std::vector<double>& x) const {
        ensure_valid();
        return impl_->value_and_gradient(x);
    }

    // --- メイン式木からの呼び出し: operator() ---

    Expression operator()(const std::vector<Expression>& args) const {
        ensure_valid();
        if (args.size() != impl_->input_dim()) {
            throw std::invalid_argument(
                "CustomFunction::operator(): argument count mismatch");
        }
        return make_custom_call(impl_, args);
    }

    Expression operator()(std::initializer_list<Expression> args) const {
        return (*this)(std::vector<Expression>(args));
    }

    // シンタックスシュガー（必要に応じて 1〜7 引数まで定義）
    Expression operator()(const Expression& a) const {
        return (*this)({a});
    }

    Expression operator()(const Expression& a,
                          const Expression& b) const {
        return (*this)({a, b});
    }

    Expression operator()(const Expression& a,
                          const Expression& b,
                          const Expression& c) const {
        return (*this)({a, b, c});
    }

    // ... 4〜7引数版も必要に応じて定義 ...

    CustomFunctionHandle handle() const noexcept { return impl_; }

private:
    CustomFunctionHandle impl_;

    void ensure_valid() const {
        if (!impl_) {
            throw std::runtime_error(
                "CustomFunction: invalid (null) handle");
        }
    }
};
```

---

## 5. ExpressionImpl との統合

### 5.1 Op 列挙への追加

```cpp
enum class Op {
    CONST,
    PARAM,
    PLUS,
    ...
    PHI2,
    CUSTOM_FUNCTION  // 新規追加
};
```

### 5.2 ExpressionImpl へのフィールド追加

```cpp
class ExpressionImpl {
    ...
    Op op_;

    // 既存の left/right/third はそのまま
    Expression left_;
    Expression right_;
    Expression third_;

    // カスタム関数用フィールド
    CustomFunctionHandle custom_func_;      // op_ == CUSTOM_FUNCTION の時のみ有効
    std::vector<Expression> custom_args_;   // メイン式木側の引数 Expression 群

    std::string debug_name_;                // print_all 用（任意）
    ...
};
```

### 5.3 CUSTOM_FUNCTION ノード生成ファクトリ

```cpp
Expression make_custom_call(const CustomFunctionHandle& func,
                            const std::vector<Expression>& args);
```

* この関数は新しい `ExpressionImpl` ノードを生成し、

  * `op_ = Op::CUSTOM_FUNCTION`
  * `custom_func_ = func`
  * `custom_args_ = args`
  * `debug_name_ = func->name()`
    などをセットする。
* ノードのアロケーション・グローバルテーブル登録は既存のパターンに従う。

### 5.4 ExpressionImpl::value() の CUSTOM_FUNCTION 分岐

擬似コード：

```cpp
double ExpressionImpl::value() {
    if (value_cached_) return value_cache_;

    switch (op_) {
    ...
    case Op::CUSTOM_FUNCTION: {
        std::vector<double> args_values;
        args_values.reserve(custom_args_.size());
        for (const Expression& e : custom_args_) {
            args_values.push_back(e->value());
        }

        double v = custom_func_->value(args_values);

        value_cache_ = v;
        value_cached_ = true;
        return v;
    }
    ...
    }
}
```

### 5.5 勾配伝播（propagate_gradient）の CUSTOM_FUNCTION 分岐

擬似コード：

```cpp
void ExpressionImpl::propagate_gradient(double upstream) {
    switch (op_) {
    ...
    case Op::CUSTOM_FUNCTION: {
        std::vector<double> args_values;
        args_values.reserve(custom_args_.size());
        for (const Expression& e : custom_args_) {
            args_values.push_back(e->value());
        }

        auto [val, grad_vec] =
            custom_func_->eval_with_gradient(args_values);

        const size_t n = custom_args_.size();
        if (grad_vec.size() != n) {
            throw std::runtime_error(
                "CustomFunction::eval_with_gradient: gradient size mismatch");
        }

        for (size_t i = 0; i < n; ++i) {
            double contrib = upstream * grad_vec[i];
            custom_args_[i]->add_gradient(contrib); // 既存の加算メソッドに合わせる
        }
        break;
    }
    ...
    }
}
```

---

## 6. print_all() / デバッグ出力

`ExpressionImpl::print()`（または同等）に `Op::CUSTOM_FUNCTION` 用の表示を追加する。

例：

```cpp
void ExpressionImpl::print(std::ostream& os) const {
    switch (op_) {
    ...
    case Op::CUSTOM_FUNCTION:
        os << "CUSTOM_FUNCTION(" << custom_func_->name() << ")";
        break;
    ...
    }
}
```

`print_all()` などノード列挙関数でこの `print()` を呼べば、
カスタム関数ノードが「名前付き」で確認できる。

---

## 7. 例外ポリシー

* **assert は使わない。**
* 呼び出し側の誤り（引数サイズ不一致、null ハンドルなど）：

  * `std::invalid_argument` を throw。
* 内部不整合（起きないはずの状態）：

  * `std::runtime_error` を throw。

具体例：

* `CustomFunctionImpl::value/gradient/value_and_gradient`
  → `x.size() != input_dim_` で `std::invalid_argument`
* `CustomFunction::operator()`
  → `!impl_` で `std::runtime_error`
  → `args.size() != impl_->input_dim()` で `std::invalid_argument`
* `ExpressionImpl::propagate_gradient`
  → `grad_vec.size() != custom_args_.size()` で `std::runtime_error`

---

## 8. スレッドセーフティに関する注意

* 現時点の実装は **1つの `CustomFunctionImpl` を複数スレッドから同時使用することを想定しない**。
* 理由：

  * `value_and_gradient()` 内で

    * `local_vars_` の値
    * `nodes_` 内各ノードの `value_cache_` / `gradient_`
      を更新するため。
* 将来対応案：

  * 評価用コンテキストを複製する（式木クローン）。
  * あるいは内部を `std::mutex` で保護。
* そのため、`ExpressionImpl` は `CustomFunctionImpl` のインタフェースにだけ依存し、内部実装詳細に依存しない。

---

## 9. テスト項目

AI 実装後、少なくとも以下のテストを行うこと。

1. **1変数 f(x) = x²**

   * `x = 3` で

     * `f.value({3}) == 9`
     * `f.gradient({3}) == {6}`

2. **2変数 f(x,y) = x*y + x² とメイン式木**

   * 定義：

     ```cpp
     CustomFunction f = CustomFunction::create(
         2,
         [](const std::vector<Variable>& v) {
             const auto& x = v[0];
             const auto& y = v[1];
             return x * y + x * x;
         },
         "f_xy"
     );
     Variable X, Y;
     Expression F = f(X, Y) + X;
     ```
   * `X=2, Y=3` で：

     * `F.value() == 12`
     * `dF/dX == 8`
     * `dF/dY == 2`

3. **5変数関数（SSTA で使いそうな形）**

   * 例えば
     `f(x0,...,x4) = x0*x1 + exp(x2) + phi_expr(x3)*x4`
   * ランダム入力に対し、有限差分との誤差が十分小さいこと。

4. **同一カスタム関数の複数回呼び出し**

   * `F = f(X,Y) + f(Y,Z) + f(X,Z)` のような式で `backward()` し、
     それぞれの変数勾配が 3つの呼び出しの寄与を合計していること。

5. **メイン式木との独立性**

   * メイン式木で `backward()` を行った後に `f.gradient()` を呼んでも、
     メイン式木側の勾配が壊れていないこと。
   * 逆も同様。

6. **print_all() 出力**

   * CUSTOM_FUNCTION ノードが `CUSTOM_FUNCTION(f_xy)` のように表示されること。
