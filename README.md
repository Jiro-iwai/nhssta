# 統計的PERT法 nhssta 0.3.3

本ドキュメントには統計的PERT法 nhssta 0.3.3 (以下、nhssta ) の概要、使用方法、及び使用上の注意点が記載されています。

## 1. 概要

### 1.1 機能

nhsstaは統計的PERT法（Statistical Static Timing Analysis, SSTA）を実装したプログラムです。デジタル回路のタイミング解析において、ゲート遅延を確率変数として扱い、統計的手法により各信号の到着時刻を計算します。

nhsstaは以下の機能を持ちます。

#### ゲートライブラリと回路定義の読み込み
- `.dlib`ファイル（ゲートライブラリ）の読み込み
  - 各ゲートタイプ（例: `and2`, `or2`）の入力ピンから出力ピンへの遅延分布を定義
  - 遅延分布として正規分布（`gauss(mean, variance)`）または定数値（`const(value)`）を指定可能
- `.bench`ファイル（回路定義）の読み込み
  - 入力信号（`INPUT`文）の定義
  - 出力信号（`OUTPUT`文）の定義
  - ゲートインスタンスと信号接続（`NET`文）の定義

#### 統計的タイミング解析
- **各ノードでの最終到着時刻（LAT: Latest Arrival Time）の統計的計算**
  - 平均値（μ）の計算: 信号が各ノードに到達する時刻の期待値
  - 標準偏差（σ）の計算: 到着時刻のばらつきを表す統計量
  - 確率変数としての遅延の伝播: ゲート遅延を確率変数として扱い、加算（ADD）、減算（SUB）、最大値（MAX）などの演算により信号遅延を計算
- **ノード間の相関解析**
  - 相関係数の計算: 異なるノード間でのLATの相関関係を共分散行列から計算
  - 相関行列の出力: すべてのノードペア間の相関係数をマトリックス形式で出力

#### クリティカルパス解析
- **クリティカルパス（最遅到達パス）の抽出**
  - 出力ノード（およびDFF出力）から入力ノード（または別のDFF出力）までのパスを逆方向にたどり、到達時刻が最大となるパスを検出
  - トップN本のパスを遅延の大きい順に報告（`-p [N]`オプション、デフォルト N=5）
  - 各パスについて、経路上の各ノードのLAT（μ, σ）をLAT出力と同じ表形式で出力

#### 感度解析
- **目的関数の計算**: `F = log(Σ exp(LAT + σ))` を計算し、各ゲートの遅延パラメータに対する感度を計算
- **トップNエンドポイントの選出**: LAT + σ が大きい順に上位N本のエンドポイントを選出（`-n [N]`オプション、デフォルト N=5）
- **ゲート感度の出力**: 各ゲートの平均遅延（μ）と標準偏差（σ）に対する目的関数の勾配（∂F/∂μ, ∂F/∂σ）を出力

#### 出力形式
- LATデータの出力: 各ノードの平均値と標準偏差を表形式で出力（`-l`オプション）
- 相関行列の出力: ノード間の相関係数をマトリックス形式で出力（`-c`オプション）
- クリティカルパスの出力: 各出力ノードに対する上位N本のクリティカルパスを、ノード名とLAT（μ, σ）の表形式で出力（`-p [N]`オプション）
- 感度解析の出力: 目的関数の値、トップNエンドポイント、各ゲートの感度を表形式で出力（`-s`オプション）

### 1.2 ディレクトリ構成

