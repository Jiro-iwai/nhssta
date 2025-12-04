# Issue #128: Sstaクラスの責務分離計画

## 現状の問題

`src/ssta.cpp`は約808行で、以下の複数の責務を担当しており、単一責任の原則（SRP）に違反しています。

### 現在の責務

1. **ファイルパース**
   - `read_dlib()` / `read_dlib_line()`: .dlibファイルのパース
   - `read_bench()` / `read_bench_input()` / `read_bench_output()` / `read_bench_net()`: .benchファイルのパース

2. **グラフ構築**
   - `connect_instances()`: インスタンスを接続してタイミング解析を実行
   - `set_instance_input()`: インスタンスの入力信号を設定
   - `set_dff_out()`: DFF出力信号を設定
   - `is_line_ready()`: ネットラインの準備状態をチェック

3. **クリティカルパス解析**
   - `getCriticalPaths()`: クリティカルパスを取得
   - `track_path()`: パスを追跡してデータ構造を構築

4. **結果データ変換**
   - `getLatResults()`: LAT結果を取得
   - `getCorrelationMatrix()`: 相関行列を取得
   - `getSensitivityResults()`: 感度解析結果を取得

## 問題点

- テストが困難（各機能を個別にテストしにくい）
- コード変更時の影響範囲が広い
- 新機能追加時にクラスが肥大化する
- コードの可読性が低い

## 推奨対策

責務ごとにクラスを分離:

```cpp
class DlibParser { /* .dlib ファイルパース */ };
class BenchParser { /* .bench ファイルパース */ };
class CircuitGraph { /* グラフ構造の管理 */ };
class CriticalPathAnalyzer { /* クリティカルパス解析 */ };
class SensitivityAnalyzer { /* 感度解析 */ };
class SstaResults { /* 結果データの生成（LAT、相関行列） */ };

class Ssta {
    // 上記クラスを組み合わせて統合
};
```

## 実装計画

### Phase 1: ファイルパーサーの分離

#### 1.1 DlibParserクラスの作成

**責務**: .dlibファイルのパースとGateオブジェクトの構築

**インターフェース**:
```cpp
class DlibParser {
public:
    explicit DlibParser(const std::string& dlib_file);
    void parse();
    const Gates& gates() const { return gates_; }
    
private:
    void parse_line(Parser& parser);
    std::string dlib_file_;
    Gates gates_;
};
```

**移行対象メソッド**:
- `Ssta::read_dlib()` → `DlibParser::parse()`
- `Ssta::read_dlib_line()` → `DlibParser::parse_line()`

#### 1.2 BenchParserクラスの作成

**責務**: .benchファイルのパースと回路構造の構築

**インターフェース**:
```cpp
class BenchParser {
public:
    explicit BenchParser(const std::string& bench_file);
    void parse();
    const Pins& inputs() const { return inputs_; }
    const Pins& outputs() const { return outputs_; }
    const Pins& dff_outputs() const { return dff_outputs_; }
    const Pins& dff_inputs() const { return dff_inputs_; }
    const Net& net() const { return net_; }
    
private:
    void parse_input(Parser& parser);
    void parse_output(Parser& parser);
    void parse_net(Parser& parser, const std::string& out_signal_name);
    std::string bench_file_;
    Pins inputs_;
    Pins outputs_;
    Pins dff_outputs_;
    Pins dff_inputs_;
    Net net_;
};
```

**移行対象メソッド**:
- `Ssta::read_bench()` → `BenchParser::parse()`（ただし、`connect_instances()`の呼び出しは除く）
- `Ssta::read_bench_input()` → `BenchParser::parse_input()`
- `Ssta::read_bench_output()` → `BenchParser::parse_output()`
- `Ssta::read_bench_net()` → `BenchParser::parse_net()`
- `Ssta::node_error()` → `BenchParser::node_error()`（エラーメッセージ生成）

**注意点**:
- `read_bench()`内の`connect_instances()`呼び出しは`Ssta`クラスで管理する
- `read_bench()`内の`gates_.clear()`も`Ssta`クラスで管理する（感度解析が有効な場合は保持）

### Phase 2: グラフ構築の分離

#### 2.1 CircuitGraphクラスの作成

**責務**: 回路グラフの構築と管理

