# ADR-0001: Qt/C++とRustの境界に`cxx`を使用する

Status: Accepted for PoC

Date: 2026-07-30

## Context

tfx for LinuxはQt Widgetsで実装されたC++アプリケーションである。UI、
`QObject`、signal/slot、Qt event loop、Linux desktop integrationはC++側に
残し、入力解析とファイル操作をRustへ段階的に移行する。

この移行の主目的は次の2点である。

- 外部入力、ファイルシステム、プロセス境界での安全性を高める。
- Qt型とOS固有処理をコアから分離し、他OSへ移植しやすくする。

言語境界そのものが新しいmemory safety上の弱点になってはならない。
特に、生pointer、buffer length、allocator、例外、panic、callback lifetimeを
手作業だけで整合させる方式は避けたい。

Rustライブラリはtfxの実行ファイルへ静的リンクする。外部アプリケーションや
pluginへ公開するABIは、現在の要件に含まれない。

## Decision

PoCおよび初期移行では、Qt/C++とRustの境界に`cxx` crateを使用する。

採用範囲は内部FFIのみとし、Qt型を`cxx` bridgeへ直接公開しない。Qtとbridgeの
間には、手書きの薄いC++ adapterを置く。

```text
Qt UI / QObject
       |
C++ Qt adapter
       |
cxx generated bridge
       |
Rust bridge crate
       |
pure Rust core crate
```

`cxx`の採用はPoCの完了条件を満たした時点で確定する。CMake、Cargo、GNU ld、
将来のMSVCとApple toolchainで重大な問題が確認された場合は、狭いC ABIへ
切り替える。

`cxx`は型とsignatureの不一致を減らすが、C++実装の動作、入力上限、thread
affinity、callback先の寿命まで自動的に保証するものではない。これらは本ADRの
contract testとcode reviewで保証する。

## 1. Crate構成

将来の実装では、少なくとも次の2 crateに分ける。

```text
rust/
  Cargo.toml
  Cargo.lock
  crates/
    tfx-core/
    tfx-bridge/
```

`tfx-core`:

- Qtと`cxx`に依存しない。
- parser、状態管理、ZIP検証、ファイル操作を実装する。
- OS差分はRustのtraitまたはtarget別moduleへ隔離する。
- 単体テスト、property test、fuzz targetの主対象とする。

`tfx-bridge`:

- `cxx::bridge`定義と変換処理だけを持つ。
- FFI入力を検証してから`tfx-core`を呼ぶ。
- panicを封じ込め、明示的なerrorへ変換する。
- business logicを持たない。

## 2. Qt型の扱い

次の型はbridgeへ渡さない。

- `QString`
- `QStringList`
- `QByteArray`
- `QFileInfo`
- `QDir`
- `QObject`
- `QVector`、`QList`、`QHash`

C++ adapterがQt型からbridgeの共有型へ変換する。Rustから返された値もadapterで
Qt型へ変換し、UIへ渡す。

bridgeでは、`cxx`が提供する次の型を基本とする。

- 固定幅整数
- shared structとshared enum
- `rust::Vec<T>` / Rust `Vec<T>`
- `rust::String` / Rust `String`
- call中だけ有効なslice

raw pointerは公開bridge APIで使用しない。将来opaque handleが必要になった
場合は`Box<T>`または`UniquePtr<T>`相当を使い、破棄責任を型で表す。

## 3. Path contract

ファイルシステムpathを`rust::String`で渡してはならない。Rustの`String`は
常にUTF-8だが、OS pathは同じ前提を持たないためである。

bridgeでは、pathを次の論理形式で表す。

```text
NativePath
  encoding: UnixBytes | WindowsUtf16Le
  data: Vec<u8>
```

規則:

- Unixでは、NULを含まないnative byte列を`data`へ格納する。
- Windowsでは、UTF-16 code unit列をlittle endianのbyte列として格納する。
- `encoding`とbuild targetが一致しない入力を拒否する。
- Windows表現はbyte数が偶数でなければ拒否する。
- path内のNULを拒否する。
- 表示用文字列と操作対象pathを別の値として扱う。
- pathをlogやerror messageへ埋め込む場合はlossyな表示であることを明確にする。

C++ adapterの初期変換:

- Linux/macOS: `QFile::encodeName()`の結果をUnix bytesとして渡す。
- Windows: `QString`のUTF-16 code unitをWindows形式として渡す。

Rust側:

- Unix: `OsStringExt::from_vec`相当で`OsString`へ変換する。
- Windows: `OsStringExt::from_wide`相当で`OsString`へ変換する。
- 内部APIは`Path`と`PathBuf`を使用し、UTF-8変換を前提にしない。

Qtのファイルモデル自体が`QString`を使うため、初期移行だけでUnix上の任意の
非UTF-8名を完全に扱えるようになるわけではない。初期目標は現行Qt対応範囲を
劣化させないこととする。

