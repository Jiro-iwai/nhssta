# Issue #130: covariance関数のdynamic_castチェーン改善計画

## 現状の問題

`src/covariance.cpp`の`covariance()`関数（320-403行）で、約80行の`dynamic_cast`チェーンを使用してRandomVariableの型を判定しています。

### 問題点

1. **拡張性が低い**: 新しい演算子追加時に`covariance()`関数を修正する必要がある
2. **可読性が低い**: 長大なif-elseチェーン
3. **保守性が低い**: 型判定ロジックが一箇所に集中
4. **パフォーマンス**: `dynamic_cast`のオーバーヘッド（各チェックで実行時型情報を確認）

## 解決策の再検討

### 問題点の明確化

最初の設計では、`compute_covariance_with()`内で`dynamic_cast`を使用していましたが、これは**`dynamic_cast`チェーンを各クラスに分散させるだけで、根本的な解決にならない**という問題があります。

### 設計方針の見直し

`dynamic_cast`を完全に排除するため、以下のアプローチを検討します：

#### 案1: Visitorパターン（推奨）

**設計**:
- `RandomVariableImpl`に`accept()`メソッドを追加
- `CovarianceVisitor`インターフェースを定義
- 各型が`accept()`でvisitorを受け取り、自身の型に応じて適切なメソッドを呼び出す

**メリット**:
- `dynamic_cast`を完全に排除できる
- 型安全
- 拡張性が高い

**デメリット**:
- 双方向処理が複雑（`covariance(a, b)`と`covariance(b, a)`の両方を考慮）
- 実装コストが高い

#### 案2: 型タグ + 仮想関数

**設計**:
- 各クラスに型ID（enum）を持たせる
- `compute_covariance_with()`内で型IDをチェック

**メリット**:
- `dynamic_cast`が不要
- 実装が比較的シンプル

**デメリット**:
- 型IDの管理が必要
- 型システムとの整合性が低い

#### 案3: 仮想関数（改善版）

**設計**:
- `covariance()`関数内の`dynamic_cast`チェーンを削減
- 各クラス内での`dynamic_cast`は許容（分散されるが、`covariance()`関数はシンプルに）

**メリット**:
- 実装が簡単
- `covariance()`関数がシンプルになる

**デメリット**:
- 各クラス内で`dynamic_cast`が残る
- 完全な解決にはならない

### 結論: 現実的な選択肢

Visitorパターンで完全に`dynamic_cast`を排除するのは理論的には可能ですが、実装が非常に複雑になります（双方向ディスパッチ、各型の組み合わせに対するメソッド定義など）。

**推奨: 案3（部分的解決）を採用**

- `covariance()`関数内の`dynamic_cast`チェーンを削減（約80行 → 約30行）
- 各クラス内での`dynamic_cast`は許容（分散されるが、`covariance()`関数はシンプルに）
- 実装が簡単で、既存コードへの影響が少ない
- 新しい型を追加する際、`covariance()`関数を変更する必要がない

**完全な解決を目指す場合**: Visitorパターンも検討可能ですが、実装コストが高い

### 実装計画（Visitorパターン）

#### Phase 1: Visitorインターフェースの定義