```
 nhssta/
        README.md            本ドキュメント
        CONTRIBUTING.md      コントリビューションガイドライン
        Makefile             ルートメイクファイル
        .clang-format        コードフォーマット設定
        .clang-tidy          静的解析設定
       /src/*.cpp            ソースファイル
       /src/*.hpp            内部ヘッダーファイル
       /src/Makefile         ソースビルド設定
       /include/nhssta/      公開APIヘッダーファイル
       /test/*.cpp           ユニットテスト（Google Test）
       /test/README.md       テストスイートの説明
       /example/*.bench      サンプル.benchデータ
               /*.dlib       サンプル.dlibデータ
               /nhssta_test  統合テストスクリプト
               /result*      テストスクリプト実行結果
       /scripts/             開発・解析用スクリプト
               /dev-setup.sh       開発環境セットアップ
               /run-all-checks.sh  全チェック実行
               /generate_coverage.sh カバレッジ生成
               /visualize_bench.py 回路可視化ツール
       /docs/                 開発ドキュメント
       /build/                ビルド成果物（`make`実行後に生成、`.gitignore`で無視）
               /src/          オブジェクトファイル（.o）と依存関係ファイル（.d）
               /test/         テストオブジェクトファイル
               /bin/          実行ファイル（nhssta, nhssta_test）
               /coverage/     コードカバレッジレポート（`make coverage`実行後に生成）
```

**注意**: ビルド成果物は`build/`ディレクトリに配置されます。`make clean`で削除できます。			   

## 2. 使用方法

### 2.1 コンパイル、テスト

- コンパイルには **C++17対応のコンパイラ**（g++ 7.0以上、clang++ 5.0以上）が必要です。

- テスト実行には **Google Test** が必要です。インストール方法：
  - macOS: `brew install googletest`
  - Ubuntu: `sudo apt-get install libgtest-dev`

- プロジェクトルートからビルドを実行します：

```bash
$ make
```

コンパイルが終了すると、ディレクトリ `build/bin/` 以下に `nhssta` という実行ファイルが作成されます。

**注意**: 初回ビルド時は、Google Testがインストールされていない場合、テストのビルドは失敗しますが、メインプログラム（`build/bin/nhssta`）のビルドは成功します。

- すべてのテスト（ユニットテスト + 統合テスト）を実行する場合：

```bash
$ make test
```

すべてのテストがパスすると、以下のように表示されます：

```
[==========] Running 791 tests from 72 test suites.
[  PASSED  ] 791 tests.
==========================================
Running integration tests...
==========================================
Total tests: 10
Passed: 10
Failed: 0
✓ All tests passed!
```

- 統合テストのみを実行する場合（後方互換性のため）：

```bash
$ make check
```

- コードカバレッジレポートを生成する場合：

```bash
$ make coverage
```

カバレッジレポートは `build/coverage/html/index.html` に生成されます。

- コードスタイルチェック（clang-tidy）を実行する場合：

```bash
$ make tidy
```

### 2.2 実行方法

`build/bin/nhssta` をパスの通った適当な場所にコピーするか、直接 `build/bin/nhssta` を実行します。

```bash
$ nhssta -d [.dlibファイル] -b [.benchファイル] [-l] [-c] [-p [N]] [-s] [-n [N]]
```

**オプション**:
- `-d` ( `--dlib` ): `.dlib`ファイル（遅延ライブラリ）のパスを指定（必須）
- `-b` ( `--bench` ): `.bench`ファイル（回路定義）のパスを指定（必須）
- `-l` ( `--lat` ): 各ノードでのLAT（Latest Arrival Time）の平均、標準偏差を標準出力に出力
- `-c` ( `--correlation` ): ノード間でのLATの相関係数を標準出力に出力
- `-p [N]` ( `--path [N]` ): クリティカルパス（最遅到達パス）を上位N本まで出力  
  - `N` を省略した場合はデフォルトで 5 本を出力  
  - `-l`/`-c`/`-s` と組み合わせて指定可能（LAT・相関・クリティカルパス・感度解析を同時に出力）
- `-s` ( `--sensitivity` ): 感度解析を出力
  - 目的関数 `F = log(Σ exp(LAT + σ))` の値と、各ゲートの遅延パラメータに対する感度（∂F/∂μ, ∂F/∂σ）を出力
- `-n [N]` ( `--top [N]` ): 感度解析で対象とするトップNエンドポイントを指定（デフォルト N=5）
  - `N` を省略した場合はデフォルトで 5 本を対象とする
  - `-s` オプションと組み合わせて使用

**注意**: `-l`、`-c`、`-p`、`-s` のいずれも指定しない場合、何も出力されませんが、エラーにはなりません。複数を同時に指定することも可能です。

