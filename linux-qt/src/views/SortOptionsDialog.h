#pragma once

#include "core/SortOptions.h"

#include <QDialog>
#include <Qt>

class QLabel;
class QListWidget;

namespace tfx::views {

// Keyboard-driven sort chooser: a terminal-styled popup listing the sort keys
// with a ">" cursor on the highlighted row. Up/Down (or k/j) move, Space (or
// Left/Right) flips the direction shown in the corner, Enter applies and Esc
// cancels.
class SortOptionsDialog : public QDialog
{
    Q_OBJECT

public:
    SortOptionsDialog(tfx::core::SortKey currentKey,
                      Qt::SortOrder currentOrder,
                      QWidget *parent = nullptr);

    tfx::core::SortKey selectedKey() const;
    Qt::SortOrder selectedOrder() const { return m_order; }

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void toggleOrder();
    void resizeToContents();
    void refreshRows();
    void refreshHint();

    QListWidget *m_list = nullptr;
    QLabel *m_hint = nullptr;
    Qt::SortOrder m_order = Qt::AscendingOrder;
};

}