```cpp
// covariance.hpp
// 双方向ディスパッチ（double dispatch）用のVisitorインターフェース
// 各型の組み合わせに対してメソッドを定義
class CovarianceVisitor {
public:
    virtual ~CovarianceVisitor() = default;
    
    // Normal × 各型
    virtual double visit_normal_normal(const NormalImpl& a, const NormalImpl& b) = 0;
    virtual double visit_normal_add(const NormalImpl& a, const OpADD& b) = 0;
    virtual double visit_normal_sub(const NormalImpl& a, const OpSUB& b) = 0;
    virtual double visit_normal_max(const NormalImpl& a, const OpMAX& b) = 0;
    virtual double visit_normal_max0(const NormalImpl& a, const OpMAX0& b) = 0;
    
    // OpADD × 各型
    virtual double visit_add_normal(const OpADD& a, const NormalImpl& b) = 0;
    virtual double visit_add_add(const OpADD& a, const OpADD& b) = 0;
    virtual double visit_add_sub(const OpADD& a, const OpSUB& b) = 0;
    virtual double visit_add_max(const OpADD& a, const OpMAX& b) = 0;
    virtual double visit_add_max0(const OpADD& a, const OpMAX0& b) = 0;
    
    // OpSUB × 各型
    virtual double visit_sub_normal(const OpSUB& a, const NormalImpl& b) = 0;
    virtual double visit_sub_add(const OpSUB& a, const OpADD& b) = 0;
    virtual double visit_sub_sub(const OpSUB& a, const OpSUB& b) = 0;
    virtual double visit_sub_max(const OpSUB& a, const OpMAX& b) = 0;
    virtual double visit_sub_max0(const OpSUB& a, const OpMAX0& b) = 0;
    
    // OpMAX × 各型
    virtual double visit_max_normal(const OpMAX& a, const NormalImpl& b) = 0;
    virtual double visit_max_add(const OpMAX& a, const OpADD& b) = 0;
    virtual double visit_max_sub(const OpMAX& a, const OpSUB& b) = 0;
    virtual double visit_max_max(const OpMAX& a, const OpMAX& b) = 0;
    virtual double visit_max_max0(const OpMAX& a, const OpMAX0& b) = 0;
    
    // OpMAX0 × 各型
    virtual double visit_max0_normal(const OpMAX0& a, const NormalImpl& b) = 0;
    virtual double visit_max0_add(const OpMAX0& a, const OpADD& b) = 0;
    virtual double visit_max0_sub(const OpMAX0& a, const OpSUB& b) = 0;
    virtual double visit_max0_max(const OpMAX0& a, const OpMAX& b) = 0;
    virtual double visit_max0_max0(const OpMAX0& a, const OpMAX0& b) = 0;
};

// random_variable.hpp
class RandomVariableImpl {
public:
    // 双方向ディスパッチ: a->accept_covariance(visitor, b) を呼び出すと、
    // visitor が a の型に応じたメソッドを呼び出し、そのメソッド内で
    // b->accept_covariance() を呼び出して b の型に応じた処理を行う
    virtual void accept_covariance(CovarianceVisitor& visitor, const RandomVariable& other, double& cov) const = 0;
};
```

#### Phase 2: 各型での実装

1. `RandomVariableImpl`に仮想関数を追加:
   ```cpp
   // random_variable.hpp
   class RandomVariableImpl {
   protected:
       // Compute covariance with another RandomVariable
       // Returns true if this type can handle the computation, false otherwise
       // If returns true, cov is set to the computed value
       // 
       // Note: This method can call covariance() recursively, but should avoid
       // infinite loops by delegating to the covariance() function which handles
       // caching and fallback logic.
       virtual bool compute_covariance_with(const RandomVariable& other, double& cov) const;
   };
   ```

2. 各派生クラスで`accept_covariance()`を実装（`dynamic_cast`完全排除）:
   ```cpp
   // add.cpp
   void OpADD::accept_covariance(CovarianceVisitor& visitor, const RandomVariable& other, double& cov) const {
       // 双方向ディスパッチ: other の accept_covariance を呼び出し、
       // other の型に応じた visit_add_* メソッドが呼び出される
       other->accept_covariance_for_add(visitor, *this, cov);
   }
   
   // sub.cpp
   void OpSUB::accept_covariance(CovarianceVisitor& visitor, const RandomVariable& other, double& cov) const {
       other->accept_covariance_for_sub(visitor, *this, cov);
   }
   
   // max.cpp
   void OpMAX::accept_covariance(CovarianceVisitor& visitor, const RandomVariable& other, double& cov) const {
       other->accept_covariance_for_max(visitor, *this, cov);
   }
   
   // max.cpp
   void OpMAX0::accept_covariance(CovarianceVisitor& visitor, const RandomVariable& other, double& cov) const {
       other->accept_covariance_for_max0(visitor, *this, cov);
   }
   
   // normal.cpp
   void NormalImpl::accept_covariance(CovarianceVisitor& visitor, const RandomVariable& other, double& cov) const {
       other->accept_covariance_for_normal(visitor, *this, cov);
   }
   ```
   
   しかし、この方法では各型に複数の`accept_covariance_for_*`メソッドが必要になり、複雑になります。
   
   **より良い方法: 双方向ディスパッチの簡略化**
   
   ```cpp
   // random_variable.hpp
   class RandomVariableImpl {
   public:
       // a->accept_covariance(visitor, b) を呼び出すと、
       // visitor が a の型に応じたメソッドを呼び出し、
       // そのメソッド内で b->accept_covariance() を呼び出して
       // b の型に応じた処理を行う（双方向ディスパッチ）
       virtual void accept_covariance(CovarianceVisitor& visitor, const RandomVariable& other, double& cov) const = 0;
   };
   
   // add.cpp
   void OpADD::accept_covariance(CovarianceVisitor& visitor, const RandomVariable& other, double& cov) const {
       // other の accept_covariance を呼び出し、other の型に応じた処理を委譲
       // 双方向ディスパッチにより、visitor.visit_add_* が呼び出される
       other->accept_covariance(visitor, RandomVariable(const_cast<OpADD*>(this)), cov);
   }
   ```
   
   これでも`dynamic_cast`は不要ですが、双方向ディスパッチの実装が複雑になります。
   
   **最もシンプルな方法: 各型が自身の型を明示的に処理**
   
   ```cpp
   // add.cpp
   void OpADD::accept_covariance(CovarianceVisitor& visitor, const RandomVariable& other, double& cov) const {
       // other の accept_covariance を呼び出し、other の型に応じた処理を委譲
       // CovarianceVisitor に OpADD を渡し、other の型に応じた visit_add_* を呼び出す
       CovarianceVisitorForAdd add_visitor(*this, visitor);
       other->accept_covariance(add_visitor, RandomVariable(), cov);
   }
   ```
   
   しかし、これも複雑です。
   
   **推奨: シンプルな双方向ディスパッチ**
   
   ```cpp
   // add.cpp
   void OpADD::accept_covariance(CovarianceVisitor& visitor, const RandomVariable& other, double& cov) const {
       // other の accept_covariance を呼び出し、other の型に応じた処理を委譲
       // CovarianceVisitor に OpADD を渡すためのヘルパー
       CovarianceVisitorHelper helper(visitor, *this);
       other->accept_covariance(helper, RandomVariable(), cov);
   }
   ```

