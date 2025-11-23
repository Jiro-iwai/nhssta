# Contributing to nhssta

このドキュメントは、nhsstaプロジェクトへの貢献方法とコーディングスタイルガイドラインを説明します。

## コーディングスタイル

### 基本方針

- **C++17標準**を使用します
- 既存コードのスタイルを尊重し、一貫性を保ちます
- 自動フォーマットツール（clang-format）と静的解析ツール（clang-tidy）を使用してスタイルと品質を維持します

### インデント

- **スペース4文字**を使用します（タブは使用しません）
- 既存コードでタブが使用されている場合は、段階的にスペースに移行します

### 命名規則

- **クラス名**: PascalCase（例: `Ssta`, `RandomVariable`, `OpADD`）
- **関数名**: camelCase（例: `getLatResults()`, `read_dlib()`）
- **メンバ変数**: snake_case with trailing underscore（例: `is_lat_`, `signals_`, `dlib_`）
- **ローカル変数**: snake_case（例: `gate_name`, `signal_name`）
- **定数**: UPPER_SNAKE_CASE（例: `MINIMUM_VARIANCE`）

### ヘッダガード

- 形式: `NH_<FILENAME>__H`（例: `NH_SSTA__H`, `NH_COVARIANCE__H`）
- アンダースコア2つを使用します

### 名前空間

- グローバル名前空間の汚染を避けるため、適切な名前空間を使用します
- `using namespace` は関数スコープ内でのみ使用可能とします
- グローバルスコープでの `using namespace std;` は避けます（既存コードの `main.C` は段階的に改善）

### コメント

- ファイルヘッダには `// -*- c++ -*-` と著者名を含めます
- 複雑なロジックには説明コメントを追加します
- 日本語コメントも可とします

### コードフォーマット

- `clang-format` を使用してコードを整形します
- プロジェクトルートの `.clang-format` ファイルに設定が定義されています
- 新しいコードや変更されたコードには `clang-format` を適用してください

### 静的解析

- `clang-tidy` を使用してコードの品質をチェックします
- プロジェクトルートの `.clang-tidy` ファイルに設定が定義されています
- ローカルで `make tidy` を実行してチェックできます
- CIでも自動的にチェックされます

### メモリ管理ポリシー

- ドメイン固有のハンドル型（`RandomVariable`, `Expression`, `Gate`, `Instance`, `CovarianceMatrix` など）は、内部的に `std::shared_ptr` を保持します
- 新しく所有権を導入する際は、既存のハンドル型を利用するか、`std::shared_ptr`/`std::unique_ptr` を直接使用してください
- 旧来の `SmartPtr` テンプレートは `NH_ENABLE_LEGACY_SMARTPTR=1` を定義した場合のみコンパイルできるようにし、プロダクションコードでは使用しないでください（`RCObject` は将来的な削除に備えて互換目的でのみ残っています）

## 開発フロー

### ローカルでの開発

1. コードを変更する前に、既存のテストがすべて通ることを確認します
   ```bash
   make test
   ```

2. コードを変更した後、以下を実行します：
   ```bash
   # テストを実行
   make test
   
   # スタイルチェック（clang-tidy）
   make tidy
   
   # コードフォーマット（必要に応じて）
   clang-format -i src/YourFile.C
   ```

3. すべてのテストがパスし、警告が許容範囲内であることを確認してからコミットします

### プルリクエスト

- 新しい機能やバグ修正は、ブランチを作成してプルリクエストとして提出してください
- プルリクエストには、変更内容の説明とテスト結果を含めてください
- CIのチェック（ビルド、テスト、リンタ）がすべて通ることを確認してください

## テスト

- 新しい機能には必ずテストを追加してください
- 既存のテストが壊れないことを確認してください
- テストは Google Test フレームワークを使用します

## スタイル適用ポリシー

### 新規コード・変更コード

- **新規に追加するファイル**や**変更されたファイル**には、`clang-format`を適用してください
- プルリクエストを提出する前に、`make tidy`を実行して警告を確認してください
- 重要な警告（バグにつながる可能性があるもの）は修正してください

### 既存コード

- 既存のコードは段階的に改善していきます
- 既存ファイルを大幅に変更する場合は、そのファイルに`clang-format`を適用してください
- ただし、動作を変えない範囲で行ってください

### CIでのチェック

