#pragma once

#include <QString>

namespace tfx::core {

struct LoadedText
{
    QString text;
    bool truncated = false;
    bool ok = false;
};

// Read up to `capBytes` of UTF-8 text from `path`. When the file is larger,
// `truncated` is set and an incomplete trailing UTF-8 sequence is dropped so
// the cut never produces a replacement character mid-glyph.
LoadedText loadTextCapped(const QString &path, qint64 capBytes);

}
