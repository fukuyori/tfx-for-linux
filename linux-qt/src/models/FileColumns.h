#pragma once

// Logical columns shown in the file list. Columns beyond what QFileSystemModel
// provides (Created/Modified/Mode/Git) are synthesised by FileSystemProxyModel.
enum FileColumn {
    ColumnName = 0,
    ColumnType,
    ColumnSize,
    ColumnCreated,
    ColumnModified,
    ColumnMode,
    ColumnGit,
    kColumnCount
};