3. `CovarianceVisitor`の実装（`covariance.cpp`内）:
   ```cpp
   class CovarianceVisitorImpl : public CovarianceVisitor {
   public:
       double visit_normal_normal(const NormalImpl& a, const NormalImpl& b) override {
           return 0.0;  // Independent normal distributions
       }
       
       double visit_add(const OpADD& a, const RandomVariable& b) override {
           double cov0 = covariance(a.left(), b);
           double cov1 = covariance(a.right(), b);
           return cov0 + cov1;
       }
       
       double visit_sub(const OpSUB& a, const RandomVariable& b) override {
           double cov0 = covariance(a.left(), b);
           double cov1 = covariance(a.right(), b);
           return cov0 - cov1;
       }
       
       double visit_max(const OpMAX& a, const RandomVariable& b) override {
           const RandomVariable& x = a.left();
           const RandomVariable& z = a.max0();
           double cov0 = covariance(z, b);
           double cov1 = covariance(x, b);
           return cov0 + cov1;
       }
       
       double visit_max0(const OpMAX0& a, const RandomVariable& b) override {
           // 複雑な処理（既存のロジックを使用）
           // ...
       }
   };
   ```

3. `CovarianceVisitor`の実装（`covariance.cpp`内、`dynamic_cast`完全排除）:
   ```cpp
   class CovarianceVisitorImpl : public CovarianceVisitor {
   public:
       // Normal × Normal
       double visit_normal_normal(const NormalImpl& a, const NormalImpl& b) override {
           return 0.0;  // Independent normal distributions
       }
       
       // Normal × OpADD: Normal は OpADD を処理できないので、対称ケースに委譲
       double visit_normal_add(const NormalImpl& a, const OpADD& b) override {
           // Cov(Normal, OpADD) = Cov(OpADD, Normal) なので、OpADD側で処理
           return visit_add_normal(b, a);
       }
       
       // OpADD × Normal
       double visit_add_normal(const OpADD& a, const NormalImpl& b) override {
           double cov0 = covariance(a.left(), RandomVariable(const_cast<NormalImpl*>(&b)));
           double cov1 = covariance(a.right(), RandomVariable(const_cast<NormalImpl*>(&b)));
           return cov0 + cov1;
       }
       
       // OpADD × OpADD
       double visit_add_add(const OpADD& a, const OpADD& b) override {
           double cov0 = covariance(a.left(), RandomVariable(const_cast<OpADD*>(&b)));
           double cov1 = covariance(a.right(), RandomVariable(const_cast<OpADD*>(&b)));
           return cov0 + cov1;
       }
       
       // OpADD × OpSUB
       double visit_add_sub(const OpADD& a, const OpSUB& b) override {
           double cov0 = covariance(a.left(), RandomVariable(const_cast<OpSUB*>(&b)));
           double cov1 = covariance(a.right(), RandomVariable(const_cast<OpSUB*>(&b)));
           return cov0 + cov1;
       }
       
       // OpADD × OpMAX
       double visit_add_max(const OpADD& a, const OpMAX& b) override {
           double cov0 = covariance(a.left(), RandomVariable(const_cast<OpMAX*>(&b)));
           double cov1 = covariance(a.right(), RandomVariable(const_cast<OpMAX*>(&b)));
           return cov0 + cov1;
       }
       
       // OpADD × OpMAX0
       double visit_add_max0(const OpADD& a, const OpMAX0& b) override {
           double cov0 = covariance(a.left(), RandomVariable(const_cast<OpMAX0*>(&b)));
           double cov1 = covariance(a.right(), RandomVariable(const_cast<OpMAX0*>(&b)));
           return cov0 + cov1;
       }
       
       // OpSUB × Normal
       double visit_sub_normal(const OpSUB& a, const NormalImpl& b) override {
           double cov0 = covariance(a.left(), RandomVariable(const_cast<NormalImpl*>(&b)));
           double cov1 = covariance(a.right(), RandomVariable(const_cast<NormalImpl*>(&b)));
           return cov0 - cov1;
       }
       
       // OpSUB × OpADD
       double visit_sub_add(const OpSUB& a, const OpADD& b) override {
           double cov0 = covariance(a.left(), RandomVariable(const_cast<OpADD*>(&b)));
           double cov1 = covariance(a.right(), RandomVariable(const_cast<OpADD*>(&b)));
           return cov0 - cov1;
       }
       
       // OpMAX × Normal
       double visit_max_normal(const OpMAX& a, const NormalImpl& b) override {
           const RandomVariable& x = a.left();
           const RandomVariable& z = a.max0();
           double cov0 = covariance(z, RandomVariable(const_cast<NormalImpl*>(&b)));
           double cov1 = covariance(x, RandomVariable(const_cast<NormalImpl*>(&b)));
           return cov0 + cov1;
       }
       
       // OpMAX0 × Normal
       double visit_max0_normal(const OpMAX0& a, const NormalImpl& b) override {
           return covariance_x_max0_y(RandomVariable(const_cast<NormalImpl*>(&b)), 
                                      RandomVariable(const_cast<OpMAX0*>(&a)));
       }
       
       // OpMAX0 × OpMAX0
       double visit_max0_max0(const OpMAX0& a, const OpMAX0& b) override {
           if (a.left() == b.left()) {
               return a.variance();
           } else {
               return covariance_max0_max0(RandomVariable(const_cast<OpMAX0*>(&a)),
                                           RandomVariable(const_cast<OpMAX0*>(&b)));
           }
       }
       
       // ... 他の組み合わせも同様に実装
   };
   ```

