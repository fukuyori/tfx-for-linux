#include "core/SortOptions.h"

#include "UiText.h"

#include <QCollator>

namespace tfx::core {

QVector<SortOption> sortOptions()
{
    // Name/Size/Date Modified/Type/Natural first to match the sort menu of the
    // macOS build; the columns only the Linux port shows follow.
    return {
        {SortKey::Name, ColumnName, false},
        {SortKey::Size, ColumnSize, false},
        {SortKey::DateModified, ColumnModified, false},
        {SortKey::Type, ColumnType, false},
        {SortKey::Natural, ColumnName, true},
        {SortKey::DateCreated, ColumnCreated, false},
        {SortKey::FileMode, ColumnMode, false},
        {SortKey::GitStatus, ColumnGit, false},
    };
}

QString sortKeyLabel(SortKey key)
{
    switch (key) {
    case SortKey::Name:
        return UiText::t("Name", "名前");
    case SortKey::Size:
        return UiText::t("Size", "サイズ");
    case SortKey::DateModified:
        return UiText::t("Date Modified", "更新日時");
    case SortKey::Type:
        return UiText::t("Type", "種類");
    case SortKey::Natural:
        return UiText::t("Natural", "自然順");
    case SortKey::DateCreated:
        return UiText::t("Date Created", "作成日時");
    case SortKey::FileMode:
        return UiText::t("File Mode", "モード");
    case SortKey::GitStatus:
        return UiText::t("Git Status", "Git ステータス");
    }
    return QString();
}

SortOption sortOptionFor(SortKey key)
{
    const QVector<SortOption> options = sortOptions();
    for (const SortOption &option : options) {
        if (option.key == key) {
            return option;
        }
    }
    return options.first();
}

SortKey sortKeyFor(int column, bool natural)
{
    for (const SortOption &option : sortOptions()) {
        if (option.column == column && option.natural == natural) {
            return option.key;
        }
    }
    // Natural ordering only exists for names; any other column keeps its own
    // key regardless of the flag.
    for (const SortOption &option : sortOptions()) {
        if (option.column == column) {
            return option.key;
        }
    }
    return SortKey::Name;
}

int sortOptionIndex(SortKey key)
{
    const QVector<SortOption> options = sortOptions();
    for (int i = 0; i < options.size(); ++i) {
        if (options.at(i).key == key) {
            return i;
        }
    }
    return 0;
}

int naturalCompare(const QString &left, const QString &right)
{
    // QCollator carries per-instance ICU state; one per thread keeps the
    // comparison usable from the model's sort without repeated setup cost.
    static thread_local QCollator collator = []() {
        QCollator c;
        c.setNumericMode(true);
        c.setCaseSensitivity(Qt::CaseInsensitive);
        return c;
    }();

    const int collated = collator.compare(left, right);
    if (collated != 0) {
        return collated;
    }
    return QString::compare(left, right, Qt::CaseSensitive);
}

}