**インターフェース**:
```cpp
class CircuitGraph {
public:
    using TrackPathCallback = std::function<void(const std::string& signal_name, 
                                                  const Instance& inst, 
                                                  const NetLineIns& ins, 
                                                  const std::string& gate_type)>;
    
    void build(const Gates& gates, const Net& net, 
               const Pins& inputs, const Pins& outputs,
               const Pins& dff_outputs, const Pins& dff_inputs,
               TrackPathCallback track_path_callback = nullptr);
    const Signals& signals() const { return signals_; }
    const std::unordered_map<std::string, std::string>& signal_to_instance() const {
        return signal_to_instance_;
    }
    const std::unordered_map<std::string, std::vector<std::string>>& instance_to_inputs() const {
        return instance_to_inputs_;
    }
    const std::unordered_map<std::string, std::string>& instance_to_gate_type() const {
        return instance_to_gate_type_;
    }
    const std::unordered_map<std::string, std::unordered_map<std::string, Normal>>& instance_to_delays() const {
        return instance_to_delays_;
    }
    
private:
    void connect_instances(const Gates& gates, const Net& net, TrackPathCallback track_path_callback);
    void set_instance_input(const Instance& inst, const NetLineIns& ins);
    void set_dff_out(const std::string& out_signal_name, TrackPathCallback track_path_callback);
    bool is_line_ready(const NetLine& line) const;
    void check_signal(const std::string& signal_name) const;
    void node_error(const std::string& head, const std::string& signal_name) const;
    
    Signals signals_;
    std::string bench_file_;  // エラーメッセージ用
    std::unordered_map<std::string, std::string> signal_to_instance_;
    std::unordered_map<std::string, std::vector<std::string>> instance_to_inputs_;
    std::unordered_map<std::string, std::string> instance_to_gate_type_;
    std::unordered_map<std::string, std::unordered_map<std::string, Normal>> instance_to_delays_;
};
```

**移行対象メソッド**:
- `Ssta::connect_instances()` → `CircuitGraph::build()`（`connect_instances()`を内部で呼ぶ）
- `Ssta::set_instance_input()` → `CircuitGraph::set_instance_input()`
- `Ssta::set_dff_out()` → `CircuitGraph::set_dff_out()`
- `Ssta::is_line_ready()` → `CircuitGraph::is_line_ready()`
- `Ssta::check_signal()` → `CircuitGraph::check_signal()`（信号の重複チェック）

**注意点**:
- `connect_instances()`内で`track_path()`が呼ばれているが、これは`CircuitGraph::build()`時にコールバックとして受け取る
- `build()`メソッドのシグネチャ: `void build(..., std::function<void(...)> track_path_callback = nullptr)`

### Phase 3: クリティカルパス解析の分離

#### 3.1 CriticalPathAnalyzerクラスの作成

**責務**: クリティカルパスの解析と追跡

**インターフェース**:
```cpp
class CriticalPathAnalyzer {
public:
    explicit CriticalPathAnalyzer(const CircuitGraph& graph);
    CriticalPaths analyze(size_t top_n) const;
    
private:
    void track_path(const std::string& signal_name, const Instance& inst, 
                    const NetLineIns& ins, const std::string& gate_type);
    const CircuitGraph& graph_;
};
```

**移行対象メソッド**:
- `Ssta::getCriticalPaths()` → `CriticalPathAnalyzer::analyze()`
- `Ssta::track_path()` → `CriticalPathAnalyzer::track_path()`（CircuitGraphのコールバックから呼ばれる）

**注意点**:
- `track_path()`は`CircuitGraph::build()`時にコールバックとして渡される
- `CriticalPathAnalyzer`は`CircuitGraph`のデータ構造を参照してパス情報を構築する

### Phase 3.5: 感度解析の分離

#### 3.2 SensitivityAnalyzerクラスの作成

**責務**: 感度解析の実行（目的関数の構築、勾配計算、ゲート感度の収集）

**インターフェース**:
```cpp
class SensitivityAnalyzer {
public:
    explicit SensitivityAnalyzer(const CircuitGraph& graph);
    SensitivityResults analyze(size_t top_n) const;
    
private:
    std::vector<SensitivityPath> collect_endpoint_paths() const;
    Expression build_objective_function(const std::vector<SensitivityPath>& endpoint_paths) const;
    void collect_gate_sensitivities(SensitivityResults& results) const;
    const CircuitGraph& graph_;
    static constexpr double GRADIENT_THRESHOLD = 1e-10;
    static constexpr double MIN_VARIANCE = 1e-10;
};
```

**移行対象メソッド**:
- `Ssta::getSensitivityResults()` → `SensitivityAnalyzer::analyze()`
- 感度解析に関連するロジック（約140行）を分離

**移行対象メソッド**:
- `Ssta::getSensitivityResults()` → `SensitivityAnalyzer::analyze()`
- 感度解析に関連するロジック（約140行）を分離

**注意点**:
- 感度解析は`instance_to_delays_`（クローンされた遅延）に依存している
- `CircuitGraph`が`instance_to_delays_`を提供する必要がある
- `zero_all_grad()`などのExpression関連の関数へのアクセスが必要
- `track_path()`は`CircuitGraph::build()`時にコールバックとして渡される（CriticalPathAnalyzerと共有）

### Phase 4: 結果データ生成の分離

#### 4.1 SstaResultsクラスの作成

**責務**: 結果データの生成と変換

**インターフェース**:
```cpp
class SstaResults {
public:
    explicit SstaResults(const CircuitGraph& graph);
    LatResults getLatResults() const;
    CorrelationMatrix getCorrelationMatrix() const;
    
private:
    const CircuitGraph& graph_;
};
```

