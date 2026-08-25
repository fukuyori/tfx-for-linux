#include "views/SortOptionsDialog.h"

#include "UiText.h"

#include <QFontMetrics>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

using namespace tfx::core;

namespace tfx::views {

SortOptionsDialog::SortOptionsDialog(SortKey currentKey, Qt::SortOrder currentOrder, QWidget *parent)
    : QDialog(parent)
    , m_order(currentOrder)
{
    setObjectName("sortOptionsDialog");
    setWindowTitle(UiText::t("Sort Options", "ソート設定"));
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setModal(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 10);
    layout->setSpacing(8);

    auto *title = new QLabel(UiText::t("Sort Options", "ソート設定"), this);
    title->setObjectName("sortOptionsTitle");
    layout->addWidget(title);

    m_list = new QListWidget(this);
    m_list->setObjectName("sortOptionsList");
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Off so the width measurement below sees every row, not just the first.
    m_list->setUniformItemSizes(false);
    for (const SortOption &option : sortOptions()) {
        auto *item = new QListWidgetItem(m_list);
        item->setData(Qt::UserRole, static_cast<int>(option.key));
    }
    m_list->setCurrentRow(sortOptionIndex(currentKey));
    layout->addWidget(m_list);

    m_hint = new QLabel(this);
    m_hint->setObjectName("sortOptionsHint");
    m_hint->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(m_hint);

    connect(m_list, &QListWidget::currentRowChanged, this, [this]() { refreshRows(); });
    connect(m_list, &QListWidget::itemActivated, this, &QDialog::accept);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &QDialog::accept);

    refreshRows();
    refreshHint();
}

SortKey SortOptionsDialog::selectedKey() const
{
    const QListWidgetItem *item = m_list->currentItem();
    if (!item) {
        return SortKey::Name;
    }
    return static_cast<SortKey>(item->data(Qt::UserRole).toInt());
}

void SortOptionsDialog::refreshRows()
{
    // The cursor is part of the row text so it lines up with the labels in a
    // monospace font, the way a terminal menu marks its selection.
    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem *item = m_list->item(row);
        const auto key = static_cast<SortKey>(item->data(Qt::UserRole).toInt());
        const QString cursor = row == m_list->currentRow() ? QStringLiteral("❯ ")
                                                           : QStringLiteral("  ");
        item->setText(cursor + sortKeyLabel(key));
    }
}

void SortOptionsDialog::refreshHint()
{
    const QString direction = m_order == Qt::AscendingOrder
        ? UiText::t("▲ ASC", "▲ 昇順")
        : UiText::t("▼ DESC", "▼ 降順");
    m_hint->setText(direction);
}

void SortOptionsDialog::toggleOrder()
{
    m_order = m_order == Qt::AscendingOrder ? Qt::DescendingOrder : Qt::AscendingOrder;
    refreshHint();
}

void SortOptionsDialog::resizeToContents()
{
    // Sizing happens once the dialog is shown, after the stylesheet has been
    // applied. The width comes from the view's own column hint rather than
    // QWidget::font(), which does not reflect a stylesheet-set font and would
    // measure the same width whatever size the theme asks for.
    int widest = m_list->sizeHintForColumn(0);
    const QFontMetrics hintMetrics(m_hint->font());
    widest = qMax(widest, hintMetrics.horizontalAdvance(m_hint->text()));

    // Fixed rather than minimum: QAbstractScrollArea hands out a 256px default
    // size hint that has nothing to do with the content, and the layout would
    // otherwise hold the popup at that width whatever the labels measure.
    const int itemPadding = 16;
    m_list->setFixedWidth(widest + itemPadding);

    const int rowHeight = m_list->sizeHintForRow(0);
    m_list->setFixedHeight(rowHeight * m_list->count() + 2 * m_list->frameWidth());

    // The measured labels decide the width; no floor, so the popup stays as
    // compact as its longest entry allows.
    adjustSize();
}

void SortOptionsDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    resizeToContents();
    m_list->setFocus();
    if (QWidget *anchor = parentWidget()) {
        const QPoint center = anchor->mapToGlobal(anchor->rect().center());
        move(center.x() - width() / 2, center.y() - height() / 2);
    }
}

void SortOptionsDialog::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Space:
    case Qt::Key_Left:
    case Qt::Key_Right:
        toggleOrder();
        return;
    case Qt::Key_Down:
    case Qt::Key_J:
        m_list->setCurrentRow(qMin(m_list->currentRow() + 1, m_list->count() - 1));
        return;
    case Qt::Key_Up:
    case Qt::Key_K:
        m_list->setCurrentRow(qMax(m_list->currentRow() - 1, 0));
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        accept();
        return;
    default:
        break;
    }

    // Digits jump straight to a row, so a key can be picked without arrowing.
    if (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_9) {
        const int row = event->key() - Qt::Key_1;
        if (row < m_list->count()) {
            m_list->setCurrentRow(row);
        }
        return;
    }

    QDialog::keyPressEvent(event);
}

}
