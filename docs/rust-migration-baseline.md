# Rust 移行 Phase 0 ベースライン評価

Status: Initial assessment complete

Assessment date: 2026-07-30

## 1. この文書の目的

この文書は、C++/Qt から Rust への段階移行を開始する前の技術的な
ベースラインを記録する。製品コードの変更案ではなく、現行実装の依存関係、
既存の防御、未対応リスク、テスト状況を整理したものである。

移行の目的は次の2点である。

- 外部入力とファイルシステムを扱う処理の安全性を高める。
- Qt と OS 固有処理をコアロジックから分離し、他 OS へ移植しやすくする。

## 2. 調査範囲とベースライン

調査対象:

- `linux-qt/src/core`
- `linux-qt/src/controllers`
- `linux-qt/src/platform`
- ZIP、外部コマンド、端末、ユーザーコマンドの呼び出し元
- `linux-qt/tests`
- CMake、GitHub Actions、ビルドスクリプト

コード規模の概算:

| 対象 | 行数 |
|---|---:|
| C++ ヘッダーと実装全体 | 14,600 |
| core、controllers、platform | 1,775 |
| Qt Test | 1,991 |

テストベースライン:

```text
Build type: Debug
Platform: Linux
Test suites: 14
Passed: 14
Failed: 0
```

テストはワークスペース外の一時ビルドディレクトリで実行した。

## 3. 現在の境界

`src/core`、`src/controllers`、`src/platform` というディレクトリ分割は
存在するが、依存関係の境界としては未完成である。

- `core` は `QString`、`QFile`、`QDir`、`QFileInfo` に依存している。
- ファイル操作は `readlink(2)`、`symlink(2)`、`lstat(2)`、
  `utimensat(2)`、`rename(2)` を直接利用している。
- `Platform.h` の公開 API は `QString` と `QStringList` を利用しており、
  Qt 非依存のプラットフォーム境界ではない。
- CMake は macOS と Windows のソースパスを選択するが、現在存在する実装は
  `platform/linux/PlatformLinux.cpp` のみである。
- ZIP 処理は `zip` と `unzip`、PDF プレビューは `pdftoppm`、Git 状態取得は
  `git` に依存する。
- 端末処理には `/bin/sh`、`/proc`、`tmux` など Linux/Unix 前提がある。

このため、最初からRustコアを完全にOS非依存にするのではなく、Qtアダプター、
Rustコア、Platform実装の責任範囲を先に固定する必要がある。

## 4. 評価基準

Qt依存:

- 低: 単純な文字列・配列の変換のみ
- 中: Qtの文字列規則やファイル情報に依存
- 高: `QObject`、signal/slot、Qt I/O、UIに依存

OS依存:

- 低: OS非依存の計算
- 中: パスやファイルシステムの意味に依存
- 高: POSIX、DBus、外部コマンド、`/proc`などに依存

セキュリティ影響:

- 低: UI状態や表示上の不整合が中心
- 中: 信頼できない入力を扱うが、直接の書き込みはない
- 高: ファイル書き込み、削除、プロセス起動、境界検証を行う

優先度:

- P0: FFIとビルドを検証するPoC
- P1: セキュリティ上、早期移行または再設計が必要
- P2: 境界確立後に移行する
- P3: Qt/OS側に残すか、他OS対応時に再評価する

## 5. モジュール評価

| 対象 | Qt依存 | OS依存 | セキュリティ影響 | 既存テスト | 優先度 | 方針 |
|---|---|---|---|---|---|---|
| `TypeAhead` | 低 | 低 | 低 | 高 | P0 | 最初のFFI PoC候補 |
| `SearchState` | 低 | 低 | 低 | 高 | P2 | Rustの純粋ロジックへ移行 |
| `DelimitedText` | 低 | 低 | 中 | 高 | P1 | 上限を維持してRustへ移行 |
| `PreviewText` | 中 | 中 | 中 | 高 | P1 | 読み込み上限と文字コードを仕様化 |
| `GitService` | 中 | 中 | 高 | 中 | P1 | 解析とパス検証をRustへ移行 |
| `TabState` | 中 | 中 | 低 | 高 | P2 | パス抽象の確立後に移行 |
| `SidebarLogic` | 低 | 高 | 低 | 高 | P3 | Platform capabilityへ分離 |
| `FileTypeInfo` | 高 | 高 | 低 | 間接 | P3 | Qt表示層に残す候補 |
| `FileOperations` | 高 | 高 | 高 | 高 | P1 | Rustファイル操作基盤へ移行 |
| `FileOperationWorker` | 高 | 高 | 高 | 高 | P1 | `QObject`を残しエンジンを分離 |
| `GitStatusController` | 高 | 中 | 高 | 間接 | P2 | `QProcess`ラッパーを残し解析を分離 |
| `PlatformLinux` ZIP処理 | 高 | 高 | 高 | 中 | P1 | RustのZIP検査・展開へ置換候補 |
| `PlatformLinux` desktop処理 | 高 | 高 | 中 | 低 | P3 | C++/Qt Platform実装に残す |
| `FilePaneArchive` | 高 | 高 | 高 | 間接 | P1 | UIを残しアーカイブ処理を分離 |
| `FilePaneCommands` | 高 | 高 | 高 | 間接 | P3 | 明示的なシェル機能として隔離 |
| `TerminalPane` | 高 | 高 | 高 | 低 | P3 | Linux固有UIとして残す |

