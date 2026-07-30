# Rust/C++ FFI TypeAhead PoC

Status: Experimental

Branch: `spike/rust-ffi-poc`

このブランチは技術検証専用であり、当面 `main` へマージしない。検証で成立した
設計を採用する場合も、製品向けの変更は別ブランチで小さく再実装する。

## 1. 目的

Qt と機種依存処理を C++ に残したまま、Qt 非依存の処理を Rust 静的
ライブラリへ移せることを確認する。最初の対象には副作用がなく、既存テストで
動作を比較しやすい `TypeAhead` を選んだ。

この PoC で確認する項目は次のとおり。

- CMake から Cargo を再現可能な設定で実行できる。
- `cxx` を介して C++/Qt と Rust の値を受け渡せる。
- Rust の panic とエラーを C++ 境界の内側で処理できる。
- Rust 実装を無効化し、既存 C++ 実装だけでビルドできる。
- Rust と Qt のテストを CTest と CI から実行できる。
- 配布物に Rust の共有ランタイムを追加せずに済む。

## 2. 構成

```text
QString / QStringList
        |
linux-qt/src/core/TypeAhead.cpp
  Qt 型と UTF-8 値の変換、エラー時の C++ フォールバック
        |
cxx generated bridge
        |
rust/crates/tfx-bridge
  panic の捕捉、FFI 用結果への変換
        |
rust/crates/tfx-core
  Qt/OS 非依存の TypeAhead ロジック
```

Rust コアは Qt 型と OS API に依存しない。CMake オプション
`TFX_ENABLE_RUST_CORE` の既定値は `OFF` であり、既存ビルドの動作を変えない。

Rust toolchain は `rust-toolchain.toml` で固定し、crate は `Cargo.lock` と
完全一致のバージョン指定で固定する。ライセンスはプロジェクトの
Apache-2.0 と両立するものに限定する。

## 3. 実行方法

既存 C++ 実装:

```sh
scripts/build.sh --tests
```

Rust コア有効:

```sh
scripts/build.sh --tests --rust-core
```

Rust 単体検査:

```sh
cargo fmt --all --check --manifest-path rust/Cargo.toml
cargo clippy --workspace --all-targets --all-features --locked \
  --manifest-path rust/Cargo.toml -- -D warnings
cargo test --workspace --locked --manifest-path rust/Cargo.toml
```

## 4. 検証結果

2026-07-30 に Linux x86_64、GCC 15.2、Qt 6 で確認した。

| 検証 | 結果 |
|---|---|
| Rust 単体テスト | 9件成功 |
| Debug、Rust 無効、CTest | 14件成功 |
| Debug、Rust 有効、CTest | 15件成功 |
| Release、Rust 有効 | ビルド成功 |
| Release、`rust_core` と `type_ahead` | 2件成功 |
| Release インストール | 成功 |
| `cargo fmt --check` | 成功 |
| `cargo clippy -D warnings` | 成功 |
| 動的リンク依存 | Rust 固有の共有ライブラリ追加なし |

未 strip の Release 実行ファイルは Rust 無効時 1,394,824 bytes、Rust 有効時
6,745,288 bytes だった。Rust 有効時は 5,350,464 bytes 増加している。
最終評価では配布時の strip、LTO、`panic` 設定を揃えて再計測する。

## 5. 判明した互換性要件

Qt の case-insensitive 比較は Unicode の full case folding ではなく simple
case folding 相当として扱う必要がある。例えば `STRASS` は `STRASSE` に
一致するが、`Straße` には一致しない。

Rust 側は `casefold` crate の simple folding を用い、この Qt の動作を
テストで固定した。また、Qt の `QString::size()` は UTF-16 code unit 数を
返すため、サロゲートペアを含む入力でも同じ prefix リセット条件になるよう
Rust 側で UTF-16 長を使用する。

## 6. 未完了事項と制約

- 検証対象は Linux のみで、Windows と macOS の CMake/リンカー設定は未確認。
- キー入力ごとに文字列一覧を UTF-8 へコピーするため、一覧が大きい場合の
  性能測定が必要。
- `cargo audit` と `cargo deny` は環境へ未導入で、脆弱性およびライセンスの
  自動監査は未実施。
- ASan、UBSan、ファズテストは未実施。
- 非 UTF-8 パスを失わない FFI 契約は TypeAhead では検証できない。
- Rust エラー時は C++ 実装へフォールバックするが、運用時の通知と
  telemetry 方針は未定。

## 7. 次の判断条件

この PoC ブランチは `main` へマージしない。製品向け実装を開始する前に、
少なくとも次を別の検証または設計レビューで完了する。

1. FFI 呼び出しの性能と配布物サイズを許容範囲として定義する。
2. 非 UTF-8 の Linux パスと Windows のネイティブパスを往復できる API を
   検証する。
3. `cargo audit`、`cargo deny`、ASan、UBSan を CI へ組み込む。
4. Rust 対応対象を副作用の小さい機能単位に分け、独立した実装ブランチとする。
