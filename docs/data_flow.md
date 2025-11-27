# nhssta データフロー

## 概要

このドキュメントでは、`nhssta`のデータ処理フローについて詳しく説明します。

## 全体フロー

```
┌─────────────┐
│ .dlib ファイル│
└──────┬──────┘
       │
       ▼
┌─────────────┐
│   Parser    │──┐
└──────┬──────┘  │
       │         │
       ▼         │
┌─────────────┐  │
│     Ssta    │  │
│  read_dlib()│  │
└──────┬──────┘  │
       │         │
       │         │
       │    ┌─────────────┐
       │    │ .bench ファイル│
       │    └──────┬──────┘
       │           │
       │           ▼
       │    ┌─────────────┐
       │    │   Parser    │
       │    └──────┬──────┘
       │           │
       │           ▼
       │    ┌─────────────┐
       │    │     Ssta    │
       │    │ read_bench()│
       │    └──────┬──────┘
       │           │
       │           ▼
       │    ┌─────────────┐
       │    │     Ssta    │
       │    │connect_inst│
       │    │  ances()    │
       │    └──────┬──────┘
       │           │
       │           ▼
       │    ┌─────────────┐
       │    │     Ssta    │
       │    │   report()  │
       │    └──────┬──────┘
       │           │
       └───────────┘
                   │
                   ▼
            ┌─────────────┐
            │   結果出力   │
            └─────────────┘
```

## 詳細フロー

### Phase 1: ゲートライブラリの読み込み（`read_dlib()`）

**入力**: `.dlib`ファイル

**処理**:
1. `Parser`を作成（コメント文字: `'#'`, 保持区切り文字: `"(),"`, 破棄区切り文字: `" \t\r"`）
2. 各行を解析:
   ```
   <gate_name> <input_pin> <output_pin> <dist_type>(<mean>[, <sigma>])
   ```
   例: `and2 0 y gauss(10.0, 1.0)`
3. `Gate`オブジェクトを作成または取得
4. `Gate::set_delay()`で遅延情報を設定
5. `gates_`マップに登録

**出力**: `Ssta::gates_`マップ（ゲートタイプ名 → `Gate`）

**関連ファイル**:
- `src/ssta.cpp`: `Ssta::read_dlib()`, `Ssta::read_dlib_line()`
- `src/parser.hpp`, `src/parser.cpp`: `Parser`クラス

### Phase 2: ベンチマーク回路の読み込み（`read_bench()`）

**入力**: `.bench`ファイル

**処理**:
1. `Parser`を作成（コメント文字: `'#'`, 保持区切り文字: `"(),="`, 破棄区切り文字: `" \t\r"`）
2. 各行を解析:

   **INPUT文**:
   ```
   INPUT(<signal_name>)
   ```
   - `signals_`に`Normal(0.0, minimum_variance)`を追加
   - `inputs_`に信号名を追加

   **OUTPUT文**:
   ```
   OUTPUT(<signal_name>)
   ```
   - `outputs_`に信号名を追加

   **NET文**:
   ```
   <output_signal> = <gate_name>(<input_signal1>, <input_signal2>, ...)
   ```
   - `NetLine`を作成
   - `net_`リストに追加

3. `connect_instances()`を呼び出し

**出力**: 
- `Ssta::signals_`マップ（信号名 → `RandomVariable`）
- `Ssta::net_`リスト（`NetLine`のリスト）
- `Ssta::inputs_`集合
- `Ssta::outputs_`集合

**関連ファイル**:
- `src/ssta.cpp`: `Ssta::read_bench()`, `Ssta::read_bench_input()`, `Ssta::read_bench_output()`, `Ssta::read_bench_net()`
- `src/parser.hpp`, `src/parser.cpp`: `Parser`クラス

### Phase 3: インスタンス接続とタイミング解析（`connect_instances()`）

**入力**: `Ssta::net_`リスト、`Ssta::gates_`マップ、`Ssta::signals_`マップ

