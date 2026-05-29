# tfx for Linux

**Linux 向け Terminal-inspired interface File eXplorer**  
Version: **0.1.0**

[English](README.md) | 日本語

`tfx-for-linux` は、`tfx` の C++/Qt による Linux 版実装です。ターミナル風の見た目と、ファイル操作に集中しやすい 2 ペイン型ファイルマネージャを目指しています。

このリポジトリには Linux Qt 版のみを含めます。元の macOS SwiftUI 版のプロジェクトファイルは含めません。

## 必要環境

- Linux
- CMake 3.20 以上
- C++17 対応コンパイラ
- Qt 6 Widgets

## ビルド

```sh
cmake -S linux-qt -B /tmp/tfx-qt-build
cmake --build /tmp/tfx-qt-build
/tmp/tfx-qt-build/tfx-qt
```

フォルダを指定して起動する場合:

```sh
/tmp/tfx-qt-build/tfx-qt /path/to/folder
```

フォルダ指定がない場合は、起動時のカレントディレクトリを表示します。

## 現在の実装範囲

- フォルダツリーとピン留めフォルダ
- 単独表示 / スプリット表示のファイル一覧
- ソース表示 / レンダリング表示を切り替えられるプレビューペイン
- 内蔵コマンドペイン
- ウインドウ、splitter、表示状態、タブ、表示項目設定の復元
- ファイル操作: 開く、名前変更、新規ファイル/フォルダ、ゴミ箱、コピー/カット/ペースト、パスコピー
- ファイル一覧の表示項目:
  - ファイル名
  - 種類
  - サイズ
  - 作成日時
  - 更新日時
  - `drwxrwxr-x` 形式のファイルモード
  - Git ステータス

ファイル一覧ヘッダのメニューから、表示項目の表示/非表示と順序変更ができます。順序変更は設定ダイアログ内で項目名をドラッグ&ドロップして行います。

## リポジトリ

```text
https://github.com/fukuyori/tfx-for-linux.git
```

## ライセンス

Linux 版としてのライセンス表記は未確定です。
