#pragma once

class QMimeData;
class QString;

namespace tfx::filepane {

bool clipboardCanPaste(const QMimeData *mime);
QString placeholderName(const QString &language, const QString &english, const QString &japanese);

}