完全なbyte fidelityが必要になった場合は、Rust側でdirectoryを列挙し、UIには
表示名とopaque path handleを返す方式を別ADRで検討する。

## 4. String contract

path以外で、人間向けtextまたはprotocol上UTF-8と定義された値だけに
`rust::String`を使用する。

対象例:

- 検索語
- Git status label
- parse済みのCSV/TSV cell
- 内部diagnostic

C++からRustへ渡す前にUTF-8へ変換し、invalid sequenceを暗黙にreplacement
characterへ変換しない。lossy変換を行う場合は、API名またはresult flagで
明示する。

## 5. Error contract

通常の失敗にpanic、C++ exception、`cxx`の`Result<T>`変換を使用しない。
`cxx`の`Result<T>`はC++側でthrow/catchとして表現されるため、Qt adapterの
通常制御には使用しない。

共有errorは概念上、次の情報を持つ。

```text
Error
  code: ErrorCode
  os_code: i32
  diagnostic: String
```

初期`ErrorCode`:

- `None`
- `InvalidInput`
- `NotFound`
- `PermissionDenied`
- `AlreadyExists`
- `Conflict`
- `Cancelled`
- `ResourceLimit`
- `Io`
- `Unsupported`
- `Internal`

規則:

- `os_code`は利用できない場合0とする。
- `diagnostic`は開発者向けであり、UIの翻訳済みmessageとして扱わない。
- UI messageはC++側で`ErrorCode`と操作contextから生成する。
- pathやfile contentをdiagnosticへ無制限に含めない。
- success/failureは戻り値の明示的なtagまたは`ErrorCode::None`で判定する。

## 6. PanicとC++ exception

Rust panicとC++ exceptionを言語境界から外へ伝播させない。

Rust側:

- exported entry pointの内側で`std::panic::catch_unwind`を使用する。
- `panic=unwind`でbuildし、捕捉したpanicを`ErrorCode::Internal`へ変換する。
- panic payloadをC++へ返さない。
- panic後に途中状態を再利用せず、そのoperationを失敗として終了する。
- `Drop`自体がpanicしない設計にする。

C++側:

- Rustから呼ばれるadapter methodを`noexcept`にする。
- Qtまたは標準ライブラリからのexceptionをadapter内で捕捉する。
- C++ exceptionをRust frameへ到達させない。

`catch_unwind`はすべてのabortを捕捉できる保証ではないため、panicを通常の
error処理として使わない。boundary testでは意図的なpanicが`Internal`へ
変換されることを確認する。

## 7. Threading contract

初期移行ではRustに非同期runtimeを導入しない。

- Qt UI threadとworker `QThread`の管理はC++側が所有する。
- Rust APIは同期関数として提供する。
- 長時間処理はC++のworker threadからRustを呼ぶ。
- Rust threadから`QObject`やQt UIを直接操作しない。
- pureで短時間の関数だけをUI threadから呼んでよい。
- Rust側でglobal mutable stateを持たない。

この方針により、Qt event loopとRust runtimeの二重管理を避ける。

## 8. Progressとcancellation

ファイル操作では、C++側の`FileOperationWorker`を維持する。

Rustの同期operationへ、寿命がcall中に限定されたobserverを渡す。observerは
概念上、次の操作だけを提供する。

```text
is_cancelled() -> bool
on_progress(completed, total, path)
```

規則:

- observerをRust側へ保存しない。
- callbackはRust関数を呼んだworker thread上で実行する。
- callbackからUIを直接操作せず、Qt signalをemitする。
- callbackは`noexcept`かつ短時間で終了する。
- Rustはfile chunkまたはdirectory entryごとにcancelを確認する。
- callback先がRust call中に破棄されないことをC++側が保証する。
- Rustからcallbackが戻った後、そのreferenceを保持しない。

PoCでobserverの静的なlifetime表現が複雑すぎる場合は、operation handleと
polling方式を比較する。

## 9. Ownership contract

- shared structはvalueで渡す。
- 大きな入力はcall中だけ有効なimmutable sliceで渡す。
- Rustが返す`Vec`と`String`は`cxx`のwrapperに所有させる。
- pointerとlengthを別々の引数として公開しない。
- borrowed dataをcall終了後にRust側へ保存しない。
- allocatorをまたいでraw memoryを解放しない。
- opaque objectを導入する場合、作成・所有・破棄を同じ型で表現する。

入力の型がmemory safeでも、巨大なlengthはDoSになり得る。Rust coreへ渡す前に
bridgeで上限を検証する。

## 10. Build and dependency contract

