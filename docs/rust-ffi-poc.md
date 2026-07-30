# Rust/C++ FFI PoC

Status: Experimental

Branch: `spike/rust-ffi-poc`

このブランチは技術検証専用であり、当面 `main` へマージしない。検証で成立した
設計を採用する場合も、製品向けの変更は別ブランチで小さく再実装する。

## 1. 目的

Qt と機種依存処理を C++ に残したまま、Qt 非依存の処理を Rust 静的
ライブラリへ移せることを確認する。最初の対象には副作用がなく、既存テストで
動作を比較しやすい `TypeAhead` を選んだ。次に、移植性の前提となる
ネイティブパス表現を検証した。

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
  TypeAhead ロジック、target別のnative PathBuf変換
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
cargo audit --file rust/Cargo.lock
cargo deny --manifest-path rust/Cargo.toml --config rust/deny.toml --locked check
```

## 4. 検証結果

2026-07-30 に Linux x86_64、GCC 15.2、Qt 6 で確認した。

| 検証 | 結果 |
|---|---|
| Rust 単体テスト | 16件成功 |
| Debug、Rust 無効、CTest | 14件成功 |
| Debug、Rust 有効、CTest | 16件成功 |
| Release、Rust 有効、CTest | 16件成功 |
| Release、Rust 有効 | ビルド成功 |
| Release、`rust_core` と `type_ahead` | 2件成功 |
| Release インストール | 成功 |
| `cargo fmt --check` | 成功 |
| `cargo clippy -D warnings` | 成功 |
| `cargo audit 0.22.2` | 35依存crate、既知脆弱性なし |
| `cargo deny 0.20.2` | advisories、bans、licenses、sources成功 |
| C++ adapterのASan、UBSan | path bridgeとTypeAhead成功 |
| GitHub Actions、Rust無効 | 成功、1分52秒 |
| GitHub Actions、Rust有効 | format、lint、監査、build、test、install成功、5分38秒 |
| 動的リンク依存 | Rust 固有の共有ライブラリ追加なし |

GitHub Actionsはworkflow dispatch run `30507151086`で確認した。初回run
`30506757284`で`actions/checkout@v4`のNode.js 20非推奨警告を確認したため、
Node.js 24対応の`actions/checkout@v6`へ更新し、再実行で警告が解消した。

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

### ネイティブパス

Linuxでは`NativePath { encoding: UnixBytes, data: Vec<u8> }`を使用し、
Rust側でNUL、1 MiBを超える入力、実行OSと異なるencodingを拒否する。
Windows表現はUTF-16LEのbyte列とし、奇数長とNUL code unitを拒否する。

Linux上で`0xFF`を含む実在ファイル名を作成し、C++のnative `QByteArray`から
Rustの`PathBuf`を経由して同一byte列へ戻せることを結合テストで確認した。

同じpathを`QFile::decodeName()`で`QString`へ変換すると、Qt 6の
`QFileInfo`から元のファイルを再参照できなかった。完全なbyte fidelityが必要な
処理では`QString`を識別子として使わず、platform adapterまたはRust側がnative
bytesを保持する必要がある。

## 6. 未完了事項と制約

- 検証対象は Linux のみで、Windows と macOS の CMake/リンカー設定は未確認。
- キー入力ごとに文字列一覧を UTF-8 へコピーするため、一覧が大きい場合の
  性能測定が必要。
- ASanとUBSanはC++/Qt adapterを検査したが、Rust code自体のsanitizerと
  ファズテストは未実施。
- Windows UTF-16LE pathは入力検証のみで、Windows上の`PathBuf`往復は未確認。
- Qt UIで任意の非UTF-8名を識別するopaque path handleは未設計。
- Rust エラー時は C++ 実装へフォールバックするが、運用時の通知と
  telemetry 方針は未定。

## 7. 次の判断条件

この PoC ブランチは `main` へマージしない。製品向け実装を開始する前に、
少なくとも次を別の検証または設計レビューで完了する。

1. FFI 呼び出しの性能と配布物サイズを許容範囲として定義する。
2. Windows上でUTF-16LE pathの往復とCMake/MSVCリンクを検証する。
3. GitHub Actions上でASanとUBSanを継続実行する。
4. opaque path handleを含むUIとファイル操作の責任範囲を設計する。
5. Rust 対応対象を副作用の小さい機能単位に分け、独立した実装ブランチとする。