- CIでは`clang-tidy`と`clang-format --dry-run`が自動的に実行されます
- 現時点では警告があってもCIは失敗しません（ログとして表示されるだけ）
- 将来的には、合意できたチェックだけを`WarningsAsErrors`に引き上げていきます

## コーディングスタイルの具体例

### 良い例と悪い例

#### 1. デストラクタの定義

**悪い例:**
```cpp
class OpADD : public _RandomVariable_ {
public:
    virtual ~OpADD() {}
};
```

**良い例:**
```cpp
class OpADD : public _RandomVariable_ {
public:
    ~OpADD() override = default;
};
```

**理由**: 自明なデストラクタは`= default`を使用し、オーバーライドする場合は`override`キーワードを明示します。

#### 2. 戻り値の無視を防ぐ

**悪い例:**
```cpp
double calc_mean() const;
```

**良い例:**
```cpp
[[nodiscard]] double calc_mean() const;
```

**理由**: `[[nodiscard]]`属性により、戻り値が無視された場合にコンパイラが警告を出します。

#### 3. 不要な`else`の削除

**悪い例:**
```cpp
if( a == zero ) {
    return b;
} else if( b == zero ) {
    return a;
}
```

**良い例:**
```cpp
if( a == zero ) {
    return b;
}
if( b == zero ) {
    return a;
}
```

**理由**: `return`の後の`else`は不要です。コードが簡潔になり、可読性が向上します。

#### 4. ブレースの使用

**悪い例:**
```cpp
if( !is_set_value() )
    return;
```

**良い例:**
```cpp
if( !is_set_value() ) {
    return;
}
```

**理由**: 単一ステートメントでもブレースを使用することで、コードの一貫性が保たれ、後からステートメントを追加する際のミスを防げます。

#### 5. Braced Initializer Listの使用

**悪い例:**
```cpp
return Expression( new Expression_(Expression_::PLUS, a, b) );
```

**良い例:**
```cpp
return Expression{ new Expression_(Expression_::PLUS, a, b) };
```

**理由**: C++17では、braced initializer listを使用することで、型の推論が明確になり、コードが読みやすくなります。

#### 6. `auto`の使用

**悪い例:**
```cpp
Delays::const_iterator i = delays_.find(io);
```

**良い例:**
```cpp
auto i = delays_.find(io);
```

**理由**: イテレータのような複雑な型には`auto`を使用することで、コードが簡潔になり、型の変更に対する保守性が向上します。

#### 7. 演算子の優先順位を明確にする

**悪い例:**
```cpp
double r = lv + 2.0 * cov + rv;
```

**良い例:**
```cpp
double r = lv + (2.0 * cov) + rv;
```

**理由**: 括弧を使用することで、演算子の優先順位が明確になり、意図しない計算順序によるバグを防げます。

#### 8. `typedef`から`using`への移行

**悪い例:**
```cpp
typedef std::pair<RandomVariable,RandomVariable> RowCol;
typedef std::map<RowCol,double> Matrix;
```

**良い例:**
```cpp
using RowCol = std::pair<RandomVariable,RandomVariable>;
using Matrix = std::map<RowCol,double>;
```

**理由**: `using`宣言はテンプレートエイリアスにも使用でき、より現代的なC++のスタイルです。

## 段階的なスタイル適用の方針

### 現在の方針（2024年11月時点）

1. **新規コード・変更コード**: 
   - 新規に追加するファイルや変更されたファイルには、`clang-format`を適用
   - 重要な`clang-tidy`警告は修正

2. **既存コード**: 
   - 既存のコードは段階的に改善
   - ファイルを大幅に変更する際に、そのファイルに`clang-format`を適用
   - 動作を変えない範囲で行う

3. **警告の優先順位**:
   - **高優先度**: バグにつながる可能性がある警告（未初期化変数、メモリリークなど）
   - **中優先度**: コード品質に関する警告（`override`の使用、`[[nodiscard]]`など）
   - **低優先度**: 設計上の理由で変更困難な警告（protected virtual destructorなど）

### 今後の計画

- **Phase 1（完了）**: ツールの導入と設定ファイルの作成
- **Phase 2（完了）**: 代表ファイルでのスタイル修正と警告削減
- **Phase 3（進行中）**: ドキュメントの充実と方針の明確化
- **Phase 4（将来）**: 段階的な全体適用とCIでの警告レベルの引き上げ

## 質問や提案

- 質問や提案がある場合は、GitHubのIssuesで議論してください
- コーディングスタイルに関する質問も歓迎します

