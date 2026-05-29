#include "UiText.h"

#include <QLocale>

namespace UiText {
bool isJapanese()
{
    return QLocale::system().language() == QLocale::Japanese;
}

QString t(const char *english, const char *japanese)
{
    return QString::fromUtf8(isJapanese() ? japanese : english);
}
}
