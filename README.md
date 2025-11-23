# 統計的PERT法 nhssta 0.0.8

本ドキュメントには統計的PERT法 nhssta 0.0.8 (以下、nhssta ) の概要、使用方法、及び使用上の注意点が記載されています。

## 1. 概要

### 1.1 機能

nhsstaは統計的PERT法を実装したプログラムです。nhsstaは以下の機能を持ちます。
   
- 各ノードでの最終到着時刻(LAT)の平均、標準偏差の計算
- 各ノードでのLATの相関係数の計算

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
       /docs/                 開発ドキュメント
       /coverage/             コードカバレッジレポート（make coverage実行後）
```			   

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

コンパイルが終了すると、ディレクトリ `src/` 以下に `nhssta` という実行ファイルが作成されます。

- ユニットテストを実行する場合：

```bash
$ make test
```

すべてのテストがパスすると、以下のように表示されます：

```
[==========] Running 239 tests from 24 test suites.
[  PASSED  ] 239 tests.
```

- 統合テスト（exampleディレクトリのテストスクリプト）を実行する場合：

```bash
$ make check
```

- コードカバレッジレポートを生成する場合：

```bash
$ make coverage
```

カバレッジレポートは `coverage/html/index.html` に生成されます。

- コードスタイルチェック（clang-tidy）を実行する場合：

```bash
$ make tidy
```

### 2.2 実行方法

src/nhssta をパスの通った適当な場所にコピーし、コマンドラインから以下のように実行します。

```
$ nhssta -d [.dlibファイル] -b [.benchファイル] [-l] [-c]
```

- -l ( --lat ) を指定すると各ノードでのLATの平均、標準偏差を標準出力に
  出力します。

- -c ( --correlation ) ノード間でのLATの相関係数を標準出力に出力します。

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

以下に example 以下で -l, -c を指定した実行例を示します。

```
$ ../src/nhssta -l -c -d ex4_gauss.dlib -b ex4.bench 
nhssta 0.0.8 (Sat Jan 21 01:39:32 2017)

#
# LAT
#
#node		     mu	     std
#---------------------------------
A                   0.000    0.001
B                   0.000    0.001
C                   0.000    0.001
N1                 35.016    3.575
N2                 15.000    2.000
N3                 50.016    4.097
N4                 43.902    3.992
Y                  89.791    4.929
#---------------------------------

#
# correlation matrix
#
#	A	B	C	N1	N2	N3	N4	Y	
#--------------------------------------------------------------------
A	1.000	0.000	0.000	0.000	0.000	0.000	0.000	0.000	
B	0.000	1.000	0.000	0.000	0.000	0.000	0.000	0.000	
C	0.000	0.000	1.000	0.000	0.000	0.000	0.000	0.000	
N1	0.000	0.000	0.000	1.000	0.554	0.873	0.274	0.556	
N2	0.000	0.000	0.000	0.554	1.000	0.483	0.495	0.401	
N3	0.000	0.000	0.000	0.873	0.483	1.000	0.239	0.617	
N4	0.000	0.000	0.000	0.274	0.495	0.239	1.000	0.404	
Y	0.000	0.000	0.000	0.556	0.401	0.617	0.404	1.000	
#--------------------------------------------------------------------
OK
```

## 3. 使用上の注意

- ノード遅延のみを考慮しパスの遅延は考慮しません

- 入力ノードとDFFのck端子の到着時刻は平均 0、標準偏差 0.001 となります。

- .dlib で遅延分布の指定は gauss と const のみが指定できます。const の場合は標準偏差 0.001 の正規分布として扱います。

- nhssta はプロトタイプであり、まだ多くのバグが含まれている可能性があります。実行結果の妥当性については十分に検証されていません。

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
make check

# または直接スクリプトを実行
./scripts/run-all-checks.sh
```

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
./scripts/generate_coverage.sh
```

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

### 4.3 コントリビューション

- 新しい機能やバグ修正は、ブランチを作成してプルリクエストとして提出してください
- すべてのテストがパスし、CIのチェックが通ることを確認してください
- 詳細は `CONTRIBUTING.md` を参照してください

### 4.4 メモリ管理ポリシー

- `RandomVariable` / `Expression` / `Gate` / `Instance` など、実行時グラフを表す主要コンポーネントは **`std::shared_ptr` ベースの薄いハンドル型**に移行済みです
- 旧来の `SmartPtr` テンプレートは `NH_ENABLE_LEGACY_SMARTPTR=1` を定義した場合のみ利用できます（デフォルト無効）。新規コードでの使用は禁止です
- `CovarianceMatrix` も同様に `std::shared_ptr` 化されており、共有キャッシュを安全に扱えます
- 今後は「所有権は標準スマートポインタで明示する」ことを原則とし、例外はドキュメント化してください

## 5. 参考文献

nhssta は 以下の文献の手法に基づいて実装されています。

Jiayong Le, Xin Li, Lawrence T. Pileggi: STAC: statistical timing
analysis with correlation. DAC 2004: 343-348