「間接」は、上位のUIテストまたは関連するヘルパーのテストはあるが、
対象クラス単独の異常系テストが限定的であることを示す。

## 6. 既存のセキュリティ対策

現行実装には、Rust移行後も維持すべき防御が既に存在する。

### ファイル操作

- シンボリックリンクをリンクとしてコピーし、リンク先を再帰しない。
- 隠しファイルとsystem entryをコピー対象に含める。
- ファイルを1 MiB単位でコピーし、チャンク間でキャンセルを確認する。
- キャンセル時に作成済みの出力ルートを削除する。
- 上書き時に一時パスへコピーし、`rename(2)`で置き換える。
- ディレクトリ上書き失敗時に旧ディレクトリを戻す。
- read、write、flush、closeの失敗を成功扱いしない。
- コピー元内部への再帰コピーをcanonical pathで検査する。

### ZIP

- 絶対パス、`..`、Windows drive prefix、先頭`-`を拒否する。
- シンボリックリンクを含むアーカイブの全展開を拒否する。
- 全展開を100,000 entries、展開後合計4 GiBに制限する。
- 一時ディレクトリへ展開し、成功後に最終名へrenameする。
- `unzip`と`pdftoppm`の待機時間に上限がある。

### 入力と外部プロセス

- テキストプレビューを4 MiBに制限する。
- CSV/TSV表示を1,000行、100列に制限する。
- ユーザーコマンド出力を標準出力・標準エラーごとに1 MiBへ制限する。
- Gitプロセスにwatchdogがあり、対象ディレクトリをcanonicalizeする。
- 外部コマンドは原則としてプログラムと引数列を分けて起動する。

## 7. リスク登録

| ID | リスク | 影響 | 現在の対策 | 追加検討 |
|---|---|---|---|---|
| R1 | ZIP一覧出力によるメモリ消費 | 高 | entry数・展開サイズを後段で検査 | parser入力自体のbyte上限、streaming解析 |
| R2 | ZIP名の改行・文字コード・表示形式差 | 高 | path検査、`zipinfo`形式の正規表現 | 外部コマンド出力ではなくZIPライブラリを評価 |
| R3 | 深いディレクトリによる再帰・走査負荷 | 高 | symlinkは再帰しない | 深度、entry数、総byte数、時間の上限 |
| R4 | ファイル検査後の差し替え（TOCTOU） | 高 | canonical path検査 | directory handle相対操作、再検証 |
| R5 | 非UTF-8ファイル名の損失 | 高 | native bytesのFFI往復をPoC済み | QStringを避けるopaque path識別子を設計 |
| R6 | Git quoted pathの不完全な復号 | 中 | quote除去、directory escape検査 | porcelain `-z`とNUL区切り解析を評価 |
| R7 | 外部ツールのPATH・版・形式差 | 中 | `findExecutable`、timeout | capability検査、Rustライブラリへの置換 |
| R8 | FFIのpointer・所有権・callback寿命 | 高 | `cxx`、panic封じ込め、境界テスト | callbackとworkerの寿命を検証 |
| R9 | Rust依存関係の供給網 | 中 | lockfile、`cargo audit`、`cargo deny`をCI確認済み | 監査tool更新方針 |
| R10 | 他OSでのパス・権限・リンク差 | 高 | CMakeの分岐のみ | Platform APIとOS別contract test |
| R11 | ユーザー設定によるシェル実行 | 高 | quote処理、出力上限、cwd検証 | 信頼境界を明示しPlatform層へ隔離 |
| R12 | PDFなど複雑なparserの攻撃面 | 中 | 外部プロセスとtimeout | sandboxまたは維持方針を決定 |