### 2.3 Exit Codes

nhsstaは以下のexit codeを返します：

| Exit Code | 意味 | 発生条件 |
|-----------|------|----------|
| 0 | 成功 | 正常に処理が完了した場合 |
| 1 | エラー（nhssta例外） | 以下のいずれかの場合：<br>- 使用方法エラー（無効なオプション、必須オプション未指定）<br>- ファイル未発見（`-d`または`-b`で指定したファイルが存在しない）<br>- パースエラー（`.dlib`または`.bench`ファイルのフォーマット不正）<br>- ランタイムエラー（未知のゲート、数値計算エラーなど） |
| 2 | エラー（標準ライブラリ例外） | 標準ライブラリの例外が発生した場合 |
| 3 | エラー（未知のエラー） | 予期しない例外が発生した場合 |

スクリプト等から`nhssta`を呼び出す場合、exit codeを確認することで処理の成功/失敗を判定できます。

### 2.4 エラーメッセージ

エラーが発生した場合、`nhssta`は標準エラー出力にエラーメッセージを出力します。エラーメッセージの形式は以下の通りです：

- **使用方法エラー**: `error: Invalid command-line arguments` の後に使用方法が表示されます
- **ファイル未発見**: `error: File error: [ファイル名]: [詳細メッセージ]`
- **パースエラー**: `error: Parse error: [ファイル名]:[行番号]: [詳細メッセージ]`
- **ランタイムエラー**: `error: Runtime error: [詳細メッセージ]`
- **標準ライブラリ例外**: `error: [例外メッセージ]`
- **未知のエラー**: `error: unknown error`

### 2.5 実行例

以下に `example` ディレクトリ内のファイルを使用した実行例を示します。

```bash
# exampleディレクトリから実行する場合
$ cd example
$ ../build/bin/nhssta -l -c -p -d ex4_gauss.dlib -b ex4.bench
nhssta 0.3.3

#
# LAT
#
#node		     mu	     std
#---------------------------------
...
#---------------------------------

#
# correlation matrix
#
...

#
# critical paths
#
...
OK
```

感度解析を実行する場合：

```bash
$ ../build/bin/nhssta -s -n 5 -d ex4_gauss.dlib -b ex4.bench
nhssta 0.3.3

#
# Sensitivity Analysis
#
# Objective: log(Σ exp(LAT + σ)) = 94.688
#
# Top 1 Endpoints (by LAT + σ):
#
#node                    LAT    sigma     score
#-----------------------------------------
Y                     89.770    4.923    94.693
#-----------------------------------------
#
# Gate Sensitivities (∂F/∂μ, ∂F/∂σ):
#
#instance   output    input   type          dF/dmu   dF/dsigma
#-------------------------------------------------------------
inv:0       N2        B       inv         1.000766    0.399863
and:0       Y         N3      and         0.708390    0.597047
nand:0      N1        N2      nand        0.706665    0.447726
inv:1       N3        N1      inv         0.708390    0.298523
and:0       Y         N4      and         0.291610    0.346626
or:0        N4        N2      or          0.294101    0.293807
nand:0      N1        A       nand        0.001725    0.005387
#-------------------------------------------------------------
OK
```

## 3. 使用上の注意

- **デフォルトはノード遅延（LAT）と相関を主対象**: パス遅延は `-p` オプションを指定した場合に、クリティカルパスとして別途報告します

- **入力ノードとDFFのck端子**: 到着時刻は平均 0、標準偏差 0.001 として扱われます

- **遅延分布の指定**: `.dlib` ファイルでは `gauss` と `const` のみが指定できます
  - `gauss(mean, variance)`: 正規分布を指定
  - `const(value)`: 定数値を指定（標準偏差 0.001 の正規分布として扱われます）

- **感度解析（`-s`オプション）**: 感度解析機能は現在実験段階です。大規模回路では Expression ツリーの構築に時間がかかる可能性があり、効率が良くない場合があります。小規模から中規模の回路での使用を推奨します。

