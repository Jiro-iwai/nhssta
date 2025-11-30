# C-6: CLI 感度解析オプション 実装状況

## 現状 (2024-11-30)

### 完了している部分

1. **CLI オプションのパース** (`src/main.cpp`)
   - `-s`, `--sensitivity`: 感度解析を有効化
   - `-n N`, `--top N`: top N パスを指定

2. **出力フォーマット** (`src/main.cpp`)
   - `formatSensitivityResults()` 関数で結果をテキスト出力
   - パス一覧と勾配一覧を表示

3. **感度計算ロジック** (`src/ssta.cpp`)
   - `getSensitivityResults()` 関数
   - パス選出: LAT + σ で降順ソート
   - 目的関数: `F = log(Σ exp(LAT + σ))`
   - backward() による勾配計算

4. **データ構造** (`include/nhssta/ssta_results.hpp`)
   - `SensitivityPath`: パス情報 (endpoint, lat, std_dev, score)
   - `GateSensitivity`: ゲート勾配 (gate_name, grad_mu, grad_sigma)
   - `SensitivityResults`: 結果全体

5. **テスト** (`test/test_main_cli.cpp`)
   - `-s` オプションのパーステスト

### 未解決の問題

#### 問題: 勾配が伝播しない

**現象**: `Gate Sensitivities` が空になる

**原因**: `src/gate.cpp` line 90 で delay をクローンしている

```cpp
// src/gate.cpp InstanceImpl::output()
Normal gate_delay = gate_->delay(ith_in_name, out_name);
RandomVariable delay = gate_delay.clone();  // ← ここでコピーを作成
```

**影響**:
1. Expression tree はクローンされた Normal に接続される
2. `backward()` で勾配はクローンの `mu_expr_`, `sigma_expr_` に蓄積
3. オリジナルの `gates_` 内の Normal には勾配が伝播しない
4. `getSensitivityResults()` で `gates_` を反復しても勾配は 0

**検証コード**:
```cpp
// これは動作する（クローンなし）
Normal delay1(10.0, 1.0);
auto signal = input + delay1;  // delay1 を直接使用
signal->mean_expr()->backward();
delay1->mean_expr()->gradient();  // 正しい勾配が得られる

// これは動作しない（クローンあり）
Normal delay1(10.0, 1.0);
Normal delay_clone = delay1.clone();
auto signal = input + delay_clone;  // クローンを使用
signal->mean_expr()->backward();
delay1->mean_expr()->gradient();  // 0（勾配はクローンに蓄積）
```

### 修正案

#### 案1: クローンしない

`src/gate.cpp` で `clone()` を削除し、オリジナルの delay を直接使用。

```cpp
// Before
RandomVariable delay = gate_delay.clone();

// After
RandomVariable delay = gate_delay;  // 参照をそのまま使用
```

**懸念点**: 同じゲートの複数インスタンスが delay を共有することになる。
数値計算には影響ないが、設計上の意図を確認する必要あり。

#### 案2: クローンを追跡

クローンされた delay を `Instance` または `Ssta` に保存し、
勾配収集時にそれらを反復する。

```cpp
// Instance に delay を保存
class InstanceImpl {
    std::vector<Normal> used_delays_;  // 追加
};

// 勾配収集時
for (const auto& inst : instances_) {
    for (const auto& delay : inst->used_delays()) {
        double grad_mu = delay->mean_expr()->gradient();
        // ...
    }
}
```

**懸念点**: コード変更が大きい。

#### 案3: Signal から逆探索

出力 signal から Expression tree を逆探索し、
葉ノード（Normal の mu_expr_, sigma_expr_）を見つけて勾配を収集。

**懸念点**: Expression tree の構造が複雑で実装が難しい。

### 推奨

**案1** が最もシンプル。clone() の元の意図を確認した上で適用する。

`clone()` が必要な理由が特にない場合は削除して問題ない。
SSTA では同じゲートタイプの複数インスタンスが同じ delay 特性を持つのが自然。

### 動作確認済みの例

```bash
# 正常動作（1出力のみ）
./build/bin/nhssta -d example/dos.dlib -b example/dos.bench -s -n 3

# エラー（const delay で σ=0、ゼロ除算）
./build/bin/nhssta -d example/ex4.dlib -b example/ex4.bench -s -n 3
# → error: Runtime error: Expression: division by zero

# 正常動作（gauss delay で σ>0）だがゲート勾配は空
./build/bin/nhssta -d example/ex4_gauss.dlib -b example/ex4.bench -s -n 3
```

### 関連ファイル

| ファイル | 変更内容 |
|---------|---------|
| `src/main.cpp` | CLI オプション、出力フォーマット |
| `src/ssta.cpp` | `getSensitivityResults()` |
| `include/nhssta/ssta.hpp` | メソッド宣言 |
| `include/nhssta/ssta_results.hpp` | データ構造 |
| `test/test_main_cli.cpp` | テスト |
| `src/gate.cpp` | **要修正**: `clone()` の削除/変更 |

### 次のステップ

1. `clone()` の設計意図を確認
2. 案1 を適用して勾配伝播を修正
3. σ=0 (const delay) のゼロ除算対策を追加
4. 統合テストで動作確認
5. PR 作成

### 参考

- Issue #167: 感度解析機能
- `docs/sensitivity_analysis_plan.md`: 設計ドキュメント