- CMakeをtop-levelのbuild driverとして維持する。
- CargoはRust crateとRust単体テストを管理する。
- Rust libraryはアプリへ静的リンクする。
- `Cargo.lock`をcommitする。
- `cxx`、C++側generator、build helperは互換する同一versionへ固定する。
- generated sourceをrepositoryへcommitしない。
- CMake configure時ではなくbuild targetとしてCargoを実行する。
- Debug/Releaseとtarget tripleをCMakeからCargoへ明示的に対応付ける。
- CIでCargo cacheがなくても再現可能にbuildできることを確認する。

`cxx`には公式に推奨された単一のCMake構成がない。PoCでは、generated header、
generated C++ source、Rust static library、link orderを明示し、GNU ldで
成立することを確認する。将来のWindows/macOS jobでも同じ契約を検証する。

現在の開発環境にあるRust versionを、そのまま最低対応versionとはしない。
PoCで依存関係を固定した後、次を満たすversionを`rust-toolchain.toml`で固定する。

- `cxx`と監査toolが対応する。
- CI runnerと対象distributionで導入できる。
- security fixを継続的に取り込める。

## 11. Security and review rules

- `tfx-core`では原則として`unsafe`を禁止する。
- `unsafe`が必要な場合はbridgeまたはOS adapterへ隔離する。
- 各`unsafe` blockに、callerが満たすべきinvariantを記録する。
- bridge API変更時はC++とRust両側のcontract testを更新する。
- 新しいcrateはlicense、maintainer、advisory、transitive dependencyを監査する。
- `cargo audit`と`cargo deny`をCIへ追加する。
- Rust dependencyの更新は機能変更と分けてreviewする。

## 12. PoC acceptance criteria

`TypeAhead`を使ったPoCで次を確認する。

- CMakeからCargoとbridge generatorを再現可能に実行できる。
- C++17、Qt6、GNU ldでDebug/Release buildが成功する。
- C++からRust関数を呼び、Qt Testの既存test vectorが通る。
- Rust単体テストとQt Testを同じCI jobで実行できる。
- empty、Unicode、長い入力、invalid enumを安全に処理できる。
- Rust panicがbridge外へ伝播しない。
- sanitizer実行時に境界由来の問題がない。
- Rust機能を無効にしてC++実装へ戻せる。
- `Cargo.lock`とtoolchain pinによりclean buildを再現できる。

Path PoCでは追加で次を確認する。

- Linux native path bytesが往復で変化しない。
- NULとencoding mismatchを拒否する。
- UTF-8 pathと日本語pathの既存動作が変わらない。
- Windows UTF-16表現をLinux上の単体テストでもencode/decodeできる。

## Consequences

利点:

- 手書きC ABIよりraw pointerとmanual ownershipを減らせる。
- shared typeのlayoutと関数signatureを生成時に検証できる。
- Rust coreをQtから独立させられる。
- 内部APIであるため、bridgeを必要に応じて変更できる。

不利益:

- Cargoに加えてbridge code generationがbuildへ加わる。
- CMake統合、link order、cross compilationの検証が必要になる。
- `cxx`とgeneratorのversionを厳密に揃える必要がある。
- Qt型との変換copyは残る。
- path完全性はQt file modelの制約を超えて改善できない。

## Alternatives considered

### 手書きC ABIと`cbindgen`

外部向けのstable ABIやC callerが必要な場合には適している。しかし現在は
C++とRustだけが同時にbuildされる内部境界であり、raw pointer、length、
allocator、destructorを手作業で管理する利点が小さいため採用しない。

`cxx`のCMake統合がPoCで成立しない場合のfallbackとする。その場合もC ABIを
直接Qt UIから呼ばず、C++ RAII wrapperを必須とする。

### Qt型をRustへ直接bindingする

Qtのlifetime、meta-object、thread affinityをRust側へ持ち込むため採用しない。
Rust coreの移植性も失われる。

### RustでUIも書き直す

今回の目的と範囲に含まれず、既存Qt UIの回帰riskが大きいため採用しない。

### IPCでRust processを分離する

parser sandboxとして将来価値があるが、file operationの状態同期、配布、
latency、process lifecycleが複雑になるため、初期移行には採用しない。
PDFなど高risk parserの隔離手段としては別途検討できる。

## References

- [CXX: safe interop between Rust and C++](https://cxx.rs/)
- [CXX built-in bindings](https://cxx.rs/bindings.html)
- [CXX shared types](https://cxx.rs/shared.html)
- [CXX and CMake](https://cxx.rs/build/cmake.html)
- [CXX with other build systems](https://cxx.rs/build/other.html)
- [Rust Reference: external blocks and ABI](https://doc.rust-lang.org/reference/items/external-blocks.html)
- [Rust Reference: panic and unwinding](https://doc.rust-lang.org/reference/panic.html)
- [Rust `catch_unwind`](https://doc.rust-lang.org/std/panic/fn.catch_unwind.html)
- [Rust `OsString`](https://doc.rust-lang.org/std/ffi/os_str/struct.OsString.html)
- [Qt `QFile` path encoding](https://doc.qt.io/qt-6/qfile.html)