**双方向ディスパッチの実装**:
- `a->accept_covariance(visitor, b)`を呼び出すと、`a`の型に応じた`accept_covariance()`が呼び出される
- その中で`b->accept_covariance()`を呼び出すと、`b`の型に応じた`visit_*_*`メソッドが呼び出される
- **`dynamic_cast`は一切使用しない**

#### Phase 3: covariance()関数のリファクタリング

```cpp
double covariance(const RandomVariable& a, const RandomVariable& b) {
    double cov = 0.0;
    
    if (!covariance_matrix->lookup(a, b, cov)) {
        if (a == b) {
            cov = a->variance();
        } else {
            // Visitorパターンを使用（dynamic_castなし）
            CovarianceVisitorImpl visitor;
            a->accept_covariance(visitor, b, cov);
            // 双方向処理は visitor 内で処理
        }
        
        check_covariance(cov, a, b);
        covariance_matrix->set(a, b, cov);
    }
    
    return cov;
}
```

**注意**: Visitorパターンでも双方向処理が必要な場合があります。その場合は、`CovarianceVisitor`に双方向処理のメソッドを追加する必要があります。

**Visitorパターンの利点**:
- **`dynamic_cast`を完全に排除**: 各型が`accept()`で自身の型を明示的に処理
- **型安全**: コンパイル時に型チェックが可能
- **拡張性**: 新しい型を追加する際、`CovarianceVisitor`にメソッドを追加するだけ
- **`covariance()`関数がシンプル**: 約10行程度に削減可能