R1からR6はRust化対象の仕様に直接反映する。R8はRust導入によって新しく
生じるリスクであり、PoC完了条件に含める。

## 8. テストの強みと不足

強み:

- ファイルコピー、移動、キャンセル、上書き、失敗時復旧の回帰テストがある。
- file、directory、relative link、directory link、broken linkを検証している。
- hidden file、timestamp、write failureを検証している。
- ZIP path traversal、drive prefix、symlink、uncompressed sizeを検証している。
- preview size cap、UTF-8途中切断、CSV quoteと行列上限を検証している。
- Git path escapeとsubdirectory prefixを検証している。

Rust移行前または移行と同時に追加するテスト:

- 非UTF-8ファイル名と不正Unicode変換
- 改行、NUL相当、引用、非常に長いZIP entry名
- ZIP一覧出力そのもののbyte上限
- 非常に深いdirectory treeと非常に多いentry
- 検査とopen/rename間で対象を差し替える競合
- cross-filesystem moveと中断時復旧
- ACL、extended attributes、sparse fileの扱いを保証するか否か
- Windows予約名、separator、大小文字非区別
- callback先の`QObject`破棄と同時にキャンセルされる場合
- FFIへ不正pointer、長さ、enum値を渡した場合

現在のCIはUbuntuのみで、通常のconfigure、build、test、installを行う。
sanitizer、ファズ、Windows、macOSのjobはまだない。

## 9. 推奨移行順

### Step 1: FFI設計のADR

- C ABIと`cxx` crateを比較する。
- path、error、buffer、ownership、thread、callbackを決定する。
- Rust toolchainの最低対応版を決定する。

初期決定は
[ADR-0001: Qt/C++とRustの境界にcxxを使用する](adr/0001-rust-cxx-boundary.md)
を参照する。toolchainの最低対応版はPoCで依存関係を固定した後に確定する。

### Step 2: `TypeAhead` PoC

- Qt型からFFI型への変換を最小の機能で検証する。
- C++版とRust版へ同じtest vectorを適用する。
- Rust無効時にC++版へ戻せることを確認する。

### Step 3: ZIP検査・展開

- RustのZIP crateを機能、保守状況、ライセンスで比較する。
- entry metadataをstreamingまたは上限付きで処理する。
- 現行のpath、symlink、entry数、展開サイズ制限を移植する。
- 攻撃用corpusとファズテストを追加する。

### Step 4: ファイル操作エンジン

- `FileOperationWorker`のQt signal/slotはC++に残す。
- copy、move、replace、rollback、cancelをRustへ移す。
- 現行Qt Testを言語境界越しのcontract testとして維持する。

### Step 5: parserと状態管理

- Gitはporcelain `-z`によるbyte-safeな解析を優先検討する。
- `DelimitedText`、`PreviewText`、`SearchState`などを順次移行する。

### Step 6: Platform境界

- desktop integrationとterminalをcapabilityとして定義する。
- Linux固有処理をPlatform実装へ集約する。
- Windows/macOS実装とOS別contract testを追加する。

## 10. 実装前に決定する事項

| 決定事項 | 選択肢 | 推奨する次の作業 |
|---|---|---|
| 最初に対応する追加OS | Windows / macOS | 利用優先度と必須機能を決める |
| パス完全性 | UTF-8限定 / OS native | OS nativeを前提にPoCする |
| FFI方式 | C ABI / `cxx` | 小さな比較PoCで決定する |
| ZIP実装 | 外部`unzip` / Rust crate | crate候補の安全性と互換性を評価する |
| Rust toolchain | current stable / MSRV固定 | CI・配布対象OSからMSRVを決定する |
| source compatibility | 一時二重実装 / 即時切替 | 一時二重実装とfeature toggleを採用する |
| 第三者表示 | LICENSEのみ / NOTICE併用 | 依存確定後にNOTICE要否を監査する |

## 11. Phase 0の判定

初期ベースライン調査は完了した。次フェーズへ進む前に必要な成果物は、
FFIとパス表現を決めるADR、および`TypeAhead`を使った破棄可能なPoCである。

製品機能のRust移行は、PoCで次を確認した後に開始する。

- CMake、Cargo、Qt Test、Rust testが同じビルドで成立する。
- pathとerrorの変換で情報を失わない。
- panic、例外、memory ownershipが言語境界を越えない。
- feature toggleでC++実装へ戻せる。
