#include "FilePane.h"
#include "UiText.h"
#include "models/FileColumns.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QDir>
#include <QEvent>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListView>
#include <QTableView>
#include <QTimer>

bool FilePane::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_pathEdit) {
        if (event->type() == QEvent::FocusIn) {
            m_pathEdit->setText(m_currentPath);
            m_pathEdit->selectAll();
            emit activated(this);
        }
        if (event->type() == QEvent::FocusOut) {
            m_pathEdit->setText(displayPath(m_currentPath));
            showBreadcrumb();
        }
        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                m_pathEdit->setText(displayPath(m_currentPath));
                showBreadcrumb();
                m_view->setFocus();
                return true;
            }
        }
    }
    if (watched == m_view || watched == m_iconView || watched == m_searchView) {
        if (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress) {
            emit activated(this);
        }
        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (watched == m_searchView) {
                if (keyEvent->key() == Qt::Key_Escape) {
                    cancelSearch();
                    focusFileList();
                    return true;
                }
                if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                    const QModelIndex current = m_searchView->currentIndex();
                    if (current.isValid()) {
                        emit m_searchView->activated(current);
                    }
                    return true;
                }
                if (keyEvent->key() == Qt::Key_Backspace) {
                    cancelSearch();
                    goUp();
                    return true;
                }
            }
            if (keyEvent->key() == Qt::Key_Down
                && !m_view->currentIndex().isValid()
                && m_view->selectionModel()->selectedIndexes().isEmpty()) {
                if (selectParentEntry()) {
                    return true;
                }
            }
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                // With an inline-rename editor open, QLineEdit ignores Enter
                // and the key bubbles up to the view while the delegate
                // commits the edit via a queued call. Consume it here so
                // Enter only confirms the new name instead of also opening
                // the renamed folder.
                QWidget *focus = QApplication::focusWidget();
                auto *view = qobject_cast<QWidget *>(watched);
                if (focus && view && focus != view && view->isAncestorOf(focus)) {
                    return true;
                }
                openSelected();
                return true;
            }
            if (keyEvent->key() == Qt::Key_Backspace) {
                goUp();
                return true;
            }
            // Quick navigation keys (checked before type-to-select): "~"
            // jumps to the home directory, "/" to the filesystem root.
            if (keyEvent->text() == QLatin1String("~")) {
                navigateTo(QDir::homePath());
                return true;
            }
            if (keyEvent->text() == QLatin1String("/")) {
                navigateTo(QStringLiteral("/"));
                return true;
            }
            if ((watched == m_view || watched == m_iconView)
                && handleTypeAheadKey(qobject_cast<QAbstractItemView *>(watched), keyEvent)) {
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

// Explorer/Finder-style type-to-select. Printable keys build a prefix within
// a 1-second window and jump to the first row whose name starts with it;
// repeating one character cycles through the rows with that initial (wrapping
// at the end, so files sorted behind the folder block stay reachable); a key
// that matches nothing keeps the current prefix and selection.
bool FilePane::handleTypeAheadKey(QAbstractItemView *view, const QKeyEvent *event)
{
    if (!view || !view->model()) {
        return false;
    }
    if (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) {
        return false;
    }
    const QString typed = event->text();
    if (typed.size() != 1 || !typed.at(0).isPrint()) {
        return false;
    }

    if (!m_typeAheadClock.isValid()) {
        m_typeAheadClock.start();
    }
    const qint64 now = m_typeAheadClock.elapsed();
    if (m_typeAheadLastMs < 0 || now - m_typeAheadLastMs > 1000) {
        m_typeAheadPrefix.clear();
    }
    m_typeAheadLastMs = now;

    QAbstractItemModel *model = view->model();
    const QModelIndex root = view->rootIndex();
    const int rows = model->rowCount(root);
    const auto nameAt = [&](int row) {
        return model->index(row, ColumnName, root).data(Qt::DisplayRole).toString();
    };
    const auto selectRow = [&](int row) {
        const QModelIndex index = model->index(row, ColumnName, root);
        selectProxyIndex(index);
        if (view == m_iconView) {
            m_iconView->scrollTo(index);
        }
    };

    if (rows > 0) {
        if (m_typeAheadPrefix.size() == 1
            && m_typeAheadPrefix.compare(typed, Qt::CaseInsensitive) == 0) {
            // Same single character again: cycle to the next row with that
            // initial, starting after the current row and wrapping.
            const int currentRow = view->currentIndex().isValid() ? view->currentIndex().row() : -1;
            for (int step = 1; step <= rows; ++step) {
                const int row = (currentRow + step + rows) % rows;
                if (nameAt(row).startsWith(m_typeAheadPrefix, Qt::CaseInsensitive)) {
                    selectRow(row);
                    break;
                }
            }
        } else {
            const QString candidate = m_typeAheadPrefix + typed;
            for (int row = 0; row < rows; ++row) {
                if (nameAt(row).startsWith(candidate, Qt::CaseInsensitive)) {
                    selectRow(row);
                    m_typeAheadPrefix = candidate;
                    break;
                }
            }
            // No match: the prefix and the selection stay where they are.
        }
    }

    if (!m_typeAheadStatusTimer) {
        m_typeAheadStatusTimer = new QTimer(this);
        m_typeAheadStatusTimer->setSingleShot(true);
        m_typeAheadStatusTimer->setInterval(1000);
        connect(m_typeAheadStatusTimer, &QTimer::timeout, this, [this]() {
            updateStatusLine();
        });
    }
    if (!m_typeAheadPrefix.isEmpty()) {
        // selectProxyIndex() refreshed the status line; overwrite it with the
        // active prefix for the duration of the 1-second window.
        m_statusLabel->setText(UiText::t(" Find: %1", " 検索: %1").arg(m_typeAheadPrefix));
        m_typeAheadStatusTimer->start();
    }
    // Consume the keystroke either way so it doesn't fall through.
    return true;
}

void FilePane::resetTypeAhead()
{
    m_typeAheadPrefix.clear();
    m_typeAheadLastMs = -1;
    if (m_typeAheadStatusTimer && m_typeAheadStatusTimer->isActive()) {
        m_typeAheadStatusTimer->stop();
        updateStatusLine();
    }
}
