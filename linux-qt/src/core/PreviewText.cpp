#include "core/PreviewText.h"

#include <QFile>

namespace tfx::core {

namespace {

// Number of bytes at the end of `data` that form an incomplete UTF-8
// sequence, or 0 when the data ends on a character boundary.
int incompleteUtf8TailLength(const QByteArray &data)
{
    const int size = data.size();
    for (int back = 1; back <= 4 && back <= size; ++back) {
        const uchar byte = static_cast<uchar>(data.at(size - back));
        if ((byte & 0x80) == 0x00) {
            return 0; // ASCII byte: complete
        }
        if ((byte & 0xC0) == 0xC0) {
            // Lead byte: complete only if its sequence fits in the tail.
            int expected = 0;
            if ((byte & 0xE0) == 0xC0) expected = 2;
            else if ((byte & 0xF0) == 0xE0) expected = 3;
            else if ((byte & 0xF8) == 0xF0) expected = 4;
            else return 0; // invalid lead; leave as-is for the decoder
            return expected > back ? back : 0;
        }
        // Continuation byte: keep scanning backwards.
    }
    return 0;
}

}

LoadedText loadTextCapped(const QString &path, qint64 capBytes)
{
    LoadedText result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return result;
    }
    result.ok = true;

    QByteArray data = file.read(qMax<qint64>(0, capBytes));
    result.truncated = !file.atEnd();
    if (result.truncated) {
        const int drop = incompleteUtf8TailLength(data);
        if (drop > 0) {
            data.chop(drop);
        }
    }
    // Normalize line endings the way QIODevice::Text would.
    data.replace("\r\n", "\n");
    result.text = QString::fromUtf8(data);
    return result;
}

}