**Visitorパターンの課題**:
- **双方向処理**: `covariance(a, b)`と`covariance(b, a)`の両方を考慮する必要がある
  - 解決策: `CovarianceVisitor`に双方向処理のメソッドを追加
  - または、`accept_covariance()`内で`other->accept_covariance()`を呼び出す

#### Phase 4: 段階的な移行

1. まず、シンプルな型（OpADD, OpSUB）から実装
2. 次に、NormalImplを実装
3. 最後に、複雑な型（OpMAX, OpMAX0）を実装
4. すべての型が実装されたら、dynamic_castチェーンを削除

#### Phase 5: 最終的な形（すべての型が実装された後）

Visitorパターンを採用した場合、`covariance()`関数は以下のように非常にシンプルになります：

```cpp
double covariance(const RandomVariable& a, const RandomVariable& b) {
    double cov = 0.0;
    
    // Step 1: Check cache
    if (!covariance_matrix->lookup(a, b, cov)) {
        // Step 2: Same variable → variance
        if (a == b) {
            cov = a->variance();
        } 
        // Step 3: Visitorパターンで型別処理（dynamic_castなし）
        else {
            CovarianceVisitorImpl visitor;
            a->accept_covariance(visitor, b, cov);
            // 双方向処理は visitor 内で処理
        }
        
        // Step 4: Validate and cache result
        check_covariance(cov, a, b);
        covariance_matrix->set(a, b, cov);
    }
    
    return cov;
}
```

**最終形の特徴**:
- **約15行**: 現在の約80行から大幅に削減
- **dynamic_castチェーン完全排除**: Visitorパターンにより、すべての型判定が仮想関数に委譲される
- **シンプルなロジック**: キャッシュチェック → 同一変数チェック → Visitor呼び出し → エラーハンドリング
- **拡張性**: 新しい型を追加する際、`covariance()`関数を変更する必要がない

**比較**:
- **現在**: 約80行の`dynamic_cast`チェーン
- **仮想関数案（問題あり）**: 各クラス内で`dynamic_cast`を使用（根本解決にならない）
- **Visitorパターン（推奨）**: `dynamic_cast`完全排除、約15行

### メリット

1. **拡張性**: 新しい演算子を追加する際、そのクラスに`compute_covariance_with()`を実装するだけで良い
2. **可読性**: 各型の処理がそのクラス内に集約される
3. **保守性**: 型固有のロジックが分散され、理解しやすくなる
4. **パフォーマンス**: `dynamic_cast`のチェーンが不要になる（仮想関数呼び出しに置き換え）

### 注意点

1. **双方向処理**: `covariance(a, b)`と`covariance(b, a)`の両方を考慮する必要がある
   - 解決策: `covariance()`関数で両方向を試す
2. **複雑なケース**: OpMAX0の組み合わせなど、複雑なケースは従来のロジックを維持
   - 解決策: 段階的な移行により、徐々に改善
3. **後方互換性**: 既存のコードとの互換性を保つ
   - 解決策: フォールバックメカニズムを維持

### 実装の優先順位

1. **高優先度**: OpADD, OpSUB（シンプルで頻繁に使用される）
2. **中優先度**: NormalImpl（基本型）
3. **低優先度**: OpMAX, OpMAX0（複雑だが、段階的に移行可能）

## 代替案の検討

### 案A: Visitorパターン

**メリット**:
- 型判定ロジックを完全に分離できる

**デメリット**:
- 双方向処理が複雑になる
- 実装コストが高い
- 既存コードへの影響が大きい

**結論**: 仮想関数の方が実用的

### 案B: 型ID + 関数テーブル

**メリット**:
- `dynamic_cast`が不要
- パフォーマンスが良い

**デメリット**:
- 型IDの管理が必要
- 型システムとの整合性が低い
- メンテナンスが困難

**結論**: 仮想関数の方が型安全で保守しやすい

## 実装ステップ

1. ✅ 設計ドキュメントの作成（このファイル）
2. ⏳ `RandomVariableImpl`に仮想関数を追加
3. ⏳ `OpADD`で実装（テスト付き）
4. ⏳ `OpSUB`で実装（テスト付き）
5. ⏳ `NormalImpl`で実装（テスト付き）
6. ⏳ `covariance()`関数をリファクタリング
7. ⏳ 既存テストがパスすることを確認
8. ⏳ OpMAX, OpMAX0の実装（段階的）
9. ⏳ dynamic_castチェーンの削除

