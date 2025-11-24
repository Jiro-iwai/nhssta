# nhssta ドメインモデル

## 概要

このドキュメントでは、`nhssta`のコアドメインモデルとその関係について詳しく説明します。

## コアエンティティ

### RandomVariable

確率変数を表現するクラス階層です。SSTAでは、各信号の遅延を確率変数として扱います。

#### クラス階層

```
_RandomVariable_ (基底クラス)
├── _Normal_          (正規分布)
├── OpADD             (加算演算)
├── OpSUB             (減算演算)
├── OpMAX             (最大値演算)
└── OpMAX0            (最大値演算、ゼロ分散チェック付き)
```

#### 主要メソッド

- `mean()`: 平均値を取得
- `variance()`: 分散を取得
- `standard_deviation()`: 標準偏差を取得
- `coefficient_of_variation()`: 変動係数を取得
- `relative_error()`: 相対誤差を取得（変動係数のエイリアス）

#### Handleパターン

`RandomVariableHandle`（`RandomVariable`としてエイリアス）は、`std::shared_ptr<_RandomVariable_>`の薄いラッパーです。

- **コピー**: 軽量（`shared_ptr`のコピーのみ）
- **所有権**: `shared_ptr`による共有所有
- **値渡し**: 所有権を共有/転送
- **const参照渡し**: 非所有参照

#### 関連ファイル

- `src/random_variable.hpp`, `src/random_variable.cpp`
- `src/normal.hpp`, `src/normal.cpp`
- `src/add.hpp`, `src/add.cpp`
- `src/max.hpp`, `src/max.cpp`
- `src/sub.hpp`, `src/sub.cpp`

### Gate

ゲート（論理素子）を表現します。各ゲートは、入力ピンから出力ピンへの遅延情報を持ちます。

#### クラス構造

```
_Gate_
├── type_name_        (ゲートタイプ名、例: "and2", "or2")
├── delays_           (入力ピン→出力ピンの遅延マップ)
└── num_instances_    (インスタンス番号カウンタ)

Instance (_Instance_)
├── gate_             (対応するGateへの参照)
├── name_             (インスタンス名)
├── inputs_            (入力信号のマップ)
└── outputs_           (出力信号のマップ)
```

#### 主要メソッド

**Gate**:
- `set_delay(in, out, delay)`: 遅延を設定
- `delay(in, out)`: 遅延を取得
- `create_instance()`: インスタンスを作成

**Instance**:
- `set_input(pin_name, signal)`: 入力信号を設定
- `output(pin_name)`: 出力信号を取得（確率変数として）

#### Handleパターン

`Gate`と`Instance`もHandleパターンを使用します。

#### 関連ファイル

- `src/gate.hpp`, `src/gate.cpp`

### Ssta

SSTA解析のメインクラスです。

#### 主要データ構造

```cpp
class Ssta {
    Gates gates_;              // ゲートライブラリ（ゲートタイプ名 → Gate）
    Signals signals_;          // 信号マップ（信号名 → RandomVariable）
    Net net_;                  // ネットリスト（NetLineのリスト）
    Pins inputs_;              // 入力ピン集合
    Pins outputs_;             // 出力ピン集合
    std::string dlib_;         // .dlibファイルパス
    std::string bench_;        // .benchファイルパス
    bool is_lat_;              // LAT出力フラグ
    bool is_correlation_;      // 相関行列出力フラグ
};
```

#### 主要メソッド

- `read_dlib()`: `.dlib`ファイルを読み込み、`gates_`を構築
- `read_bench()`: `.bench`ファイルを読み込み、`net_`と`signals_`を構築
- `connect_instances()`: `net_`を処理し、インスタンスを接続してタイミング解析を実行
- `report()`: 結果を出力（LAT、相関行列）

#### 処理フロー

1. `read_dlib()`: 各ゲートタイプの遅延情報を`gates_`に登録
2. `read_bench()`: 
   - `INPUT`文: 入力信号を`signals_`に追加
   - `OUTPUT`文: 出力信号を`outputs_`に追加
   - `NET`文: ネット接続を`net_`に追加
3. `connect_instances()`: 
   - `net_`から順次処理可能なインスタンスを処理
   - 各インスタンスの出力を`signals_`に追加
   - すべてのインスタンスが処理されるまで繰り返し

#### 関連ファイル

- `include/nhssta/ssta.hpp`
- `src/ssta.cpp`

### Parser

ファイルパーサーです。`.dlib`および`.bench`ファイルを解析します。

#### クラス構造

```cpp
class Parser {
    std::string file_;              // ファイルパス
    std::ifstream infile_;          // 入力ストリーム
    Tokenizer* tokenizer_;          // トークナイザー
    int line_number_;               // 現在の行番号
    char begin_comment_;            // コメント開始文字（例: '#'）
    std::string keep_separator_;    // 保持する区切り文字（例: "(),="）
    std::string drop_separator_;    // 破棄する区切り文字（例: " \t\r"）
};
```

#### 主要メソッド

- `getLine()`: 次の行を読み込み、トークン化
- `getToken<T>(u)`: 次のトークンを型`T`に変換して取得
- `checkFile()`: ファイルが開けるかチェック（`Nh::FileException`を投げる可能性）
- `checkTermination()`: 行末をチェック（`Nh::ParseException`を投げる可能性）
- `unexpectedToken()`: 予期しないトークンエラーを報告（`Nh::ParseException`を投げる）

#### 関連ファイル

- `src/parser.hpp`, `src/parser.cpp`
- `src/tokenizer.hpp`, `src/tokenizer.cpp`

### Covariance

共分散行列を管理します。確率変数間の相関を計算するために使用されます。

#### クラス構造

```cpp
class _CovarianceMatrix_ {
    Matrix cmat_;  // std::map<RowCol, double>
                   // RowCol = std::pair<RandomVariable, RandomVariable>
};
```

#### 主要メソッド

- `lookup(a, b, cov)`: 共分散を検索
- `set(a, b, cov)`: 共分散を設定
- `covariance(a, b)`: 確率変数間の共分散を計算

#### 関連ファイル

- `src/covariance.hpp`, `src/covariance.cpp`

## エンティティ間の関係

### Gate ↔ Instance

- 1つの`Gate`から複数の`Instance`を作成可能
- `Instance`は`Gate`への参照を持つ
- `Instance::create_instance()`で`Gate`から`Instance`を作成

### Instance ↔ RandomVariable

- `Instance`の入力と出力は`RandomVariable`として表現される
- `Instance::set_input()`で入力信号（`RandomVariable`）を設定
- `Instance::output()`で出力信号（`RandomVariable`）を取得

### Ssta ↔ Gate/Instance

- `Ssta`は`Gates`マップでゲートライブラリを管理
- `Ssta::connect_instances()`で`Instance`を作成し、接続

### Ssta ↔ Parser

- `Ssta::read_dlib()`と`Ssta::read_bench()`が`Parser`を使用
- `Parser`がファイルを解析し、`Ssta`が内部データ構造を構築

### RandomVariable ↔ Covariance

- `CovarianceMatrix`が`RandomVariable`ペア間の共分散を管理
- `Ssta::report()`で相関解析に使用

## 関連ファイル

- `src/random_variable.hpp`, `src/random_variable.cpp`: RandomVariableの実装
- `src/gate.hpp`, `src/gate.cpp`: GateとInstanceの実装
- `include/nhssta/ssta.hpp`, `src/ssta.cpp`: Sstaの実装
- `src/parser.hpp`, `src/parser.cpp`: Parserの実装
- `src/covariance.hpp`, `src/covariance.cpp`: Covarianceの実装

