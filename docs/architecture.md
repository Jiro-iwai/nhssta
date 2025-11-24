# nhssta アーキテクチャ概要

## 概要

`nhssta`は、Statistical Static Timing Analysis (SSTA) を実行するC++17ライブラリおよびCLIツールです。このドキュメントでは、システムの全体像、コアドメインモデル、例外設計ポリシーについて説明します。

## コアドメインモデル

### 主要コンポーネント

1. **RandomVariable**: 確率変数を表現するクラス階層
   - `_RandomVariable_`: 基底クラス（平均、分散、標準偏差などを保持）
   - `Normal`: 正規分布を表現
   - `OpADD`, `OpSUB`, `OpMAX`, `OpMAX0`: 確率変数間の演算を表現
   - Handleパターンを使用して所有権を管理（`std::shared_ptr`ベース）

2. **Gate**: ゲート（論理素子）を表現
   - `_Gate_`: ゲートの遅延情報を保持（入力ピン→出力ピンの遅延マップ）
   - `Instance`: ゲートのインスタンス（実際の回路内での使用）
   - Handleパターンを使用

3. **Ssta**: SSTA解析のメインクラス
   - `read_dlib()`: ゲートライブラリ（`.dlib`）を読み込み
   - `read_bench()`: ベンチマーク回路（`.bench`）を読み込み
   - `connect_instances()`: インスタンスを接続してタイミング解析を実行
   - `report()`: 結果を出力

4. **Parser**: ファイルパーサー
   - `.dlib`および`.bench`ファイルを解析
   - `Tokenizer`を使用してトークン化
   - 構文エラーを`Nh::ParseException`として報告

5. **Covariance**: 共分散行列の管理
   - `CovarianceMatrix`: 確率変数間の共分散を保持
   - 相関解析に使用

### 関係図

```
┌─────────────┐
│   main.cpp  │  (CLI layer)
└──────┬──────┘
       │
       ▼
┌─────────────┐
│     Ssta    │◄────┐
└──────┬──────┘     │
       │            │
       ├────────────┼──────────────┐
       │            │              │
       ▼            ▼              ▼
┌──────────┐  ┌──────────┐  ┌──────────────┐
│  Parser  │  │   Gate   │  │RandomVariable │
└────┬─────┘  └────┬─────┘  └──────┬───────┘
     │              │               │
     │              ▼               │
     │         ┌──────────┐         │
     │         │ Instance │         │
     │         └──────────┘         │
     │                               │
     ▼                               ▼
┌──────────┐                  ┌──────────────┐
│ Tokenizer│                  │   Covariance │
└──────────┘                  └──────────────┘
```

## データフロー

詳細は `docs/data_flow.md` を参照してください。

### 簡易フロー

1. **入力**: `.dlib`ファイル（ゲートライブラリ）と`.bench`ファイル（回路記述）
2. **解析**: `Parser`がファイルを解析し、`Ssta`が内部データ構造を構築
3. **処理**: `Ssta::connect_instances()`がゲートインスタンスを接続し、タイミング解析を実行
4. **出力**: LAT（Latest Arrival Time）データおよび相関行列

## 例外設計ポリシー

### 例外階層

```
Nh::Exception (基底クラス)
├── Nh::FileException      (ファイル関連エラー)
├── Nh::ParseException     (パースエラー)
├── Nh::ConfigurationException (設定エラー)
└── Nh::RuntimeException   (実行時エラー)
```

### 例外の使用方針

1. **ファイルエラー**: `Parser::checkFile()`が`Nh::FileException`を投げる
2. **パースエラー**: `Parser`が構文エラーを`Nh::ParseException`として報告
3. **設定エラー**: `Ssta::check()`が設定不備を`Nh::ConfigurationException`として報告
4. **実行時エラー**: 
   - `Gate::delay()`が未定義ピンアクセス時に`Nh::RuntimeException`を投げる
   - `Ssta::connect_instances()`が浮遊ノード検出時に`Nh::RuntimeException`を投げる
   - `RandomVariable`の演算で不正な分散値が検出された場合に`Nh::RuntimeException`を投げる

### 例外処理の原則

- **ライブラリ層（`Ssta`など）**: 例外を投げるのみ。I/Oや`exit()`は呼ばない
- **CLI層（`main.cpp`）**: 例外をキャッチし、適切なエラーメッセージと終了コードを返す
- **エラーメッセージ**: ユーザーに分かりやすい形式で、問題の原因と場所を明示

## ディレクトリ構造

```
nhssta/
├── include/nhssta/          # 公開APIヘッダー
│   ├── ssta.hpp
│   ├── exception.hpp
│   └── ssta_results.hpp
├── src/                     # 実装ファイル
│   ├── ssta.cpp
│   ├── gate.cpp
│   ├── parser.cpp
│   ├── random_variable.cpp
│   └── ...
├── test/                    # ユニットテスト
└── docs/                    # ドキュメント
    ├── architecture.md      # このファイル
    ├── domain_model.md      # ドメインモデル詳細
    └── data_flow.md         # データフロー詳細
```

## 関連ファイル

- `include/nhssta/ssta.hpp`: Sstaクラスの定義
- `src/ssta.cpp`: Sstaクラスの実装
- `src/gate.hpp`, `src/gate.cpp`: GateとInstanceの実装
- `src/random_variable.hpp`, `src/random_variable.cpp`: RandomVariableの実装
- `src/parser.hpp`, `src/parser.cpp`: Parserの実装
- `src/main.cpp`: CLIエントリーポイント

