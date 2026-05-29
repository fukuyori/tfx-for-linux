# tfx for Linux Qt

Version: **0.1.0**

このディレクトリは、`tfx-for-linux` の C++/Qt 6 Widgets 実装です。

## ビルド

```sh
cmake -S linux-qt -B /tmp/tfx-qt-build
cmake --build /tmp/tfx-qt-build
/tmp/tfx-qt-build/tfx-qt
```

## 実装範囲

- フォルダツリーとピン留めフォルダ
- 単独表示 / スプリット表示のファイル一覧
- ソース表示 / レンダリング表示を切り替えられるプレビューペイン
- 内蔵コマンドペイン
- ウインドウサイズ、表示状態、splitter 幅、タブ、ファイル一覧表示項目の復元
- ファイル一覧の表示項目:
  - ファイル名
  - 種類
  - サイズ
  - 作成日時
  - 更新日時
  - ファイルモード
  - Git ステータス

元の macOS SwiftUI 版ソースは、この Linux Qt 実装には含めません。
