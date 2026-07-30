# Rust/C++ FFI PoC

Status: Experimental

Branch: `spike/rust-ffi-poc`

このブランチは技術検証専用であり、当面 `main` へマージしない。検証で成立した
設計を採用する場合も、製品向けの変更は別ブランチで小さく再実装する。

## 1. 目的

Qt と機種依存処理を C++ に残したまま、Qt 非依存の処理を Rust 静的
ライブラリへ移せることを確認する。最初の対象には副作用がなく、既存テストで
動作を比較しやすい `TypeAhead` を選んだ。次に、移植性の前提となる
ネイティブパス表現と、外部入力を1回で受け渡せる `DelimitedText` parserを
検証した。

この PoC で確認する項目は次のとおり。

- CMake から Cargo を再現可能な設定で実行できる。
- `cxx` を介して C++/Qt と Rust の値を受け渡せる。
- Rust の panic とエラーを C++ 境界の内側で処理できる。
- Rust 実装を無効化し、既存 C++ 実装だけでビルドできる。
- Rust と Qt のテストを CTest と CI から実行できる。
- 配布物に Rust の共有ランタイムを追加せずに済む。

## 2. 構成

```text
QString / QStringList / DelimitedTable
        |
linux-qt/src/core/TypeAhead.cpp、DelimitedText.cpp
  Qt 型と UTF-8 値の変換、エラー時の C++ フォールバック
        |
cxx generated bridge
        |
rust/crates/tfx-bridge
  panic の捕捉、FFI 用結果への変換
        |
rust/crates/tfx-core
  TypeAhead、DelimitedText parser、target別のnative PathBuf変換
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

TypeAheadのC++/Rust比較:

```sh
scripts/benchmark_type_ahead.sh
```

`TFX_BENCHMARK_NAMES`と`TFX_BENCHMARK_ITERATIONS`で一覧件数と反復回数を
変更できる。benchmark targetは`TFX_BUILD_BENCHMARKS=ON`の場合だけ生成する。

DelimitedTextのC++/Rust比較:

```sh
scripts/benchmark_delimited_text.sh
```

`TFX_BENCHMARK_ROWS`、`TFX_BENCHMARK_COLUMNS`、
`TFX_BENCHMARK_ITERATIONS`で入力と反復回数を変更できる。

## 4. 検証結果

2026-07-30 に Linux x86_64、GCC 15.2、Qt 6 で確認した。

| 検証 | 結果 |
|---|---|
| Rust 単体テスト | 24件成功 |
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
| C++ adapterのASan、UBSan | path bridge、TypeAhead、DelimitedText成功 |
| GitHub Actions、sanitizer | ASan、UBSan境界テスト成功、1分3秒 |
| GitHub Actions、Rust無効 | 成功、2分19秒 |
| GitHub Actions、Rust有効 | format、lint、監査、build、test、install成功、7分1秒 |
| 動的リンク依存 | Rust 固有の共有ライブラリ追加なし |

GitHub Actionsの最終結果はworkflow dispatch run `30509639442`で確認した。
初回run `30506757284`で`actions/checkout@v4`のNode.js 20非推奨警告を確認した
ため、Node.js 24対応の`actions/checkout@v6`へ更新し、後続runで警告が解消した。

未 strip の Release 実行ファイルは Rust 無効時 1,394,824 bytes、Rust 有効時
6,745,288 bytes だった。Rust 有効時は 5,350,464 bytes 増加している。
最終評価では配布時の strip、LTO、`panic` 設定を揃えて再計測する。

## 5. 性能評価

Release buildで、一覧末尾の要素に一致する検索を反復した。これは一覧全体を
走査する条件であり、C++版とRust/cxx版で同じ公開関数を呼び出している。

| 一覧件数 | 反復 | C++、1呼び出し | Rust/cxx、1呼び出し | 比率 |
|---:|---:|---:|---:|---:|
| 1,000 | 5,000 | 11,498 ns | 115,116 ns | 10.0倍 |
| 10,000 | 1,000 | 102,903 ns | 1,096,612 ns | 10.7倍 |

現在のRust adapterはキー入力ごとに全`QStringList`をUTF-8へ変換し、
`rust::Vec<rust::String>`へコピーする。このコストが支配的である。

`TypeAhead`は外部入力の解析やファイル操作を行わず、Rust化によるセキュリティ
効果が小さいため、現在のstateless FFI方式は製品向けに採用しない。C++実装を
維持し、次のRust化候補は入力を1回で渡せるparserか、処理量の大きい機能を
優先する。

Rustで`TypeAhead`を再検討する場合は、一覧更新時だけRustへ転送するopaque
indexを設計し、所有権、更新通知、寿命を含む別PoCとして扱う。

DelimitedTextは1000行、100列、約1.4M UTF-16 code unitsのCSVを1回ずつ
解析した。100回反復した結果は次のとおり。

| 入力 | 反復 | C++、1解析 | Rust/cxx、1解析 | 比率 |
|---:|---:|---:|---:|---:|
| 1,000行 x 100列 | 100 | 21,139,483 ns | 20,423,919 ns | 0.966倍 |

入力全体を1回だけUTF-8へ変換し、結果をflatなcell列と行長列で返す境界では、
FFIを含む処理時間はC++版と同等だった。外部入力の解析をmemory-safeなcoreへ
移せるため、`DelimitedText`は製品向け再実装の候補とする。ただし、このPoC
ブランチ自体は`main`へマージしない。

## 6. 判明した互換性要件

Qt の case-insensitive 比較は Unicode の full case folding ではなく simple
case folding 相当として扱う必要がある。例えば `STRASS` は `STRASSE` に
一致するが、`Straße` には一致しない。

Rust 側は `casefold` crate の simple folding を用い、この Qt の動作を
テストで固定した。また、Qt の `QString::size()` は UTF-16 code unit 数を
返すため、サロゲートペアを含む入力でも同じ prefix リセット条件になるよう
Rust 側で UTF-16 長を使用する。

### DelimitedText

既存C++版と同じく、CSV/TSVのdelimiter、quoted field、escaped quote、
quoted newline、CRLF、空行、終端されていないquoteを処理する。生成した
256通りの非quoted tableをproperty-style testで往復し、Qt側の互換テストでも
改行、quote、空field、Unicode、TSVを確認した。

Rust coreはUTF-8入力を16 MiB、行数を10,000、列数を1,000、行数と列数の
積を1,000,000 cellsに制限する。製品のpreview側には別途4 MiB、
1,000行 x 100列のより小さい上限がある。負値、ゼロ、上限超過、不正な
delimiterは`InvalidInput`として拒否する。

`InvalidInput`時に制限のないC++ parserへ戻すとfail-openになるため、Qt側は
空tableとtruncation flagを返す。panic、内部エラー、FFI結果の構造不整合だけは
既存C++版へフォールバックし、Rustを無効化したbuildも維持する。

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

## 7. 未完了事項と制約

- 検証対象は Linux のみで、Windows と macOS の CMake/リンカー設定は未確認。
- TypeAheadのstateless FFIは性能評価で不採用としたが、PoCコードは比較と
  境界検証のため`TFX_ENABLE_RUST_CORE`の既定値`OFF`で残している。
- ASanとUBSanはC++/Qt adapterを検査したが、Rust code自体のsanitizerと
  ファズテストは未実施。
- DelimitedTextはproperty-style testを追加したが、継続的なfuzz targetと
  differential testは未実施。
- Windows UTF-16LE pathは入力検証のみで、Windows上の`PathBuf`往復は未確認。
- Qt UIで任意の非UTF-8名を識別するopaque path handleは未設計。
- Rustの内部エラー時はC++実装へフォールバックするが、運用時の通知と
  telemetry方針は未定。

## 8. 次の判断条件

この PoC ブランチは `main` へマージしない。製品向け実装を開始する前に、
少なくとも次を別の検証または設計レビューで完了する。

1. `DelimitedText`を別の製品ブランチで小さく再実装し、differential fuzzを
   追加してからC++版を削除する。
2. Windows上でUTF-16LE pathの往復とCMake/MSVCリンクを検証する。
3. 外部入力parserへ継続的なfuzz targetを追加する。
4. opaque path handleを含むUIとファイル操作の責任範囲を設計する。
5. Rust 対応対象を副作用の小さい機能単位に分け、独立した実装ブランチとする。