- **プロトタイプ**: nhssta はプロトタイプであり、実行結果の妥当性については十分に検証されていない可能性があります。重要な用途では結果を検証してください。

## 4. 開発者向け情報

### 4.1 コーディングスタイル

nhsstaプロジェクトでは、一貫したコーディングスタイルを維持するため、以下のツールを使用しています：

- **clang-format**: コードフォーマットの自動整形
- **clang-tidy**: 静的解析とコード品質チェック

詳細は `CONTRIBUTING.md` を参照してください。

### 4.2 ローカルでの開発

#### 開発環境のセットアップ

新しい開発者は、まず開発環境のセットアップスクリプトを実行してください：

```bash
# Makefile経由で実行（推奨）
make dev-setup

# または直接スクリプトを実行
./scripts/dev-setup.sh
```

このスクリプトは、必要なツール（コンパイラ、Google Test、clang-format、clang-tidyなど）がインストールされているかを確認し、ビルドが成功するかテストします。

#### すべてのチェックを実行

開発中は、以下のコマンドでビルド、テスト、lint、カバレッジのすべてのチェックを一度に実行できます：

```bash
# Makefile経由で実行（推奨）
make dev-check

# または直接スクリプトを実行
./scripts/run-all-checks.sh
```

**注意**: `make test`はユニットテストと統合テストの両方を実行します。`make check`は統合テストのみを実行します（後方互換性のため）。開発チェックには`make dev-check`を使用してください。

#### 個別のコマンド

##### テストの実行

```bash
make test
```

##### スタイルチェック

```bash
# clang-tidyを実行（警告を表示）
make tidy

# clang-formatでコードを整形（必要に応じて）
clang-format -i src/your_file.cpp
```

##### ビルド

```bash
cd src
make
```

##### コードカバレッジ

```bash
make coverage
```

カバレッジレポートは `build/coverage/html/index.html` に生成されます。

**注意**: テストをビルドする場合、Google Testのパスを環境変数またはmake引数で指定できます：

```bash
# 方法1: 環境変数を使用（推奨）
# macOS (Homebrew以外の場所にインストールした場合)
export GTEST_DIR=/usr/local/opt/googletest
cd src
make test

# Ubuntu/Debianの場合
export GTEST_DIR=/usr
cd src
make test

# 方法2: make引数で直接指定
cd src
make GTEST_DIR=/usr/local/opt/googletest test

# 環境変数とmake引数の両方が指定された場合、make引数が優先されます
```

### 4.3 回路可視化ツール

`.bench` 形式の回路ファイルを可視化するためのPythonスクリプト（`scripts/visualize_bench.py`）が用意されています。

```bash
# 依存パッケージのインストール
pip install networkx matplotlib

# 回路全体図の生成
python scripts/visualize_bench.py example/s27.bench output

# クリティカルパスの強調表示
./build/bin/nhssta -p 3 -d example/gaussdelay.dlib -b example/s298.bench > critical.txt
python scripts/visualize_bench.py example/s298.bench output --highlight-path critical.txt
```

詳細は `docs/visualization.md` を参照してください。

### 4.4 コントリビューション

- 新しい機能やバグ修正は、ブランチを作成してプルリクエストとして提出してください
- すべてのテストがパスし、CIのチェックが通ることを確認してください
- 詳細は `CONTRIBUTING.md` を参照してください

### 4.5 メモリ管理ポリシー

- `RandomVariable` / `Expression` / `Gate` / `Instance` など、実行時グラフを表す主要コンポーネントは **`std::shared_ptr` ベースの薄いハンドル型**を使用しています
- `CovarianceMatrix` は確率変数間の共分散値を保持するデータ構造で、`std::shared_ptr` を使用して複数の場所から安全に共有・参照できます
- 所有権は標準スマートポインタで明示することを原則とし、例外はドキュメント化してください

## 5. 参考文献

nhssta は 以下の文献の手法に基づいて実装されています。

Jiayong Le, Xin Li, Lawrence T. Pileggi: STAC: statistical timing
analysis with correlation. DAC 2004: 343-348
