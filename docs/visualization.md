# 回路可視化ツール

`.bench` 形式の回路ファイルを可視化するためのPythonスクリプトです。

## 依存パッケージ

```bash
pip install networkx matplotlib
```

## 使用方法

### 基本的な使い方

```bash
# プロジェクトルートから実行
python scripts/visualize_bench.py example/s27.bench output

# または example ディレクトリから実行
cd example
python ../scripts/visualize_bench.py s27.bench output
```

### 1. 回路全体図の生成

```bash
python scripts/visualize_bench.py example/s820.bench s820
```

`s820.png` が生成されます。

### 2. 特定の出力へのパス可視化

```bash
python scripts/visualize_bench.py example/s820.bench s820 --path G290
```

特定の出力ノードへのパスを可視化します。
`--max-depth` で探索の深さを調整できます（デフォルト: 10）。

### 3. クリティカルパスの強調表示

1. 事前に `nhssta` でクリティカルパスを出力します。
    ```bash
    ./build/bin/nhssta -p 3 -d example/gaussdelay.dlib -b example/s298.bench > critical_paths.txt
    ```
2. 概要図上でパスを強調表示します。
    ```bash
    python scripts/visualize_bench.py example/s298.bench s298 --highlight-path critical_paths.txt
    ```
    `s298.png` が生成され、クリティカルパスが赤色で強調表示されます。

### 4. クリティカルパスのみ抽出

```bash
python scripts/visualize_bench.py example/s298.bench s298 --extract-path critical_paths.txt
```

`s298_extract.png` が生成され、クリティカルパス上のノードとエッジのみが描画されます。

## コマンドラインオプション

| オプション | 説明 |
|-----------|------|
| `bench_file` | 可視化する `.bench` ファイル（必須） |
| `output_name` | 出力ファイルのベース名（デフォルト: `circuit`） |
| `--path OUTPUT` | 指定した出力ノードへのパスを可視化 |
| `--highlight-path FILE` | nhssta出力からクリティカルパスを読み込み、全体図上で強調表示 |
| `--extract-path FILE` | nhssta出力からクリティカルパスを読み込み、パスのみを抽出表示 |
| `--max-depth N` | `--path` 使用時の探索深さ（デフォルト: 10） |

## 出力例

- **統計情報**: 入力数、出力数、DFF数、ゲートタイプ別の数（標準出力）
- **PNG画像**: 視覚的な回路図

## 可視化の特徴

- **入力ノード**: 水色、左側に配置
- **出力ノード**: 緑色、右側に配置
- **DFF**: 黄色、下部に配置（各DFFは固有のy座標）
- **ゲート**: タイプ別に色分け（AND: 赤、OR: シアン、NAND: ピンク、NOR: サーモン、NOT/INV: グレー）
- **クリティカルパス**: 赤色で強調、非クリティカル要素は薄く表示
- **DFF入力**: 青い点線で表示
- **配線接続点**: 小さな黒い点で表示
- **ノードラベル**: ノードの外側（上部）に表示

## 注意事項

- 大きな回路（300ゲート以上）の場合、全体の可視化は複雑になる可能性があります
- `nhssta` の出力フォーマット（`# Path ...`）が必要です。`-p` オプション付きで実行してください
- 特定の出力へのパス可視化（`--path`オプション）を使用すると、より見やすい図が生成されます