**処理**:
1. `net_`が空になるまで繰り返し:
   a. `net_`の各`NetLine`について:
      - `is_line_ready()`で入力信号が準備できているかチェック
      - 準備できている場合:
        - `Gate::create_instance()`で`Instance`を作成
        - `set_instance_input()`で入力信号を設定
        - `Instance::output()`で出力信号を取得
        - `signals_`に出力信号を追加
        - `net_`から`NetLine`を削除
   b. 1回のループで`net_`のサイズが変化しなかった場合、浮遊ノードエラーを報告

2. すべてのインスタンスが処理されるまで繰り返し

**出力**: `Ssta::signals_`マップが更新され、すべての信号の遅延が計算される

**関連ファイル**:
- `src/ssta.cpp`: `Ssta::connect_instances()`, `Ssta::is_line_ready()`, `Ssta::set_instance_input()`
- `src/gate.cpp`: `Instance::set_input()`, `Instance::output()`

### Phase 4: 結果出力（`report()`）

**入力**: `Ssta::signals_`マップ、`Ssta::outputs_`集合

**処理**:
1. `is_lat_`フラグが設定されている場合:
   - `report_lat()`を呼び出し
   - 各出力信号のLAT（Latest Arrival Time）を出力

2. `is_correlation_`フラグが設定されている場合:
   - `report_correlation()`を呼び出し
   - 出力信号間の相関行列を出力

**出力**: LATデータおよび相関行列（標準出力または`SstaResults`オブジェクト）

**関連ファイル**:
- `src/ssta.cpp`: `Ssta::report()`, `Ssta::report_lat()`, `Ssta::report_correlation()`
- `include/nhssta/ssta_results.hpp`: `LatResults`, `CorrelationMatrix`

## データ構造の変化

### `read_dlib()`後

```
gates_ = {
    "and2" -> Gate { delays_ = {("0", "y") -> Normal(10.0, 1.0), ...} },
    "or2"  -> Gate { delays_ = {("0", "y") -> Normal(12.0, 1.5), ...} },
    ...
}
```

### `read_bench()`後（`connect_instances()`前）

```
signals_ = {
    "a" -> Normal(0.0, 1e-6),
    "b" -> Normal(0.0, 1e-6),
    ...
}
net_ = [
    NetLine { out_ = "y", gate_ = "and2", ins_ = ["a", "b"] },
    ...
]
inputs_ = {"a", "b", ...}
outputs_ = {"y", ...}
```

### `connect_instances()`後

```
signals_ = {
    "a" -> Normal(0.0, 1e-6),
    "b" -> Normal(0.0, 1e-6),
    "y" -> AddImpl(Normal(0.0, 1e-6), Normal(10.0, 1.0)),  // 計算済み
    ...
}
net_ = []  // 空
```

## エラーハンドリング

### ファイルエラー

- `Parser::checkFile()`が`Nh::FileException`を投げる
- `Ssta::read_dlib()`と`Ssta::read_bench()`がキャッチし、`Nh::Exception`に変換

### パースエラー

- `Parser::unexpectedToken()`が`Nh::ParseException`を投げる
- `Ssta::read_dlib()`と`Ssta::read_bench()`がキャッチし、`Nh::Exception`に変換

### 実行時エラー

- `Ssta::connect_instances()`が浮遊ノードを検出した場合、`Nh::RuntimeException`を投げる
- `Gate::delay()`が未定義ピンにアクセスした場合、`Nh::RuntimeException`を投げる
- `Instance::set_input()`が未定義ピンにアクセスした場合、`Nh::RuntimeException`を投げる

## 関連ファイル

- `src/ssta.cpp`: メインのデータフロー処理
- `src/parser.hpp`, `src/parser.cpp`: ファイル解析
- `src/gate.cpp`: インスタンス処理
- `src/random_variable.cpp`: 確率変数の演算
- `src/main.cpp`: CLIエントリーポイント