**移行対象メソッド**:
- `Ssta::getLatResults()` → `SstaResults::getLatResults()`
- `Ssta::getCorrelationMatrix()` → `SstaResults::getCorrelationMatrix()`

**注意**: 感度解析は`SensitivityAnalyzer`クラスに分離されるため、`SstaResults`には含めない

### Phase 5: Sstaクラスの統合

**最終的なSstaクラス**:
```cpp
class Ssta {
public:
    Ssta();
    ~Ssta();
    
    void check();
    void read_dlib();
    void read_bench();  // BenchParser::parse() + CircuitGraph::build()を呼ぶ
    
    // 設定メソッド
    void set_lat() { is_lat_ = true; }
    void set_correlation() { is_correlation_ = true; }
    void set_critical_path(size_t top_n = DEFAULT_CRITICAL_PATH_COUNT);
    void set_sensitivity();
    void set_sensitivity_top_n(size_t top_n);
    
    void set_dlib(const std::string& dlib);
    void set_bench(const std::string& bench);
    
    // 結果取得メソッド（委譲）
    LatResults getLatResults() const;
    CorrelationMatrix getCorrelationMatrix() const;
    CriticalPaths getCriticalPaths(size_t top_n) const;
    CriticalPaths getCriticalPaths() const {
        return getCriticalPaths(critical_path_count_);
    }
    SensitivityResults getSensitivityResults(size_t top_n) const;
    SensitivityResults getSensitivityResults() const {
        return getSensitivityResults(sensitivity_top_n_);
    }
    
private:
    std::string dlib_;
    std::string bench_;
    bool is_lat_ = false;
    bool is_correlation_ = false;
    bool is_critical_path_ = false;
    bool is_sensitivity_ = false;
    size_t critical_path_count_ = DEFAULT_CRITICAL_PATH_COUNT;
    size_t sensitivity_top_n_ = DEFAULT_CRITICAL_PATH_COUNT;
    
    // 分離されたコンポーネント
    std::unique_ptr<DlibParser> dlib_parser_;
    std::unique_ptr<BenchParser> bench_parser_;
    std::unique_ptr<CircuitGraph> circuit_graph_;
    std::unique_ptr<CriticalPathAnalyzer> path_analyzer_;
    std::unique_ptr<SensitivityAnalyzer> sensitivity_analyzer_;
    std::unique_ptr<SstaResults> results_;
    
    // Gatesは感度解析が有効な場合のみ保持（メモリ最適化）
    Gates gates_;  // DlibParserから取得後、感度解析が無効ならクリア
};
```

## 実装ステップ

1. ✅ 実装計画ドキュメントの作成（このファイル）
2. ⏳ Phase 1: DlibParserクラスの作成とテスト
3. ⏳ Phase 1: BenchParserクラスの作成とテスト
4. ⏳ Phase 2: CircuitGraphクラスの作成とテスト
5. ⏳ Phase 3: CriticalPathAnalyzerクラスの作成とテスト
6. ⏳ Phase 3.5: SensitivityAnalyzerクラスの作成とテスト
7. ⏳ Phase 4: SstaResultsクラスの作成とテスト
8. ⏳ Phase 5: Sstaクラスの統合と既存テストの確認
9. ⏳ リファクタリング完了後のクリーンアップ

## メリット

1. **テスト容易性**: 各クラスを個別にテスト可能
2. **保守性**: 責務が明確で変更の影響範囲が限定的
3. **拡張性**: 新機能追加時に既存コードへの影響が少ない
4. **可読性**: コードの構造が明確で理解しやすい

## 注意点

1. **段階的な移行**: 一度にすべてを変更せず、段階的に移行する
2. **後方互換性**: 既存のAPIを維持し、内部実装のみを変更する
3. **テスト**: 各フェーズで既存テストがパスすることを確認する
4. **パフォーマンス**: 分離によるオーバーヘッドを最小限に抑える
5. **コールバックの設計**: `CircuitGraph::build()`時に`track_path()`コールバックを渡すことで、クリティカルパス解析と感度解析の両方に対応
6. **メモリ管理**: 感度解析が無効な場合、`gates_`をクリアしてメモリを節約（`Ssta`クラスで管理）

## 追加で考慮すべき点

### 未カバーのメソッド

以下のメソッドは既に計画に含まれていますが、明示的に記載します：

1. **check()** - 設定の検証 → `Ssta`クラスに残す
2. **node_error()** - エラーメッセージ生成 → `BenchParser`と`CircuitGraph`に移動
3. **check_signal()** - 信号の重複チェック → `CircuitGraph`に移動
4. **set_*() / is_*()** - 設定メソッド → `Ssta`クラスに残す
5. **デストラクタ** - 静的キャッシュのクリア → `Ssta`クラスに残す

